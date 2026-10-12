; kernel text console. a 512 line document of (char, attr) cells with a 78x27 window on it.
; while a kernel command runs for lds (sys_kcommand) terminal_capture is set and everything
; printed goes into the capture buffer instead of the screen

terminal_init:
    mov byte [terminal_foreground], COLOR_WHITE
    mov byte [terminal_background], COLOR_BLACK
    mov byte [terminal_cursor_enabled], 1
    mov byte [terminal_cursor_phase], 1
    mov byte [terminal_cursor_dirty], 1
    mov byte [terminal_cursor_drawn], 0

    call document_reset
    call terminal_redraw
    ret

document_reset:
    push rax
    push rcx
    push rdx
    push rdi

    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]

    mov al, ' '
    mov ah, dl

    mov rdi, document_buffer
    mov ecx, DOCUMENT_MAX_LINES * TERMINAL_COLUMNS
    cld
    rep stosw

    mov rdi, document_line_lengths
    xor eax, eax
    mov ecx, DOCUMENT_MAX_LINES
    rep stosb

    mov rdi, document_protected_lengths
    xor eax, eax
    mov ecx, DOCUMENT_MAX_LINES
    rep stosb

    ; document_continued is a leftover, nothing reads it anymore. remove it one day
    mov rdi, document_continued
    mov ecx, DOCUMENT_MAX_LINES
    rep stosb

    mov word [document_line_count], 1
    mov word [document_cursor_line], 0
    mov word [document_cursor_column], 0
    mov word [document_view_top], 0

    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

document_clear_line:
    push rax
    push rcx
    push rdx
    push rdi
    push r10

    mov r10d, eax

    imul eax, TERMINAL_LINE_BYTES
    lea rdi, [document_buffer + rax]

    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]

    mov al, ' '
    mov ah, dl

    mov ecx, TERMINAL_COLUMNS
    cld
    rep stosw

    mov byte [document_line_lengths + r10], 0
    mov byte [document_protected_lengths + r10], 0
    mov byte [document_continued + r10], 0

    pop r10
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

document_ensure_line:
    push rax
    push rbx
    push r12

    mov r12d, eax

    cmp r12d, DOCUMENT_MAX_LINES
    jae .done

    movzx ebx, word [document_line_count]

.next:
    cmp ebx, r12d
    ja .done

    mov eax, ebx
    call document_clear_line

    inc ebx
    mov [document_line_count], bx
    jmp .next

.done:
    pop r12
    pop rbx
    pop rax
    ret

document_drop_first_line:
    push rax
    push rcx
    push rsi
    push rdi

    mov rsi, document_buffer + TERMINAL_LINE_BYTES
    mov rdi, document_buffer
    mov ecx, (DOCUMENT_MAX_LINES - 1) * TERMINAL_LINE_BYTES
    cld
    rep movsb

    mov rsi, document_line_lengths + 1
    mov rdi, document_line_lengths
    mov ecx, DOCUMENT_MAX_LINES - 1
    rep movsb

    mov rsi, document_protected_lengths + 1
    mov rdi, document_protected_lengths
    mov ecx, DOCUMENT_MAX_LINES - 1
    cld
    rep movsb

    mov byte [document_protected_lengths + DOCUMENT_MAX_LINES - 1], 0

    mov rsi, document_continued + 1
    mov rdi, document_continued
    mov ecx, DOCUMENT_MAX_LINES - 1
    rep movsb

    mov eax, DOCUMENT_MAX_LINES - 1
    call document_clear_line

    cmp word [document_cursor_line], 0
    je .view

    dec word [document_cursor_line]

.view:
    cmp word [document_view_top], 0
    je .done

    dec word [document_view_top]

.done:
    pop rdi
    pop rsi
    pop rcx
    pop rax
    ret

terminal_prepare_change:
    call mouse_cursor_erase
    call terminal_cursor_erase
    ret

terminal_cursor_touch:
    mov byte [terminal_cursor_phase], 1
    mov byte [terminal_cursor_dirty], 1
    mov byte [mouse_cursor_dirty], 1
    ret

terminal_set_foreground:
    and al, 0x0F
    mov [terminal_foreground], al
    ret

; eax = column, ebx = screen row. redraws that one cell from the document
terminal_draw_cell:
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14

    mov r10d, eax
    mov r11d, ebx

    cmp r10d, TERMINAL_COLUMNS
    jae .done

    cmp r11d, TERMINAL_ROWS
    jae .done

    movzx r12d, word [document_view_top]
    add r12d, r11d

    mov r14b, ' '
    mov r13b, [terminal_background]
    shl r13b, 4
    or r13b, [terminal_foreground]

    movzx eax, word [document_line_count]
    cmp r12d, eax
    jae .render

    mov eax, r12d
    imul eax, TERMINAL_LINE_BYTES
    mov edx, r10d
    shl edx, 1
    add eax, edx

    mov r14b, [document_buffer + rax]
    mov r13b, [document_buffer + rax + 1]

