include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    mov ebx, eax
    mov ecx, eax
    call use_stdin
    cmp ebx, [arg_count]
    jae .usage
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    cmp byte [number_lines], 0
    jne .numbered
    lea rdi, [file_data]
    mov esi, [file_size]
    call out_bytes
    jmp .file
.numbered:
    call print_numbered
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

print_numbered:
    push rbx
    push r12
    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
.line:
    cmp rbx, r12
    jae .done
    inc dword [line_number]
    mov edi, [line_number]
    mov esi, 6
    call out_padded
    mov edi, 9
    call out_char
    mov rdi, rbx
    mov rsi, r12
    mov edx, 1
    call out_lines
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    lea rbx, [rax + 1]
    jmp .line
.done:
    pop r12
    pop rbx
    ret

program_name db 'cat', 0
usage_text db 'usage: cat [-n] file...', 0
flag_letters db 'n', 0
flags:
number_lines db 0
align 4
line_number dd 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
