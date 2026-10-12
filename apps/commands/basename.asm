include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    mov eax, [arg_count]
    test eax, eax
    jz .usage
    cmp eax, 2
    ja .usage

    mov rdi, [arg_list]
    call strip_slashes
    mov rsi, rdi
    call base_name
    mov rbx, rax
    cmp dword [arg_count], 2
    jne .print
    mov rdi, rbx
    call str_len
    mov r12, rax
    mov rdi, [arg_list + 8]
    call str_len
    cmp rax, r12
    jae .print
    mov rdi, rbx
    add rdi, r12
    sub rdi, rax
    mov rsi, [arg_list + 8]
    push rdi
    call str_equal
    pop rdi
    test eax, eax
    jz .print
    mov byte [rdi], 0
.print:
    mov rdi, rbx
    call print_line
    xor eax, eax
    pop r12
    pop rbx
    ret

.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'basename', 0
usage_text db 'usage: basename path [suffix]', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
