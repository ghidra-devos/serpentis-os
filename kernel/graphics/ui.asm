; black border around the text console area (24 px top and bottom, 8 px sides)
ui_draw:
    cmp byte [terminal_capture], 0
    je .live
    ret
.live:
    mov eax, 0
    mov ebx, 0
    mov ecx, 640
    mov edx, 24
    mov r8b, COLOR_BLACK
    call vga_fill_rect_aligned

    mov eax, 0
    mov ebx, 456
    mov ecx, 640
    mov edx, 24
    mov r8b, COLOR_BLACK
    call vga_fill_rect_aligned

    mov eax, 0
    mov ebx, 24
    mov ecx, 8
    mov edx, 432
    mov r8b, COLOR_BLACK
    call vga_fill_rect_aligned

    mov eax, 632
    mov ebx, 24
    mov ecx, 8
    mov edx, 432
    mov r8b, COLOR_BLACK
    call vga_fill_rect_aligned

    ret
