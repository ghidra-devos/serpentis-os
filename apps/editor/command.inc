; the bottom line: : commands, / and ? search, and the tree prompts (new file, rename...)
; which reuse the same line editing. plus loading and saving

COMMAND_MAX = 120
PATTERN_MAX = 64
READ_CHUNK = 0x10000

prompt_key:
    mov eax, [key]
    cmp eax, KEY_ESCAPE
    je .cancel
    cmp eax, 10
    je .execute
    cmp byte [prompt_selected], 0
    je .typing
    mov byte [prompt_selected], 0
    cmp eax, 8
    je .clear
    cmp eax, ' '
    jb .typing
    cmp eax, 126
    ja .typing
.clear:
    mov dword [command_length], 0
    mov byte [command_line], 0
    cmp eax, 8
    je .done
.typing:
    cmp eax, 8
    je .erase
    cmp eax, ' '
    jb .done
    cmp eax, 126
    ja .done
    mov ecx, [command_length]
    cmp ecx, COMMAND_MAX
    jae .done
    mov [command_line + rcx], al
    inc ecx
    mov [command_length], ecx
    mov byte [command_line + rcx], 0
.done:
    ret
.erase:
    mov ecx, [command_length]
    test ecx, ecx
    jz .cancel
    dec ecx
    mov [command_length], ecx
    mov byte [command_line + rcx], 0
    ret
.cancel:
    mov al, [prompt_return]
    mov [mode], al
    ret
.execute:
    mov al, [prompt_return]
    mov [mode], al
    movzx eax, byte [prompt_kind]
    jmp qword [prompt_actions + rax * 8]

PROMPT_COMMAND = 0
PROMPT_SEARCH = 1
PROMPT_NEW_FILE = 2
PROMPT_NEW_DIRECTORY = 3
PROMPT_RENAME = 4
PROMPT_DELETE = 5

start_prompt:
    mov [prompt_kind], al
    mov [prompt_label], rsi
    mov al, MODE_NORMAL
    cmp byte [mode], MODE_FILES
    jne .return_ready
    mov al, MODE_FILES
.return_ready:
    mov [prompt_return], al
    mov byte [mode], MODE_COMMAND
    cmp byte [prompt_kind], PROMPT_SEARCH
    jne .clear
    mov byte [mode], MODE_SEARCH
.clear:
    mov byte [prompt_selected], 0
    mov dword [command_length], 0
    mov byte [command_line], 0
    ret

execute_search:
    mov ecx, [command_length]
    test ecx, ecx
    jz .repeat
    cmp ecx, PATTERN_MAX
    jbe .copy
    mov ecx, PATTERN_MAX
.copy:
    mov [search_length], ecx
    lea rsi, [command_line]
    lea rdi, [search_pattern]
    rep movsb
.repeat:
    mov byte [search_highlight], 1
    mov al, [search_direction]
    call search_from_cursor
    jc .missing
    mov [cursor], eax
    mov edi, eax
    call column_of
    mov [want_column], eax
    ret
.missing:
    lea rdi, [pattern_not_found_text]
    jmp show_error

; al = 0 forward, 1 back. starts one past the cursor and wraps around the file, plain
; substring match, no regex
search_from_cursor:
    cmp dword [search_length], 0
    je .missing
    mov r9b, al
    mov edx, [cursor]
    mov r8d, [text_size]
    inc r8d
.step:
    dec r8d
    jz .missing
    test r9b, r9b
    jnz .backward
    inc edx
    cmp edx, [text_size]
    jb .check
    xor edx, edx
    jmp .check
.backward:
    test edx, edx
    jnz .back_one
    mov edx, [text_size]
.back_one:
    dec edx
.check:
    mov edi, edx
    call pattern_at
    jnc .step
    mov eax, edx
    clc
    ret
.missing:
    stc
    ret

show_error:
    mov [message], rdi
    mov byte [message_error], 1
    ret

show_message:
    mov [message], rdi
    mov byte [message_error], 0
    ret

command_word_is:
    push rsi
    push rdi
    lea rdi, [command_line]
.compare:
    mov al, [rsi]
    test al, al
    jz .end
    cmp al, [rdi]
    jne .no
    inc rsi
    inc rdi
    jmp .compare
.end:
    mov al, [rdi]
    test al, al
    jz .yes
    cmp al, ' '
    jne .no
.yes:
    pop rdi
    pop rsi
    stc
    ret
.no:
    pop rdi
    pop rsi
    clc
    ret

command_argument:
    lea rax, [command_line]