.render:
    movzx r8d, r13b
    and r8d, 0x0F

    movzx r9d, r13b
    shr r9d, 4
    and r9d, 0x0F

    mov ebx, r10d
    shl ebx, 3
    add ebx, TERMINAL_X

    mov ecx, r11d
    shl ecx, 4
    add ecx, TERMINAL_Y

    mov al, r14b
    call font_draw_char

.done:
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; same, inverted. if fg == bg we would draw nothing visible, use yellow on blue instead
terminal_draw_cursor_cell:
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14

    mov r10d, eax
    mov r11d, ebx

    cmp r10d, TERMINAL_COLUMNS
    jae .done

    cmp r11d, TERMINAL_ROWS
    jae .done

    movzx r12d, word [document_view_top]
    add r12d, r11d

    mov r14b, ' '
    mov r13b, [terminal_background]
    shl r13b, 4
    or r13b, [terminal_foreground]

    movzx eax, word [document_line_count]
    cmp r12d, eax
    jae .render

    mov eax, r12d
    imul eax, TERMINAL_LINE_BYTES
    mov edx, r10d
    shl edx, 1
    add eax, edx

    mov r14b, [document_buffer + rax]
    mov r13b, [document_buffer + rax + 1]

.render:
    movzx r9d, r13b
    and r9d, 0x0F

    movzx r8d, r13b
    shr r8d, 4
    and r8d, 0x0F

    cmp r8d, r9d
    jne .colors_ready

    mov r8d, COLOR_YELLOW
    mov r9d, COLOR_BLUE

.colors_ready:
    mov ebx, r10d
    shl ebx, 3
    add ebx, TERMINAL_X

    mov ecx, r11d
    shl ecx, 4
    add ecx, TERMINAL_Y

    mov al, r14b
    call font_draw_char

.done:
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

terminal_cursor_erase:
    cmp byte [terminal_capture], 0
    je .live
    ret
.live:
    cmp byte [terminal_cursor_drawn], 0
    je .done

    mov byte [terminal_cursor_drawn], 0

    movzx edx, word [terminal_cursor_drawn_line]
    movzx ecx, word [document_view_top]

    cmp edx, ecx
    jb .done

    sub edx, ecx

    cmp edx, TERMINAL_ROWS
    jae .done

    movzx eax, word [terminal_cursor_drawn_column]
    mov ebx, edx
    call terminal_draw_cell

.done:
    ret

terminal_cursor_draw_current:
    cmp byte [terminal_cursor_enabled], 0
    je .done

    cmp byte [terminal_cursor_phase], 0
    je .done

    movzx edx, word [document_cursor_line]
    movzx ecx, word [document_view_top]

    cmp edx, ecx
    jb .done

    sub edx, ecx

    cmp edx, TERMINAL_ROWS
    jae .done

    movzx eax, word [document_cursor_column]

    cmp eax, TERMINAL_COLUMNS
    jae .done

    mov ebx, edx
    call terminal_draw_cursor_cell

    mov ax, [document_cursor_line]
    mov [terminal_cursor_drawn_line], ax

    mov ax, [document_cursor_column]
    mov [terminal_cursor_drawn_column], ax

    mov byte [terminal_cursor_drawn], 1

.done:
    ret

terminal_clear:
    cmp byte [terminal_capture], 0
    je .live
    push rax
    mov al, 12
    call terminal_capture_char
    pop rax
    ret
.live:
    call terminal_prepare_change
    call document_reset

    mov eax, TERMINAL_X
    mov ebx, TERMINAL_Y
    mov ecx, TERMINAL_WIDTH
    mov edx, TERMINAL_HEIGHT
    mov r8b, [terminal_background]
    call vga_fill_rect_aligned

    mov byte [terminal_cursor_drawn], 0
    call terminal_cursor_touch
    ret

terminal_redraw:
    cmp byte [terminal_capture], 0
    je .live
    ret
.live:
    push r12
    push r13

    call terminal_prepare_change

    mov eax, TERMINAL_X
    mov ebx, TERMINAL_Y
    mov ecx, TERMINAL_WIDTH
    mov edx, TERMINAL_HEIGHT
    mov r8b, [terminal_background]
    call vga_fill_rect_aligned

    xor r12d, r12d

.row:
    cmp r12d, TERMINAL_ROWS
    jae .done

    xor r13d, r13d

