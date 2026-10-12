include '../runtime/program.inc'

main:
    push r12
    push r13

    call arg_next
    test rax, rax
    jz .usage
    mov r12, rax
    mov rdi, rdx
    call arg_next
    test rax, rax
    jz .usage
    mov r13, rax

    mov rdi, r12
    mov rsi, r13
    call sys_rename
    test eax, eax
    jns .done

    lea rdi, [program_name]
    mov rsi, r12
    mov edx, eax
    call report_error
    mov eax, 1
    jmp .out

.usage:
    lea rdi, [usage_text]
    call print_line
    mov eax, 2
    jmp .out

.done:
    xor eax, eax

.out:
    pop r13
    pop r12
    ret

program_name db 'mv', 0
usage_text db 'usage: mv <source> <destination>', 0

include '../runtime/library.inc'
