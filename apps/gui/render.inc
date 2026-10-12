; scene = everything but the pointer, frame = scene + pointer, that is what goes to the
; screen. if only the pointer, the clock or some window contents changed we patch frame and
; blit just those rectangles, a full 300 KB blit every tick is too slow
render:
    cmp byte [slide_active], 0
    jne render_slide
    cmp byte [scene_dirty], 0
    je .cursor_only

    call compose_scene
    lea rsi, [scene]
    lea rdi, [frame]
    mov ecx, SCREEN_BYTES / 8
    rep movsq
    call draw_cursor
    lea rdi, [frame]
    mov esi, SCREEN_W
    xor edx, edx
    mov ecx, (SCREEN_H shl 16) or SCREEN_W
    call sys_fb_blit
    mov byte [scene_dirty], 0
    mov byte [clock_dirty], 0
    mov dword [damaged_windows], 0
    jmp .remember

.cursor_only:
    cmp byte [clock_dirty], 0
    je .windows
    call render_clock
.windows:
    cmp dword [damaged_windows], 0
    je .pointer
    call render_damaged
.pointer:
    mov eax, [cursor_x]
    cmp eax, [drawn_x]
    jne .moved
    mov eax, [cursor_y]
    cmp eax, [drawn_y]
    je .done
.moved:
    call restore_cursor_area
    call draw_cursor
    call blit_cursor_union

.remember:
    mov eax, [cursor_x]
    mov [drawn_x], eax
    mov eax, [cursor_y]
    mov [drawn_y], eax
.done:
    ret

; two captured layers, old and new workspace, offset by slide_pos. SLIDE_KEY pixels show
; the backdrop and anything pinned shows through the whole time
render_slide:
    cmp byte [slide_dirty], 0
    jne .draw
    mov eax, [cursor_x]
    cmp eax, [drawn_x]
    jne .draw
    mov eax, [cursor_y]
    cmp eax, [drawn_y]
    je .done
.draw:
    mov byte [slide_dirty], 0
    call draw_bar
    push rbx
    push r12
    push r13
    push r14
    push r15
    lea rsi, [scene]
    lea rdi, [frame]
    mov ecx, BAR_H * SCREEN_W / 8
    rep movsq
    mov r12d, BAR_H
.row:
    cmp r12d, SCREEN_H
    jae .present
    mov edi, r12d
    call mark_pinned_row
    imul r13d, r12d, SCREEN_W
    mov r14d, [slide_pos]
    xor ebx, ebx
.pixel:
    cmp ebx, SCREEN_W
    jae .next_row
    cmp byte [pinned_row + rbx], 0
    jne .backdrop
    cmp dword [slide_direction], 0
    jl .backward
    lea eax, [rbx + r14]
    cmp eax, SCREEN_W
    jae .forward_new
    add eax, r13d
    movzx eax, byte [slide_old + rax]
    jmp .pick
.forward_new:
    sub eax, SCREEN_W
    add eax, r13d
    movzx eax, byte [slide_new + rax]
    jmp .pick
.backward:
    mov eax, ebx
    sub eax, r14d
    js .backward_new
    add eax, r13d
    movzx eax, byte [slide_old + rax]
    jmp .pick
.backward_new:
    add eax, SCREEN_W
    add eax, r13d
    movzx eax, byte [slide_new + rax]
.pick:
    cmp eax, SLIDE_KEY
    jne .store
.backdrop:
    lea eax, [r13 + rbx]
    movzx eax, byte [scene + rax]
.store:
    lea r15d, [r13 + rbx]
    mov [frame + r15], al
    inc ebx
    jmp .pixel
.next_row:
    inc r12d
    jmp .row
.present:
    call draw_cursor
    lea rdi, [frame]
    mov esi, SCREEN_W
    xor edx, edx
    mov ecx, (SCREEN_H shl 16) or SCREEN_W
    call sys_fb_blit
    mov eax, [cursor_x]
    mov [drawn_x], eax
    mov eax, [cursor_y]
    mov [drawn_y], eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
.done:
    ret

mark_pinned_row:
    push rdi
    lea rdi, [pinned_row]
    xor eax, eax
    mov ecx, SCREEN_W
    rep stosb
    pop rdi
    xor ecx, ecx
.window:
    cmp ecx, WINDOW_MAX
    jae .done
    imul eax, ecx, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_USED], 0
    je .next
    cmp dword [r8 + WIN_PINNED], 0
    je .next
    mov eax, edi
    sub eax, [r8 + WIN_DRAW_Y]
    jl .next
    cmp eax, [r8 + WIN_DRAW_H]
    jge .next
    mov edx, [r8 + WIN_DRAW_X]
    mov r9d, [r8 + WIN_DRAW_W]
.span:
    test r9d, r9d
    jle .next
    cmp edx, SCREEN_W
    jae .skip
    mov byte [pinned_row + rdx], 1
.skip:
    inc edx
    dec r9d
    jmp .span
.next:
    inc ecx
    jmp .window
.done:
    ret

render_clock:
    mov byte [clock_dirty], 0
    lea rdi, [scene]
    mov esi, SCREEN_W
    mov edx, SCREEN_W
    mov ecx, SCREEN_H
    call draw_set_target
    mov edi, CLOCK_X - 4
    xor esi, esi
    mov edx, SCREEN_W - CLOCK_X + 4
    mov ecx, BAR_H
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    call draw_clock
    mov edi, CLOCK_X - 4
    xor esi, esi
    mov edx, SCREEN_W - CLOCK_X + 4
    mov ecx, BAR_H
    jmp present_rect