.column:
    cmp r13d, TERMINAL_COLUMNS
    jae .next_row

    mov eax, r13d
    mov ebx, r12d
    call terminal_draw_cell

    inc r13d
    jmp .column

.next_row:
    inc r12d
    jmp .row

.done:
    mov byte [terminal_cursor_drawn], 0
    mov byte [mouse_cursor_drawn], 0
    call terminal_cursor_touch

    pop r13
    pop r12
    ret

terminal_redraw_line_absolute:
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r12
    push r13

    mov r12d, eax

    movzx r13d, word [document_view_top]

    cmp r12d, r13d
    jb .done

    sub r13d, r12d
    neg r13d

    cmp r13d, TERMINAL_ROWS
    jae .done

    mov eax, TERMINAL_X
    mov ebx, r13d
    shl ebx, 4
    add ebx, TERMINAL_Y

    mov ecx, TERMINAL_WIDTH
    mov edx, FONT_HEIGHT
    mov r8b, [terminal_background]
    call vga_fill_rect_aligned

    xor ecx, ecx

.draw:
    cmp ecx, TERMINAL_COLUMNS
    jae .done

    mov eax, ecx
    mov ebx, r13d
    call terminal_draw_cell

    inc ecx
    jmp .draw

.done:
    pop r13
    pop r12
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

terminal_ensure_cursor_visible:
    push rax
    push rbx
    push rcx

    movzx eax, word [document_cursor_line]
    movzx ebx, word [document_view_top]

    mov ecx, ebx
    add ecx, TERMINAL_ROWS

    cmp eax, ebx
    jb .move_up

    cmp eax, ecx
    jae .move_down

    call terminal_cursor_touch
    jmp .done

.move_up:
    mov [document_view_top], ax
    call terminal_redraw
    jmp .done

.move_down:
    sub eax, TERMINAL_ROWS - 1
    mov [document_view_top], ax
    call terminal_redraw

.done:
    pop rcx
    pop rbx
    pop rax
    ret

; al = char. \n, \r and tab are handled, everything else lands at the cursor.
; echoes to com1 too, handy when the screen is gone
terminal_putc:
    cmp byte [terminal_capture], 0
    jne terminal_capture_char
    cmp al, 10
    je terminal_newline

    cmp al, 13
    je terminal_carriage_return

    cmp al, 9
    je terminal_tab

    call serial_putc

    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov r15b, al

    call terminal_prepare_change

    movzx r12d, word [document_cursor_line]

    mov eax, r12d
    call document_ensure_line

    movzx r13d, word [document_cursor_column]

    cmp r13d, TERMINAL_COLUMNS
    jb .column_ready

    mov word [document_cursor_column], 0
    call terminal_newline

    movzx r12d, word [document_cursor_line]
    xor r13d, r13d

.column_ready:
    mov eax, r12d
    imul eax, TERMINAL_LINE_BYTES

    mov edx, r13d
    shl edx, 1
    add eax, edx

    mov [document_buffer + rax], r15b

    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]

    mov [document_buffer + rax + 1], dl

    movzx r14d, byte [document_line_lengths + r12]

    mov r10d, r13d
    inc r10d

    cmp r10d, r14d
    jbe .draw

    mov [document_line_lengths + r12], r10b

.draw:
    movzx ebx, word [document_view_top]

    cmp r12d, ebx
    jb .advance

    mov r11d, r12d
    sub r11d, ebx

    cmp r11d, TERMINAL_ROWS
    jae .advance

    mov eax, r13d
    mov ebx, r11d
    call terminal_draw_cell

.advance:
    cmp r13d, TERMINAL_COLUMNS - 1
    je .wrap

    inc r13d
    mov [document_cursor_column], r13w

    call terminal_cursor_touch
    jmp .restore

.wrap:
    call terminal_newline

.restore:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax

.done_direct:
    ret

terminal_carriage_return:
    call terminal_prepare_change
    mov word [document_cursor_column], 0
    call terminal_cursor_touch
    ret

terminal_newline:
    cmp byte [terminal_capture], 0
    je .live
    push rax
    mov al, 10
    call terminal_capture_char
    pop rax
    ret
.live:
    push rax
    push r12

    mov al, 10
    call serial_putc

    call terminal_prepare_change

    mov word [document_cursor_column], 0

    movzx r12d, word [document_cursor_line]
    inc r12d

    cmp r12d, DOCUMENT_MAX_LINES
    jb .normal

    call document_drop_first_line

    mov r12d, DOCUMENT_MAX_LINES - 1
    mov [document_cursor_line], r12w

    call terminal_redraw
    jmp .visible

.normal:
    mov [document_cursor_line], r12w

    mov eax, r12d
    call document_ensure_line

