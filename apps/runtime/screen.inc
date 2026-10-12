SCREEN_PIXEL_W = 640
SCREEN_PIXEL_H = 480

SCREEN_CONSOLE_X = 8            ; console mode keeps the kernel's black frame around the area
SCREEN_CONSOLE_Y = 24
SCREEN_CONSOLE_COLUMNS = 78
SCREEN_CONSOLE_ROWS = 27
SCREEN_PAD_X = 4
SCREEN_PAD_Y = 2
SCREEN_FONT_BYTES = 256 * 16

SCREEN_KEY = 1
SCREEN_CLICK = 2
SCREEN_RESIZE = 3
SCREEN_CLOSE = 4
SCREEN_TTY = 5
SCREEN_WHEEL = 6

screen_open:
    push rbx
    mov rbx, rdi
    mov [screen_requested_width], esi
    mov [screen_requested_height], edx
    call gui_connect
    test eax, eax
    js .console

    mov edi, [screen_requested_width]
    mov esi, [screen_requested_height]
    mov rdx, rbx
    call gui_open_window
    test eax, eax
    js .refused
    mov [screen_window], eax
    mov [screen_surface], rdx
    mov byte [screen_gui], 1
    mov edi, [screen_requested_width]
    mov esi, [screen_requested_height]
    call screen_resize
    pop rbx
    ret

.refused:
    mov edi, 1
    call sys_exit

.console:
    mov edi, 3
    call sys_service
    mov edi, 2
    lea rsi, [screen_font]
    call sys_service
    lea rax, [screen_font]
    mov [draw_font], rax
    lea rdi, [screen_pixels]
    mov esi, SCREEN_PIXEL_W
    mov edx, SCREEN_PIXEL_W
    mov ecx, SCREEN_PIXEL_H
    call draw_set_target
    xor edi, edi
    xor esi, esi
    mov edx, SCREEN_PIXEL_W
    mov ecx, SCREEN_PIXEL_H
    xor r8d, r8d
    call draw_fill
    mov dword [screen_origin_x], SCREEN_CONSOLE_X
    mov dword [screen_origin_y], SCREEN_CONSOLE_Y
    mov dword [screen_columns], SCREEN_CONSOLE_COLUMNS
    mov dword [screen_rows], SCREEN_CONSOLE_ROWS
    mov dword [screen_width], SCREEN_PIXEL_W
    mov dword [screen_height], SCREEN_PIXEL_H
    pop rbx
    ret

screen_resize:
    mov [screen_width], edi
    mov [screen_height], esi
    mov rdi, [screen_surface]
    mov esi, [screen_width]
    mov edx, esi
    mov ecx, [screen_height]
    call draw_set_target
    mov dword [screen_origin_x], SCREEN_PAD_X
    mov dword [screen_origin_y], SCREEN_PAD_Y
    mov eax, [screen_width]
    sub eax, SCREEN_PAD_X * 2
    shr eax, 3
    mov [screen_columns], eax
    mov eax, [screen_height]
    sub eax, SCREEN_PAD_Y * 2
    shr eax, 4
    mov [screen_rows], eax

screen_clear:
    xor edi, edi
    xor esi, esi
    mov edx, [screen_width]
    mov ecx, [screen_height]
    xor r8d, r8d
    jmp draw_fill

; rdi -> title. only means something inside the gui
screen_set_title:
    cmp byte [screen_gui], 0
    je .done
    mov rsi, rdi
    mov edi, [screen_window]
    jmp gui_set_title
.done:
    ret

screen_close:
    cmp byte [screen_gui], 0
    je sys_con_restore
    mov edi, [screen_window]
    jmp gui_close_window

screen_put:
    push rdi
    push rsi
    cmp edi, [screen_columns]
    jae .done
    cmp esi, [screen_rows]
    jae .done
    movzx r8d, cl
    shr r8d, 4
    and ecx, 0x0F
    shl edi, 3
    add edi, [screen_origin_x]
    shl esi, 4
    add esi, [screen_origin_y]
    call draw_char
