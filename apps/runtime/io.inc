; stdout helpers

; rdi -> zero terminated string
print:
    push rdi
    call str_len
    pop rsi
    mov rdx, rax
    mov edi, FD_STDOUT
    jmp sys_write

print_line:
    call print
    mov edi, 10

print_char:
    push rdi
    mov rsi, rsp
    mov edi, FD_STDOUT
    mov edx, 1
    call sys_write
    pop rdi
    ret

u64_to_dec:
    mov rax, rdi
    lea r8, [number_buffer + 20]
    mov byte [r8], 0
    mov ecx, 10
.digit:
    xor edx, edx
    div rcx
    add dl, '0'
    dec r8
    mov [r8], dl
    test rax, rax
    jnz .digit
    mov rax, r8
    ret

print_u64:
    call u64_to_dec
    mov rdi, rax
    jmp print

print_i64:
    test rdi, rdi
    jns print_u64
    push rdi
    mov edi, '-'
    call print_char
    pop rdi
    neg rdi
    jmp print_u64

number_buffer:
    times 21 db 0