.word:
    mov cl, [rax]
    test cl, cl
    jz .done
    cmp cl, ' '
    je .spaces
    inc rax
    jmp .word
.spaces:
    cmp byte [rax], ' '
    jne .done
    inc rax
    jmp .spaces
.done:
    ret

; :123 goes to a line, :s/// and :%s///, everything else from ex_commands
execute_command:
    push rbx
    mov byte [message_error], 0
    movzx eax, byte [command_line]
    test eax, eax
    jz .done
    cmp eax, '%'
    je .substitute_all
    cmp eax, '0'
    jb .words
    cmp eax, '9'
    jbe .goto_line
    cmp eax, 's'
    jne .words
    cmp byte [command_line + 1], '/'
    je .substitute_line

.words:
    lea rbx, [ex_commands]
.entry:
    mov rsi, [rbx]
    test rsi, rsi
    jz .unknown
    call command_word_is
    jc .run
    add rbx, 16
    jmp .entry
.run:
    call qword [rbx + 8]
    jmp .done

.goto_line:
    lea rdi, [command_line]
    call parse_decimal
    mov edi, eax
    test edi, edi
    jz .first
    dec edi
.first:
    call line_offset
    mov edi, eax
    call leading_blank_end
    mov [cursor], eax
    mov edi, eax
    call column_of
    mov [want_column], eax
    jmp .done

.substitute_line:
    lea rsi, [command_line + 1]
    mov byte [substitute_all_lines], 0
    call substitute
    jmp .done
.substitute_all:
    cmp byte [command_line + 1], 's'
    jne .unknown
    cmp byte [command_line + 2], '/'
    jne .unknown
    lea rsi, [command_line + 2]
    mov byte [substitute_all_lines], 1
    call substitute
    jmp .done

.unknown:
    lea rdi, [not_command_text]
    call show_error
.done:
    pop rbx
    ret

parse_decimal:
    xor eax, eax
.digit:
    movzx ecx, byte [rdi]
    sub ecx, '0'
    cmp ecx, 9
    ja .done
    imul eax, 10
    add eax, ecx
    inc rdi
    jmp .digit
.done:
    ret

ex_write:
    call command_argument
    cmp byte [rax], 0
    je save_file
    mov rsi, rax
    lea rdi, [file_path]
    call str_copy
    call detect_file_kind
    jmp save_file

ex_quit:
    xor eax, eax
    jmp close_buffer

ex_force_quit:
    mov al, 1
    jmp close_buffer

ex_write_quit:
    call ex_write
    jc .done
    xor eax, eax
    jmp close_buffer
.done:
    ret

; :x only writes when something changed
ex_exit:
    cmp byte [dirty], 0
    je ex_quit
    jmp ex_write_quit

ex_quit_all:
    call any_buffer_dirty
    jnz unsaved_error
ex_force_quit_all:
    mov byte [running], 0
    ret

ex_edit:
    call command_argument
    cmp byte [rax], 0
    jne .open
    cmp byte [dirty], 0
    jne unsaved_error
    jmp ex_reload
.open:
    mov rsi, rax
    jmp open_path

ex_force_edit:
    call command_argument
    cmp byte [rax], 0
    jne ex_edit.open
ex_reload:
    cmp byte [file_path], 0
    jne load_file
    lea rdi, [no_file_name_text]
    jmp show_error

unsaved_error:
    lea rdi, [unsaved_text]
    jmp show_error

ex_set:
    call command_argument
    mov rbx, rax
    lea rdx, [set_options]
.option:
    mov rsi, [rdx]
    test rsi, rsi
    jz .unknown
    mov rdi, rbx
    push rdx
    call str_equal
    pop rdx
    test eax, eax
    jnz .apply
    add rdx, 17
    jmp .option
.apply:
    mov rdi, [rdx + 8]
    mov al, [rdx + 16]
    mov [rdi], al
    ret
.unknown:
    lea rdi, [unknown_option_text]
    jmp show_error

ex_no_highlight:
    mov byte [search_highlight], 0
    ret


substitute:
    push rbx
    push r12
    push r13
    push r14

    inc rsi
    lea rdi, [search_pattern]
    xor ecx, ecx
.pattern:
    mov al, [rsi]
    test al, al
    jz .bad
    inc rsi
    cmp al, '/'
    je .pattern_done
    cmp ecx, PATTERN_MAX
    jae .pattern
    mov [rdi + rcx], al
    inc ecx
    jmp .pattern
