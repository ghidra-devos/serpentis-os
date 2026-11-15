SEGMENT_A = 1
SEGMENT_B = 2
SEGMENT_C = 4
SEGMENT_D = 8
SEGMENT_E = 16
SEGMENT_F = 32
SEGMENT_G = 64

draw_digit:
    push rbx
    push r12
    push r13
    push r14
    movzx ebx, byte [digit_masks + rdi]
    mov r12d, [segment_size]
    mov r13d, [digit_height]
    shr r13d, 1
    mov r14d, r13d
    sub r14d, r12d
    mov eax, r12d
    shr eax, 1
    sub r14d, eax

    mov edi, [digit_x]
    add edi, r12d
    mov esi, [digit_y]
    mov edx, [digit_width]
    sub edx, r12d
    sub edx, r12d
    mov ecx, r12d
    mov eax, SEGMENT_A
    call draw_segment

    mov edi, [digit_x]
    add edi, [digit_width]
    sub edi, r12d
    mov esi, [digit_y]
    add esi, r12d
    mov edx, r12d
    mov ecx, r14d
    mov eax, SEGMENT_B
    call draw_segment

    mov edi, [digit_x]
    add edi, [digit_width]
    sub edi, r12d
    call lower_half
    mov edx, r12d
    mov ecx, r14d
    mov eax, SEGMENT_C
    call draw_segment

    mov edi, [digit_x]
    add edi, r12d
    mov esi, [digit_y]
    add esi, [digit_height]
    sub esi, r12d
    mov edx, [digit_width]
    sub edx, r12d
    sub edx, r12d
    mov ecx, r12d
    mov eax, SEGMENT_D
    call draw_segment

    mov edi, [digit_x]
    call lower_half
    mov edx, r12d
    mov ecx, r14d
    mov eax, SEGMENT_E
    call draw_segment

    mov edi, [digit_x]
    mov esi, [digit_y]
    add esi, r12d
    mov edx, r12d
    mov ecx, r14d
    mov eax, SEGMENT_F
    call draw_segment

    mov edi, [digit_x]
    add edi, r12d
    mov esi, [digit_y]
    add esi, r13d
    mov eax, r12d
    shr eax, 1
    sub esi, eax
    mov edx, [digit_width]
    sub edx, r12d
    sub edx, r12d
    mov ecx, r12d
    mov eax, SEGMENT_G
    call draw_segment

    pop r14
    pop r13
    pop r12
    pop rbx
    ret

lower_half:
    mov esi, [digit_y]
    add esi, r13d
    mov eax, r12d
    shr eax, 1
    add esi, eax
    ret

draw_segment:
    test ebx, eax
    jz .dark
    mov r8d, COLOR_LIGHT_CYAN
    jmp draw_fill
.dark:
    ret

digit_masks:
    db SEGMENT_A or SEGMENT_B or SEGMENT_C or SEGMENT_D or SEGMENT_E or SEGMENT_F
    db SEGMENT_B or SEGMENT_C
    db SEGMENT_A or SEGMENT_B or SEGMENT_D or SEGMENT_E or SEGMENT_G
    db SEGMENT_A or SEGMENT_B or SEGMENT_C or SEGMENT_D or SEGMENT_G
    db SEGMENT_B or SEGMENT_C or SEGMENT_F or SEGMENT_G
    db SEGMENT_A or SEGMENT_C or SEGMENT_D or SEGMENT_F or SEGMENT_G
    db SEGMENT_A or SEGMENT_C or SEGMENT_D or SEGMENT_E or SEGMENT_F or SEGMENT_G
    db SEGMENT_A or SEGMENT_B or SEGMENT_C
    db SEGMENT_A or SEGMENT_B or SEGMENT_C or SEGMENT_D or SEGMENT_E or SEGMENT_F or SEGMENT_G
    db SEGMENT_A or SEGMENT_B or SEGMENT_C or SEGMENT_D or SEGMENT_F or SEGMENT_G
