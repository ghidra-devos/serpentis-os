include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    push r14
    push r15
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    push rax
    mov ecx, eax
    call use_stdin
    pop rax
    mov ecx, [arg_count]
    sub ecx, eax
    cmp ecx, 1
    jne .usage
    mov rdi, [arg_list + rax * 8]
    call load_input
    jc .failed

    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
    xor r15d, r15d
.line:
    cmp rbx, r12
    jae .last
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    mov rcx, rax
    sub rcx, rbx
    test r15d, r15d
    jz .new_group
    cmp rcx, r14
    jne .new_group
    test rcx, rcx
    jz .same
    mov rsi, r13
    mov rdi, rbx
    repe cmpsb
    jne .new_group
.same:
    inc r15d
    jmp .advance
.new_group:
    push rax
    call print_group
    pop rax
    mov r13, rbx
    mov r14, rax
    sub r14, rbx
    mov r15d, 1
.advance:
    lea rbx, [rax + 1]
    jmp .line

.last:
    call print_group
    call out_flush
    xor eax, eax
    jmp .out

.failed:
    mov eax, 1
.out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.usage:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

print_group:
    test r15d, r15d
    jz .done
    cmp byte [repeated_only], 0
    je .count
    cmp r15d, 2
    jb .done
.count:
    cmp byte [show_counts], 0
    je .text
    mov edi, r15d
    mov esi, 7
    call out_padded
    mov edi, ' '
    call out_char
.text:
    mov rdi, r13
    mov esi, r14d
    call out_bytes
    mov edi, 10
    jmp out_char
.done:
    ret

program_name db 'uniq', 0
usage_text db 'usage: uniq [-cd] file', 0
flag_letters db 'cd', 0

flags:
show_counts db 0
repeated_only db 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
