include '../runtime/program.inc'


START_W = 400
START_H = 300
PALETTE_H = 16
BRUSH = 3
PIC_HEADER = 8
PIC_MAX_W = 640
PIC_MAX_H = 448
KEY_ESCAPE = 27
KEY_SAVE = 19
NOTE_COLOR = PALETTE_FIXED + 15

main:
    push rbx
    mov dword [width], START_W
    mov dword [height], START_H
    push rdi
    call drawings_home
    pop rdi
    call arg_next
    test rax, rax
    jz .connect
    mov rsi, rax
    call load_picture

.connect:
    call gui_connect
    test eax, eax
    js .no_gui

    mov edi, [width]
    mov esi, [height]
    lea rdx, [title]
    call gui_open_window
    test eax, eax
    js .no_gui
    mov [window], eax
    mov [surface], rdx
    mov rdi, rdx
    mov esi, [width]
    mov edx, esi
    mov ecx, [height]
    call draw_set_target
    mov eax, [width]
    shr eax, 4
    mov [swatch], eax

    mov dword [color], 0
    call clear_canvas
    cmp byte [loaded], 0
    je .wait
    call show_picture
    mov byte [dirty], 0
    mov edi, [window]
    call gui_damage

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
    cmp eax, KEY_ESCAPE
    je .quit
    cmp eax, KEY_SAVE
    je .save
    or eax, 0x20
    cmp eax, 'c'
    jne .ignore
    call clear_canvas
    xor eax, eax
    ret
.save:
    call save_picture
    xor eax, eax
    ret

; button down: first message of a drag starts at the pointer, the rest draw a line from
; the last point so fast moves don't leave gaps
.mouse:
    test dword [gui_event + EVENT_PAYLOAD + MSG_C], 1
    jz .release
    cmp byte [noted], 0
    je .pressed
    mov byte [noted], 0
    call draw_palette
.pressed:
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
    mov ecx, [swatch]
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
    add r8d, PALETTE_FIXED
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
    mov edx, [width]
    mov ecx, [height]
    sub ecx, PALETTE_H
    mov r8d, PALETTE_FIXED + 15
    call draw_fill

draw_palette:
    push rbx
    xor ebx, ebx
.swatch:
    mov edi, ebx
    imul edi, [swatch]
    xor esi, esi
    mov edx, [swatch]
    mov ecx, PALETTE_H
    lea r8d, [rbx + PALETTE_FIXED]
    call draw_fill
    inc ebx
    cmp ebx, 16
    jb .swatch

    mov edi, [color]
    imul edi, [swatch]
    mov eax, [swatch]
    shr eax, 1
    lea edi, [rdi + rax - 3]
    mov esi, PALETTE_H / 2 - 3
    mov edx, 6
    mov ecx, 6
    mov r8d, PALETTE_FIXED + 15
    cmp dword [color], 15
    jne .mark
    mov r8d, PALETTE_FIXED
.mark:
    call draw_fill
    mov byte [dirty], 1
    pop rbx
    ret

canvas_bytes:
    mov eax, [height]
    sub eax, PALETTE_H
    imul eax, [width]
    ret

canvas_start:
    mov eax, [width]
    imul eax, eax, PALETTE_H
    mov rsi, [surface]
    add rsi, rax
    ret

