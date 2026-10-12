SECRET_MAX = 63

; rdi -> buffer of SECRET_MAX + 1. reads a line from stdin without echoing it, programs here
; echo their own typing so leaving it out is all it takes. eax = length
read_secret:
    push rbx
    push r12
    mov rbx, rdi
    xor r12d, r12d
.key:
    mov edi, FD_STDIN
    lea rsi, [secret_key]
    mov edx, 1
    call sys_read
    test eax, eax
    jle .done
    movzx eax, byte [secret_key]
    cmp al, 10
    je .done
    cmp al, KEY_END_OF_INPUT
    je .done
    cmp al, 8
    je .erase
    cmp al, ' '
    jb .key
    cmp r12d, SECRET_MAX
    jae .key
    mov [rbx + r12], al
    inc r12d
    jmp .key
.erase:
    test r12d, r12d
    jz .key
    dec r12d
    jmp .key
.done:
    mov byte [rbx + r12], 0
    mov edi, 10
    call print_char
    mov eax, r12d
    pop r12
    pop rbx
    ret

secret_key db 0, 0, 0, 0
