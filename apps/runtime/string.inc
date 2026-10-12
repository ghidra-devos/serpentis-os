; rdi -> string. out: rax = length
str_len:
    mov rax, rdi
.scan:
    cmp byte [rax], 0
    je .done
    inc rax
    jmp .scan
.done:
    sub rax, rdi
    ret

str_equal:
.next:
    mov al, [rdi]
    cmp al, [rsi]
    jne .different
    test al, al
    jz .same
    inc rdi
    inc rsi
    jmp .next
.same:
    mov eax, 1
    ret
.different:
    xor eax, eax
    ret

str_copy:
    mov rax, rdi
.next:
    mov cl, [rsi]
    mov [rdi], cl
    inc rsi
    inc rdi
    test cl, cl
    jnz .next
    ret

base_name:
    mov rax, rsi
.scan:
    mov dl, [rsi]
    test dl, dl
    jz .done
    inc rsi
    cmp dl, '/'
    jne .scan
    cmp byte [rsi], 0
    je .done
    mov rax, rsi
    jmp .scan
.done:
    ret

arg_next:
    xor eax, eax
    mov rdx, rdi
    test rdi, rdi
    jz .done

.skip:
    cmp byte [rdx], ' '
    jne .start
    inc rdx
    jmp .skip

.start:
    cmp byte [rdx], 0
    je .done
    mov r8b, [rdx]
    cmp r8b, '"'
    je .quoted
    cmp r8b, 39
    je .quoted
    mov rax, rdx

.scan:
    inc rdx
    mov cl, [rdx]
    test cl, cl
    jz .done
    cmp cl, ' '
    jne .scan
    mov byte [rdx], 0
    inc rdx
    ret

.quoted:
    inc rdx
    mov rax, rdx
.quote_scan:
    mov cl, [rdx]
    test cl, cl
    jz .done
    cmp cl, r8b
    je .quote_end
    inc rdx
    jmp .quote_scan
.quote_end:
    mov byte [rdx], 0
    inc rdx
    cmp byte [rdx], ' '
    jne .done
    inc rdx

.done:
    ret
