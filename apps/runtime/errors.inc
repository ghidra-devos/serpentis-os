report_error:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13d, edx

    mov rdi, rbx
    call print
    lea rdi, [error_separator]
    call print
    mov rdi, r12
    call print
    lea rdi, [error_separator]
    call print

    lea rsi, [error_table]
.find:
    mov eax, [rsi]
    test eax, eax
    jz .unknown
    cmp eax, r13d
    je .known
    add rsi, 12
    jmp .find

.known:
    mov rdi, [rsi + 4]
    call print_line
    jmp .done

.unknown:
    lea rdi, [error_unknown]
    call print
    movsxd rdi, r13d
    call print_i64
    mov edi, 10
    call print_char

.done:
    pop r13
    pop r12
    pop rbx
    ret

error_separator db ': ', 0
error_unknown db 'error ', 0
error_not_found db 'no such file or directory', 0
error_exists db 'already exists', 0
error_is_directory db 'is a directory', 0
error_not_directory db 'not a directory', 0
error_busy db 'directory not empty', 0
error_access db 'permission denied', 0
error_invalid db 'invalid argument', 0
error_full db 'no space left', 0

align 4
error_table:
    dd SYS_ERR_NOT_FOUND
    dq error_not_found
    dd SYS_ERR_EXISTS
    dq error_exists
    dd SYS_ERR_IS_DIRECTORY
    dq error_is_directory
    dd SYS_ERR_NOT_DIRECTORY
    dq error_not_directory
    dd SYS_ERR_BUSY
    dq error_busy
    dd SYS_ERR_ACCESS
    dq error_access
    dd SYS_ERR_INVALID
    dq error_invalid
    dd SYS_ERR_FULL
    dq error_full
    dd 0
