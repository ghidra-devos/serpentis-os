; text drawing with the bios 8x16 font the loader left a pointer to

; al = char, ebx = x, ecx = y in pixels, r8b = fg, r9b = bg
font_draw_char:
    call screen_blocked         ; the gui owns the screen, kernel text would scribble over it
    jnc .allowed
    ret
.allowed:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r10
    push r11

    movzx r10d, al

    cmp ebx, VGA_WIDTH - FONT_WIDTH
    ja .done
    cmp ecx, VGA_HEIGHT - FONT_HEIGHT
    ja .done

    mov rsi, qword [FONT_POINTER_ADDRESS]
    test rsi, rsi
    jz .done
    shl r10d, 4
    add rsi, r10

    mov edi, ecx
    imul edi, FRAME_STRIDE
    add edi, ebx
    add rdi, [frame_base]

    mov r11d, FONT_HEIGHT
.row:
    mov dl, [rsi]
    mov ecx, FONT_WIDTH
.column:
    mov al, r9b
    test dl, 0x80
    jz .pixel
    mov al, r8b
.pixel:
    mov [rdi], al
    inc rdi
    shl dl, 1
    dec ecx
    jnz .column
    add rdi, FRAME_STRIDE - FONT_WIDTH
    inc rsi
    dec r11d
    jnz .row

.done:
    pop r11
    pop r10
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

text_draw_string:
    push rax
    push rbx
    push rcx
    push rsi
    push r8
    push r9
    push r10

    mov r10d, ebx

.next_character:
    lodsb
    test al, al
    jz .done

    cmp al, 10
    je .new_line

    cmp ecx, VGA_HEIGHT - FONT_HEIGHT
    ja .done

    call font_draw_char

    add ebx, FONT_WIDTH
    cmp ebx, VGA_WIDTH - FONT_WIDTH
    jbe .next_character

.new_line:
    mov ebx, r10d
    add ecx, FONT_HEIGHT
    jmp .next_character

.done:
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rcx
    pop rbx
    pop rax
    ret
