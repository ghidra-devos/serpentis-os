include '../../apps/runtime/program.inc'

main:
    push rbx

    lea rdi, [target]
    mov esi, OPEN_READ
    call sys_open
    cmp eax, FD_STDERR
    jle .failed
    mov ebx, eax

    lea rdi, [target]
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .close_failed
    cmp qword [status + USER_STAT_FILE_SIZE], 0
    je .close_failed
    test dword [status + USER_STAT_FLAGS], FS_FLAG_DIRECTORY
    jnz .close_failed

    mov edi, ebx
    call sys_close
    test eax, eax
    js .failed

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    pop rbx
    ret

.close_failed:
    mov edi, ebx
    call sys_close
.failed:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

target db '/bin/hello.serp', 0
ok_text db 'FILE SYSCALLS OK', 0
fail_text db 'FILE SYSCALLS FAIL', 0

include '../../apps/runtime/library.inc'

align 8
status rb USER_STAT_SIZE
