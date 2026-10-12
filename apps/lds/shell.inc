COLOR_BLACK = 0
COLOR_BLUE = 1
COLOR_LIGHT_GREEN = 10
COLOR_LIGHT_CYAN = 11
COLOR_LIGHT_RED = 12
COLOR_YELLOW = 14
COLOR_WHITE = 15

TYPEAHEAD_MAX = 256
TTY_DRAIN = -1

COMMAND_MAX = 256
HISTORY_ENTRIES = 16
CAPTURE_MAX = 0x10000
PATH_LIMIT = 240
STAGES_MAX = 8
PIPELINE_MAX = 8
STREAM_INPUT = 1
STREAM_OUTPUT = 2

shell_start:
    lea rdi, [temporary_directory]
    call sys_mkdir
    call sys_getuid
    mov [shell_uid], eax
    mov edi, eax
    call user_name
    lea rdi, [shell_prompt_user]
    mov rsi, rax
    call str_copy
    lea rdi, [shell_prompt_user]
    call str_len
    lea rdi, [shell_prompt_user + rax]
    lea rsi, [shell_prompt_host]
    call str_copy
    mov byte [terminal_foreground], COLOR_LIGHT_GREEN
    lea rsi, [shell_welcome]
    call terminal_print

; prompt is user@serpentis:cwd$ (# for root), the part before the cursor becomes read only
shell_prompt:
    cmp word [document_cursor_column], 0
    je .line_ready
    call terminal_newline
.line_ready:
    mov al, COLOR_LIGHT_CYAN
    call terminal_set_foreground
    lea rsi, [shell_prompt_user]
    call terminal_print
    mov al, COLOR_YELLOW
    call terminal_set_foreground
    lea rdi, [shell_cwd]
    mov esi, PATH_LIMIT
    call sys_getcwd
    lea rsi, [shell_cwd]
    call terminal_print
    lea rsi, [shell_prompt_end]
    cmp dword [shell_uid], 0
    jne .user
    lea rsi, [shell_prompt_root]
.user:
    call terminal_print
    call document_mark_prompt
    mov al, COLOR_WHITE
    call terminal_set_foreground
    mov al, [history_count]
    mov [history_nav], al
    ret

shell_capture_line:
    push rbx
    push r12
    push r13
    push r14
    push r15

    movzx r12d, word [document_cursor_line]
.first_line:
    test r12d, r12d
    jz .start
    cmp byte [document_continued + r12], 0
    je .start
    dec r12d
    jmp .first_line
.start:
    movzx r13d, byte [document_protected_lengths + r12]
    xor r15d, r15d
.line:
    movzx r14d, byte [document_line_lengths + r12]
    mov eax, r12d
    call document_line_address
    mov rbx, rax
.copy:
    cmp r13d, r14d
    jae .next_line
    cmp r15d, COMMAND_MAX - 1
    jae .trim
    mov dl, [rbx + r13 * 2]
    mov [command_buffer + r15], dl
    inc r13d
    inc r15d
    jmp .copy
.next_line:
    inc r12d
    movzx eax, word [document_line_count]
    cmp r12d, eax
    jae .trim
    cmp byte [document_continued + r12], 0
    je .trim
    xor r13d, r13d
    jmp .line

.trim:
    test r15d, r15d
    jz .terminate
    mov dl, [command_buffer + r15 - 1]
    cmp dl, ' '
    je .cut
    cmp dl, 9
    jne .leading
.cut:
    dec r15d
    jmp .trim
.leading:
    xor ecx, ecx
.skip:
    mov dl, [command_buffer + rcx]
    cmp dl, ' '
    je .blank
    cmp dl, 9
    jne .shift
.blank:
    inc ecx
    jmp .skip
.shift:
    test ecx, ecx
    jz .terminate
    sub r15d, ecx
    lea rsi, [command_buffer + rcx]
    lea rdi, [command_buffer]
    mov ecx, r15d
    cld
    rep movsb
.terminate:
    mov byte [command_buffer + r15], 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

shell_input_enter:
    call shell_capture_line
    call terminal_move_to_document_end
    call terminal_newline
    cmp byte [command_buffer], 0
    je shell_prompt
    call shell_history_add
    call shell_execute
    cmp dword [child_pid], 0
    jne .running
    jmp shell_prompt
.running:
    ret

shell_history_add:
    movzx eax, byte [history_count]
    test eax, eax
    jz .destination
    dec eax
    shl eax, 8
    lea rdi, [command_buffer]
    lea rsi, [history_buffer + rax]
    call str_equal
    test eax, eax
    jnz .finish
    movzx eax, byte [history_count]
.destination:
    cmp eax, HISTORY_ENTRIES
    jb .append
    lea rdi, [history_buffer]
    lea rsi, [history_buffer + COMMAND_MAX]
    mov edx, (HISTORY_ENTRIES - 1) * COMMAND_MAX
    call mem_copy
    mov eax, HISTORY_ENTRIES - 1
    jmp .copy
.append:
    inc byte [history_count]
.copy:
    shl eax, 8
    lea rdi, [history_buffer + rax]
    lea rsi, [command_buffer]
    call str_copy
.finish:
    mov al, [history_count]
    mov [history_nav], al
    ret

shell_history_previous:
    cmp byte [history_count], 0
    je .done
    cmp byte [history_nav], 0
    je .load
    dec byte [history_nav]
.load:
    movzx eax, byte [history_nav]
    shl eax, 8
    lea rsi, [history_buffer + rax]
    call document_replace_command
.done:
    ret

shell_history_next:
    mov al, [history_nav]
    cmp al, [history_count]
    jae .done
    inc byte [history_nav]
    mov al, [history_nav]
    cmp al, [history_count]
    jae .empty
    movzx eax, byte [history_nav]
    shl eax, 8
    lea rsi, [history_buffer + rax]
    jmp document_replace_command
.empty:
    lea rsi, [empty_string]
    call document_replace_command
.done:
    ret

; splits the command at | (not inside quotes) and starts the first stage
shell_execute:
    push rbx
    lea rdi, [pipeline_buffer]
    lea rsi, [command_buffer]
    call str_copy
    mov dword [stage_count], 0
    mov dword [stage_next], 0
    mov dword [pipeline_used], 0
    mov dword [pending_read], -1
    lea rbx, [pipeline_buffer]
    xor ecx, ecx
.stage:
    mov eax, [stage_count]
    cmp eax, STAGES_MAX
    jae .split
    mov [stage_list + rax * 8], rbx
    inc dword [stage_count]
.scan:
    mov al, [rbx]
    test al, al
    jz .split
    inc rbx
    test cl, cl
    jnz .quoted
    cmp al, '"'
    je .open_quote
    cmp al, 39
    je .open_quote
    cmp al, '|'
    jne .scan
    mov byte [rbx - 1], 0
    jmp .stage
.open_quote:
    mov cl, al
    jmp .scan
.quoted:
    cmp al, cl
    jne .scan
    xor ecx, ecx
    jmp .scan
.split:
    pop rbx

shell_next_stage:
    push rbx
.next:
    mov ebx, [stage_next]
    cmp ebx, [stage_count]
    jae .finished
    inc dword [stage_next]
    mov rsi, [stage_list + rbx * 8]
    call shell_parse_redirects
    jc .abort
    call shell_open_streams
    jc .abort
    call shell_run_stage
    call shell_close_streams
    cmp dword [child_pid], 0
    je .next
    call shell_track_child
    jmp .next
.abort:
    call shell_close_streams
    mov eax, [stage_count]
    mov [stage_next], eax
.finished:
    call shell_drop_pending
    pop rbx
    ret

shell_track_child:
    mov eax, [pipeline_used]
    cmp eax, PIPELINE_MAX
    jae .done
    mov ecx, [child_pid]
    mov [pipeline_pids + rax * 4], ecx
    inc dword [pipeline_used]
.done:
    ret

shell_drop_pending:
    cmp dword [pending_read], 0
    jl .done
    mov edi, [pending_read]
    call sys_close
    mov dword [pending_read], -1
.done:
    ret

shell_reap_pipeline:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, [pipeline_used]
    jae .done
    mov edi, [pipeline_pids + rbx * 4]
    test edi, edi
    jz .skip
    cmp edi, [child_pid]
    je .skip
    mov esi, WAIT_NOHANG
    call sys_waitpid
    cmp eax, SYS_ERR_BUSY
    je .skip
    mov dword [pipeline_pids + rbx * 4], 0
.skip:
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

shell_parse_redirects:
    mov byte [input_path], 0
    mov byte [output_path], 0
    mov byte [output_append], 0
    lea rdi, [stage_command]
    xor ecx, ecx
.leading:
    cmp byte [rsi], ' '
    jne .copy
    inc rsi
    jmp .leading
.copy:
    mov al, [rsi]
    test al, al
    jz .end
    test cl, cl
    jnz .quoted
    cmp al, '"'
    je .open_quote
    cmp al, 39
    je .open_quote
    cmp al, '>'
    je .output
    cmp al, '<'
    je .input
.store:
    mov [rdi], al
    inc rdi
    inc rsi
    jmp .copy
.open_quote:
    mov cl, al
    jmp .store
.quoted:
    cmp al, cl
    jne .store
    xor ecx, ecx
    jmp .store
.output:
    inc rsi
    cmp byte [rsi], '>'
    jne .output_name
    mov byte [output_append], 1
    inc rsi
.output_name:
    push rdi
    lea rdi, [output_path]
    call shell_take_word
    pop rdi
    jc .missing
    jmp .copy
.input:
    inc rsi
    push rdi
    lea rdi, [input_path]
    call shell_take_word
    pop rdi
    jc .missing
    jmp .copy
.end:
    lea rax, [stage_command]
.trim:
    cmp rdi, rax
    jbe .terminate
    cmp byte [rdi - 1], ' '
    jne .terminate
    dec rdi
    jmp .trim
.terminate:
    mov byte [rdi], 0
    clc
    ret
.missing:
    lea rsi, [missing_target_text]
    lea rdx, [empty_string]
    call shell_print_error
    stc
    ret

shell_take_word:
    cmp byte [rsi], ' '
    jne .start
    inc rsi
    jmp shell_take_word
.start:
    xor ecx, ecx
.copy:
    mov al, [rsi]
    test al, al
    jz .done
    cmp al, ' '
    je .done
    cmp al, '<'
    je .done
    cmp al, '>'
    je .done
    cmp ecx, PATH_LIMIT - 1
    jae .skip
    mov [rdi + rcx], al
    inc ecx
.skip:
    inc rsi
    jmp .copy
.done:
    mov byte [rdi + rcx], 0
    test ecx, ecx
    jz .empty
    clc
    ret
.empty:
    stc
    ret

shell_open_streams:
    cmp dword [pending_read], 0
    jl .input_file
    mov eax, [pending_read]
    mov esi, FD_STDIN
    call shell_attach
    mov dword [pending_read], -1
    or byte [streams_open], STREAM_INPUT
    jmp .output
.input_file:
    cmp byte [input_path], 0
    je .output
    lea rdi, [input_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .failed
    mov esi, FD_STDIN
    call shell_attach
    or byte [streams_open], STREAM_INPUT

.output:
    mov eax, [stage_next]
    cmp eax, [stage_count]
    jae .output_file
    lea rdi, [stage_pipe]
    call sys_pipe
    test eax, eax
    js .pipe_failed
    mov eax, [stage_pipe]
    mov [pending_read], eax
    mov eax, [stage_pipe + 4]
    mov esi, FD_STDOUT
    call shell_attach
    or byte [streams_open], STREAM_OUTPUT
    clc
    ret
.output_file:
    cmp byte [output_path], 0
    je .done
    lea rdi, [output_path]
    cmp byte [output_append], 0
    jne .append
.create:
    push rdi
    call sys_unlink
    pop rdi
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    jmp .attach
.append:
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    push rax
    mov edi, eax
    xor esi, esi
    mov edx, SEEK_END
    call sys_seek
    pop rax
.attach:
    mov esi, FD_STDOUT
    call shell_attach
    or byte [streams_open], STREAM_OUTPUT
.done:
    clc
    ret
.failed:
    mov rdx, rdi
    lea rsi, [cannot_open_text]
    call shell_print_error
    stc
    ret
.pipe_failed:
    lea rsi, [cannot_pipe_text]
    lea rdx, [empty_string]
    call shell_print_error
    stc
    ret

shell_attach:
    push rax
    mov edi, eax
    call sys_dup2
    pop rdi
    jmp sys_close

shell_close_streams:
    test byte [streams_open], STREAM_INPUT
    jz .output
    mov edi, FD_STDIN
    call sys_close
.output:
    test byte [streams_open], STREAM_OUTPUT
    jz .done
    mov edi, FD_STDOUT
    call sys_close
.done:
    mov byte [streams_open], 0
    ret

shell_print_error:
    push rdx
    push rsi
    mov al, COLOR_LIGHT_RED
    call terminal_set_foreground
    pop rsi
    call terminal_print
    pop rsi
    call terminal_print_line
    mov al, COLOR_WHITE
    jmp terminal_set_foreground

shell_run_stage:
    push rbx
    push r12

    lea rdi, [command_word]
    lea rsi, [stage_command]
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
.word:
    mov al, [rsi + rdx]
    test al, al
    jz .word_done
    test r8b, r8b
    jnz .quoted
    cmp al, '"'
    je .open_quote
    cmp al, 39
    je .open_quote
    cmp al, ' '
    je .word_done
    jmp .letter
.open_quote:
    mov r8b, al
    inc edx
    jmp .word
.quoted:
    cmp al, r8b
    jne .letter
    xor r8d, r8d
    inc edx
    jmp .word
.letter:
    cmp ecx, 62
    jae .word_done
    mov [rdi + rcx], al
    inc ecx
    inc edx
    jmp .word
.word_done:
    mov byte [rdi + rcx], 0
    test ecx, ecx
    jz .done
    lea rsi, [stage_command + rdx]
.skip:
    cmp byte [rsi], ' '
    jne .arguments_ready
    inc rsi
    jmp .skip
.arguments_ready:
    mov r12, rsi

    lea rbx, [builtin_table]
.builtin:
    mov rsi, [rbx]
    test rsi, rsi
    jz .kernel
    lea rdi, [command_word]
    call str_equal
    test eax, eax
    jnz .run_builtin
    add rbx, 16
    jmp .builtin
.run_builtin:
    mov rdi, r12
    call qword [rbx + 8]
    jmp .done

.kernel:
    test byte [streams_open], STREAM_INPUT
    jnz .program_first
    lea rbx, [program_first_table]
.preferred:
    mov rsi, [rbx]
    test rsi, rsi
    jz .kernel_command
    lea rdi, [command_word]
    call str_equal
    test eax, eax
    jnz .program_first
    add rbx, 8
    jmp .preferred
.program_first:
    lea rdi, [command_word]
    mov rsi, r12
    call shell_spawn
    jnc .done
.kernel_command:
    lea rdi, [stage_command]
    lea rsi, [capture_buffer]
    mov edx, CAPTURE_MAX
    call sys_kcommand
    cmp eax, SYS_ERR_NOT_FOUND
    je .program
    test eax, eax
    js .done
    mov edi, eax
    test byte [streams_open], STREAM_OUTPUT
    jnz .write_capture
    call shell_insert_capture
    jmp .done
.write_capture:
    call shell_write_capture
    jmp .done

.program:
    lea rdi, [command_word]
    mov rsi, r12
    call shell_spawn
    jnc .done
    lea rsi, [unknown_prefix]
    lea rdx, [command_word]
    call shell_print_error

.done:
    pop r12
    pop rbx
    ret

shell_write_capture:
    xor eax, eax
    xor ecx, ecx
.next:
    cmp eax, edi
    jae .write
    mov dl, [capture_buffer + rax]
    mov [capture_buffer + rcx], dl
    inc ecx
    add eax, 2
    jmp .next
.write:
    mov edx, ecx
    mov edi, FD_STDOUT
    lea rsi, [capture_buffer]
    jmp sys_write

shell_insert_capture:
    push rbx
    push r12
    mov r12d, edi
    xor ebx, ebx
    mov r8b, [terminal_foreground]
    mov r9b, [terminal_background]
.next:
    cmp ebx, r12d
    jae .done
    mov dl, [capture_buffer + rbx + 1]
    mov al, dl
    and al, 0x0F
    mov [terminal_foreground], al
    shr dl, 4
    mov [terminal_background], dl
    mov al, [capture_buffer + rbx]
    call terminal_putc
    add ebx, 2
    jmp .next
.done:
    mov [terminal_foreground], r8b
    mov [terminal_background], r9b
    pop r12
    pop rbx
    ret

; rdi -> name, rsi -> arguments. name with a slash: relative or absolute path, otherwise
; /bin/<name>.serp. CF if it could not be started
shell_spawn:
    push rbx
    mov rbx, rsi
    lea rsi, [rdi]
.scan:
    mov al, [rsi]
    test al, al
    jz .prefixed
    cmp al, '/'
    je .direct
    inc rsi
    jmp .scan
.direct:
    cmp byte [rdi], '/'
    je .absolute
.relative:
    push rdi
    call str_len
    push rax
    lea rdi, [spawn_path]
    mov esi, PATH_LIMIT
    call sys_getcwd
    lea rdi, [spawn_path]
    call str_len
    pop rcx
    pop rsi
    lea rdx, [rax + rcx + 2]
    cmp rdx, PATH_LIMIT
    ja .failed
    lea rdi, [spawn_path + rax]
    cmp rax, 1
    je .joined
    mov byte [rdi], '/'
    inc rdi
.joined:
    call str_copy
    jmp .spawn
.absolute:
    mov rsi, rdi
    lea rdi, [spawn_path]
    call str_copy
    jmp .spawn
.prefixed:
    push rdi
    lea rdi, [spawn_path]
    lea rsi, [bin_prefix]
    call str_copy
    lea rdi, [spawn_path]
    call str_len
    lea rdi, [spawn_path + rax]
    pop rsi
    call str_copy
    lea rdi, [spawn_path]
    call str_len
    lea rdi, [spawn_path + rax]
    lea rsi, [serp_suffix]
    call str_copy
.spawn:
    lea rdi, [spawn_path]
    mov rsi, rbx
    call sys_spawn
    test eax, eax
    js .failed
    mov [child_pid], eax
    pop rbx
    clc
    ret
.failed:
    pop rbx
    stc
    ret

ansi_putc:
    movzx edx, byte [ansi_state]
    test edx, edx
    jnz .escape
    cmp al, 27
    jne terminal_putc
    mov byte [ansi_state], 1
    ret
.escape:
    cmp edx, 1
    jne .parameter
    cmp al, '['
    jne .finish
    mov byte [ansi_state], 2
    mov dword [ansi_value], 0
    ret
.parameter:
    cmp al, '0'
    jb .separator
    cmp al, '9'
    ja .separator
    movzx eax, al
    sub eax, '0'
    imul edx, [ansi_value], 10
    add edx, eax
    mov [ansi_value], edx
    ret
.separator:
    cmp al, ';'
    je .apply
    cmp al, 'm'
    jne .finish
.apply:
    push rax
    mov edi, [ansi_value]
    call ansi_apply
    pop rax
    mov dword [ansi_value], 0
    cmp al, ';'
    je .done
.finish:
    mov byte [ansi_state], 0
.done:
    ret

ansi_apply:
    test edi, edi
    jz .reset
    cmp edi, 39
    je .default_foreground
    cmp edi, 49
    je .default_background
    mov eax, edi
    xor edx, edx
    cmp eax, 90
    jb .normal
    sub eax, 60
    mov edx, 8
.normal:
    cmp eax, 30
    jb .done
    cmp eax, 47
    ja .done
    cmp eax, 38
    jae .background
    sub eax, 30
    movzx eax, byte [ansi_colors + rax]
    add eax, edx
    mov [terminal_foreground], al
    ret
.background:
    sub eax, 40
    jb .done
    movzx eax, byte [ansi_colors + rax]
    add eax, edx
    mov [terminal_background], al
    ret
.reset:
    mov byte [terminal_background], COLOR_BLACK
.default_foreground:
    mov byte [terminal_foreground], COLOR_WHITE
    ret
.default_background:
    mov byte [terminal_background], COLOR_BLACK
.done:
    ret

shell_child_output:
    push rbx
.read:
    mov edi, [shell_tty]
    lea rsi, [capture_buffer]
    mov edx, 512
    call sys_tty_read
    test eax, eax
    jle .check
    mov ebx, eax
    xor ecx, ecx
.byte:
    mov al, [capture_buffer + rcx]
    push rcx
    call ansi_putc
    pop rcx
    inc ecx
    cmp ecx, ebx
    jb .byte
    jmp .read
.check:
    call shell_reap_pipeline
    mov edi, [child_pid]
    test edi, edi
    jz .done
    mov esi, WAIT_NOHANG
    call sys_waitpid
    cmp eax, SYS_ERR_BUSY
    je .done
    mov dword [child_pid], 0
    cmp byte [report_exit], 0
    je .next_stage
    mov byte [report_exit], 0
    push rax
    lea rsi, [exit_code_text]
    call terminal_print
    pop rdi
    call u64_to_dec
    mov rsi, rax
    call terminal_print_line
.next_stage:
    call shell_next_stage
    cmp dword [child_pid], 0
    jne .done
    mov byte [full_redraw], 1
    call shell_prompt
    call shell_replay_typeahead
.done:
    pop rbx
    ret

; keys typed while the program ran went into its tty. the ones it never read are taken back
; (TTY_DRAIN tells how many) and replayed into the new prompt, so typing ahead works
shell_replay_typeahead:
    push rbx
    push r12
    mov edi, [shell_tty]
    mov esi, TTY_DRAIN
    call sys_tty_input
    mov ecx, [typeahead_count]
    mov dword [typeahead_count], 0
    test eax, eax
    jle .done
    cmp eax, ecx
    jbe .count
    mov eax, ecx
.count:
    mov r12d, eax
    sub ecx, eax
    xor ebx, ebx
.copy:
    cmp ebx, r12d
    jae .replay
    lea edx, [rcx + rbx]
    mov edx, [typeahead_keys + rdx * 4]
    mov [replay_keys + rbx * 4], edx
    inc ebx
    jmp .copy
.replay:
    xor ebx, ebx
.next:
    cmp ebx, r12d
    jae .done
    mov edi, [replay_keys + rbx * 4]
    call handle_key
    inc ebx
    jmp .next
.done:
    pop r12
    pop rbx
    ret

builtin_clear:
    call document_reset
    ret

builtin_history:
    push rbx
    xor ebx, ebx
.next:
    cmp bl, [history_count]
    jae .done
    lea rsi, [history_indent]
    call terminal_print
    lea edi, [rbx + 1]
    call u64_to_dec
    mov rsi, rax
    call terminal_print
    lea rsi, [history_indent]
    call terminal_print
    mov eax, ebx
    shl eax, 8
    lea rsi, [history_buffer + rax]
    call terminal_print_line
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

builtin_exit:
    mov byte [running], 0
    ret

builtin_edit:
    mov rsi, rdi
    lea rdi, [edit_word]
    call shell_spawn
    ret

builtin_run:
    mov rsi, rdi
    call arg_next
    test rax, rax
    jz .done
    push rax
    push rdx
    mov rdi, rax
    mov rsi, rdx
    call shell_spawn_here
    pop rsi
    pop rdi
    jnc .started
    push rdi
    call shell_spawn
    pop rsi
    jnc .started
    push rsi
    lea rsi, [cannot_run_text]
    call terminal_print
    pop rsi
    call terminal_print_line
    ret
.started:
    mov byte [report_exit], 1
.done:
    ret

shell_spawn_here:
    push rbx
    mov rbx, rsi
    jmp shell_spawn.relative

shell_welcome:
    db ' SSSSS EEEEE RRRR  PPPP  EEEEE N   N TTTTT IIIII SSSSS', 10
    db ' S     E     R   R P   P E     NN  N   T     I   S', 10
    db ' SSSSS EEEE  RRRR  PPPP  EEEE  N N N   T     I   SSSSS', 10
    db '     S E     R  R  P     E     N  NN   T     I       S', 10
    db ' SSSSS EEEEE R   R P     EEEEE N   N   T   IIIII SSSSS', 10, 10, 0
shell_prompt_host db '@serpentis:', 0
shell_prompt_end db '$ ', 0
shell_prompt_root db '# ', 0
unknown_prefix db 'Unknown command: ', 0
exit_code_text db 'Program exited with code: ', 0
history_indent db '  ', 0
empty_string db 0
ansi_colors db 0, 4, 2, 6, 1, 5, 3, 7
bin_prefix db '/bin/', 0
serp_suffix db '.serp', 0
temporary_directory db '/tmp', 0
cannot_pipe_text db 'Cannot create pipe', 0
missing_target_text db 'Missing file name after redirection', 0
cannot_open_text db 'Cannot open ', 0
cannot_run_text db 'Cannot run ', 0
word_clear db 'clear', 0
word_history db 'history', 0
word_exit db 'exit', 0
edit_word db 'edit', 0
word_run db 'run', 0

word_cat db 'cat', 0
word_wc db 'wc', 0

align 8
program_first_table:
    dq word_cat, word_wc, 0

builtin_table:
    dq word_clear, builtin_clear
    dq word_history, builtin_history
    dq word_exit, builtin_exit
    dq edit_word, builtin_edit
    dq word_run, builtin_run
    dq 0, 0
