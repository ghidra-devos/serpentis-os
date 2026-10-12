animate_retarget:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, WINDOW_MAX
    jae .done
    imul eax, ebx, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_USED], 0
    je .skip
    cmp dword [r8 + WIN_FRESH], 0
    je .moving
    mov dword [r8 + WIN_FRESH], 0
    mov eax, [r8 + WIN_TILE_W]
    shr eax, 1
    mov [r8 + WIN_DRAW_W], eax
    shr eax, 1
    add eax, [r8 + WIN_X]
    mov [r8 + WIN_DRAW_X], eax
    mov eax, [r8 + WIN_TILE_H]
    shr eax, 1
    mov [r8 + WIN_DRAW_H], eax
    shr eax, 1
    add eax, [r8 + WIN_Y]
    mov [r8 + WIN_DRAW_Y], eax
    mov dword [r8 + WIN_GOAL_W], -1
.moving:
    mov byte [animating], 1
.skip:
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

; one tick event. EVENT_CODE is the tick count, if we were slow we run the missed frames
; too (up to ANIMATION_CATCH_UP) so the speed doesn't depend on how busy we are
animate_tick:
    call clock_refresh
    mov eax, [event + EVENT_CODE]
    mov ecx, eax
    sub ecx, [last_tick]
    mov [last_tick], eax
    cmp ecx, 1
    jge .limit
    mov ecx, 1
.limit:
    cmp ecx, ANIMATION_CATCH_UP
    jle .steps
    mov ecx, ANIMATION_CATCH_UP
.steps:
    push rbx
    mov ebx, ecx
.step:
    call slide_step
    call animate_step
    cmp byte [slide_active], 0
    jne .more
    cmp byte [animating], 0
    je .done
.more:
    dec ebx
    jnz .step
.done:
    pop rbx
    ret

clock_refresh:
    mov eax, [toast_until]
    test eax, eax
    jz .clock
    cmp [event + EVENT_CODE], eax
    jb .clock
    mov dword [toast_until], 0
    mov byte [scene_dirty], 1
.clock:
    mov eax, [event + EVENT_CODE]
    sub eax, [clock_ticks]
    cmp eax, CLOCK_INTERVAL
    jb .done
    mov eax, [event + EVENT_CODE]
    mov [clock_ticks], eax

    lea rdi, [clock_time]
    call sys_time
    test eax, eax
    js .done

    movzx eax, byte [clock_time + TIME_HOUR]
    lea rdi, [clock_text]
    call clock_pair
    mov byte [clock_text + 2], ':'
    movzx eax, byte [clock_time + TIME_MINUTE]
    lea rdi, [clock_text + 3]
    call clock_pair
    mov byte [clock_text + 5], ':'
    movzx eax, byte [clock_time + TIME_SECOND]
    lea rdi, [clock_text + 6]
    call clock_pair
    mov byte [clock_text + CLOCK_CHARS], 0

    xor ecx, ecx
.compare:
    cmp ecx, CLOCK_CHARS + 1
    jae .done
    mov al, [clock_text + rcx]
    cmp al, [clock_shown + rcx]
    jne .changed
    inc ecx
    jmp .compare

.changed:
    xor ecx, ecx
.store:
    cmp ecx, CLOCK_CHARS + 1
    jae .dirty
    mov al, [clock_text + rcx]
    mov [clock_shown + rcx], al
    inc ecx
    jmp .store
.dirty:
    mov byte [clock_dirty], 1
.done:
    ret

clock_pair:
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    mov [rdi], al
    add dl, '0'
    mov [rdi + 1], dl
    ret

animate_step:
    push rbx
    xor ebx, ebx
    mov byte [animating], 0
.window:
    cmp ebx, WINDOW_MAX
    jae .done
    imul eax, ebx, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_USED], 0
    jne .animate
    cmp dword [r8 + WIN_CLOSING], 0
    je .next
.animate:
    call animate_window
    cmp dword [r8 + WIN_CLOSING], 0
    je .next
    cmp dword [r8 + WIN_DRAW_H], 8  ; small enough, now it can vanish
    jg .next
    mov dword [r8 + WIN_CLOSING], 0
    mov byte [scene_dirty], 1
.next:
    inc ebx
    jmp .window
.done:
    pop rbx
    ret

animate_window:
    mov eax, [r8 + WIN_X]
    cmp eax, [r8 + WIN_GOAL_X]
    jne .retarget
    mov eax, [r8 + WIN_Y]
    cmp eax, [r8 + WIN_GOAL_Y]
    jne .retarget
    mov eax, [r8 + WIN_TILE_W]
    cmp eax, [r8 + WIN_GOAL_W]
    jne .retarget
    mov eax, [r8 + WIN_TILE_H]
    cmp eax, [r8 + WIN_GOAL_H]
    je .advance
.retarget:
    xor ecx, ecx
.copy:
    mov eax, [r8 + WIN_DRAW_X + rcx * 4]
    mov [r8 + WIN_FROM_X + rcx * 4], eax
    inc ecx
    cmp ecx, 4
    jb .copy
    mov eax, [r8 + WIN_X]
    mov [r8 + WIN_GOAL_X], eax
    mov eax, [r8 + WIN_Y]
    mov [r8 + WIN_GOAL_Y], eax
    mov eax, [r8 + WIN_TILE_W]
    mov [r8 + WIN_GOAL_W], eax
    mov eax, [r8 + WIN_TILE_H]
    mov [r8 + WIN_GOAL_H], eax
    mov dword [r8 + WIN_PHASE], 0
.advance:
    mov eax, [r8 + WIN_PHASE]
    cmp eax, MOVE_FRAMES
    jae .done
    inc eax
    mov [r8 + WIN_PHASE], eax
    movzx r9d, word [move_curve + rax * 2 - 2]
    xor ecx, ecx
.value:
    mov eax, [r8 + WIN_GOAL_X + rcx * 4]
    sub eax, [r8 + WIN_FROM_X + rcx * 4]
    imul eax, r9d
    sar eax, 12
    add eax, [r8 + WIN_FROM_X + rcx * 4]
    mov [r8 + WIN_DRAW_X + rcx * 4], eax
    inc ecx
    cmp ecx, 4
    jb .value
    mov byte [animating], 1
    mov byte [scene_dirty], 1
.done:
    ret

slide_step:
    cmp byte [slide_active], 0
    je .done
    mov eax, [slide_frame]
    inc eax
    mov [slide_frame], eax
    cmp eax, SLIDE_FRAMES
    jb .store
    mov byte [slide_active], 0
    mov byte [scene_dirty], 1
    mov eax, SLIDE_FRAMES
.store:
    movzx eax, word [slide_curve + rax * 2 - 2]
    mov [slide_pos], eax
    mov byte [slide_dirty], 1
.done:
    ret

move_curve:
repeat MOVE_FRAMES
    dw 4096 - (4096 * (MOVE_FRAMES - %) * (MOVE_FRAMES - %) * (MOVE_FRAMES - %)) / (MOVE_FRAMES * MOVE_FRAMES * MOVE_FRAMES)
end repeat

slide_curve:
repeat SLIDE_FRAMES
    dw SCREEN_W - (SCREEN_W * (SLIDE_FRAMES - %) * (SLIDE_FRAMES - %) * (SLIDE_FRAMES - %)) / (SLIDE_FRAMES * SLIDE_FRAMES * SLIDE_FRAMES)
end repeat