load_picture:
    push rbx
    lea rdi, [picture_path]
    call str_copy
    lea rdi, [picture_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .forget
    mov ebx, eax
    mov edi, ebx
    lea rsi, [picture]
    mov edx, PIC_HEADER + PIC_MAX_W * PIC_MAX_H
    call sys_read
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    cmp eax, PIC_HEADER
    jl .forget
    cmp dword [picture], 'SPIC'
    jne .forget
    movzx ecx, word [picture + 4]
    movzx edx, word [picture + 6]
    cmp ecx, 16             ; swatch is width/16, can't be 0
    jb .forget
    cmp ecx, PIC_MAX_W
    ja .forget
    test edx, edx
    jz .forget
    cmp edx, PIC_MAX_H - PALETTE_H
    ja .forget
    mov ebx, ecx
    imul ebx, edx
    add ebx, PIC_HEADER
    cmp eax, ebx
    jl .forget
    mov [width], ecx
    add edx, PALETTE_H
    mov [height], edx
    mov byte [loaded], 1
    call keep_path
    pop rbx
    ret
.forget:
    mov byte [picture_path], 0
    pop rbx
    ret

; only remember the path if it's in our own drawings dir, ctrl+s on something
; from /share/drawings makes a new numbered copy instead
keep_path:
    lea rsi, [drawings_prefix]
    lea rdi, [picture_path]
.compare:
    mov al, [rsi]
    test al, al
    jz .done
    cmp al, [rdi]
    jne .other
    inc rsi
    inc rdi
    jmp .compare
.other:
    mov byte [picture_path], 0
.done:
    ret

show_picture:
    call canvas_start
    mov rdi, rsi
    lea rsi, [picture + PIC_HEADER]
    call canvas_bytes
    mov ecx, eax
    rep movsb
    mov byte [dirty], 1
    ret

save_picture:
    push rbx
    cmp byte [picture_path], 0
    jne .write
    lea rdi, [drawings_dir]
    call sys_mkdir
    mov ebx, 1
.find:
    mov edi, ebx
    call name_drawing
    lea rdi, [picture_path]
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .write
    inc ebx
    cmp ebx, 1000
    jb .find
    mov byte [picture_path], 0
    jmp .failed

.write:
    mov dword [picture], 'SPIC'
    mov eax, [width]
    mov word [picture + 4], ax
    mov eax, [height]
    sub eax, PALETTE_H
    mov word [picture + 6], ax
    call canvas_start
    lea rdi, [picture + PIC_HEADER]
    call canvas_bytes
    mov ecx, eax
    rep movsb
    lea rdi, [picture_path]
    call sys_unlink         ; no O_TRUNC yet, kill the old one first
    lea rdi, [picture_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax
    call canvas_bytes
    lea edx, [rax + PIC_HEADER]
    mov edi, ebx
    lea rsi, [picture]
    call sys_write
    push rax
    mov edi, ebx
    call sys_close
    pop rcx
    call canvas_bytes
    add eax, PIC_HEADER
    cmp ecx, eax
    jne .failed
    lea rdi, [note]
    lea rsi, [saved_text]
    call str_copy
    lea rsi, [picture_path]
    call base_name
    push rax
    lea rdi, [note]
    call str_len
    lea rdi, [note + rax]
    pop rsi
    call str_copy
    jmp .show

.failed:
    lea rdi, [note]
    lea rsi, [failed_text]
    call str_copy

.show:
    call show_note
    pop rbx
    ret

; drawings go under the home of whoever is logged in, not a fixed guest
drawings_home:
    call sys_getuid
    mov edi, eax
    call user_home
    lea rdi, [drawings_dir]
    mov rsi, rax
    call str_copy
    lea rdi, [drawings_dir]
    call str_len
    lea rdi, [drawings_dir + rax]
    lea rsi, [drawings_tail]
    call str_copy
    lea rdi, [drawings_prefix]
    lea rsi, [drawings_dir]
    call str_copy
    lea rdi, [drawings_prefix]
    call str_len
    mov word [drawings_prefix + rax], '/'
    ret

name_drawing:
    push rdi
    lea rdi, [picture_path]
    lea rsi, [drawings_prefix]
    call str_copy
    pop rdi
    call u64_to_dec
    push rax
    lea rdi, [picture_path]
    call str_len
    lea rdi, [picture_path + rax]
    lea rsi, [drawing_word]
    call str_copy
    lea rdi, [picture_path]
    call str_len
    lea rdi, [picture_path + rax]
    pop rsi
    call str_copy
    lea rdi, [picture_path]
    call str_len
    lea rdi, [picture_path + rax]
    lea rsi, [pic_suffix]
    jmp str_copy

show_note:
    lea rdi, [note]
    call str_len
    shl eax, 3
    add eax, 8
    mov edx, eax
    mov edi, [width]
    sub edi, eax
    xor esi, esi
    mov ecx, PALETTE_H
    mov r8d, PALETTE_FIXED
    push rdi
    call draw_fill
    pop rdi
    add edi, 4
    xor esi, esi
    lea rdx, [note]
    mov cl, NOTE_COLOR
    mov r8d, -1
    call draw_text
    mov byte [noted], 1
    mov byte [dirty], 1
    ret

title db 'Paint', 0
no_gui_text db 'paint: gui is not running', 0
drawings_tail db '/drawings', 0
drawing_word db 'drawing', 0
pic_suffix db '.pic', 0
saved_text db 'saved ', 0
failed_text db 'could not save', 0

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/users_bss.inc'

align 8
surface rq 1
window rd 1
width rd 1
height rd 1
swatch rd 1
color rd 1
last_x rd 1
last_y rd 1
dirty rb 1
stroke rb 1
loaded rb 1
noted rb 1
status rb USER_STAT_SIZE
note rb 64
drawings_dir rb 256
drawings_prefix rb 256
picture_path rb 256
picture rb PIC_HEADER + PIC_MAX_W * PIC_MAX_H
