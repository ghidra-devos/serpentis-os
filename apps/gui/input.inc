; keys. launcher open: it gets all of them. super+key is ours, a few ctrl keys too, f1-f5
; start apps, everything else goes to the focused window
handle_key:
    mov eax, [event + EVENT_CODE]
    cmp byte [launcher_open], 0
    jne launcher_key
    test eax, KEY_SUPER
    jnz .super
    cmp eax, KEY_PIN
    je .pin
    cmp eax, KEY_LAYOUT
    je cycle_layout
    cmp eax, KEY_FULLSCREEN
    je .fullscreen
    cmp eax, KEY_EXIT
    je .exit
    cmp eax, KEY_F1
    je .launch_demo
    cmp eax, KEY_F1 + 1
    je .launch_paint
    cmp eax, KEY_F1 + 2
    je .launch_terminal
    cmp eax, KEY_F1 + 3
    je .launch_calculator
    cmp eax, KEY_F1 + 4
    je .launch_files
    mov edi, [focus]
    test edi, edi
    js .done
    mov esi, GUI_KEY
    mov edx, eax
    xor ecx, ecx
    xor r8d, r8d
    jmp send_to_window

.super:
    and eax, not KEY_SUPER
    cmp eax, 'A'
    jb .super_ready
    cmp eax, 'Z'
    ja .super_ready
    or eax, 0x20
.super_ready:
    cmp eax, 10                 ; super+enter = terminal like in dwm
    je .launch_terminal
    cmp eax, ' '
    je launcher_toggle
    cmp eax, 'd'
    je .launch_demo
    cmp eax, 'p'
    je .launch_paint
    cmp eax, 'c'
    je .launch_calculator
    cmp eax, 'f'
    je .launch_files
    cmp eax, 'e'
    je .launch_editor
    cmp eax, 't'
    je .launch_clock
    cmp eax, 'q'
    je .close
    cmp eax, 'v'
    je .float
    cmp eax, 'h'
    je .focus_left
    cmp eax, 'j'
    je .focus_down
    cmp eax, 'k'
    je .focus_up
    cmp eax, 'l'
    je .focus_right
    cmp eax, '1'
    jb .ignore
    cmp eax, '0' + WORKSPACE_MAX
    ja .ignore
    sub eax, '1'
    mov edi, eax
    jmp switch_workspace
.ignore:
    ret

.launch_demo:
    xor edi, edi
    jmp launch_app
.launch_paint:
    mov edi, 1
    jmp launch_app
.launch_terminal:
    mov edi, 2
    jmp launch_app
.launch_calculator:
    mov edi, 3
    jmp launch_app
.launch_files:
    mov edi, 4
    jmp launch_app
.launch_editor:
    mov edi, 5
    jmp launch_app
.launch_clock:
    mov edi, 6
    jmp launch_app
.close:
    mov edi, [focus]
    test edi, edi
    js .done
    mov esi, GUI_CLOSE
    xor edx, edx
    xor ecx, ecx
    xor r8d, r8d
    jmp send_to_window
.float:
    mov edi, [focus]
    test edi, edi
    js .done
    jmp toggle_floating
.pin:
    mov edi, [focus]
    test edi, edi
    js .done
    jmp toggle_pin
.fullscreen:
    mov edi, [focus]
    test edi, edi
    js .done
    jmp toggle_fullscreen
.focus_left:
    xor edi, edi
    jmp focus_direction
.focus_down:
    mov edi, 1
    jmp focus_direction
.focus_up:
    mov edi, 2
    jmp focus_direction
.focus_right:
    mov edi, 3
    jmp focus_direction
.exit:
    mov byte [quitting], 1
.done:
    ret

launcher_toggle:
    xor byte [launcher_open], 1
    mov dword [launcher_index], 0
    mov dword [search_length], 0
    call launcher_filter
    mov byte [scene_dirty], 1
    ret

launcher_key:
    test eax, KEY_SUPER
    jz .plain
    and eax, not KEY_SUPER
    cmp eax, ' '
    je launcher_toggle
    ret
.plain:
    cmp eax, KEY_ESCAPE
    je launcher_toggle
    cmp eax, 10
    je .launch
    cmp eax, KEY_UP
    je .up
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 8
    je .erase
    cmp eax, ' '
    jb .ignore
    cmp eax, 126
    ja .ignore
    mov ecx, [search_length]
    cmp ecx, SEARCH_MAX
    jae .ignore
    or al, 0x20
    mov [search_text + rcx], al
    inc dword [search_length]
    jmp .refilter
