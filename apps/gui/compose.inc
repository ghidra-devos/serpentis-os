; builds the whole scene in back to front order: wallpaper, bar, windows that are still
; shrinking away, tiles, floats, launcher. compose_mode cuts out parts of it for the slide
compose_scene:
    push rbx

    lea rdi, [scene]
    mov esi, SCREEN_W
    mov edx, SCREEN_W
    mov ecx, SCREEN_H
    call draw_set_target

    cmp byte [compose_mode], COMPOSE_LAYER
    jne .backdrop
    lea rdi, [scene]
    mov eax, SLIDE_KEY
    mov ecx, SCREEN_BYTES
    rep stosb
    jmp .tiled

.backdrop:
    cmp byte [wallpaper_ready], 0
    jne .wallpaper
    xor edi, edi
    xor esi, esi
    mov edx, SCREEN_W
    mov ecx, SCREEN_H
    mov r8d, COLOR_BLACK
    call draw_fill
.wallpaper:
    call draw_wallpaper

    xor edi, edi
    xor esi, esi
    mov edx, SCREEN_W
    mov ecx, BAR_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill

    call draw_workspace_bar
    call draw_layout_symbol
    call draw_clock
    call draw_toast
    cmp byte [compose_mode], COMPOSE_BACKDROP
    je .floating

    xor ebx, ebx
.closing:
    cmp ebx, WINDOW_MAX
    jae .tiled
    imul eax, ebx, WIN_SIZE
    cmp dword [windows + rax + WIN_CLOSING], 0
    je .next_closing
    lea rcx, [windows + rax]
    call window_shown
    jne .next_closing
    mov edi, ebx
    call draw_window
.next_closing:
    inc ebx
    jmp .closing

.tiled:
    xor ebx, ebx
.windows:
    cmp ebx, [z_count]
    jae .floating
    mov edi, [zorder + rbx * 4]
    call draw_window
    inc ebx
    jmp .windows

; pinned floats stay put during a slide, so they belong to the backdrop and not the layer
.floating:
    xor ebx, ebx
.float:
    cmp ebx, [float_count]
    jae .overlay
    mov edi, [float_stack + rbx * 4]
    inc ebx
    imul eax, edi, WIN_SIZE
    lea rcx, [windows + rax]
    call window_shown
    jne .float
    mov eax, [rcx + WIN_PINNED]
    cmp byte [compose_mode], COMPOSE_LAYER
    je .layer_float
    cmp byte [compose_mode], COMPOSE_BACKDROP
    jne .draw_float
    test eax, eax
    jz .float
    jmp .draw_float
.layer_float:
    test eax, eax
    jnz .float
.draw_float:
    call draw_window
    jmp .float

.overlay:
    cmp byte [compose_mode], COMPOSE_FULL
    jne .finished
    cmp byte [launcher_open], 0
    je .finished
    call draw_launcher
.finished:
    pop rbx
    ret

draw_launcher:
    push rbx
    mov edi, LAUNCHER_X
    mov esi, LAUNCHER_Y
    mov edx, LAUNCHER_W
    mov ecx, LAUNCHER_H
    call apply_shadow

    mov edi, LAUNCHER_X
    mov esi, LAUNCHER_Y
    mov edx, LAUNCHER_W
    mov ecx, LAUNCHER_H
    mov r8d, COLOR_LIGHT_CYAN
    call draw_fill

    mov edi, LAUNCHER_X + 1
    mov esi, LAUNCHER_Y + 1
    mov edx, LAUNCHER_W - 2
    mov ecx, LAUNCHER_H - 2
    mov r8d, COLOR_DARK_GRAY
    call draw_fill

    mov edi, LAUNCHER_X + 2
    mov esi, LAUNCHER_Y + 2
    mov edx, LAUNCHER_W - 4
    mov ecx, TITLE_H - 3
    mov r8d, COLOR_BLUE
    call draw_fill

    mov edi, LAUNCHER_X + 6
    mov esi, LAUNCHER_Y + 1
    lea rdx, [launcher_title]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text

    mov edi, LAUNCHER_X + 4
    mov esi, LAUNCHER_Y + TITLE_H + 2
    mov edx, LAUNCHER_W - 8
    mov ecx, SEARCH_ROW - 4
    mov r8d, COLOR_BLACK
    call draw_fill
    mov byte [search_line], '>'
    mov byte [search_line + 1], ' '
    xor ecx, ecx
