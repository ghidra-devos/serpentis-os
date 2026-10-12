; messages from apps. EVENT_CODE is the sender pid, 0 means it came from the kernel
handle_message:
    push rbx
    mov ebx, [event + EVENT_CODE]
    mov eax, [event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_CREATE
    je .create
    cmp eax, GUI_DAMAGE
    je .damage
    cmp eax, GUI_DESTROY
    je .destroy
    cmp eax, GUI_RESIZED
    je .resized
    cmp eax, GUI_NOTIFY_EXIT
    je .exited
    cmp eax, GUI_TITLE
    je .title
    pop rbx
    ret

.title:
    mov edi, [event + EVENT_PAYLOAD + MSG_WINDOW]
    call window_of_sender
    jc .done
    imul eax, edi, WIN_SIZE
    lea rdi, [windows + rax + WIN_TITLE]
    lea rsi, [event + EVENT_PAYLOAD + MSG_TITLE]
    mov ecx, MSG_TITLE_MAX
    rep movsb
    mov byte [rdi - 1], 0
    mov byte [scene_dirty], 1
    pop rbx
    ret

.create:
    mov edi, ebx
    call create_window
    pop rbx
    ret

; the app finished drawing. while something moves the whole scene is rebuilt anyway,
; otherwise only that window is copied again
.damage:
    mov edi, [event + EVENT_PAYLOAD + MSG_WINDOW]
    call window_of_sender
    jc .done
    cmp byte [animating], 0
    jne .full
    cmp byte [slide_active], 0
    jne .full
    bts dword [damaged_windows], edi
    jmp .done
.full:
    mov byte [scene_dirty], 1
.done:
    pop rbx
    ret

.resized:
    mov edi, [event + EVENT_PAYLOAD + MSG_WINDOW]
    call window_of_sender
    jc .done
    mov ecx, [event + EVENT_PAYLOAD + MSG_A]
    mov edx, [event + EVENT_PAYLOAD + MSG_B]
    test ecx, ecx
    jz .done
    test edx, edx
    jz .done
    mov r8d, ecx
    imul r8d, edx
    cmp r8d, SURFACE_BYTES
    ja .done
    imul eax, edi, WIN_SIZE
    mov [windows + rax + WIN_W], ecx
    mov [windows + rax + WIN_H], edx
    mov byte [scene_dirty], 1
    pop rbx
    ret

.destroy:
    mov edi, [event + EVENT_PAYLOAD + MSG_WINDOW]
    call window_of_sender
    jc .done
    mov edi, [event + EVENT_PAYLOAD + MSG_WINDOW]
    call remove_window
    pop rbx
    ret

.exited:
    test ebx, ebx
    jnz .done
    mov ebx, [event + EVENT_PAYLOAD + MSG_WINDOW]
    xor ecx, ecx
.scan:
    cmp ecx, WINDOW_MAX
    jae .reap
    imul eax, ecx, WIN_SIZE
    cmp dword [windows + rax + WIN_USED], 0
    je .next
    cmp [windows + rax + WIN_OWNER], ebx
    jne .next
    push rcx
    mov edi, ecx
    call remove_window
    pop rcx
.next:
    inc ecx
    jmp .scan
.reap:
    mov edi, ebx
    mov esi, WAIT_NOHANG
    call sys_waitpid
    pop rbx
    ret

window_of_sender:
    cmp edi, WINDOW_MAX
    jae .no
    imul eax, edi, WIN_SIZE
    cmp dword [windows + rax + WIN_USED], 0
    je .no
    cmp [windows + rax + WIN_OWNER], ebx
    jne .no
    clc
    ret
.no:
    stc
    ret

create_window:
    push rbx
    push r12
    push r13
    mov r12d, edi

    mov eax, [event + EVENT_PAYLOAD + MSG_A]
    mov ecx, [event + EVENT_PAYLOAD + MSG_B]
    cmp eax, 16
    jb .refuse
    cmp eax, SCREEN_W - 8
    ja .refuse
    cmp ecx, 16
    jb .refuse
    cmp ecx, SCREEN_H - TITLE_H
    ja .refuse
    imul eax, ecx
    cmp eax, SURFACE_BYTES
    ja .refuse

    xor ebx, ebx
.find:
    cmp ebx, WINDOW_MAX
    jae .refuse
    imul eax, ebx, WIN_SIZE
    cmp dword [windows + rax + WIN_USED], 0
    je .found
    inc ebx
    jmp .find

.found:
    imul r13d, ebx, WIN_SIZE
    lea r13, [windows + r13]
    mov dword [r13 + WIN_USED], 1
    mov [r13 + WIN_OWNER], r12d
    mov eax, [event + EVENT_PAYLOAD + MSG_A]
    mov [r13 + WIN_W], eax
    mov ecx, [event + EVENT_PAYLOAD + MSG_B]
    mov [r13 + WIN_H], ecx
    mov dword [r13 + WIN_X], 0
    mov dword [r13 + WIN_Y], 0
    mov dword [r13 + WIN_TILE_W], 0
    mov dword [r13 + WIN_TILE_H], 0
    mov dword [r13 + WIN_SENT_W], 0
    mov dword [r13 + WIN_SENT_H], 0
    mov dword [r13 + WIN_FRESH], 1
    mov dword [r13 + WIN_CLOSING], 0
    mov eax, [current_workspace]
    mov [r13 + WIN_WORKSPACE], eax
    mov dword [r13 + WIN_PINNED], 0
    mov dword [r13 + WIN_FLOAT], 0
    mov dword [r13 + WIN_FULL], 0
    lea rdi, [r13 + WIN_TITLE]
    lea rsi, [event + EVENT_PAYLOAD + MSG_TITLE]
    mov ecx, MSG_TITLE_MAX
    rep movsb
    mov byte [r13 + WIN_TITLE + MSG_TITLE_MAX - 1], 0

    imul rdi, rbx, SURFACE_BYTES
    add rdi, SHM_BASE
    mov eax, COLOR_BLACK
    mov ecx, [r13 + WIN_W]
    imul ecx, [r13 + WIN_H]
    rep stosb

    mov edi, ebx
    call raise_window
    mov [focus], ebx
    mov byte [scene_dirty], 1

    mov edi, ebx
    mov esi, GUI_CREATED
    imul edx, ebx, SURFACE_BYTES
    mov ecx, [r13 + WIN_W]
    xor r8d, r8d
    mov r9d, r12d
    call send_message
    call relayout_windows
    jmp .out

; no free slot or a bad size: a short note on the bar and GUI_REFUSED back to the app
.refuse:
    mov eax, [last_tick]
    add eax, TOAST_TICKS
    or eax, 1                   ; 0 means no toast
    mov [toast_until], eax
    mov byte [scene_dirty], 1
    mov edi, -1
    mov esi, GUI_REFUSED
    xor edx, edx
    xor ecx, ecx
    xor r8d, r8d
    mov r9d, r12d
    call send_message

.out:
    pop r13
    pop r12
    pop rbx
    ret

send_message:
    mov [outgoing + MSG_OP], esi
    mov [outgoing + MSG_WINDOW], edi
    mov [outgoing + MSG_A], edx
    mov [outgoing + MSG_B], ecx
    mov [outgoing + MSG_C], r8d
    mov edi, r9d
    lea rsi, [outgoing]
    jmp sys_send

send_to_window:
    push rbx
    mov ebx, edi
    imul eax, edi, WIN_SIZE
    mov r9d, [windows + rax + WIN_OWNER]
    call send_message
    cmp eax, SYS_ERR_NO_PROCESS
    jne .done
    mov edi, ebx
    call remove_window
.done:
    pop rbx
    ret

remove_window:
    push rbx
    mov ebx, edi
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    mov dword [r8 + WIN_USED], 0
    call start_closing
    call unlink_zorder
    mov edi, ebx
    call unlink_float
    cmp [dragging], ebx
    jne .resize
    mov dword [dragging], -1
.resize:
    cmp [resizing], ebx
    jne .capture
    mov dword [resizing], -1
.capture:
    cmp [mouse_capture], ebx
    jne .focus
    mov dword [mouse_capture], -1
.focus:
    cmp [focus], ebx
    jne .layout
    call focus_topmost
.layout:
    call relayout_windows
    mov byte [scene_dirty], 1
    pop rbx
    ret

; focus goes to the top float on this workspace, else the last tiled window
focus_topmost:
    mov dword [focus], -1
    mov ecx, [float_count]
.float:
    dec ecx
    js .tiled
    mov eax, [float_stack + rcx * 4]
    imul edx, eax, WIN_SIZE
    push rcx
    lea rcx, [windows + rdx]
    call window_shown
    pop rcx
    jne .float
    mov [focus], eax
    ret
.tiled:
    mov eax, [z_count]
    test eax, eax
    jz .done
    mov eax, [zorder + rax * 4 - 4]
    mov [focus], eax
.done:
    ret

start_closing:
    mov dword [r8 + WIN_CLOSING], 1
    mov eax, [r8 + WIN_DRAW_W]
    shr eax, 1
    add eax, [r8 + WIN_DRAW_X]
    mov [r8 + WIN_X], eax
    mov eax, [r8 + WIN_DRAW_H]
    shr eax, 1
    add eax, [r8 + WIN_DRAW_Y]
    mov [r8 + WIN_Y], eax
    mov dword [r8 + WIN_TILE_W], 4
    mov dword [r8 + WIN_TILE_H], 4
    mov byte [animating], 1
    ret

unlink_zorder:
    lea r9, [zorder]
    lea r10, [z_count]
    jmp unlink_list

unlink_float:
    lea r9, [float_stack]
    lea r10, [float_count]

unlink_list:
    xor ecx, ecx
.find:
    cmp ecx, [r10]
    jae .done
    cmp [r9 + rcx * 4], edi
    je .shift
    inc ecx
    jmp .find
.shift:
    inc ecx
    cmp ecx, [r10]
    jae .shrink
    mov eax, [r9 + rcx * 4]
    mov [r9 + rcx * 4 - 4], eax
    jmp .shift
.shrink:
    dec dword [r10]
.done:
    ret

raise_window:
    call unlink_zorder
    mov eax, [z_count]
    mov [zorder + rax * 4], edi
    inc dword [z_count]
    ret

raise_float:
    push rdi
    call unlink_float
    pop rdi
    mov eax, [float_count]
    mov [float_stack + rax * 4], edi
    inc dword [float_count]
    ret

; takes the window out of the tiling where it is drawn now, too big ones get a default size
make_floating:
    push rbx
    mov ebx, edi
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_FLOAT], 0
    jne .raise
    mov dword [r8 + WIN_FLOAT], 1
    call unlink_zorder
    mov eax, [r8 + WIN_DRAW_X]
    mov [r8 + WIN_X], eax
    mov eax, [r8 + WIN_DRAW_Y]
    mov [r8 + WIN_Y], eax
    mov eax, [r8 + WIN_DRAW_W]
    mov [r8 + WIN_TILE_W], eax
    mov eax, [r8 + WIN_DRAW_H]
    mov [r8 + WIN_TILE_H], eax
    cmp dword [r8 + WIN_TILE_W], FLOAT_MAX_W
    jle .height
    mov dword [r8 + WIN_TILE_W], FLOAT_W