.pattern_done:
    test ecx, ecx
    jz .bad
    mov [search_length], ecx
    lea rdi, [replacement]
    xor ecx, ecx
.replacement:
    mov al, [rsi]
    test al, al
    jz .replacement_done
    inc rsi
    cmp al, '/'
    je .replacement_done
    cmp ecx, PATTERN_MAX
    jae .replacement
    mov [rdi + rcx], al
    inc ecx
    jmp .replacement
.replacement_done:
    mov [replacement_length], ecx
    mov byte [substitute_global], 0
    cmp byte [rsi], 'g'
    jne .range
    mov byte [substitute_global], 1

.range:
    call begin_change
    xor r14d, r14d
    xor r12d, r12d
    mov r13d, [text_size]
    cmp byte [substitute_all_lines], 0
    jne .scan
    mov edi, [cursor]
    call line_start
    mov r12d, eax
    mov edi, [cursor]
    call line_end
    mov r13d, eax

.scan:
    mov eax, r12d
    add eax, [search_length]
    cmp eax, r13d
    ja .finished
    mov edi, r12d
    call pattern_at
    jnc .advance
    mov edi, r12d
    mov ecx, [search_length]
    call edit_delete
    mov edi, r12d
    lea rsi, [replacement]
    mov ecx, [replacement_length]
    call edit_insert
    mov eax, [replacement_length]
    sub eax, [search_length]
    add r13d, eax
    add r12d, [replacement_length]
    inc r14d
    cmp byte [substitute_global], 0
    jne .scan
    mov edi, r12d
    call line_end
    mov r12d, eax
    jmp .scan
.advance:
    inc r12d
    jmp .scan

.finished:
    test r14d, r14d
    jz .missing
    mov byte [search_highlight], 0
    jmp .out
.missing:
    lea rdi, [pattern_not_found_text]
    call show_error
    jmp .out
.bad:
    lea rdi, [not_command_text]
    call show_error
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

detect_file_kind:
    mov byte [syntax_enabled], 0
    lea rax, [plain_kind]
    mov [file_kind_name], rax
    lea rdi, [file_path]
    call str_len
    cmp eax, 4
    jb .done
    lea rdi, [file_path + rax - 4]
    lea rsi, [asm_extension]
    push rax
    call str_equal
    mov ecx, eax
    pop rax
    test ecx, ecx
    jnz .assembly
    lea rdi, [file_path + rax - 4]
    lea rsi, [inc_extension]
    call str_equal
    test eax, eax
    jz .done
.assembly:
    mov byte [syntax_enabled], 1
    lea rax, [assembly_kind]
    mov [file_kind_name], rax
.done:
    lea rdi, [file_path]
    call str_len
    cmp eax, 2
    jb .out
    cmp byte [file_path + rax - 2], '.'
    jne .out
    mov cl, [file_path + rax - 1]
    cmp cl, 'c'
    je .c_source
    cmp cl, 'h'
    jne .out
.c_source:
    mov byte [syntax_enabled], 2
    lea rax, [c_kind]
    mov [file_kind_name], rax
.out:
    ret