.done:
    pop rsi
    pop rdi
    ret

; push the frame out: damage message in gui mode, a full blit with the pointer on top otherwise
screen_present:
    cmp byte [screen_gui], 0
    je .console
    mov edi, [screen_window]
    jmp gui_damage
.console:
    call pointer_draw
    lea rdi, [screen_pixels]
    mov esi, SCREEN_PIXEL_W
    xor edx, edx
    mov ecx, (SCREEN_PIXEL_H shl 16) or SCREEN_PIXEL_W
    call sys_fb_blit
    jmp pointer_restore

pointer_draw:
    xor edx, edx
.row:
    mov eax, [screen_pointer_y]
    add eax, edx
    cmp eax, SCREEN_PIXEL_H
    jae .done
    imul eax, SCREEN_PIXEL_W
    xor ecx, ecx
.column:
    mov r8d, [screen_pointer_x]
    add r8d, ecx
    cmp r8d, SCREEN_PIXEL_W
    jae .next_row
    add r8d, eax
    mov r9b, [screen_pixels + r8]
    imul r10d, edx, POINTER_W
    add r10d, ecx
    mov [pointer_saved + r10], r9b
    mov r11b, [pointer_shape + r10]
    cmp r11b, '.'
    je .next_column
    mov r9b, 0
    cmp r11b, 'X'
    je .put
    mov r9b, 15
.put:
    mov [screen_pixels + r8], r9b
.next_column:
    inc ecx
    cmp ecx, POINTER_W
    jb .column
.next_row:
    inc edx
    cmp edx, POINTER_H
    jb .row
.done:
    ret

pointer_restore:
    xor edx, edx
.row:
    mov eax, [screen_pointer_y]
    add eax, edx
    cmp eax, SCREEN_PIXEL_H
    jae .done
    imul eax, SCREEN_PIXEL_W
    xor ecx, ecx
.column:
    mov r8d, [screen_pointer_x]
    add r8d, ecx
    cmp r8d, SCREEN_PIXEL_W
    jae .next_row
    add r8d, eax
    imul r10d, edx, POINTER_W
    add r10d, ecx
    mov r9b, [pointer_saved + r10]
    mov [screen_pixels + r8], r9b
    inc ecx
    cmp ecx, POINTER_W
    jb .column
.next_row:
    inc edx
    cmp edx, POINTER_H
    jb .row
.done:
    ret

pointer_move:
    push rbx
    push r12
    mov ebx, [screen_pointer_x]
    mov r12d, [screen_pointer_y]
    mov [screen_pointer_x], edi
    mov [screen_pointer_y], esi
    call pointer_draw
    mov eax, ebx
    cmp eax, [screen_pointer_x]
    jbe .left
    mov eax, [screen_pointer_x]
.left:
    and eax, not 7
    mov ecx, ebx
    cmp ecx, [screen_pointer_x]
    jae .right
    mov ecx, [screen_pointer_x]
.right:
    add ecx, POINTER_W + 7
    and ecx, not 7
    cmp ecx, SCREEN_PIXEL_W
    jbe .width
    mov ecx, SCREEN_PIXEL_W
.width:
    sub ecx, eax
    mov edx, r12d
    cmp edx, [screen_pointer_y]
    jbe .top
    mov edx, [screen_pointer_y]
.top:
    mov r8d, r12d
    cmp r8d, [screen_pointer_y]
    jae .bottom
    mov r8d, [screen_pointer_y]
.bottom:
    add r8d, POINTER_H
    cmp r8d, SCREEN_PIXEL_H
    jbe .height
    mov r8d, SCREEN_PIXEL_H
.height:
    sub r8d, edx
    imul edi, edx, SCREEN_PIXEL_W
    add edi, eax
    lea rdi, [screen_pixels + rdi]
    shl edx, 16
    or edx, eax
    shl r8d, 16
    or ecx, r8d
    mov esi, SCREEN_PIXEL_W
    call sys_fb_blit
    call pointer_restore
    pop r12
    pop rbx
    ret