.height:
    cmp dword [r8 + WIN_TILE_H], FLOAT_MAX_H
    jle .placed
    mov dword [r8 + WIN_TILE_H], FLOAT_H
.placed:
    call keep_on_screen
    mov edi, ebx
    call raise_float
    call relayout_windows
    jmp .done
.raise:
    mov edi, ebx
    call raise_float
.done:
    mov byte [scene_dirty], 1
    pop rbx
    ret

make_tiled:
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_FLOAT], 0
    je .done
    push rdi
    call unlink_float
    pop rdi
    mov dword [r8 + WIN_FLOAT], 0
    mov dword [r8 + WIN_PINNED], 0
    mov eax, [current_workspace]
    mov [r8 + WIN_WORKSPACE], eax
    call raise_window
    call relayout_windows
    mov byte [scene_dirty], 1
.done:
    ret

toggle_fullscreen:
    push rbx
    mov ebx, edi
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_FULL], 0
    jne .restore
    mov eax, [r8 + WIN_FLOAT]
    mov [r8 + WIN_SAVE_FLOAT], eax
    mov eax, [r8 + WIN_X]
    mov [r8 + WIN_SAVE_X], eax
    mov eax, [r8 + WIN_Y]
    mov [r8 + WIN_SAVE_Y], eax
    mov eax, [r8 + WIN_TILE_W]
    mov [r8 + WIN_SAVE_W], eax
    mov eax, [r8 + WIN_TILE_H]
    mov [r8 + WIN_SAVE_H], eax
    cmp dword [r8 + WIN_FLOAT], 0
    jne .floated
    mov dword [r8 + WIN_FLOAT], 1
    call unlink_zorder