.visible:
    call terminal_ensure_cursor_visible
    call terminal_cursor_touch

    pop r12
    pop rax
    ret

terminal_tab:
    push rax
    push rcx
    push rdx

    movzx ecx, word [document_cursor_column]
    and ecx, 3

    mov edx, 4
    sub edx, ecx

.next:
    mov al, ' '
    call terminal_putc

    dec edx
    jnz .next

    pop rdx
    pop rcx
    pop rax
    ret

terminal_print:
.next:
    lodsb

    test al, al
    jz .done

    call terminal_putc
    jmp .next

.done:
    ret

terminal_print_line:
    call terminal_print
    call terminal_newline
    ret

document_delete_backward:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    call terminal_prepare_change

    movzx r12d, word [document_cursor_line]
    movzx r13d, word [document_cursor_column]

    movzx edx, byte [document_protected_lengths + r12]

    cmp r13d, edx
    jb .done

    ja .same_line

    test edx, edx
    jnz .done

    test r13d, r13d
    jnz .same_line

    test r12d, r12d
    jz .done

    dec r12d
    mov [document_cursor_line], r12w

    movzx r13d, byte [document_line_lengths + r12]

    cmp r13d, TERMINAL_COLUMNS
    jb .previous_column_ready

    mov r13d, TERMINAL_COLUMNS - 1

.previous_column_ready:
    mov [document_cursor_column], r13w

    call terminal_ensure_cursor_visible
    jmp .done

.same_line:
    dec r13d

    movzx r14d, byte [document_line_lengths + r12]

    cmp r13d, r14d
    jb .delete_character

    mov [document_cursor_column], r13w
    jmp .done

.delete_character:
    call document_remove_char_in_line

    mov [document_cursor_column], r13w

.done:
    call terminal_cursor_touch

    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

document_remove_char_in_line:
    mov eax, r12d
    imul eax, TERMINAL_LINE_BYTES
    lea rbx, [document_buffer + rax]

    mov ecx, r14d
    sub ecx, r13d
    dec ecx

    jz .clear_tail

    mov eax, r13d
    shl eax, 1
    lea rdi, [rbx + rax]

    lea rsi, [rdi + 2]

    cld
    rep movsw

.clear_tail:
    dec r14d

    mov eax, r14d
    shl eax, 1

    mov byte [rbx + rax], ' '

    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]

    mov [rbx + rax + 1], dl

    mov [document_line_lengths + r12], r14b

    mov eax, r12d
    call terminal_redraw_line_absolute

    ret

document_set_cursor_screen:
    push rax
    push rbx
    push r12
    push r13

    mov r12d, eax
    mov r13d, ebx

    call terminal_prepare_change

    cmp r12d, TERMINAL_COLUMNS
    jb .column_ready

    mov r12d, TERMINAL_COLUMNS - 1

.column_ready:
    cmp r13d, TERMINAL_ROWS
    jb .row_ready

    mov r13d, TERMINAL_ROWS - 1

.row_ready:
    movzx eax, word [document_view_top]
    add eax, r13d

    cmp eax, DOCUMENT_MAX_LINES
    jb .line_ready

    mov eax, DOCUMENT_MAX_LINES - 1

.line_ready:
    mov [document_cursor_line], ax
    mov [document_cursor_column], r12w

    call document_ensure_line
    call document_clamp_cursor
    call terminal_cursor_touch

    pop r13
    pop r12
    pop rbx
    pop rax
    ret

terminal_backspace:
    call document_delete_backward
    ret

document_clamp_cursor:
    movzx eax, word [document_cursor_line]
    cmp eax, DOCUMENT_MAX_LINES
    jae .done

    movzx edx, byte [document_protected_lengths + rax]
    cmp word [document_cursor_column], dx
    jae .done

    mov [document_cursor_column], dx

.done:
    ret

terminal_foreground db COLOR_WHITE
terminal_background db COLOR_BLACK
terminal_cursor_enabled db 1
terminal_cursor_phase db 1
terminal_cursor_dirty db 1
terminal_cursor_drawn db 0
terminal_cursor_drawn_line dw 0
terminal_cursor_drawn_column dw 0

document_line_count dw 1
document_cursor_line dw 0
document_cursor_column dw 0
document_view_top dw 0

align 16
; document_line_lengths: used length per line, document_protected_lengths: how far backspace
; may go back on that line
document_line_lengths:
times DOCUMENT_MAX_LINES db 0

align 16
document_protected_lengths:
times DOCUMENT_MAX_LINES db 0

align 16
document_continued:
times DOCUMENT_MAX_LINES db 0

align 16
document_buffer:
times DOCUMENT_BUFFER_BYTES db 0