.query:
    cmp ecx, [search_length]
    jae .cursor
    mov al, [search_text + rcx]
    mov [search_line + rcx + 2], al
    inc ecx
    jmp .query
.cursor:
    mov byte [search_line + rcx + 2], '_'
    mov byte [search_line + rcx + 3], 0
    mov edi, LAUNCHER_X + 8
    mov esi, LAUNCHER_Y + TITLE_H + 2
    lea rdx, [search_line]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text

    cmp dword [match_count], 0
    jne .list
    mov edi, LAUNCHER_X + 12
    mov esi, LAUNCHER_Y + TITLE_H + SEARCH_ROW + 4
    lea rdx, [no_match_text]
    mov cl, COLOR_LIGHT_GRAY
    mov r8d, -1
    call draw_text
    jmp .done

.list:
    xor ebx, ebx
.item:
    cmp ebx, [match_count]
    jae .done
    cmp ebx, [launcher_index]
    jne .label
    mov edi, LAUNCHER_X + 4
    imul esi, ebx, LAUNCHER_ROW
    add esi, LAUNCHER_Y + TITLE_H + SEARCH_ROW + 3
    mov edx, LAUNCHER_W - 8
    mov ecx, LAUNCHER_ROW
    mov r8d, COLOR_BLUE
    call draw_fill
.label:
    mov edi, LAUNCHER_X + 12
    imul esi, ebx, LAUNCHER_ROW
    add esi, LAUNCHER_Y + TITLE_H + SEARCH_ROW + 4
    movzx eax, byte [match_list + rbx]
    mov rdx, [app_names + rax * 8]
    mov cl, COLOR_LIGHT_GRAY
    cmp ebx, [launcher_index]
    jne .colored
    mov cl, COLOR_WHITE
.colored:
    mov r8d, -1
    call draw_text
    inc ebx
    jmp .item
.done:
    pop rbx
    ret

draw_layout_symbol:
    mov eax, [current_workspace]
    movzx eax, byte [layout_modes + rax]
    lea rdx, [layout_symbols + rax * 4]
    mov edi, 6 + WORKSPACE_MAX * 14 + 8
    mov esi, 4
    mov cl, COLOR_LIGHT_GRAY
    mov r8d, -1
    jmp draw_text

draw_clock:
    cmp byte [clock_text], 0
    je .done
    mov edi, SCREEN_W - 7 - CLOCK_CHARS * 8
    mov esi, 4
    lea rdx, [clock_text]
    mov cl, COLOR_WHITE
    mov r8d, -1
    jmp draw_text
.done:
    ret

draw_workspace_bar:
    push rbx
    imul edi, [current_workspace], 14
    cmp byte [slide_active], 0
    je .indicator
    imul eax, [slide_from], 14
    sub edi, eax
    imul edi, [slide_pos]
    mov ecx, SCREEN_W
    mov eax, edi
    cdq
    idiv ecx
    imul edi, [slide_from], 14
    add edi, eax
.indicator:
    add edi, 5
    mov esi, 3
    mov edx, 11
    mov ecx, BAR_H - 6
    mov r8d, COLOR_BLUE
    call draw_fill
    xor ebx, ebx
.next:
    cmp ebx, WORKSPACE_MAX
    jae .done
    mov eax, ebx
    add eax, '1'
    mov [workspace_digit], al
    mov byte [workspace_digit + 1], 0
    imul edi, ebx, 14
    add edi, 6
    mov esi, 4
    lea rdx, [workspace_digit]
    mov cl, COLOR_LIGHT_GRAY
    cmp ebx, [current_workspace]
    jne .colored
    mov cl, COLOR_WHITE