.floated:
    mov dword [r8 + WIN_FULL], 1
    mov dword [r8 + WIN_X], 0
    mov dword [r8 + WIN_Y], 0
    mov dword [r8 + WIN_TILE_W], SCREEN_W
    mov dword [r8 + WIN_TILE_H], SCREEN_H
    mov edi, ebx
    call raise_float
    jmp .layout
.restore:
    mov dword [r8 + WIN_FULL], 0
    mov eax, [r8 + WIN_SAVE_X]
    mov [r8 + WIN_X], eax
    mov eax, [r8 + WIN_SAVE_Y]
    mov [r8 + WIN_Y], eax
    mov eax, [r8 + WIN_SAVE_W]
    mov [r8 + WIN_TILE_W], eax
    mov eax, [r8 + WIN_SAVE_H]
    mov [r8 + WIN_TILE_H], eax
    cmp dword [r8 + WIN_SAVE_FLOAT], 0
    jne .layout
    mov [focus], ebx
    mov edi, ebx
    call make_tiled
    pop rbx
    ret
.layout:
    mov [focus], ebx
    call relayout_windows
    mov byte [scene_dirty], 1
    pop rbx
    ret

toggle_floating:
    imul eax, edi, WIN_SIZE
    cmp dword [windows + rax + WIN_FLOAT], 0
    jne make_tiled
    jmp make_floating

