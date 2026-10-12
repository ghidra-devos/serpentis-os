include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    mov qword [count_value], 80
    call take_count_option
    jc .usage
    cmp qword [count_value], 0
    je .usage
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
    xor ecx, ecx
    xor r12d, r12d
.byte:
    cmp ecx, [file_size]
    jae .file
    movzx edi, byte [file_data + rcx]
    inc ecx
    cmp edi, 10
    je .newline
    cmp r12, [count_value]
    jb .put
    push rcx
    push rdi
    mov edi, 10
    call out_char
    pop rdi
    pop rcx
    xor r12d, r12d
.put:
    inc r12d
    push rcx
    call out_char
    pop rcx
    jmp .byte
.newline:
    xor r12d, r12d
    push rcx
    call out_char
    pop rcx
    jmp .byte
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

program_name db 'fold', 0
usage_text db 'usage: fold [-w width] file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