.colored:
    mov r8d, -1
    call draw_text
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

; only the bar, when the clock ticks and nothing else changed
draw_bar:
    lea rdi, [scene]
    mov esi, SCREEN_W
    mov edx, SCREEN_W
    mov ecx, SCREEN_H
    call draw_set_target
    xor edi, edi
    xor esi, esi
    mov edx, SCREEN_W
    mov ecx, BAR_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    call draw_workspace_bar
    call draw_layout_symbol
    call draw_clock

draw_toast:
    cmp dword [toast_until], 0
    je .done
    mov edi, (SCREEN_W - (toast_text_end - toast_text - 1) * 8) / 2
    mov esi, 4
    lea rdx, [toast_text]
    mov cl, COLOR_YELLOW
    mov r8d, -1
    jmp draw_text
.done:
    ret

toast_text db 'window limit reached - close a window first', 0
toast_text_end:

draw_window:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    imul ebx, edi, WIN_SIZE
    lea rbx, [windows + rbx]

    mov r8d, COLOR_DARK_GRAY
    cmp r12d, [focus]
    jne .border_ready
    mov r8d, COLOR_LIGHT_CYAN
.border_ready:

    mov edi, [rbx + WIN_DRAW_X]
    mov esi, [rbx + WIN_DRAW_Y]
    mov edx, [rbx + WIN_DRAW_W]
    mov ecx, [rbx + WIN_DRAW_H]
    cmp byte [compose_mode], COMPOSE_LAYER  ; no shadow in a layer, it would get slid too
    je .frame
    push rcx
    push rdx
    push rsi
    push rdi
    call apply_shadow
    pop rdi
    pop rsi
    pop rdx
    pop rcx
.frame:
    call draw_fill

    mov edi, [rbx + WIN_DRAW_X]
    inc edi
    mov esi, [rbx + WIN_DRAW_Y]
    inc esi
    mov edx, [rbx + WIN_DRAW_W]
    sub edx, 2
    mov ecx, [rbx + WIN_DRAW_H]
    sub ecx, 2
    mov r8d, COLOR_BLACK
    call draw_fill

    cmp dword [rbx + WIN_DRAW_W], 40
    jl .done
    cmp dword [rbx + WIN_DRAW_H], TITLE_H + 2
    jl .done

    mov r8d, COLOR_DARK_GRAY
    cmp r12d, [focus]
    jne .title_ready
    mov r8d, COLOR_BLUE
.title_ready:
    mov edi, [rbx + WIN_DRAW_X]
    add edi, 2
    mov esi, [rbx + WIN_DRAW_Y]
    add esi, 2
    mov edx, [rbx + WIN_DRAW_W]
    sub edx, 4
    mov ecx, TITLE_H - 3
    call draw_fill

    mov eax, [rbx + WIN_DRAW_X]
    add eax, [rbx + WIN_DRAW_W]
    sub eax, 18
    mov [draw_width], eax
    mov edi, [rbx + WIN_DRAW_X]
    add edi, 6
    mov esi, [rbx + WIN_DRAW_Y]
    inc esi
    lea rdx, [rbx + WIN_TITLE]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text
    mov dword [draw_width], SCREEN_W

    mov edi, [rbx + WIN_DRAW_X]
    add edi, [rbx + WIN_DRAW_W]
    sub edi, 17
    mov esi, [rbx + WIN_DRAW_Y]
    add esi, 3
    mov edx, 13
    mov ecx, 12
    mov r8d, COLOR_RED
    call draw_fill

    mov edi, [rbx + WIN_DRAW_X]
    add edi, [rbx + WIN_DRAW_W]
    sub edi, 14
    mov esi, [rbx + WIN_DRAW_Y]
    add esi, 1
    mov dl, 'x'
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_char

    mov edi, r12d
    call draw_window_content

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

