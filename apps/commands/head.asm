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
    lea rdi, [file_data]
    mov esi, [file_size]
    add rsi, rdi
    mov rdx, [count_value]
    call out_lines
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

program_name db 'head', 0
usage_text db 'usage: head [-n lines] file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
