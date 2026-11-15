include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r13d, r13d
.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call print_backwards
    jmp .file
.failed:
    mov r13d, 1
    jmp .file
.done:
    call out_flush
    mov eax, r13d
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

print_backwards:
    push rbx
    push r12
    push r13
    lea r12, [file_data]
    mov ebx, [file_size]
    add rbx, r12
    cmp rbx, r12
    je .done
    cmp byte [rbx - 1], 10
    jne .line
    dec rbx
.line:
    mov r13, rbx
.start:
    cmp r13, r12
    je .print
    cmp byte [r13 - 1], 10
    je .print
    dec r13
    jmp .start
.print:
    mov rdi, r13
    mov rsi, rbx
    sub rsi, r13
    call out_bytes
    mov edi, 10
    call out_char
    cmp r13, r12
    je .done
    lea rbx, [r13 - 1]
    jmp .line
.done:
    pop r13
    pop r12
    pop rbx
    ret

program_name db 'tac', 0
usage_text db 'usage: tac file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