.erase:
    cmp dword [search_length], 0
    je .ignore
    dec dword [search_length]
.refilter:
    mov dword [launcher_index], 0
    call launcher_filter
    mov byte [scene_dirty], 1
.ignore:
    ret
.up:
    mov eax, [launcher_index]
    test eax, eax
    jz .ignore
    dec eax
    jmp .moved
.down:
    mov eax, [launcher_index]
    inc eax
    cmp eax, [match_count]
    jae .ignore
.moved:
    mov [launcher_index], eax
    mov byte [scene_dirty], 1
    ret
.launch:
    cmp dword [match_count], 0
    je .ignore
    mov eax, [launcher_index]
    movzx edi, byte [match_list + rax]
    mov byte [launcher_open], 0
    mov byte [scene_dirty], 1
    jmp launch_app

launcher_filter:
    push rbx
    mov dword [match_count], 0
    xor ebx, ebx
.app:
    cmp ebx, APP_COUNT
    jae .done
    mov rdi, [app_names + rbx * 8]
    call name_matches
    jnc .next
    mov eax, [match_count]
    mov [match_list + rax], bl
    inc dword [match_count]
.next:
    inc ebx
    jmp .app
.done:
    pop rbx
    ret

name_matches:
    mov ecx, [search_length]
    test ecx, ecx
    jz .yes
.start:
    cmp byte [rdi], 0
    je .no
    xor edx, edx
.compare:
    cmp edx, ecx
    jae .yes
    mov al, [rdi + rdx]
    test al, al
    jz .no
    or al, 0x20
    cmp al, [search_text + rdx]
    jne .advance
    inc edx
    jmp .compare
.advance:
    inc rdi
    jmp .start
.yes:
    stc
    ret
.no:
    clc
    ret

; edi: 0 left 1 down 2 up 3 right. picks the closest tile center in that direction, distance
; along the direction counts 1024 times more than the sideways offset, and a tile that does
; not overlap ours on the other axis at all only wins if nothing else is there
focus_direction:
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 16                 ; our tile edges: left right top bottom

    mov r14d, edi
    mov edi, [focus]
    test edi, edi
    js .done
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    mov eax, [r8 + WIN_X]
    mov [rsp], eax
    add eax, [r8 + WIN_TILE_W]
    mov [rsp + 4], eax
    mov eax, [r8 + WIN_Y]
    mov [rsp + 8], eax
    add eax, [r8 + WIN_TILE_H]
    mov [rsp + 12], eax
    mov r12d, [r8 + WIN_X]
    mov eax, [r8 + WIN_TILE_W]
    shr eax, 1
    add r12d, eax
    mov r13d, [r8 + WIN_Y]
    mov eax, [r8 + WIN_TILE_H]
    shr eax, 1
    add r13d, eax

    mov r15d, 0x7FFFFFFF
    mov r11d, -1
    xor ebx, ebx

.scan:
    cmp ebx, [z_count]
    jae .select
    mov edx, [zorder + rbx * 4]
    cmp edx, [focus]
    je .next
    imul eax, edx, WIN_SIZE
    lea r8, [windows + rax]
    mov r9d, [r8 + WIN_X]
    mov eax, [r8 + WIN_TILE_W]
    shr eax, 1
    add r9d, eax
    mov r10d, [r8 + WIN_Y]
    mov eax, [r8 + WIN_TILE_H]
    shr eax, 1
    add r10d, eax
    sub r9d, r12d
    sub r10d, r13d

    cmp r14d, 0
    je .left
    cmp r14d, 1
    je .down
    cmp r14d, 2
    je .up
    test r9d, r9d
    jle .next
    mov ecx, r9d
    mov eax, r10d
    jmp .score
.left:
    test r9d, r9d
    jge .next
    mov ecx, r9d
    neg ecx
    mov eax, r10d
    jmp .score
.down:
    test r10d, r10d
    jle .next
    mov ecx, r10d
    mov eax, r9d
    jmp .score
.up:
    test r10d, r10d
    jge .next
    mov ecx, r10d
    neg ecx
    mov eax, r9d
