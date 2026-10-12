include '../runtime/program.inc'

; four function calculator on 64 bit integers, the kind with no precedence: 2+3*4 = 20

WIDTH = 200
HEIGHT = GRID_Y + 4 * (KEY_H + KEY_GAP) + 4
KEY_W = 44
KEY_H = 36
KEY_GAP = 4
GRID_X = 8
GRID_Y = 52
DISPLAY_H = 36
COLOR_KEY = 7
COLOR_OPERATOR = 9
COLOR_TEXT = 0
COLOR_DISPLAY = 15
COLOR_BODY = 8

main:
    push rbx

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

    call clear_all
    call render

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
    jne .wait

    mov eax, [gui_event + EVENT_PAYLOAD + MSG_C]
    mov ecx, [last_buttons]
    mov [last_buttons], eax
    cmp eax, 1
    jne .wait
    test ecx, 1
    jnz .wait
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    call key_at
    test eax, eax
    jz .wait
    jmp .press

.key:
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_A]
    cmp eax, 10
    jne .mapped
    mov eax, '='
.mapped:
    cmp eax, 8                  ; backspace clears, there is no undo of one digit
    jne .press
    mov eax, 'C'

.press:
    mov edi, eax
    call apply_key
    call render
    jmp .wait

.close:
    mov edi, [window]
    call gui_close_window
    xor eax, eax
    pop rbx
    ret

.no_gui:
    lea rdi, [no_gui_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

key_at:
    sub edi, GRID_X
    js .none
    sub esi, GRID_Y
    js .none
    mov eax, edi
    xor edx, edx
    mov ecx, KEY_W + KEY_GAP
    div ecx
    cmp edx, KEY_W
    jae .none
    cmp eax, 4
    jae .none
    mov r8d, eax
    mov eax, esi
    xor edx, edx
    mov ecx, KEY_H + KEY_GAP
    div ecx
    cmp edx, KEY_H
    jae .none
    cmp eax, 4
    jae .none
    shl eax, 2
    add eax, r8d
    movzx eax, byte [keypad + rax]
    ret
.none:
    xor eax, eax
    ret

apply_key:
    cmp edi, '0'
    jb .operator
    cmp edi, '9'
    ja .operator
    cmp byte [fresh], 0
    je .append
    mov qword [operand], 0
    mov byte [fresh], 0
.append:
    mov rax, [operand]
    imul rax, 10
    jo .overflow
    sub edi, '0'
    add rax, rdi
    jo .overflow
    mov [operand], rax
    ret

.operator:
    cmp edi, 'C'
    je clear_all
    cmp edi, 'c'
    je clear_all
    push rdi
    call fold
    pop rdi
    cmp edi, '='
    jne .remember
    xor edi, edi
.remember:
    mov [pending], dil
    mov byte [fresh], 1
    ret

.overflow:
    mov byte [error], 1
    ret

fold:
    mov rax, [accumulator]
    mov rcx, [operand]
    movzx edx, byte [pending]
    test edx, edx
    jz .replace
    cmp edx, '+'
    je .add
    cmp edx, '-'
    je .subtract
    cmp edx, '*'
    je .multiply
    test rcx, rcx
    jz .failed
    cqo
    idiv rcx
    jmp .store
.add:
    add rax, rcx
    jmp .store
.subtract:
    sub rax, rcx
    jmp .store
.multiply:
    imul rax, rcx
    jmp .store
.replace:
    mov rax, rcx
.store:
    mov [accumulator], rax
    mov [operand], rax
    ret
.failed:
    mov byte [error], 1
    ret

clear_all:
    mov qword [accumulator], 0
    mov qword [operand], 0
    mov byte [pending], 0
    mov byte [fresh], 0
    mov byte [error], 0
    ret

render:
    push rbx
    push r12

    xor edi, edi
    xor esi, esi
    mov edx, WIDTH
    mov ecx, HEIGHT
    mov r8d, COLOR_BODY
    call draw_fill

    mov edi, GRID_X
    mov esi, 8
    mov edx, WIDTH - GRID_X * 2
    mov ecx, DISPLAY_H
    mov r8d, COLOR_DISPLAY
    call draw_fill

    lea rdx, [error_text]
    cmp byte [error], 0
    jne .show
    mov rdi, [operand]
    call format_signed
    mov rdx, rax
.show:
    mov rdi, rdx
    push rdx
    call str_len
    pop rdx
    shl eax, 3
    mov edi, WIDTH - GRID_X - 8
    sub edi, eax
    mov esi, 18
    mov cl, COLOR_TEXT
    mov r8d, -1
    call draw_text

    xor ebx, ebx
.key:
    mov eax, ebx
    and eax, 3
    imul edi, eax, KEY_W + KEY_GAP
    add edi, GRID_X
    mov eax, ebx
    shr eax, 2
    imul esi, eax, KEY_H + KEY_GAP
    add esi, GRID_Y
    movzx r12d, byte [keypad + rbx]
    mov r8d, COLOR_KEY
    cmp r12d, '0'
    jb .operator_color
    cmp r12d, '9'
    jbe .draw_key
.operator_color:
    mov r8d, COLOR_OPERATOR
.draw_key:
    push rdi
    push rsi
    mov edx, KEY_W
    mov ecx, KEY_H
    call draw_fill
    pop rsi
    pop rdi
    add edi, KEY_W / 2 - 4
    add esi, KEY_H / 2 - 8
    mov dl, r12b
    mov cl, COLOR_TEXT
    mov r8d, -1
    call draw_char
    inc ebx
    cmp ebx, 16
    jb .key

    mov edi, [window]
    call gui_damage
    pop r12
    pop rbx
    ret

format_signed:
    test rdi, rdi
    jns .positive
    neg rdi
    call u64_to_dec
    dec rax
    mov byte [rax], '-'
    ret
.positive:
    jmp u64_to_dec

title db 'Calculator', 0
error_text db 'ERROR', 0
no_gui_text db 'calc: gui is not running', 0
keypad db '789/456*123-C0=+'

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/library.inc'

align 8
accumulator rq 1
operand rq 1
window rd 1
last_buttons rd 1
pending rb 1
fresh rb 1
error rb 1