; esi = sys_event flags. out: eax = SCREEN_* or 0 for nothing interesting. clicks report the
; text cell in screen_click_column/row and the pixel in screen_click_x/y
screen_wait:
    lea rdi, [screen_event]
    call sys_event
    test eax, eax
    jz .none
    cmp eax, EVENT_TTY
    je .tty
    cmp byte [screen_gui], 0
    jne .gui

    cmp eax, EVENT_KEY
    jne .console_mouse
    mov eax, [screen_event + EVENT_CODE]
    mov [screen_key], eax
    mov eax, SCREEN_KEY
    ret
.console_mouse:
    cmp eax, EVENT_MOUSE
    jne .none
    mov eax, [screen_event + EVENT_WHEEL]
    test eax, eax
    jnz .wheel
    mov edi, [screen_event + EVENT_X]
    mov esi, [screen_event + EVENT_Y]
    call pointer_move
    test dword [screen_event + EVENT_BUTTONS], 1
    jz .none
    mov edi, [screen_event + EVENT_X]
    mov esi, [screen_event + EVENT_Y]
    jmp .click

.gui:
    cmp eax, EVENT_MESSAGE
    jne .none
    mov eax, [screen_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_KEY
    je .gui_key
    cmp eax, GUI_MOUSE
    je .gui_mouse
    cmp eax, GUI_RESIZE
    je .gui_resize
    cmp eax, GUI_CLOSE
    je .close
    cmp eax, GUI_WHEEL
    jne .none
    mov eax, [screen_event + EVENT_PAYLOAD + MSG_A]
.wheel:
    mov [screen_wheel], eax
    mov eax, SCREEN_WHEEL
    ret
.none:
    xor eax, eax
    ret
.gui_key:
    mov eax, [screen_event + EVENT_PAYLOAD + MSG_A]
    mov [screen_key], eax
    mov eax, SCREEN_KEY
    ret
.gui_mouse:
    test dword [screen_event + EVENT_PAYLOAD + MSG_C], 1
    jz .none
    mov edi, [screen_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [screen_event + EVENT_PAYLOAD + MSG_B]

.click:
    sub edi, [screen_origin_x]
    js .none
    sub esi, [screen_origin_y]
    js .none
    mov [screen_click_x], edi
    mov [screen_click_y], esi
    shr edi, 3
    shr esi, 4
    cmp edi, [screen_columns]
    jae .none
    cmp esi, [screen_rows]
    jae .none
    mov [screen_click_column], edi
    mov [screen_click_row], esi
    mov eax, SCREEN_CLICK
    ret

.gui_resize:
    mov edi, [screen_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [screen_event + EVENT_PAYLOAD + MSG_B]
    call screen_resize
    mov eax, SCREEN_RESIZE
    ret

.close:
    mov eax, SCREEN_CLOSE
    ret

.tty:
    mov eax, SCREEN_TTY
    ret

screen_acknowledge_resize:
    cmp byte [screen_gui], 0
    je .done
    mov edi, [screen_window]
    mov esi, [screen_width]
    mov edx, [screen_height]
    jmp gui_ack_resize
.done:
    ret

align 8
screen_surface dq 0
screen_window dd 0
screen_origin_x dd 0
screen_origin_y dd 0
screen_columns dd 0
screen_rows dd 0
screen_width dd 0
screen_height dd 0
screen_requested_width dd 0
screen_requested_height dd 0
screen_key dd 0
screen_click_column dd 0
screen_click_row dd 0
screen_click_x dd 0
screen_click_y dd 0
screen_wheel dd 0
screen_pointer_x dd SCREEN_PIXEL_W / 2
screen_pointer_y dd SCREEN_PIXEL_H / 2
pointer_saved:
    times POINTER_W * POINTER_H db 0
screen_gui db 0
