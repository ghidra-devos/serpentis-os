; eax = 1 if the zero terminated strings at rsi and rdi match
str_equal:
    push rsi
    push rdi
    push rdx

.compare:
    mov dl, [rdi]
    cmp dl, [rsi]
    jne .different

    test dl, dl
    jz .equal

    inc rdi
    inc rsi
    jmp .compare

.equal:
    mov eax, 1
    pop rdx
    pop rdi
    pop rsi
    ret

.different:
    xor eax, eax
    pop rdx
    pop rdi
    pop rsi
    ret

str_length:
    push rsi

    xor eax, eax

.next:
    cmp byte [rsi + rax], 0
    je .done

    inc rax
    jmp .next

.done:
    pop rsi
    ret
