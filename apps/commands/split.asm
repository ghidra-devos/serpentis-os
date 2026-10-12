; split into prefixaa, prefixab ... each with -l lines (1000 if not given). 676 files at most
; like the real one with two letter suffixes

include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    push r14
    call collect_args
    mov qword [count_value], 1000
    call take_count_option
    jc .usage
    cmp qword [count_value], 0
    je .usage
    mov eax, [arg_count]
    test eax, eax
    jz .usage
    cmp eax, 2
    ja .usage
    lea rsi, [default_prefix]
    cmp eax, 2
    jne .prefix
    mov rsi, [arg_list + 8]
.prefix:
    lea rdi, [output_name]
    call str_copy
    lea rdi, [output_name]
    call str_len
    mov [suffix_at], eax

    mov rdi, [arg_list]
    call load_input
    jc .failed
    lea rbx, [file_data]
    mov r13d, [file_size]
    add r13, rbx
    xor r14d, r14d

.chunk:
    cmp rbx, r13
    jae .done
    mov r12, rbx
    mov rcx, [count_value]
.line:
    cmp rbx, r13
    jae .write
    push rcx
    mov rdi, rbx
    mov rsi, r13
    call line_end_at
    pop rcx
    lea rbx, [rax + 1]
    cmp rbx, r13
    jbe .counted
    mov rbx, r13
.counted:
    dec rcx
    jnz .line
.write:
    mov eax, r14d
    xor edx, edx
    mov ecx, 26
    div ecx
    cmp eax, 26
    jae .too_many
    mov ecx, [suffix_at]
    add al, 'a'
    add dl, 'a'
    mov [output_name + rcx], al
    mov [output_name + rcx + 1], dl
    mov byte [output_name + rcx + 2], 0
    inc r14d
    lea rdi, [output_name]
    call sys_unlink
    lea rdi, [output_name]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .open_failed
    push rax
    mov edi, eax
    mov rsi, r12
    mov rdx, rbx
    sub rdx, r12
    call sys_write
    pop rdi
    call sys_close
    jmp .chunk

.open_failed:
    lea rdi, [program_name]
    lea rsi, [output_name]
    mov edx, eax
    call report_error
    jmp .failed
.too_many:
    lea rdi, [too_many_text]
    call print_line
.failed:
    mov eax, 1
    jmp .out
.done:
    xor eax, eax
.out:
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

program_name db 'split', 0
usage_text db 'usage: split [-l lines] file [prefix]', 0
too_many_text db 'split: output file suffixes exhausted', 0
default_prefix db 'x', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

suffix_at rd 1
output_name rb 256
