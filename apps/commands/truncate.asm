include '../runtime/program.inc'

ZERO_BYTES = 4096               ; zeros go out in blocks this big

main:
    push rbx
    push r12
    call collect_args
    mov qword [count_value], -1
    call take_count_option
    jc .usage
    cmp qword [count_value], -1
    je .usage
    cmp qword [count_value], FILE_MAX
    ja .too_large
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call resize
    test eax, eax
    jns .file
    lea rdi, [program_name]
    mov rsi, [arg_list + rbx * 8 - 8]
    mov edx, eax
    call report_error
    mov r12d, 1
    jmp .file

.done:
    mov eax, r12d
    pop r12
    pop rbx
    ret
.too_large:
    lea rdi, [too_large_text]
    call print_line
    mov eax, 1
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

resize:
    push rbx
    push r12
    push r13
    mov r12, rdi
    lea rsi, [file_data]
    mov edx, FILE_MAX
    call read_whole
    test eax, eax
    jns .loaded
    xor eax, eax
.loaded:
    mov r13d, eax
    mov rdi, r12
    call sys_unlink
    mov rdi, r12
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .out
    mov ebx, eax
    mov rdx, [count_value]
    cmp rdx, r13
    jbe .keep
    mov rdx, r13
.keep:
    test edx, edx
    jz .grow
    mov edi, ebx
    lea rsi, [file_data]
    call sys_write
.grow:
    mov eax, dword [count_value]
    sub eax, r13d
    jbe .close
    mov r13d, eax
.zeros:
    mov edx, r13d
    cmp edx, ZERO_BYTES
    jbe .last
    mov edx, ZERO_BYTES
.last:
    sub r13d, edx
    mov edi, ebx
    lea rsi, [zero_block]
    call sys_write
    test r13d, r13d
    jnz .zeros
.close:
    mov edi, ebx
    call sys_close
    xor eax, eax
.out:
    pop r13
    pop r12
    pop rbx
    ret

program_name db 'truncate', 0
usage_text db 'usage: truncate -s size file...', 0
too_large_text db 'truncate: size is larger than a file can be', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

zero_block rb ZERO_BYTES