render_damaged:
    push rbx
    push r12
    xor r12d, r12d
.window:
    cmp r12d, WINDOW_MAX
    jae .done
    bt dword [damaged_windows], r12d
    jnc .next
    imul eax, r12d, WIN_SIZE
    lea rbx, [windows + rax]
    cmp dword [rbx + WIN_USED], 0
    je .next
    mov rcx, rbx
    call window_shown
    jne .next
    call window_covered
    jnc .draw
    mov byte [scene_dirty], 1
    jmp .next
.draw:
    mov edi, r12d
    call draw_window_content
    mov edi, [rbx + WIN_DRAW_X]
    inc edi
    mov esi, [rbx + WIN_DRAW_Y]
    add esi, TITLE_H
    mov edx, [rbx + WIN_DRAW_W]
    sub edx, 2
    mov ecx, [rbx + WIN_DRAW_H]
    sub ecx, TITLE_H + 1
    call present_rect
.next:
    inc r12d
    jmp .window
.done:
    mov dword [damaged_windows], 0
    pop r12
    pop rbx
    ret

; scene -> frame for the rectangle, pointer on top, then blit it. fb_blit wants x and w in
; multiples of 8, so the rectangle grows to that
present_rect:
    test edx, edx
    jle .done
    test ecx, ecx
    jle .done
    mov eax, edi
    and edi, not 7
    sub eax, edi
    add edx, eax
    add edx, 7
    and edx, not 7
    lea eax, [rdi + rdx]
    cmp eax, SCREEN_W
    jbe .width
    mov edx, SCREEN_W
    sub edx, edi
.width:
    lea eax, [rsi + rcx]
    cmp eax, SCREEN_H
    jbe .height
    mov ecx, SCREEN_H
    sub ecx, esi
.height:
    push rdi
    push rsi
    push rdx
    push rcx
.copy:
    imul eax, esi, SCREEN_W
    add eax, edi
    push rsi
    push rdi
    push rcx
    lea rsi, [scene + rax]
    lea rdi, [frame + rax]
    mov ecx, edx
    rep movsb
    pop rcx
    pop rdi
    pop rsi
    inc esi
    dec ecx
    jnz .copy
    call draw_cursor
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    imul eax, esi, SCREEN_W
    add eax, edi
    shl esi, 16
    or esi, edi
    shl ecx, 16
    or ecx, edx
    mov edx, esi
    lea rdi, [frame + rax]
    mov esi, SCREEN_W
    jmp sys_fb_blit
.done:
    ret

restore_cursor_area:
    mov edx, [drawn_y]
    mov ecx, CURSOR_H
.row:
    cmp edx, SCREEN_H
    jae .done
    imul eax, edx, SCREEN_W
    add eax, [drawn_x]
    lea rsi, [scene + rax]
    lea rdi, [frame + rax]
    mov r8d, SCREEN_W
    sub r8d, [drawn_x]
    cmp r8d, CURSOR_W
    jbe .copy
    mov r8d, CURSOR_W
.copy:
    push rcx
    mov ecx, r8d
    rep movsb
    pop rcx
    inc edx
    dec ecx
    jnz .row
.done:
    ret

draw_cursor:
    lea rsi, [pointer_shape]
    xor edx, edx
.row:
    mov eax, [cursor_y]
    add eax, edx
    cmp eax, SCREEN_H
    jae .done
    imul eax, SCREEN_W
    lea rdi, [frame + rax]
    xor ecx, ecx
.column:
    mov eax, [cursor_x]
    add eax, ecx
    cmp eax, SCREEN_W
    jae .next_row
    mov r8b, [rsi + rcx]
    cmp r8b, '.'
    je .next_column
    mov r9b, COLOR_BLACK
    cmp r8b, 'X'
    je .put
    mov r9b, COLOR_WHITE
.put:
    mov [rdi + rax], r9b
.next_column:
    inc ecx
    cmp ecx, CURSOR_W
    jb .column
.next_row:
    add rsi, CURSOR_W
    inc edx
    cmp edx, CURSOR_H
    jb .row
.done:
    ret

blit_cursor_union:
    mov eax, [cursor_x]
    mov ecx, [drawn_x]
    mov edx, eax
    cmp ecx, eax
    jae .x_max
    mov eax, ecx
    jmp .x_ready
.x_max:
    mov edx, ecx
.x_ready:
    and eax, not 7
    add edx, CURSOR_W + 7
    and edx, not 7
    cmp edx, SCREEN_W
    jbe .x_clamped
    mov edx, SCREEN_W
.x_clamped:
    sub edx, eax
    mov r8d, eax

    mov eax, [cursor_y]
    mov ecx, [drawn_y]
    mov r9d, eax
    cmp ecx, eax
    jae .y_max
    mov eax, ecx
    jmp .y_ready
.y_max:
    mov r9d, ecx
.y_ready:
    add r9d, CURSOR_H
    cmp r9d, SCREEN_H
    jbe .y_clamped
    mov r9d, SCREEN_H
.y_clamped:
    sub r9d, eax

    imul ecx, eax, SCREEN_W
    add ecx, r8d
    lea rdi, [frame + rcx]
    mov esi, SCREEN_W
    mov ecx, r9d
    shl ecx, 16
    or ecx, edx
    mov edx, eax
    shl edx, 16
    or edx, r8d
    jmp sys_fb_blit
