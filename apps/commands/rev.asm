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
    call reverse_lines
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

reverse_lines:
    push rbx
    push r12
    push r13
    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
.line:
    cmp rbx, r12
    jae .done
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    push rax
    mov r13, rax
.char:
    cmp r13, rbx
    je .newline
    dec r13
    movzx edi, byte [r13]
    call out_char
    jmp .char
.newline:
    mov edi, 10
    call out_char
    pop rbx
    inc rbx
    jmp .line
.done:
    pop r13
    pop r12
    pop rbx
    ret

program_name db 'rev', 0
usage_text db 'usage: rev file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
