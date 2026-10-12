block_reset:
    mov qword [repeat_counter], 0
    ret

block_word:
.skip:
    cmp rsi, rdx
    jae .none
    mov al, [rsi]
    cmp al, ' '
    je .next
    cmp al, 9
    je .next
    cmp al, 13
    je .next
    jmp .start
.next:
    inc rsi
    jmp .skip
.start:
    mov rax, rsi
.scan:
    cmp rsi, rdx
    jae .done
    mov cl, [rsi]
    cmp cl, ' '
    je .done
    cmp cl, 9
    je .done
    cmp cl, 13
    je .done
    cmp cl, ';'
    je .done
    inc rsi
    jmp .scan
.done:
    mov rcx, rsi
    sub rcx, rax
    ret
.none:
    mov rax, rsi
    xor ecx, ecx
    ret

block_word_is:
    push rsi
    mov rsi, rax
.compare:
    test ecx, ecx
    jz .end
    mov al, [rdi]
    test al, al
    jz .differ
    mov ah, [rsi]
    or ax, 0x2020
    cmp al, ah
    jne .differ
    inc rdi
    inc rsi
    dec ecx
    jmp .compare
.end:
    cmp byte [rdi], 0
    jne .differ
    pop rsi
    stc
    ret
.differ:
    pop rsi
    clc
    ret

block_file_end:
    mov rax, [line_rsp]
    mov rax, [rax + 8]
    ret

; rdi -> first body line. finds the matching "end repeat", nested repeats counted.
; out: CF, rax = body end, rdx = end of the end repeat line
block_find_repeat_end:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rbx, rdi
    call block_file_end
    mov r12, rax
    mov r13d, 1
.line:
    cmp rbx, r12
    jae .missing
    mov r14, rbx
.scan:
    cmp r14, r12
    jae .ready
    cmp byte [r14], 10
    je .ready
    inc r14
    jmp .scan
.ready:
    mov rsi, rbx
    mov rdx, r14
    call block_word
    test ecx, ecx
    jz .advance
    mov r15, rax
    mov [block_word_length], ecx
    push rsi
    lea rdi, [kw_repeat]
    call block_word_is
    pop rsi
    jc .deeper
    mov rax, r15
    mov ecx, [block_word_length]
    push rsi
    lea rdi, [kw_end]
    call block_word_is
    pop rsi
    jnc .advance
    mov rdx, r14
    call block_word
    test ecx, ecx
    jz .advance
    lea rdi, [kw_repeat]
    call block_word_is
    jnc .advance
    dec r13d
    jz .found
    jmp .advance
.deeper:
    inc r13d
.advance:
    lea rbx, [r14 + 1]
    jmp .line
.found:
    mov rax, rbx
    mov rdx, r14
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    stc
    ret
.missing:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret

block_run_lines:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rbx, rdi
    mov r12, rsi
    mov r13, [line_start]
    mov r14, [line_end]
    mov r15d, [line_number]
    mov [line_number], edx
.next:
    cmp rbx, r12
    jae .done
    mov [line_start], rbx
    mov rdi, rbx
.scan:
    cmp rdi, r12
    jae .ready
    cmp byte [rdi], 10
    je .ready
    inc rdi
    jmp .scan
.ready:
    mov [line_end], rdi
    inc dword [line_number]
    call process_line
    mov rbx, [line_end]
    inc rbx
    jmp .next
.done:
    mov [line_start], r13
    mov [line_end], r14
    mov [line_number], r15d
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

directive_repeat:
    push rbx
    push r12
    push r13
    push r14
    push r15

    call expr_parse
    mov r15, rax
    call expect_end

    mov r12, [line_end]
    inc r12
    mov rdi, r12
    call block_find_repeat_end
    jnc .missing
    mov r13, rax
    mov r14, rdx
    mov [line_end], r14

    cmp r15, 0
    jle .done
    mov rbx, [repeat_counter]
    push rbx
    xor ebx, ebx
.iteration:
    inc rbx
    cmp rbx, r15
    jg .restore
    mov [repeat_counter], rbx
    mov rdi, r12
    mov rsi, r13
    mov edx, [line_number]
    call block_run_lines
    jmp .iteration
.restore:
    pop rbx
    mov [repeat_counter], rbx
.done:
    mov [line_end], r14
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.missing:
    lea rdi, [error_repeat_open]
    jmp asm_error
