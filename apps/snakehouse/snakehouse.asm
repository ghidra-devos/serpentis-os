include '../runtime/program.inc'

; the pictures are paint drawings in the fixed colors, one byte per pixel, same size as the window
WIDTH = 624
HEIGHT = 404
PICTURE_BYTES = WIDTH * HEIGHT
DATA_MAX = 0x10000
HOTSPOT_MAX = 512
NO_SCENE = 0xFFFF
KEY_ESCAPE = 27

; game.dat, written by tools/make_game.py
GAME_MAGIC = 0x484B4E53
HDR_BYTES = 32
HDR_SCENES = 6
HDR_START = 8
HDR_SCENE_TABLE = 16
HDR_HOT_TABLE = 20
HDR_LINE_TABLE = 24
SCENE_PATH = 0
SCENE_FIRST = 4
SCENE_COUNT = 6
SCENE_SIZE = 8
HOT_X0 = 0
HOT_Y0 = 2
HOT_X1 = 4
HOT_Y1 = 6
HOT_FIRST = 8
HOT_LINES = 10
HOT_GOTO = 12
HOT_FLAGS = 14
HOT_SHIFT = 4
HOT_ONCE = 1

BOX_MARGIN = 8
BOX_PAD = 8
ROW_H = 18
ROW_CHARS = 73

main:
    push rbx

    lea rdi, [data_path]
    lea rsi, [game_data]
    mov edx, DATA_MAX
    call load_file
    cmp eax, HDR_BYTES
    jl .no_data
    cmp dword [game_data], GAME_MAGIC
    jne .no_data

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

    mov dword [pending], NO_SCENE
    movzx edi, word [game_data + HDR_START]
    call enter_scene

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

