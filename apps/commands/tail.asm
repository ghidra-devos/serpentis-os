include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    mov qword [count_value], 10
    call take_count_option
    jc .usage
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call print_tail
    jmp .file

.failed:
    mov r12d, 1
    jmp .file

.done:
    call out_flush
    mov eax, r12d
    pop r12
    pop rbx
    ret

.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

print_tail:
    lea rdi, [file_data]
    mov esi, [file_size]
    add rsi, rdi
    mov rdx, [count_value]
    test rdx, rdx
    jz .done
    mov rax, rsi
    cmp rax, rdi
    je .done
    cmp byte [rax - 1], 10
    jne .scan
    dec rax
.scan:
    cmp rax, rdi
    je .print
    cmp byte [rax - 1], 10
    jne .back
    dec rdx
    jz .print
.back:
    dec rax
    jmp .scan
.print:
    mov rdi, rax
    mov rdx, -1
    jmp out_lines
.done:
    ret

program_name db 'tail', 0
usage_text db 'usage: tail [-n lines] file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
