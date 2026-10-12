; toolbar with the back button and the path, places, the list, status line with the count
; or the last message. whole window every time, it is small
render:
    push rbx
    push r12
    push r13
    call keep_visible

    xor edi, edi
    xor esi, esi
    mov edx, [width]
    mov ecx, [height]
    mov r8d, COLOR_BLACK
    call draw_fill

    xor edi, edi
    xor esi, esi
    mov edx, [width]
    mov ecx, TOOLBAR_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    mov edi, 4
    mov esi, 3
    mov edx, 18
    mov ecx, 16
    mov r8d, COLOR_LIGHT_GRAY
    call draw_fill
    mov edi, 9
    mov esi, 3
    mov dl, '<'
    mov cl, COLOR_BLACK
    mov r8d, -1
    call draw_char
    mov edi, 32
    mov esi, 3
    lea rdx, [current]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text

    xor edi, edi
    mov esi, TOOLBAR_H
    mov edx, SIDEBAR_W
    mov ecx, [height]
    sub ecx, TOOLBAR_H + STATUS_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    xor ebx, ebx
.place:
    cmp ebx, PLACE_COUNT
    jae .list
    imul esi, ebx, ROW_H
    add esi, TOOLBAR_H + 6
    lea eax, [rsi + ROW_H]
    mov ecx, [height]
    sub ecx, STATUS_H
    cmp eax, ecx
    jg .list
    mov rdi, [place_paths + rbx * 8]
    lea rsi, [current]
    push rsi
    push rdi
    call str_equal
    pop rdi
    pop rsi
    imul esi, ebx, ROW_H
    add esi, TOOLBAR_H + 6
    test eax, eax
    jz .place_text
    xor edi, edi
    mov edx, SIDEBAR_W
    mov ecx, ROW_H
    mov r8d, COLOR_BLUE
    push rsi
    call draw_fill
    pop rsi
.place_text:
    mov edi, 8
    inc esi
    mov rdx, [place_names + rbx * 8]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text
    inc ebx
    jmp .place

.list:
    call visible_rows
    mov r13d, eax
    xor r12d, r12d
.row:
    cmp r12d, r13d
    jae .status
    mov ebx, [top]
    add ebx, r12d
    cmp ebx, [entry_count]
    jae .status
    imul esi, r12d, ROW_H
    add esi, TOOLBAR_H + 2
    cmp ebx, [selected]
    jne .entry
    mov edi, SIDEBAR_W
    mov edx, [width]
    sub edx, SIDEBAR_W
    mov ecx, ROW_H
    mov r8d, COLOR_BLUE
    push rsi
    call draw_fill
    pop rsi
.entry:
    mov edi, ebx
    call draw_entry
    inc r12d
    jmp .row

.status:
    xor edi, edi
    mov esi, [height]
    sub esi, STATUS_H
    mov edx, [width]
    mov ecx, STATUS_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    mov rdx, [message]
    test rdx, rdx
    jnz .message
    mov edi, [entry_count]
    call u64_to_dec
    lea rdi, [status_text]
    mov rsi, rax
    call str_copy
    lea rdi, [status_text]
    call str_len
    lea rdi, [status_text + rax]
    lea rsi, [items_text]
    call str_copy
    lea rdx, [status_text]
.message:
    mov edi, 8
    mov esi, [height]
    sub esi, STATUS_H - 1
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text

    mov edi, [window]
    call gui_damage
    pop r13
    pop r12
    pop rbx
    ret

draw_entry:
    push rbx
    push r12
    push r13
    mov eax, edi
    call entry_address
    mov rbx, rax
    mov r12d, esi
    cmp dword [rbx + ENTRY_FLAGS], 0
    je .file_icon
    mov edi, SIDEBAR_W + 8
    lea esi, [r12 + 4]
    mov edx, 6
    mov ecx, 3
    mov r8d, COLOR_YELLOW
    call draw_fill
    mov edi, SIDEBAR_W + 8
    lea esi, [r12 + 6]
    mov edx, 14
    mov ecx, 9
    mov r8d, COLOR_YELLOW
    call draw_fill
    jmp .name
.file_icon:
    mov r13d, COLOR_WHITE
    lea rdi, [rbx + ENTRY_NAME]
    call icon_color
    mov edi, SIDEBAR_W + 10
    lea esi, [r12 + 3]
    mov edx, 10
    mov ecx, 12
    mov r8d, r13d
    call draw_fill
    mov edi, SIDEBAR_W + 12
    lea esi, [r12 + 6]
    mov edx, 6
    mov ecx, 1
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    mov edi, SIDEBAR_W + 12
    lea esi, [r12 + 9]
    mov edx, 6
    mov ecx, 1
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
; the name is clipped so it doesn't run into the size on the right
.name:
    mov r13d, [width]
    cmp dword [rbx + ENTRY_FLAGS], 0
    jne .clip
    mov edi, [rbx + ENTRY_SIZE]
    call format_size
    lea rdi, [size_text]
    call str_len
    shl eax, 3
    sub r13d, eax
    sub r13d, 16
.clip:
    mov [draw_width], r13d
    mov edi, SIDEBAR_W + 28
    lea esi, [r12 + 1]
    lea rdx, [rbx + ENTRY_NAME]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text
    mov eax, [width]
    mov [draw_width], eax
    cmp dword [rbx + ENTRY_FLAGS], 0
    jne .done
    lea edi, [r13 + 8]
    lea esi, [r12 + 1]
    lea rdx, [size_text]
    mov cl, COLOR_LIGHT_GRAY
    mov r8d, -1
    call draw_text
.done:
    pop r13
    pop r12
    pop rbx
    ret

icon_color:
    call str_len
    cmp eax, 4
    jb .done
    mov ecx, [rdi + rax - 4]
    cmp ecx, 'serp'
    je .program
    cmp ecx, '.asm'
    je .source
    cmp ecx, '.inc'
    je .source
    ret
.program:
    mov r13d, COLOR_LIGHT_GREEN
    ret
.source:
    mov r13d, COLOR_LIGHT_CYAN
.done:
    ret

format_size:
    lea rsi, [bytes_suffix]
    cmp edi, 1024
    jb .number
    shr edi, 10
    lea rsi, [kilobytes_suffix]
.number:
    push rsi
    call u64_to_dec
    lea rdi, [size_text]
    mov rsi, rax
    call str_copy
    lea rdi, [size_text]
    call str_len
    lea rdi, [size_text + rax]
    pop rsi
    jmp str_copy
