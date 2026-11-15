include '../runtime/program.inc'


WIDTH = 320
HEIGHT = 200

main:
    push rbx
    push r12

    call gui_connect
    test eax, eax
    js .no_gui

    mov edi, WIDTH
    mov esi, HEIGHT
    lea rdx, [title]
    call gui_open_window
    test eax, eax
    js .no_gui
    mov [window], eax
    mov rdi, rdx
    mov esi, WIDTH
    mov edx, WIDTH
    mov ecx, HEIGHT
    call draw_set_target

    call paint
    mov edi, [window]
    call gui_damage

.wait:
    lea rdi, [gui_event]
    xor esi, esi
    call sys_event
    cmp eax, EVENT_MESSAGE
    jne .wait
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_CLOSE
    je .close
    cmp eax, GUI_KEY
    je .key
    cmp eax, GUI_MOUSE
    je .mouse
    jmp .wait

.key:
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_A]
    cmp eax, ' '
    jb .wait
    cmp eax, 126
    ja .wait
    mov [key_text + key_text_value], al
    call paint
    mov edi, [window]
    call gui_damage
    jmp .wait

.mouse:
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    sub edi, 3
    sub esi, 3
    mov edx, 7
    mov ecx, 7
    mov r8d, 4                  ; red
    call draw_fill
    mov edi, [window]
    call gui_damage
    jmp .wait

.close:
    mov edi, [window]
    call gui_close_window
    xor eax, eax
    pop r12
    pop rbx
    ret

.no_gui:
    lea rdi, [no_gui_text]
    call print_line
    mov eax, 1
    pop r12
    pop rbx
    ret

paint:
    push rbx
    xor ebx, ebx
.bar:
    imul edi, ebx, WIDTH / 16
    xor esi, esi
    mov edx, WIDTH / 16
    mov ecx, 40
    mov r8d, ebx
    call draw_fill
    inc ebx
    cmp ebx, 16
    jb .bar

    xor edi, edi
    mov esi, 40
    mov edx, WIDTH
    mov ecx, HEIGHT - 40
    mov r8d, 15
    call draw_fill

    mov edi, 12
    mov esi, 56
    lea rdx, [line_one]
    mov cl, 1
    mov r8d, -1
    call draw_text
    mov edi, 12
    mov esi, 80
    lea rdx, [line_two]
    mov cl, 0
    mov r8d, -1
    call draw_text
    mov edi, 12
    mov esi, 104
    lea rdx, [key_text]
    mov cl, 4
    mov r8d, -1
    call draw_text
    pop rbx
    ret

title db 'Demo', 0
line_one db 'Hello from a Ring 3 window', 0
line_two db 'Type a key or click here.', 0
key_text db 'Last key: -', 0
key_text_value = 10
no_gui_text db 'demo: gui is not running', 0

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/library.inc'

window rd 1
