include '../runtime/program.inc'


WIDTH = 400
HEIGHT = 300
SWATCH = WIDTH / 16
PALETTE_H = 16
BRUSH = 3

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

    mov dword [color], 0
    call clear_canvas

.wait:
    lea rdi, [gui_event]
    xor esi, esi
    call sys_event
    call handle
    test eax, eax
    jnz .close

.drain:
    lea rdi, [gui_event]
    mov esi, SYS_GETKEY_NOWAIT
    call sys_event
    test eax, eax
    jz .present
    call handle
    test eax, eax
    jnz .close
    jmp .drain

.present:
    cmp byte [dirty], 0
    je .wait
    mov byte [dirty], 0
    mov edi, [window]
    call gui_damage
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

handle:
    cmp eax, EVENT_MESSAGE
    jne .ignore
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_CLOSE
    je .quit
    cmp eax, GUI_KEY
    je .key
    cmp eax, GUI_MOUSE
    je .mouse
.ignore:
    xor eax, eax
    ret
.quit:
    mov eax, 1
    ret

.key:
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_A]
    or eax, 0x20
    cmp eax, 'c'
    jne .ignore
    call clear_canvas
    xor eax, eax
    ret

; button down: first message of a drag starts at the pointer, the rest draw a line from
; the last point so fast moves don't leave gaps
.mouse:
    test dword [gui_event + EVENT_PAYLOAD + MSG_C], 1
    jz .release
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    cmp esi, PALETTE_H
    jl .pick
    cmp byte [stroke], 0
    jne .line
    mov [last_x], edi
    mov [last_y], esi
    mov byte [stroke], 1
.line:
    call stroke_to
    mov byte [dirty], 1
    xor eax, eax
    ret

.release:
    mov byte [stroke], 0
    xor eax, eax
    ret

.pick:
    mov eax, edi
    xor edx, edx
    mov ecx, SWATCH
    div ecx
    cmp eax, 15
    jbe .picked
    mov eax, 15
.picked:
    mov [color], eax
    call draw_palette
    xor eax, eax
    ret

stroke_to:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, [last_x]
    mov r13d, [last_y]
    mov [last_x], edi
    mov [last_y], esi
    sub edi, r12d
    sub esi, r13d
    mov r14d, edi
    mov r15d, esi
    mov eax, edi
    cdq
    xor eax, edx
    sub eax, edx
    mov ebx, eax
    mov eax, esi
    cdq
    xor eax, edx
    sub eax, edx
    cmp eax, ebx
    jle .steps
    mov ebx, eax
.steps:
    test ebx, ebx
    jnz .walk
    inc ebx
.walk:
    xor ecx, ecx
.point:
    push rcx
    mov eax, r14d
    imul eax, ecx
    cdq
    idiv ebx
    lea edi, [r12 + rax - BRUSH / 2]
    mov ecx, [rsp]
    mov eax, r15d
    imul eax, ecx
    cdq
    idiv ebx
    lea esi, [r13 + rax - BRUSH / 2]
    cmp esi, PALETTE_H          ; never paint over the palette
    jl .skip
    mov edx, BRUSH
    mov ecx, BRUSH
    mov r8d, [color]
    call draw_fill
.skip:
    pop rcx
    inc ecx
    cmp ecx, ebx
    jbe .point
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

clear_canvas:
    xor edi, edi
    mov esi, PALETTE_H
    mov edx, WIDTH
    mov ecx, HEIGHT - PALETTE_H
    mov r8d, 15
    call draw_fill

draw_palette:
    push rbx
    xor ebx, ebx
.swatch:
    imul edi, ebx, SWATCH
    xor esi, esi
    mov edx, SWATCH
    mov ecx, PALETTE_H
    mov r8d, ebx
    call draw_fill
    inc ebx
    cmp ebx, 16
    jb .swatch

    imul edi, [color], SWATCH
    add edi, SWATCH / 2 - 3
    mov esi, PALETTE_H / 2 - 3
    mov edx, 6
    mov ecx, 6
    mov r8d, 15
    cmp dword [color], 15
    jne .mark
    xor r8d, r8d
.mark:
    call draw_fill
    mov byte [dirty], 1
    pop rbx
    ret

title db 'Paint', 0
no_gui_text db 'paint: gui is not running', 0

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/library.inc'

window rd 1
color rd 1
last_x rd 1
last_y rd 1
dirty rb 1
stroke rb 1