.score:
    test eax, eax
    jns .minor
    neg eax
.minor:
    mov r10d, eax
    xor esi, esi
    cmp r14d, 1
    je .vertical_overlap
    cmp r14d, 2
    je .vertical_overlap

    mov eax, [r8 + WIN_Y]
    cmp eax, [rsp + 12]
    jae .overlap_penalty
    add eax, [r8 + WIN_TILE_H]
    cmp eax, [rsp + 8]
    jbe .overlap_penalty
    jmp .score_ready

.vertical_overlap:
    mov eax, [r8 + WIN_X]
    cmp eax, [rsp + 4]
    jae .overlap_penalty
    add eax, [r8 + WIN_TILE_W]
    cmp eax, [rsp]
    jbe .overlap_penalty
    jmp .score_ready

.overlap_penalty:
    mov esi, 0x100000
.score_ready:
    imul ecx, ecx, 1024
    add ecx, r10d
    add ecx, esi
    cmp ecx, r15d
    jae .next
    mov r15d, ecx
    mov r11d, edx
.next:
    inc ebx
    jmp .scan

.select:
    test r11d, r11d
    js .done
    mov [focus], r11d
    mov byte [scene_dirty], 1
.done:
    add rsp, 16
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; drag and resize own the mouse until the button goes up, same for a press inside a window
; (mouse_capture) so the app sees the release even outside its tile
handle_mouse:
    push rbx
    push r12
    push r13
    mov r12d, [event + EVENT_X]
    mov r13d, [event + EVENT_Y]
    mov ebx, [event + EVENT_BUTTONS]
    mov [cursor_x], r12d
    mov [cursor_y], r13d
    call send_wheel

    mov edi, [dragging]
    test edi, edi
    jns .drag
    mov edi, [resizing]
    test edi, edi
    jns .resize
    mov edi, [mouse_capture]
    test edi, edi
    jns .captured

    test ebx, ebx
    jz .done
    cmp dword [previous_buttons], 0
    jne .done
    call handle_click
    jmp .done

.drag:
    test ebx, ebx
    jz .drop
    call drag_move
    jmp .done
.drop:
    mov dword [dragging], -1
    mov byte [scene_dirty], 1
    jmp .done

.resize:
    test ebx, ebx
    jz .resized
    call resize_move
    jmp .done
.resized:
    mov dword [resizing], -1
    call relayout_windows
    mov byte [scene_dirty], 1
    jmp .done

.captured:
    mov esi, r12d
    mov edx, r13d
    mov ecx, ebx
    call send_window_mouse
    test ebx, ebx
    jnz .done
    mov dword [mouse_capture], -1

.done:
    mov [previous_buttons], ebx
    pop r13
    pop r12
    pop rbx
    ret

; wheel goes to the window under the pointer, not the focused one
send_wheel:
    mov r8d, [event + EVENT_WHEEL]
    test r8d, r8d
    jz .done
    push r8
    call window_at
    pop r8
    jnc .done
    mov edi, eax
    mov esi, GUI_WHEEL
    mov ecx, r9d
    sub ecx, TITLE_H
    mov eax, r8d
    mov r8d, ecx
    mov ecx, edx
    mov edx, eax
    jmp send_to_window
.done:
    ret

send_window_mouse:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    mov r15d, edx
    mov ebp, ecx
    imul eax, edi, WIN_SIZE
    lea rbx, [windows + rax]

    mov r13d, esi
    sub r13d, [rbx + WIN_X]
    dec r13d
    jns .x_nonnegative
    xor r13d, r13d
.x_nonnegative:
    mov eax, [rbx + WIN_TILE_W]
    sub eax, 2
    cmp r13d, eax
    jb .x_inside
    lea r13d, [rax - 1]
.x_inside:
    mov eax, r13d
    mov edx, [rbx + WIN_W]
    imul rax, rdx
    mov r14d, [rbx + WIN_TILE_W]
    sub r14d, 2
    xor edx, edx
    div r14
    mov r13d, eax

    mov r14d, r15d
    sub r14d, [rbx + WIN_Y]
    sub r14d, TITLE_H
    jns .y_nonnegative
    xor r14d, r14d
.y_nonnegative:
    mov eax, [rbx + WIN_TILE_H]
    sub eax, TITLE_H + 1
    cmp r14d, eax
    jb .y_inside
    lea r14d, [rax - 1]