draw_window_content:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, edi
    imul ebx, edi, WIN_SIZE
    lea rbx, [windows + rbx]
    imul r13, r12, SURFACE_BYTES
    add r13, SHM_BASE
    mov r14d, [rbx + WIN_DRAW_W]
    sub r14d, 2
    mov r15d, [rbx + WIN_DRAW_H]
    sub r15d, TITLE_H + 1
    test r14d, r14d
    jle .done
    test r15d, r15d
    jle .done
    cmp r14d, [rbx + WIN_W]
    jne .scaled
    cmp r15d, [rbx + WIN_H]
    je .direct

.scaled:
    xor ecx, ecx
.map:
    mov eax, ecx
    mul dword [rbx + WIN_W]
    div r14d
    mov [column_map + rcx * 4], eax
    inc ecx
    cmp ecx, r14d
    jb .map
    xor r8d, r8d
.row:
    cmp r8d, r15d
    jae .done
    mov eax, r8d
    mul dword [rbx + WIN_H]
    div r15d
    mul dword [rbx + WIN_W]
    lea r9, [r13 + rax]
    mov eax, [rbx + WIN_DRAW_Y]
    add eax, TITLE_H
    add eax, r8d
    imul eax, SCREEN_W
    add eax, [rbx + WIN_DRAW_X]
    inc eax
    lea r10, [scene + rax]
    xor ecx, ecx
.column:
    mov eax, [column_map + rcx * 4]
    movzx eax, byte [r9 + rax]
    mov [r10 + rcx], al
    inc ecx
    cmp ecx, r14d
    jb .column
    inc r8d
    jmp .row

.direct:
    mov eax, [rbx + WIN_DRAW_Y]
    add eax, TITLE_H
    imul eax, SCREEN_W
    add eax, [rbx + WIN_DRAW_X]
    inc eax
    lea rdi, [scene + rax]
    mov rsi, r13
.direct_row:
    push rdi
    mov ecx, r14d
    rep movsb
    pop rdi
    add rdi, SCREEN_W
    dec r15d
    jnz .direct_row

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

apply_shadow:
    push rbx
    push r12
    push r13
    push r14
    mov r12d, edi
    mov r13d, esi
    mov r14d, edx
    mov ebx, ecx
    lea edi, [r12 - SHADOW]
    lea esi, [r13 - SHADOW]
    lea edx, [r14 + SHADOW * 2]
    mov ecx, SHADOW
    call shade_rect
    lea edi, [r12 - SHADOW]
    lea esi, [r13 + rbx]
    lea edx, [r14 + SHADOW * 2]
    mov ecx, SHADOW
    call shade_rect
    lea edi, [r12 - SHADOW]
    mov esi, r13d
    mov edx, SHADOW
    mov ecx, ebx
    call shade_rect
    lea edi, [r12 + r14]
    mov esi, r13d
    mov edx, SHADOW
    mov ecx, ebx
    call shade_rect
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

shade_rect:
    test edi, edi
    jns .left
    add edx, edi
    xor edi, edi
.left:
    cmp esi, BAR_H
    jge .top
    sub esi, BAR_H
    add ecx, esi
    mov esi, BAR_H
.top:
    lea eax, [rdi + rdx]
    cmp eax, SCREEN_W
    jle .right
    mov edx, SCREEN_W
    sub edx, edi
.right:
    lea eax, [rsi + rcx]
    cmp eax, SCREEN_H
    jle .bottom
    mov ecx, SCREEN_H
    sub ecx, esi
.bottom:
    test edx, edx
    jle .done
    test ecx, ecx
    jle .done
    imul eax, esi, SCREEN_W
    add eax, edi
    lea r10, [scene + rax]
.row:
    xor r9d, r9d
.pixel:
    movzx eax, byte [r10 + r9]
    mov al, [shade + rax]
    mov [r10 + r9], al
    inc r9d
    cmp r9d, edx
    jb .pixel
    add r10, SCREEN_W
    dec ecx
    jnz .row
.done:
    ret
