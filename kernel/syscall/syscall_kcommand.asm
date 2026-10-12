CAPTURE_ADDRESS = 0xA60000
; SYS_KCOMMAND: lds runs the old kernel commands (ls, cp, fsck, ...) through this. the command
; runs inside the kernel with the caller's cwd, its output is captured as (char, attr) pairs
; and copied back for lds to print

CAPTURE_BYTES = 0x10000         ; 32K characters of output, the rest is dropped

terminal_capture_char:
    push rbx
    push rdx
    mov rbx, [capture_used]
    cmp rbx, CAPTURE_BYTES - 2
    ja .full
    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]
    mov [CAPTURE_ADDRESS + rbx], al
    mov [CAPTURE_ADDRESS + rbx + 1], dl
    add qword [capture_used], 2
.full:
    pop rdx
    pop rbx
    ret

syscall_kcommand:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rsi
    mov r13, rdx

    xor ecx, ecx
.copy_line:
    cmp ecx, SHELL_BUFFER_SIZE - 1
    jae .line_copied
    push rcx
    push rdi
    lea rdi, [rdi + rcx]
    mov edx, 1
    call syscall_user_range_valid
    pop rdi
    pop rcx
    jc .invalid
    mov al, [rdi + rcx]
    mov [kcommand_line + rcx], al
    test al, al
    jz .line_done
    inc ecx
    jmp .copy_line
.line_copied:
    mov byte [kcommand_line + rcx], 0
.line_done:

    mov rdi, r12
    mov rdx, r13
    call syscall_user_range_valid
    jc .invalid

    mov rsi, shell_command_buffer
    mov rdi, kcommand_saved_line
    mov ecx, SHELL_BUFFER_SIZE
    rep movsb
    mov rsi, kcommand_line
    mov rdi, shell_command_buffer
    mov ecx, SHELL_BUFFER_SIZE
    rep movsb

    call shell_parse
    cmp qword [shell_argc], 0
    je .unknown

    mov rbx, shell_command_table
.search:
    cmp qword [rbx], 0
    je .unknown
    mov rdi, [shell_argv]
    mov rsi, [rbx]
    call str_equal
    test eax, eax
    jnz .found
    add rbx, 16
    jmp .search

.found:
    mov rsi, fs_current_path
    mov rdi, kcommand_saved_path
    mov ecx, FS_PATH_MAX
    rep movsb
    mov ax, [fs_current_dir]
    mov [kcommand_saved_dir], ax

    call process_get_cwd
    mov rdi, fs_current_path
    mov ecx, FS_PATH_MAX
    rep movsb
    mov rsi, fs_current_path
    call fs_find_path
    jc .cwd_ready
    mov [fs_current_dir], ax
.cwd_ready:

    mov al, [terminal_foreground]
    mov [kcommand_saved_foreground], al
    mov al, [shell_output_foreground]
    call terminal_set_foreground

    mov [kcommand_output], r12
    mov [kcommand_output_size], r13
    mov qword [capture_used], 0
    mov byte [terminal_capture], 1
    mov rdi, [shell_argc]
    mov rsi, shell_argv
    call qword [rbx + 8]
    mov byte [terminal_capture], 0
    mov r12, [kcommand_output]
    mov r13, [kcommand_output_size]

    mov al, [kcommand_saved_foreground]
    mov [terminal_foreground], al

    mov rsi, fs_current_path
    call process_set_cwd
    mov rsi, kcommand_saved_path
    mov rdi, fs_current_path
    mov ecx, FS_PATH_MAX
    rep movsb
    mov ax, [kcommand_saved_dir]
    mov [fs_current_dir], ax

    mov rcx, [capture_used]
    cmp rcx, r13
    jbe .copy_out
    mov rcx, r13
    and rcx, not 1
.copy_out:
    mov rax, rcx
    mov rsi, CAPTURE_ADDRESS
    mov rdi, r12
    rep movsb
    jmp .restore_line

.unknown:
    mov rax, SYS_ERR_NOT_FOUND

.restore_line:
    push rax
    mov rsi, kcommand_saved_line
    mov rdi, shell_command_buffer
    mov ecx, SHELL_BUFFER_SIZE
    rep movsb
    pop rax
    jmp .out

.invalid:
    mov rax, SYS_ERR_INVALID_ADDRESS

.out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

syscall_log:
    push rbx
    mov rbx, rdi
    mov rdx, rsi
    call syscall_user_range_valid
    jc .invalid
    xor ecx, ecx
.next:
    cmp rcx, rsi
    jae .done
    mov al, [rbx + rcx]
    call serial_putc
    inc rcx
    jmp .next
.done:
    mov rax, rsi
    pop rbx
    ret
.invalid:
    mov eax, SYS_ERR_INVALID_ADDRESS
    pop rbx
    ret

align 8
capture_used dq 0
kcommand_output dq 0
kcommand_output_size dq 0
kcommand_saved_dir dw 0
terminal_capture db 0
kcommand_saved_foreground db 0
kcommand_line:
    times SHELL_BUFFER_SIZE db 0
kcommand_saved_line:
    times SHELL_BUFFER_SIZE db 0
kcommand_saved_path:
    times FS_PATH_MAX db 0