.y_inside:
    mov eax, r14d
    mov edx, [rbx + WIN_H]
    imul rax, rdx
    mov r14d, [rbx + WIN_TILE_H]
    sub r14d, TITLE_H + 1
    xor edx, edx
    div r14

    mov edx, r13d
    mov ecx, eax
    mov r8d, ebp
    mov edi, r12d
    mov esi, GUI_MOUSE
    call send_to_window

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

handle_click:
    call window_at
    jnc .done
    mov [focus], eax
    mov byte [scene_dirty], 1
    cmp dword [rcx + WIN_FLOAT], 0
    je .chosen
    push rax
    push rcx
    push rdx
    push r9
    mov edi, eax
    call raise_float
    pop r9
    pop rdx
    pop rcx
    pop rax
.chosen:
    test dword [event + EVENT_CODE], MOUSE_SUPER
    jz .plain
    test ebx, 1
    jnz begin_drag
    test ebx, 2
    jnz begin_resize
    ret
.plain:
    cmp r9d, TITLE_H
    jge .body
    test ebx, 1
    jz .done
    cmp edx, RESIZE_EDGE
    jl begin_resize
    mov r8d, [rcx + WIN_TILE_W]
    sub r8d, 18
    cmp edx, r8d
    jl begin_drag
    mov edi, eax
    mov esi, GUI_CLOSE
    xor edx, edx
    xor ecx, ecx
    xor r8d, r8d
    jmp send_to_window
.body:
    test ebx, 1
    jz .content
    mov r8d, [rcx + WIN_TILE_H]
    sub r8d, RESIZE_EDGE
    cmp r9d, r8d
    jl .content
    cmp edx, RESIZE_EDGE
    jl begin_resize
    mov r8d, [rcx + WIN_TILE_W]
    sub r8d, RESIZE_EDGE
    cmp edx, r8d
    jge begin_resize
.content:
    mov [mouse_capture], eax
    mov edi, eax
    mov esi, r12d
    mov edx, r13d
    mov ecx, ebx
    jmp send_window_mouse
.done:
    ret

; eax = window, rcx -> record, edx/r9d = position inside it. CF on a hit. floats first, top
; down, then the tiles
window_at:
    mov r10d, [float_count]
.float:
    dec r10d
    js .tiled
    mov eax, [float_stack + r10 * 4]
    imul ecx, eax, WIN_SIZE
    lea rcx, [windows + rcx]
    call window_shown
    jne .float
    call window_hit
    jnc .float
    ret
.tiled:
    mov r10d, [z_count]
.hit:
    dec r10d
    js .miss
    mov eax, [zorder + r10 * 4]
    imul ecx, eax, WIN_SIZE
    lea rcx, [windows + rcx]
    call window_hit
    jnc .hit
    ret
.miss:
    clc
    ret

window_hit:
    mov edx, r12d
    sub edx, [rcx + WIN_X]
    js .miss
    cmp edx, [rcx + WIN_TILE_W]
    jge .miss
    mov r9d, r13d
    sub r9d, [rcx + WIN_Y]
    js .miss
    cmp r9d, [rcx + WIN_TILE_H]
    jge .miss
    stc
    ret
.miss:
    clc
    ret

begin_drag:
    push rbx
    mov ebx, eax
    mov edi, eax
    call make_floating
    imul eax, ebx, WIN_SIZE
    lea r8, [windows + rax]
    mov dword [r8 + WIN_FULL], 0
    mov eax, r12d
    sub eax, [r8 + WIN_DRAW_X]
    mov ecx, [r8 + WIN_TILE_W]
    cmp eax, ecx
    jl .x_ready
    shr ecx, 1
    mov eax, ecx
.x_ready:
    mov [grab_x], eax
    mov eax, r13d
    sub eax, [r8 + WIN_DRAW_Y]
    mov ecx, [r8 + WIN_TILE_H]
    cmp eax, ecx
    jl .y_ready
    mov eax, TITLE_H / 2
.y_ready:
    mov [grab_y], eax
    mov [dragging], ebx
    mov edi, ebx
    call drag_move
    pop rbx
    ret