load_file:
    push rbx
    mov dword [text_size], 0
    mov dword [cursor], 0
    mov dword [top_line], 0
    mov dword [left_column], 0
    mov dword [undo_used], 0
    mov dword [redo_used], 0
    call detect_file_kind
    lea rdi, [file_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .new_file
    mov ebx, eax
.read:
    mov edx, TEXT_MAX
    sub edx, [text_size]
    jz .loaded
    cmp edx, READ_CHUNK
    jbe .chunk
    mov edx, READ_CHUNK
.chunk:
    mov eax, [text_size]
    lea rsi, [text + rax]
    mov edi, ebx
    call sys_read
    test eax, eax
    jle .loaded
    add [text_size], eax
    jmp .read
.loaded:
    mov edi, ebx
    call sys_close
    xor esi, esi
    xor edi, edi
.strip:
    cmp esi, [text_size]
    jae .stripped
    mov al, [text + rsi]
    inc esi
    cmp al, 13
    je .strip
    mov [text + rdi], al
    inc edi
    jmp .strip
.stripped:
    mov [text_size], edi
    mov byte [dirty], 0
    call describe_file
    pop rbx
    ret
.new_file:
    mov byte [dirty], 0
    lea rdi, [new_file_text]
    call show_message
    pop rbx
    ret

describe_file:
    lea rdi, [file_info]
    mov byte [rdi], '"'
    inc rdi
    lea rsi, [file_path]
    call str_copy
    lea rdi, [file_info]
    call str_len
    lea rbx, [file_info + rax]
    mov word [rbx], '" '
    add rbx, 2
    call line_count
    mov edi, eax
    call u64_to_dec
    mov rdi, rbx
    mov rsi, rax
    call str_copy
    lea rdi, [file_info]
    call str_len
    lea rdi, [file_info + rax]
    lea rsi, [lines_word]
    call str_copy
    lea rdi, [file_info]
    call str_len
    lea rbx, [file_info + rax]
    mov edi, [text_size]
    call u64_to_dec
    mov rdi, rbx
    mov rsi, rax
    call str_copy
    lea rdi, [file_info]
    call str_len
    lea rdi, [file_info + rax]
    lea rsi, [bytes_word]
    call str_copy
    lea rdi, [file_info]
    jmp show_message

; open doesn't truncate here so the old file goes first. crash right in between and the
; file is gone. not safe i know, fix it when the fs gets a real truncate
save_file:
    push rbx
    cmp byte [file_path], 0
    je .no_name
    lea rdi, [file_path]
    call sys_unlink
    lea rdi, [file_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax
    mov edx, [text_size]
    test edx, edx
    jz .written
    mov edi, ebx
    lea rsi, [text]
    call sys_write
    cmp eax, [text_size]
    jne .failed_close
.written:
    mov edi, ebx
    call sys_close
    mov byte [dirty], 0
    call describe_file
    lea rdi, [file_info]
    call str_len
    lea rdi, [file_info + rax]
    lea rsi, [written_word]
    call str_copy
    pop rbx
    clc
    ret
.failed_close:
    mov edi, ebx
    call sys_close
.failed:
    lea rdi, [cannot_write_text]
    call show_error
    pop rbx
    stc
    ret
.no_name:
    lea rdi, [no_file_name_text]
    call show_error
    pop rbx
    stc
    ret

align 8
prompt_actions:
    dq execute_command, execute_search, panel_new_file, panel_new_directory, panel_rename, panel_delete

ex_commands:
    dq word_w, ex_write
    dq word_write, ex_write
    dq word_q_bang, ex_force_quit
    dq word_q, ex_quit
    dq word_quit, ex_quit
    dq word_wq, ex_write_quit
    dq word_x, ex_exit
    dq word_e_bang, ex_force_edit
    dq word_e, ex_edit
    dq word_edit, ex_edit
    dq word_qa_bang, ex_force_quit_all
    dq word_qa, ex_quit_all
    dq word_bn, next_buffer
    dq word_bnext, next_buffer
    dq word_bp, previous_buffer
    dq word_bprevious, previous_buffer
    dq word_bd_bang, ex_force_quit
    dq word_bd, ex_quit
    dq word_set, ex_set
    dq word_noh, ex_no_highlight
    dq 0

set_options:
    dq option_nu, show_numbers
    db 1
    dq option_nonu, show_numbers
    db 0
    dq option_rnu, relative_numbers
    db 1
    dq option_nornu, relative_numbers
    db 0
    dq 0

word_w db 'w', 0
word_write db 'write', 0
word_q db 'q', 0
word_q_bang db 'q!', 0
word_quit db 'quit', 0
word_wq db 'wq', 0
word_x db 'x', 0
word_e db 'e', 0
word_e_bang db 'e!', 0
word_edit db 'edit', 0
word_set db 'set', 0
word_qa db 'qa', 0
word_qa_bang db 'qa!', 0
word_bn db 'bn', 0
word_bnext db 'bnext', 0
word_bp db 'bp', 0
word_bprevious db 'bprevious', 0
word_bd db 'bd', 0
word_bd_bang db 'bd!', 0
word_noh db 'noh', 0
option_nu db 'nu', 0
option_nonu db 'nonu', 0
option_rnu db 'rnu', 0
option_nornu db 'nornu', 0
asm_extension db '.asm', 0
inc_extension db '.inc', 0
assembly_kind db 'Assembly', 0
c_kind db 'C', 0
plain_kind db 'Plain Text', 0
lines_word db 'L, ', 0
bytes_word db 'B', 0
written_word db ' written', 0
new_file_text db '[New File]', 0
not_command_text db 'E492: Not an editor command', 0
unsaved_text db 'E37: No write since last change (add ! to override)', 0
no_file_name_text db 'E32: No file name', 0
cannot_write_text db "E212: Can't open file for writing", 0
pattern_not_found_text db 'E486: Pattern not found', 0
unknown_option_text db 'E518: Unknown option', 0
