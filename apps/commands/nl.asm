include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    push r14
    call collect_args
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r13d, r13d
    xor r14d, r14d
.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call number_lines
    jmp .file
.failed:
    mov r13d, 1
    jmp .file
.done:
    call out_flush
    mov eax, r13d
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.usage:
    pop r14
    pop r13
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

number_lines:
    push rbx
    push r12
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
    cmp rax, rbx
    je .newline
    inc r14d
    mov edi, r14d
    mov esi, 6
    call out_padded
    mov edi, 9
    call out_char
    mov rdi, rbx
    mov rsi, [rsp]
    sub rsi, rbx
    call out_bytes
.newline:
    mov edi, 10
    call out_char
    pop rbx
    inc rbx
    jmp .line
.done:
    pop r12
    pop rbx
    ret

program_name db 'nl', 0
usage_text db 'usage: nl file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