drag_move:
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    mov eax, r12d
    sub eax, [grab_x]
    mov [r8 + WIN_X], eax
    mov eax, r13d
    sub eax, [grab_y]
    mov [r8 + WIN_Y], eax
    call keep_on_screen
    mov eax, [r8 + WIN_X]
    mov [r8 + WIN_DRAW_X], eax
    mov eax, [r8 + WIN_Y]
    mov [r8 + WIN_DRAW_Y], eax
    mov byte [scene_dirty], 1
    mov byte [animating], 1
    ret

begin_resize:
    push rbx
    mov ebx, eax
    mov edi, eax
    call make_floating
    imul eax, ebx, WIN_SIZE
    lea r8, [windows + rax]
    mov dword [r8 + WIN_FULL], 0
    mov eax, [r8 + WIN_TILE_W]
    shr eax, 1
    add eax, [r8 + WIN_X]
    xor ecx, ecx
    cmp r12d, eax
    setl cl
    mov [resize_left], ecx
    mov eax, [r8 + WIN_TILE_H]
    shr eax, 1
    add eax, [r8 + WIN_Y]
    xor ecx, ecx
    cmp r13d, eax
    setl cl
    mov [resize_top], ecx

    mov eax, [r8 + WIN_X]
    mov ecx, eax
    add ecx, [r8 + WIN_TILE_W]
    cmp dword [resize_left], 0
    je .grab_right
    mov [anchor_x], ecx
    mov edx, r12d
    sub edx, eax
    jmp .grab_x
.grab_right:
    mov [anchor_x], eax
    mov edx, ecx
    sub edx, r12d
.grab_x:
    mov [grab_x], edx

    mov eax, [r8 + WIN_Y]
    mov ecx, eax
    add ecx, [r8 + WIN_TILE_H]
    cmp dword [resize_top], 0
    je .grab_bottom
    mov [anchor_y], ecx
    mov edx, r13d
    sub edx, eax
    jmp .grab_y
.grab_bottom:
    mov [anchor_y], eax
    mov edx, ecx
    sub edx, r13d
.grab_y:
    mov [grab_y], edx
    mov [resizing], ebx
    pop rbx
    ret

resize_move:
    imul eax, edi, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [resize_left], 0
    je .right
    mov eax, r12d
    sub eax, [grab_x]
    mov ecx, [anchor_x]
    sub ecx, MIN_W
    xor edx, edx
    call clamp_range
    mov [r8 + WIN_X], eax
    mov ecx, [anchor_x]
    sub ecx, eax
    mov [r8 + WIN_TILE_W], ecx
    jmp .vertical
.right:
    mov eax, r12d
    add eax, [grab_x]
    mov ecx, SCREEN_W
    mov edx, [anchor_x]
    add edx, MIN_W
    call clamp_range
    mov ecx, [anchor_x]
    mov [r8 + WIN_X], ecx
    sub eax, ecx
    mov [r8 + WIN_TILE_W], eax
.vertical:
    cmp dword [resize_top], 0
    je .bottom
    mov eax, r13d
    sub eax, [grab_y]
    mov ecx, [anchor_y]
    sub ecx, MIN_H
    mov edx, BAR_H
    call clamp_range
    mov [r8 + WIN_Y], eax
    mov ecx, [anchor_y]
    sub ecx, eax
    mov [r8 + WIN_TILE_H], ecx
    jmp .placed
.bottom:
    mov eax, r13d
    add eax, [grab_y]
    mov ecx, SCREEN_H
    mov edx, [anchor_y]
    add edx, MIN_H
    call clamp_range
    mov ecx, [anchor_y]
    mov [r8 + WIN_Y], ecx
    sub eax, ecx
    mov [r8 + WIN_TILE_H], eax
.placed:
    mov eax, [r8 + WIN_X]
    mov [r8 + WIN_DRAW_X], eax
    mov eax, [r8 + WIN_Y]
    mov [r8 + WIN_DRAW_Y], eax
    mov eax, [r8 + WIN_TILE_W]
    mov [r8 + WIN_DRAW_W], eax
    mov eax, [r8 + WIN_TILE_H]
    mov [r8 + WIN_DRAW_H], eax
    mov byte [scene_dirty], 1
    ret

launch_app:
    cmp edi, APP_COUNT
    jae .done
    mov rdi, [app_programs + rdi * 8]
    xor esi, esi
    jmp sys_spawn
.done:
    ret
