include '../runtime/program.inc'

LINES_MAX = 32768


main:
    push rbx
    push r12
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    mov ebx, eax
    mov ecx, ebx
    call use_stdin
    cmp ebx, [arg_count]
    jae .usage
.file:
    cmp ebx, [arg_count]
    jae .sort
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_more
    jnc .file
    mov eax, 2
    pop r12
    pop rbx
    ret

.sort:
    call index_lines
    call shell_sort
    call print_sorted
    call out_flush
    xor eax, eax
    pop r12
    pop rbx
    ret

.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

index_lines:
    lea rdi, [file_data]
    mov esi, [file_size]
    add rsi, rdi
    xor ecx, ecx
.next:
    cmp rdi, rsi
    jae .done
    cmp ecx, LINES_MAX
    jae .done
    mov [lines + rcx * 8], rdi
    inc ecx
    push rcx
    call line_end_at
    pop rcx
    lea rdi, [rax + 1]
    jmp .next
.done:
    mov [line_total], ecx
    ret

; shell sort with halving gaps. no recursion, no extra memory, quick enough for 32K lines
shell_sort:
    push rbx
    push r12
    push r13
    push r14
    mov ebx, [line_total]
.gap:
    shr ebx, 1
    jz .done
    mov r12d, ebx
.item:
    cmp r12d, [line_total]
    jae .gap
    mov r14, [lines + r12 * 8]
    mov r13d, r12d
.shift:
    cmp r13d, ebx
    jb .place
    mov eax, r13d
    sub eax, ebx
    mov rdi, [lines + rax * 8]
    mov rsi, r14
    call compare_lines
    test eax, eax
    jle .place
    mov eax, r13d
    sub eax, ebx
    mov rax, [lines + rax * 8]
    mov [lines + r13 * 8], rax
    sub r13d, ebx
    jmp .shift
.place:
    mov [lines + r13 * 8], r14
    inc r12d
    jmp .item
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

compare_lines:
    cmp byte [numeric], 0
    je .text
    push rsi
    call leading_number
    mov rdx, rax
    mov rdi, [rsp]
    push rdx
    call leading_number
    pop rdx
    pop rsi
    cmp rdx, rax
    jl .less
    jg .greater
    jmp .equal
.text:
    call line_compare
    test eax, eax
    jz .equal
    jns .greater
.less:
    mov eax, -1
    jmp .direction
.greater:
    mov eax, 1
.direction:
    cmp byte [reverse], 0
    je .done
    neg eax
.done:
    ret
.equal:
    xor eax, eax
    ret

leading_number:
    xor eax, eax
    xor r8d, r8d
.blank:
    cmp byte [rdi], ' '
    jne .sign
    inc rdi
    jmp .blank
.sign:
    cmp byte [rdi], '-'
    jne .digit
    mov r8d, 1
    inc rdi
.digit:
    movzx ecx, byte [rdi]
    sub ecx, '0'
    cmp ecx, 9
    ja .done
    imul rax, 10
    add rax, rcx
    inc rdi
    jmp .digit
.done:
    test r8d, r8d
    jz .positive
    neg rax
.positive:
    ret

print_sorted:
    push rbx
    push r12
    xor ebx, ebx
.next:
    cmp ebx, [line_total]
    jae .done
    mov r12, [lines + rbx * 8]
    cmp byte [unique], 0
    je .print
    test ebx, ebx
    jz .print
    mov rdi, [lines + rbx * 8 - 8]
    mov rsi, r12
    call compare_lines
    test eax, eax
    jz .skip
.print:
    mov rdi, r12
    mov esi, [file_size]
    lea rsi, [file_data + rsi]
    mov rdx, 1
    call out_lines
.skip:
    inc ebx
    jmp .next
.done:
    pop r12
    pop rbx
    ret

program_name db 'sort', 0
usage_text db 'usage: sort [-nru] file...', 0
flag_letters db 'nru', 0

flags:
numeric db 0
reverse db 0
unique db 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
lines rq LINES_MAX
line_total rd 1
