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
    mov al, [show_lines]
    or al, [show_words]
    or al, [show_bytes]
    jnz .chosen
    mov byte [show_lines], 1
    mov byte [show_words], 1
    mov byte [show_bytes], 1
.chosen:
    mov eax, [arg_count]
    sub eax, ebx
    mov [file_total], eax
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .total
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call count_input
    mov rdi, [arg_list + rbx * 8 - 8]
    lea rsi, [counts]
    call print_counts
    mov rax, [counts]
    add [totals], rax
    mov rax, [counts + 8]
    add [totals + 8], rax
    mov rax, [counts + 16]
    add [totals + 16], rax
    jmp .file
.failed:
    mov r12d, 1
    jmp .file

.total:
    cmp dword [file_total], 1
    jbe .done
    lea rdi, [total_name]
    lea rsi, [totals]
    call print_counts
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

count_input:
    xor eax, eax
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    mov ecx, [file_size]
.byte:
    cmp eax, ecx
    jae .done
    mov r10b, [file_data + rax]
    inc eax
    cmp r10b, 10
    jne .space
    inc edx
.space:
    cmp r10b, ' '
    je .gap
    cmp r10b, 10
    je .gap
    cmp r10b, 9
    je .gap
    test r9d, r9d
    jnz .byte
    mov r9d, 1
    inc r8d
    jmp .byte
.gap:
    xor r9d, r9d
    jmp .byte
.done:
    mov [counts], rdx
    mov [counts + 8], r8
    mov [counts + 16], rcx
    ret

print_counts:
    push rbx
    push r12
    mov r12, rdi
    mov rbx, rsi
    cmp byte [show_lines], 0
    je .words
    mov rdi, [rbx]
    mov esi, 7
    call out_padded
.words:
    cmp byte [show_words], 0
    je .bytes
    mov rdi, [rbx + 8]
    mov esi, 8
    call out_padded
.bytes:
    cmp byte [show_bytes], 0
    je .name
    mov rdi, [rbx + 16]
    mov esi, 8
    call out_padded
.name:
    cmp word [r12], '-'
    je .end
    mov edi, ' '
    call out_char
    mov rdi, r12
    call out_text
.end:
    mov edi, 10
    call out_char
    pop r12
    pop rbx
    ret

program_name db 'wc', 0
usage_text db 'usage: wc [-lwc] file...', 0
total_name db 'total', 0
flag_letters db 'lwc', 0
flags:
show_lines db 0
show_words db 0
show_bytes db 0
align 8
totals dq 0, 0, 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

counts rq 3
file_total rd 1
