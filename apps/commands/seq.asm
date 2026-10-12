include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    mov ebx, 1
    mov r12d, 1
    mov ecx, [arg_count]
    cmp ecx, 1
    je .last
    cmp ecx, 2
    je .first
    cmp ecx, 3
    jne .usage
    mov rdi, [arg_list + 8]
    call parse_number
    jc .usage
    test rax, rax
    jz .usage
    mov r12, rax
    mov rax, [arg_list + 16]
    mov [arg_list + 8], rax
.first:
    mov rdi, [arg_list]
    call parse_number
    jc .usage
    mov rbx, rax
    mov rax, [arg_list + 8]
    mov [arg_list], rax
.last:
    mov rdi, [arg_list]
    call parse_number
    jc .usage
    mov r13, rax

.print:
    cmp rbx, r13
    ja .done
    mov rdi, rbx
    call out_number
    mov edi, 10
    call out_char
    add rbx, r12
    jmp .print

.done:
    call out_flush
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret

.usage:
    pop r13
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'seq', 0
usage_text db 'usage: seq [first [step]] last', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
