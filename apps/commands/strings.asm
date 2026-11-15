include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    mov qword [count_value], 4
    call take_count_option
    jc .usage
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
    jae .flush_run
    mov al, [file_data + rcx]
    cmp al, 9
    je .printable
    cmp al, ' '
    jb .break
    cmp al, 126
    ja .break
.printable:
    inc ecx
    jmp .byte
.break:
    push rcx
    call print_run
    pop rcx
    inc ecx
    mov r12d, ecx
    jmp .byte
.flush_run:
    call print_run
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

print_run:
    mov eax, ecx
    sub eax, r12d
    cmp rax, [count_value]
    jb .done
    lea rdi, [file_data + r12]
    mov esi, eax
    call out_bytes
    mov edi, 10
    jmp out_char
.done:
    ret

program_name db 'strings', 0
usage_text db 'usage: strings [-n length] file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
