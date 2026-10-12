FONT_HEIGHT_PIXELS = 16
POINTER_W = 8
POINTER_H = 12

draw_set_target:
    mov [draw_base], rdi
    mov [draw_stride], esi
    mov [draw_width], edx
    mov [draw_height], ecx
    ret

draw_fill:
    push rbx
    test edi, edi
    jns .x_ok
    add edx, edi
    xor edi, edi
.x_ok:
    test esi, esi
    jns .y_ok
    add ecx, esi
    xor esi, esi
.y_ok:
    mov eax, [draw_width]
    sub eax, edi
    jle .done
    cmp edx, eax
    jle .w_ok
    mov edx, eax
.w_ok:
    mov eax, [draw_height]
    sub eax, esi
    jle .done
    cmp ecx, eax
    jle .h_ok
    mov ecx, eax
.h_ok:
    test edx, edx
    jle .done
    test ecx, ecx
    jle .done

    mov eax, esi
    imul eax, [draw_stride]
    add eax, edi
    mov rbx, [draw_base]
    add rbx, rax
    mov eax, r8d
.row:
    mov rdi, rbx
    push rcx
    mov ecx, edx
    rep stosb
    pop rcx
    mov eax, r8d
    mov esi, [draw_stride]
    add rbx, rsi
    dec ecx
    jnz .row
.done:
    pop rbx
    ret

draw_char:
    push rbx
    push r12
    push r13
    push r14
    push r15

    movzx eax, dl
    shl eax, 4
    mov r12, [draw_font]
    add r12, rax
    mov r13d, edi
    mov r14d, esi
    xor r15d, r15d

.row:
    mov eax, r14d
    add eax, r15d
    js .next_row
    cmp eax, [draw_height]
    jge .done
    imul eax, [draw_stride]
    mov rbx, [draw_base]
    add rbx, rax
    mov al, [r12 + r15]
    xor edx, edx
.column:
    mov esi, r13d
    add esi, edx
    js .next_column
    cmp esi, [draw_width]
    jge .next_row
    test al, 0x80
    jz .background
    mov [rbx + rsi], cl
    jmp .next_column
.background:
    test r8d, r8d
    js .next_column
    mov [rbx + rsi], r8b
.next_column:
    shl al, 1
    inc edx
    cmp edx, 8
    jb .column
.next_row:
    inc r15d
    cmp r15d, FONT_HEIGHT_PIXELS
    jb .row
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

draw_text:
    push rbx
    push r12
    push r13
    mov rbx, rdx
    mov r12d, edi
    mov r13d, esi
.next:
    mov dl, [rbx]
    test dl, dl
    jz .done
    mov edi, r12d
    mov esi, r13d
    call draw_char
    add r12d, 8
    inc rbx
    jmp .next
.done:
    pop r13
    pop r12
    pop rbx
    ret

pointer_shape:
    db 'X.......'
    db 'XX......'
    db 'XoX.....'
    db 'XooX....'
    db 'XoooX...'
    db 'XooooX..'
    db 'XoooooX.'
    db 'XooooooX'
    db 'XoooXXXX'
    db 'XoXoX...'
    db 'XX.XoX..'
    db 'X...XX..'

align 8
draw_base dq 0
draw_font dq 0
draw_stride dd 0
draw_width dd 0
draw_height dd 0
