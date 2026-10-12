FIXED_DIGITS = 4
FIXED_SCALE = 10000

; xmm0 -> text at rdi
format_value:
    push rbx
    push r12
    mov rbx, rdi
    call is_finite
    jc .finite
    movq rax, xmm0
    lea rsi, [undefined_text]
    mov rcx, 0x000FFFFFFFFFFFFF
    test rax, rcx
    jnz .word
    lea rsi, [infinity_text]
    test rax, rax
    jns .word
    lea rsi, [minus_infinity_text]
.word:
    mov rdi, rbx
    call str_copy
    jmp .out
.finite:
    movsd xmm1, xmm0
    andpd xmm1, [abs_mask]
    ucomisd xmm1, [zero_limit]
    jae .signed
    xorpd xmm0, xmm0
.signed:
    movq rax, xmm0
    test rax, rax
    jns .positive
    mov rcx, 0x7FFFFFFFFFFFFFFF
    test rax, rcx
    jz .positive
    mov byte [rbx], '-'
    inc rbx
    andpd xmm0, [abs_mask]
.positive:
    ucomisd xmm0, [scientific_high]
    jae .scientific
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    je .fixed
    ucomisd xmm0, [scientific_low]
    jb .scientific
.fixed:
    mulsd xmm0, [fixed_scale]
    call math_round
    cvttsd2si rax, xmm0
    xor edx, edx
    mov ecx, FIXED_SCALE
    div rcx
    mov r12, rdx
    call write_unsigned
    test r12, r12
    jz .terminate
    mov byte [rbx], '.'
    inc rbx
    mov rax, r12
    mov ecx, FIXED_DIGITS
    call write_fraction
    jmp .terminate

.scientific:
    xor r12d, r12d
.shrink:
    ucomisd xmm0, [ten]
    jb .grow
    divsd xmm0, [ten]
    inc r12d
    jmp .shrink
.grow:
    ucomisd xmm0, [one]
    jae .mantissa
    mulsd xmm0, [ten]
    dec r12d
    jmp .grow
.mantissa:
    mulsd xmm0, [thousand]
    call math_round
    cvttsd2si rax, xmm0
    cmp rax, 10000
    jb .digits
    xor edx, edx
    mov ecx, 10
    div rcx
    inc r12d
.digits:
    xor edx, edx
    mov ecx, 1000
    div rcx
    push rdx
    call write_unsigned
    pop rax
    test rax, rax
    jz .exponent
    mov byte [rbx], '.'
    inc rbx
    mov ecx, 3
    call write_fraction
.exponent:
    mov byte [rbx], 'e'
    inc rbx
    mov eax, r12d
    test eax, eax
    jns .exponent_digits
    mov byte [rbx], '-'
    inc rbx
    neg eax
.exponent_digits:
    call write_unsigned
.terminate:
    mov byte [rbx], 0
.out:
    pop r12
    pop rbx
    ret

write_unsigned:
    lea rsi, [digit_scratch + 24]
    mov byte [rsi], 0
    mov ecx, 10
.next:
    xor edx, edx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test rax, rax
    jnz .next
.copy:
    mov al, [rsi]
    test al, al
    jz .done
    mov [rbx], al
    inc rbx
    inc rsi
    jmp .copy
.done:
    ret

write_fraction:
    lea rsi, [digit_scratch + rcx]
    mov r8, rsi
    mov r9d, ecx
    mov ecx, 10
.next:
    test r9d, r9d
    jz .trim
    xor edx, edx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    dec r9d
    jmp .next
.trim:
    cmp r8, rsi
    je .copy
    cmp byte [r8 - 1], '0'
    jne .copy
    dec r8
    jmp .trim
.copy:
    cmp rsi, r8
    jae .done
    mov al, [rsi]
    mov [rbx], al
    inc rbx
    inc rsi
    jmp .copy
.done:
    ret

align 8
scientific_high dq 0x41CDCD6500000000
scientific_low dq 0x3F1A36E2EB1C432D  ; 1e-4
fixed_scale dq 0x40C3880000000000
thousand dq 0x408F400000000000
zero_limit dq 0x3D719799812DEA11

undefined_text db 'undefined', 0
infinity_text db 'infinity', 0
minus_infinity_text db '-infinity', 0
