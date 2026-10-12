include '../runtime/program.inc'

main:
    push r12
    push r13
    push r14
    xor r14d, r14d

.next:
    call arg_next
    test rax, rax
    jz .done
    mov r13, rdx
    mov r12, rax

    mov rdi, rax
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed

    mov edi, eax
    call sys_close
    jmp .advance

.failed:
    lea rdi, [program_name]
    mov rsi, r12
    mov edx, eax
    call report_error
    mov r14d, 1

.advance:
    mov rdi, r13
    jmp .next

.done:
    mov eax, r14d
    pop r14
    pop r13
    pop r12
    ret

program_name db 'touch', 0

include '../runtime/library.inc'
