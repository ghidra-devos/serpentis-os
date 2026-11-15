parse_u64:
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10

    xor eax, eax
    xor r10d, r10d

    cmp byte [rsi], '0'
    jne .decimal

    mov bl, [rsi + 1]
    cmp bl, 'x'
    je .hex_prefix

    cmp bl, 'X'
    je .hex_prefix

.decimal:
    movzx ecx, byte [rsi]
    test ecx, ecx
    jz .success

    cmp ecx, '0'
    jb .failed

    cmp ecx, '9'
    ja .failed

    sub ecx, '0'

    mov rbx, 10
    mul rbx

    test rdx, rdx
    jnz .failed

    add rax, rcx
    jc .failed

    inc rsi
    inc r10d
    jmp .decimal

.hex_prefix:
    add rsi, 2

.hexadecimal:
    movzx ecx, byte [rsi]
    test ecx, ecx
    jz .success

    cmp ecx, '0'
    jb .hex_alpha

    cmp ecx, '9'
    ja .hex_alpha

    sub ecx, '0'
    jmp .hex_digit_ready

.hex_alpha:
    cmp ecx, 'a'
    jb .hex_upper

    cmp ecx, 'f'
    ja .failed

    sub ecx, 'a' - 10
    jmp .hex_digit_ready

.hex_upper:
    cmp ecx, 'A'
    jb .failed

    cmp ecx, 'F'
    ja .failed

    sub ecx, 'A' - 10

.hex_digit_ready:
    mov rbx, rax
    shr rbx, 60
    jnz .failed

    shl rax, 4
    add rax, rcx

    inc rsi
    inc r10d
    jmp .hexadecimal

.success:
    test r10d, r10d
    jz .failed

    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.failed:
    xor eax, eax
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

terminal_print_u64:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi

    mov rdi, number_buffer + 31
    mov byte [rdi], 0

    test rax, rax
    jnz .convert

    dec rdi
    mov byte [rdi], '0'
    jmp .print

.convert:
    mov rbx, 10

.next_digit:
    xor edx, edx
    div rbx

    add dl, '0'
    dec rdi
    mov [rdi], dl

    test rax, rax
    jnz .next_digit

.print:
    mov rsi, rdi
    call terminal_print

    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

terminal_print_hex64:
    push rax
    push rbx
    push rcx
    push rdi
    push rsi

    mov rbx, rax

    mov rdi, hex_buffer + 17
    mov ecx, 16

.next_digit:
    mov rax, rbx
    and eax, 0x0F
    mov al, [numbers_hex_chars + rax]
    mov [rdi], al
    dec rdi

    shr rbx, 4
    dec ecx
    jnz .next_digit

    mov rsi, hex_buffer
    call terminal_print

    pop rsi
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret

terminal_print_two_digits:
    push rax
    push rcx
    push rdx
    push rsi

    movzx eax, al

    xor edx, edx
    mov ecx, 10
    div ecx

    add al, '0'
    add dl, '0'

    mov [two_digit_buffer], al
    mov [two_digit_buffer + 1], dl

    mov rsi, two_digit_buffer
    call terminal_print

    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

numbers_hex_chars db '0123456789ABCDEF'
align 8
number_buffer: times 32 db 0
hex_buffer db '0x0000000000000000', 0
two_digit_buffer db '00', 0
