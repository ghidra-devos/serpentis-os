SERP_STACK = 0x8000

include '../../apps/runtime/program.inc'

BIG = 512 * 1024
DEPTH = 1000

main:
    push rbx

    lea rdi, [big_buffer]
    mov ecx, BIG
.check_zero:
    cmp byte [rdi], 0
    jne .fail
    inc rdi
    dec ecx
    jnz .check_zero

    lea rdi, [big_buffer]
    mov ecx, BIG / 8
    mov rax, 0x5A5A5A5A5A5A5A5A
    rep stosq

    mov byte [big_buffer + BIG - 1], 10
    mov edi, FD_STDOUT
    lea rsi, [big_buffer + BIG - 1]
    mov edx, 1
    call sys_write
    cmp eax, 1
    jne .fail

    mov ecx, DEPTH
    call recurse
    cmp eax, DEPTH
    jne .fail

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    pop rbx
    ret

.fail:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

recurse:
    sub rsp, 16
    mov [rsp], rcx
    xor eax, eax
    dec ecx
    jz .bottom
    call recurse
.bottom:
    inc eax
    add rsp, 16
    ret

ok_text db 'BSS STACK OK', 0
fail_text db 'BSS STACK FAIL', 0

include '../../apps/runtime/library.inc'

big_buffer rb BIG
