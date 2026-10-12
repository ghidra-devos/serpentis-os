include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    push r14
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [hidden]
    call parse_flags
    jc .usage
    mov ebx, eax
    mov ecx, [arg_count]
    sub ecx, ebx
    cmp ecx, 2
    jne .usage

    mov rdi, [arg_list + rbx * 8]
    call load_more
    jc .failed
    mov r12, rax
    lea r13, [rax + rdx]
    mov rdi, [arg_list + rbx * 8 + 8]
    call load_more
    jc .failed
    mov rbx, rax
    lea r14, [rax + rdx]

.step:
    cmp r12, r13
    jae .rest_second
    cmp rbx, r14
    jae .rest_first
    mov rdi, r12
    mov rsi, rbx
    call line_compare
    test eax, eax
    jz .both
    js .first
.second:
    mov rdi, rbx
    mov esi, 1
    call column_line
    mov rbx, rax
    jmp .step
.first:
    mov rdi, r12
    xor esi, esi
    call column_line
    mov r12, rax
    jmp .step
.both:
    mov rdi, r12
    mov esi, 2
    call column_line
    mov r12, rax
    mov rdi, rbx
    mov rsi, r14
    call line_end_at
    lea rbx, [rax + 1]
    jmp .step

.rest_first:
    cmp r12, r13
    jae .done
    mov rdi, r12
    xor esi, esi
    call column_line
    mov r12, rax
    jmp .rest_first
.rest_second:
    cmp rbx, r14
    jae .done
    mov rdi, rbx
    mov esi, 1
    call column_line
    mov rbx, rax
    jmp .rest_second

.done:
    call out_flush
    xor eax, eax
    jmp .out
.failed:
    mov eax, 1
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

column_line:
    push rbx
    push r12
    mov rbx, rdi
    mov r12d, esi
    mov rsi, -1
    call line_end_at
    push rax
    cmp byte [hidden + r12], 0
    jne .skip
    xor ecx, ecx
.indent:
    cmp ecx, r12d
    jae .text
    cmp byte [hidden + rcx], 0
    jne .next_indent
    push rcx
    mov edi, 9
    call out_char
    pop rcx
.next_indent:
    inc ecx
    jmp .indent
.text:
    mov rdi, rbx
    mov rsi, [rsp]
    mov edx, 1
    call out_lines
.skip:
    pop rax
    inc rax
    pop r12
    pop rbx
    ret

program_name db 'comm', 0
usage_text db 'usage: comm [-123] file1 file2', 0
flag_letters db '123', 0
hidden db 0, 0, 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