keep_on_screen:
    mov eax, [r8 + WIN_X]
    mov ecx, SCREEN_W
    sub ecx, [r8 + WIN_TILE_W]
    xor edx, edx
    call clamp_range
    mov [r8 + WIN_X], eax
    mov eax, [r8 + WIN_Y]
    mov ecx, SCREEN_H
    sub ecx, [r8 + WIN_TILE_H]
    mov edx, BAR_H              ; never under the bar
    call clamp_range
    mov [r8 + WIN_Y], eax
    ret

clamp_range:
    cmp eax, ecx
    jle .upper
    mov eax, ecx
.upper:
    cmp eax, edx
    jge .done
    mov eax, edx
.done:
    ret

switch_workspace:
    push rbx
    cmp edi, [current_workspace]
    je .done
    mov ebx, edi
    mov eax, [current_workspace]
    mov [slide_from], eax
    mov eax, 1
    cmp edi, [current_workspace]
    ja .direction
    neg eax
.direction:
    mov [slide_direction], eax
    lea rdi, [slide_old]
    call capture_layer
    mov [current_workspace], ebx
    mov dword [z_count], 0
    xor ebx, ebx
.collect:
    cmp ebx, WINDOW_MAX
    jae .focus
    imul eax, ebx, WIN_SIZE
    cmp dword [windows + rax + WIN_USED], 0
    je .next
    cmp dword [windows + rax + WIN_FLOAT], 0
    jne .next
    mov ecx, [windows + rax + WIN_WORKSPACE]
    cmp ecx, [current_workspace]
    jne .next
    mov eax, [z_count]
    mov [zorder + rax * 4], ebx
    inc dword [z_count]
.next:
    inc ebx
    jmp .collect
.focus:
    call focus_topmost
    call relayout_windows
    call settle_shown
    lea rdi, [slide_new]
    call capture_layer
    mov byte [compose_mode], COMPOSE_BACKDROP
    call compose_scene
    mov byte [compose_mode], COMPOSE_FULL
    mov dword [slide_frame], 0
    mov dword [slide_pos], 0
    mov byte [slide_active], 1
    mov byte [slide_dirty], 1
    mov byte [scene_dirty], 1
.done:
    pop rbx
    ret

capture_layer:
    push rdi
    mov byte [compose_mode], COMPOSE_LAYER
    call compose_scene
    mov byte [compose_mode], COMPOSE_FULL
    pop rdi
    lea rsi, [scene]
    mov ecx, SCREEN_BYTES / 8
    rep movsq
    ret

settle_shown:
    xor ecx, ecx
