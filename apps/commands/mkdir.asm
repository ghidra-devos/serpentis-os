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
    call sys_mkdir
    test eax, eax
    jns .advance

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

program_name db 'mkdir', 0

include '../runtime/library.inc'