.no_data:
    lea rdi, [no_data_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

.no_gui:
    lea rdi, [no_gui_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

; rdi -> path, rsi -> buffer, edx = max bytes. out: eax = bytes read or an error
load_file:
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13d, edx
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
    mov edi, ebx
    mov rsi, r12
    mov edx, r13d
    call sys_read
    mov r12d, eax
    mov edi, ebx
    call sys_close
    mov eax, r12d
.done:
    pop r13
    pop r12
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
    cmp eax, KEY_ESCAPE
    je .quit
    or eax, 0x20
    cmp eax, 'q'
    je .quit
    cmp byte [box_open], 0
    je .ignore
    call close_box
    jmp .ignore

; the gui sends every move, a click is only the moment the button goes down
.mouse:
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_C]
    and eax, 1
    movzx edx, byte [button]
    mov [button], al
    test eax, eax
    jz .ignore
    test edx, edx
    jnz .ignore
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    call click
    jmp .ignore

; edi = scene number
enter_scene:
    movzx eax, word [game_data + HDR_SCENES]
    cmp edi, eax
    jae .done
    mov [scene], edi
    imul eax, edi, SCENE_SIZE
    add eax, dword [game_data + HDR_SCENE_TABLE]
    mov eax, dword [game_data + rax + SCENE_PATH]
    lea rdi, [game_data + rax]
    lea rsi, [picture]
    mov edx, PICTURE_BYTES
    call load_file
    cmp eax, PICTURE_BYTES
    je .loaded
    ; no picture, white page so the hotspots still work
    lea rdi, [picture]
    mov esi, PALETTE_FIXED + 15
    mov edx, PICTURE_BYTES
    call mem_set
.loaded:
    mov byte [box_open], 0
    call show_picture
.done:
    ret

show_picture:
    mov rdi, [draw_base]
    lea rsi, [picture]
    mov ecx, PICTURE_BYTES / 8
    rep movsq
    mov byte [dirty], 1
    ret

; edi, esi = click in picture pixels
click:
    push rbx
    push r12
    push r13
    push r14
    cmp byte [box_open], 0
    je .find
    call close_box
    jmp .done
.find:
    mov r12d, edi
    mov r13d, esi
    imul eax, [scene], SCENE_SIZE
    add eax, dword [game_data + HDR_SCENE_TABLE]
    movzx ebx, word [game_data + rax + SCENE_FIRST]
    movzx r14d, word [game_data + rax + SCENE_COUNT]
    add r14d, ebx
.next:
    cmp ebx, r14d
    jae .done
    mov eax, ebx
    shl eax, HOT_SHIFT
    add eax, dword [game_data + HDR_HOT_TABLE]
    lea rcx, [game_data + rax]
    movzx eax, word [rcx + HOT_X0]
    cmp r12d, eax
    jb .skip
    movzx eax, word [rcx + HOT_X1]
    cmp r12d, eax
    jae .skip
    movzx eax, word [rcx + HOT_Y0]
    cmp r13d, eax
    jb .skip
    movzx eax, word [rcx + HOT_Y1]
    cmp r13d, eax
    jae .skip
    mov edi, ebx
    call use_hotspot
    jmp .done
.skip:
    inc ebx
    jmp .next
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; edi = hotspot number. says the next line, the goto waits until the box is closed
use_hotspot:
    push rbx
    push r12
    mov r12d, edi
    mov eax, edi
    shl eax, HOT_SHIFT
    add eax, dword [game_data + HDR_HOT_TABLE]
    lea rbx, [game_data + rax]
    movzx eax, word [rbx + HOT_GOTO]
    mov [pending], eax
    movzx ecx, word [rbx + HOT_LINES]
    test ecx, ecx
    jz .go
    movzx edx, word [said + r12 * 2]
    lea eax, [rdx + 1]
    cmp eax, ecx
    jb .advance
    xor eax, eax
    test word [rbx + HOT_FLAGS], HOT_ONCE
    jz .advance
    mov eax, edx
.advance:
    mov [said + r12 * 2], ax
    movzx eax, word [rbx + HOT_FIRST]
    add eax, edx
    shl eax, 2
    add eax, dword [game_data + HDR_LINE_TABLE]
    mov eax, dword [game_data + rax]
    lea rdi, [game_data + rax]
    call open_box
    jmp .done
.go:
    call follow
.done:
    pop r12
    pop rbx
    ret

follow:
    mov edi, [pending]
    mov dword [pending], NO_SCENE
    cmp edi, NO_SCENE
    je .done
    jmp enter_scene
.done:
    ret

close_box:
    mov byte [box_open], 0
    call show_picture
    jmp follow

; rdi -> text, rows split on 10. make_game.py already wrapped them
open_box:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12d, 1
    mov rsi, rdi
.count:
    mov al, [rsi]
    test al, al
    jz .counted
    cmp al, 10
    jne .count_next
    inc r12d
.count_next:
    inc rsi
    jmp .count
.counted:
    imul r13d, r12d, ROW_H
    add r13d, BOX_PAD * 2
    mov esi, HEIGHT - BOX_MARGIN
    sub esi, r13d
    mov [box_top], esi
    mov edi, BOX_MARGIN
    mov edx, WIDTH - BOX_MARGIN * 2
    mov ecx, r13d
    mov r8d, PALETTE_FIXED
    call draw_fill
    mov edi, BOX_MARGIN + 2
    mov esi, [box_top]
    add esi, 2
    mov edx, WIDTH - BOX_MARGIN * 2 - 4
    lea ecx, [r13 - 4]
    mov r8d, PALETTE_FIXED + 15
    call draw_fill

    mov r13d, [box_top]
    add r13d, BOX_PAD + 1
.row:
    lea rdi, [row_text]
    xor ecx, ecx
.copy:
    mov al, [rbx]
    test al, al
    jz .put
    inc rbx
    cmp al, 10
    je .put
    cmp ecx, ROW_CHARS
    jae .copy
    mov [rdi + rcx], al
    inc ecx
    jmp .copy
.put:
    mov byte [rdi + rcx], 0
    mov r12d, eax
    mov edi, BOX_MARGIN + BOX_PAD + 4
    mov esi, r13d
    lea rdx, [row_text]
    mov ecx, PALETTE_FIXED
    mov r8d, -1
    call draw_text
    add r13d, ROW_H
    test r12b, r12b
    jnz .row

    mov byte [box_open], 1
    mov byte [dirty], 1
    pop r13
    pop r12
    pop rbx
    ret

title db 'Snake House', 0
data_path db '/share/game/game.dat', 0
no_data_text db 'snakehouse: /share/game/game.dat is missing or broken', 0
no_gui_text db 'snakehouse: gui is not running', 0

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/library.inc'

window rd 1
scene rd 1
pending rd 1
box_top rd 1
dirty rb 1
box_open rb 1
button rb 1
row_text rb ROW_CHARS + 1
said rw HOTSPOT_MAX
game_data rb DATA_MAX
picture rb PICTURE_BYTES