.next:
    cmp ecx, WINDOW_MAX
    jae .done
    imul eax, ecx, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_USED], 0
    je .skip
    cmp dword [r8 + WIN_PINNED], 0
    jne .skip
    mov eax, [r8 + WIN_WORKSPACE]
    cmp eax, [current_workspace]
    jne .skip
    mov dword [r8 + WIN_FRESH], 0
    mov eax, [r8 + WIN_X]
    mov [r8 + WIN_DRAW_X], eax
    mov eax, [r8 + WIN_Y]
    mov [r8 + WIN_DRAW_Y], eax
    mov eax, [r8 + WIN_TILE_W]
    mov [r8 + WIN_DRAW_W], eax
    mov eax, [r8 + WIN_TILE_H]
    mov [r8 + WIN_DRAW_H], eax
.skip:
    inc ecx
    jmp .next
.done:
    ret

window_shown:
    cmp dword [rcx + WIN_PINNED], 0
    jne .shown
    push rax
    mov eax, [rcx + WIN_WORKSPACE]
    cmp eax, [current_workspace]
    pop rax
    ret
.shown:
    cmp eax, eax
    ret

toggle_pin:
    push rbx
    mov ebx, edi
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_PINNED], 0
    jne .unpin
    cmp dword [r8 + WIN_FLOAT], 0
    jne .pin
    mov dword [r8 + WIN_FLOAT], 1
    call unlink_zorder
    call pinned_count
    imul eax, PIN_CASCADE
    mov ecx, PIN_X
    sub ecx, eax
    mov [r8 + WIN_X], ecx
    mov ecx, PIN_Y
    sub ecx, eax
    mov [r8 + WIN_Y], ecx
    mov dword [r8 + WIN_TILE_W], PIN_W
    mov dword [r8 + WIN_TILE_H], PIN_H
.pin:
    mov dword [r8 + WIN_PINNED], 1
    mov edi, ebx
    call raise_float
    jmp .layout
.unpin:
    mov dword [r8 + WIN_PINNED], 0
    mov eax, [current_workspace]
    mov [r8 + WIN_WORKSPACE], eax
.layout:
    mov [focus], ebx
    call relayout_windows
    mov byte [scene_dirty], 1
    pop rbx
    ret

pinned_count:
    xor eax, eax
    xor ecx, ecx
.next:
    cmp ecx, WINDOW_MAX
    jae .done
    imul edx, ecx, WIN_SIZE
    cmp dword [windows + rdx + WIN_USED], 0
    je .skip
    cmp dword [windows + rdx + WIN_PINNED], 0
    je .skip
    inc eax
.skip:
    inc ecx
    jmp .next
.done:
    cmp eax, PIN_STACK - 1
    jbe .ready
    mov eax, PIN_STACK - 1
.ready:
    ret

cycle_layout:
    mov eax, [current_workspace]
    movzx ecx, byte [layout_modes + rax]
    inc ecx
    cmp ecx, LAYOUT_COUNT
    jb .store
    xor ecx, ecx
.store:
    mov [layout_modes + rax], cl
    call relayout_windows
    mov byte [scene_dirty], 1
    ret

window_covered:
    push rbx
    xor ebx, ebx
    cmp dword [rcx + WIN_FLOAT], 0
    je .scan
.own:
    cmp ebx, [float_count]
    jae .clear
    mov eax, [float_stack + rbx * 4]
    inc ebx
    imul eax, WIN_SIZE
    lea r9, [windows + rax]
    cmp r9, rcx
    jne .own
.scan:
    cmp ebx, [float_count]
    jae .clear
    mov eax, [float_stack + rbx * 4]
    inc ebx
    imul eax, WIN_SIZE
    lea r9, [windows + rax]
    cmp r9, rcx
    je .scan
    push rcx
    mov rcx, r9
    call window_shown
    pop rcx
    jne .scan
    mov eax, [rcx + WIN_DRAW_X]
    add eax, [rcx + WIN_DRAW_W]
    cmp eax, [r9 + WIN_DRAW_X]
    jle .scan
    mov eax, [r9 + WIN_DRAW_X]
    add eax, [r9 + WIN_DRAW_W]
    cmp eax, [rcx + WIN_DRAW_X]
    jle .scan
    mov eax, [rcx + WIN_DRAW_Y]
    add eax, [rcx + WIN_DRAW_H]
    cmp eax, [r9 + WIN_DRAW_Y]
    jle .scan
    mov eax, [r9 + WIN_DRAW_Y]
    add eax, [r9 + WIN_DRAW_H]
    cmp eax, [rcx + WIN_DRAW_Y]
    jle .scan
    pop rbx
    stc
    ret
.clear:
    pop rbx
    clc
    ret
