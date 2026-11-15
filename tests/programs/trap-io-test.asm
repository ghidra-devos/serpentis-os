include '../../apps/runtime/program.inc'

main:
    push rbx

    lea rdi, [path]
    mov esi, OPEN_CREATE or OPEN_WRITE
    call sys_open
    cmp eax, FD_STDERR
    jle .failed
    mov ebx, eax
    mov edi, ebx
    lea rsi, [message]
    mov edx, message_end - message
    call sys_write
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    cmp eax, message_end - message
    jne .failed

    lea rdi, [path]
    mov esi, OPEN_READ
    call sys_open
    cmp eax, FD_STDERR
    jle .failed
    mov ebx, eax
    mov edi, ebx
    lea rsi, [buffer]
    mov edx, message_end - message
    call sys_read
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    cmp eax, message_end - message
    jne .failed

    lea rsi, [message]
    lea rdi, [buffer]
    mov ecx, message_end - message
    repe cmpsb
    jne .failed

    mov edi, FD_STDOUT
    lea rsi, [buffer]
    mov edx, message_end - message
    call sys_write
    xor eax, eax
    pop rbx
    ret

.failed:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

path db '/bin/trap-io.txt', 0
message db 'TRAP FILE IO OK', 10
message_end:
fail_text db 'FILE READ WRITE FAIL', 0

include '../../apps/runtime/library.inc'

buffer rb message_end - message
