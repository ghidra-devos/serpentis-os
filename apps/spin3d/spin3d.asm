; spin3d alone opens a window and turns the meshes in /share/meshes. spin3d cube (or apple,
; or a path) puts the mesh in the terminal instead and lds keeps turning it there by itself,
; under whatever gets typed next. x once in 4 s, z once in 5.2 s

SERP_STACK = 0x4000

include '../runtime/program.inc'

WINDOW_W = 480
WINDOW_H = 360
MESH_COUNT = 2
MESH_BYTES = 0x10000
ZBUFFER_BYTES = SURFACE_BYTES * 4
FRAME_TICKS = 4
STEP_MAX = 10
BACKGROUND = PALETTE_FIXED
TEXT_COLOR = PALETTE_FIXED + 15
HINT_COLOR = PALETTE_FIXED + 7
KEY_ESCAPE = 27
SPIN_W = 240
SPIN_H = 180
X_TURN_TICKS = 4 * PIT_FREQUENCY
Z_TURN_TICKS = 52 * PIT_FREQUENCY / 10
PATH_MAX = 96

main:
    push rbx
    call arg_next
    test rax, rax
    jz .window
    call inline
    pop rbx
    ret
.window:
    lea rdi, [window_title]
    mov esi, WINDOW_W
    mov edx, WINDOW_H
    call screen_open
    mov edi, ZBUFFER_BYTES
    call sys_alloc
    test rax, rax
    jle .exit
    mov [zbuffer], rax
    mov byte [spinning], 1
    mov dword [mesh_index], -1
    call next_mesh
    mov byte [running], 1
    mov byte [dirty], 1

.loop:
    call frame
    cmp byte [running], 0
    je .exit
    mov esi, EVENT_WANT_TICK
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_CLICK
    je .drag
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .exit
    call release
    jmp .loop

.key:
    mov eax, [screen_key]
    call handle_key
    jmp .loop

.drag:
    call drag
    jmp .loop

.resize:
    mov byte [dirty], 1
    call frame
    call screen_acknowledge_resize
    jmp .loop

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

handle_key:
    cmp eax, 'q'
    je .quit
    cmp eax, KEY_ESCAPE
    je .quit
    mov byte [dirty], 1
    cmp eax, 'n'
    je next_mesh
    cmp eax, 'w'
    je .wire
    cmp eax, ' '
    je .pause
    movss xmm0, [key_turn]
    cmp eax, KEY_LEFT
    je turn_z
    cmp eax, KEY_UP
    je turn_x
    xorps xmm0, xmm0
    subss xmm0, [key_turn]
    cmp eax, KEY_RIGHT
    je turn_z
    cmp eax, KEY_DOWN
    je turn_x
    ret
.quit:
    mov byte [running], 0
    ret
.wire:
    xor byte [wire], G3_WIRE
    ret
.pause:
    xor byte [spinning], 1
    ret

; xmm0 = radians to add
turn_x:
    lea rdi, [angle_x]
    jmp turn
turn_z:
    lea rdi, [angle_z]

turn:
    addss xmm0, [rdi]
    comiss xmm0, [pi]
    jbe .low
    subss xmm0, [two_pi]
.low:
    xorps xmm1, xmm1
    subss xmm1, [pi]
    comiss xmm0, xmm1
    jae .done
    addss xmm0, [two_pi]
.done:
    movss [rdi], xmm0
    ret

; every gui mouse message with the button down shows up as a click, the gui keeps sending them
; while the button is held even outside the window
drag:
    push rbx
    push r12
    mov ebx, [screen_click_x]
    mov r12d, [screen_click_y]
    cmp byte [dragging], 0
    je .grab
    mov eax, ebx
    sub eax, [drag_x]
    cvtsi2ss xmm0, eax
    mulss xmm0, [drag_rate]
    call turn_z
    mov eax, r12d
    sub eax, [drag_y]
    cvtsi2ss xmm0, eax
    mulss xmm0, [drag_rate]
    call turn_x
    mov byte [dirty], 1
.grab:
    mov byte [dragging], 1
    mov [drag_x], ebx
    mov [drag_y], r12d
    pop r12
    pop rbx
    ret

; screen_wait says nothing about the button going up, so look at the raw event
release:
    mov eax, [screen_event + EVENT_TYPE]
    cmp eax, EVENT_MOUSE
    je .console
    cmp eax, EVENT_MESSAGE
    jne .done
    cmp dword [screen_event + EVENT_PAYLOAD + MSG_OP], GUI_MOUSE
    jne .done
    test dword [screen_event + EVENT_PAYLOAD + MSG_C], 1
    jnz .done
    mov byte [dragging], 0
.done:
    ret
.console:
    test dword [screen_event + EVENT_BUTTONS], 1
    jnz .done
    mov byte [dragging], 0
    ret

; hands the mesh to the terminal and is done, the picture outlives us
inline:
    call mesh_path
    jc .no_mesh
    lea rdi, [path]
    call load_mesh
    lea rdi, [mesh_data]
    mov esi, [mesh_size]
    call g3_mesh_check
    test eax, eax
    jz .no_mesh
    mov [mesh_size], eax
    mov edi, SPIN_W
    mov esi, SPIN_H
    call tg_open
    test rax, rax
    js .no_terminal
    mov rdi, rax
    lea rsi, [mesh_data]
    mov ecx, [mesh_size]
    rep movsb
    mov edi, X_TURN_TICKS
    mov esi, Z_TURN_TICKS
    call tg_spin
    call tg_close
    xor eax, eax
    ret
.no_mesh:
    lea rsi, [no_mesh_text]
    mov edx, no_mesh_length
    jmp .fail
.no_terminal:
    lea rsi, [no_terminal_text]
    mov edx, no_terminal_length
.fail:
    mov edi, FD_STDERR
    call sys_write
    mov eax, 1
    ret

; a path stays a path, a file here wins, anything else is /share/meshes/<name>.msh
mesh_path:
    push rbx
    mov rbx, rax
    mov rdi, rax
    call str_len
    cmp eax, PATH_MAX - mesh_dir_length - 5
    ja .long
    cmp byte [rbx], '.'
    je .as_is
    mov rdi, rbx
.slash:
    mov al, [rdi]
    test al, al
    jz .plain
    cmp al, '/'
    je .as_is
    inc rdi
    jmp .slash
.plain:
    mov rdi, rbx
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .shared
    mov edi, eax
    call sys_close
    jmp .as_is
.shared:
    lea rdi, [path]
    lea rsi, [mesh_dir]
    call str_copy
    lea rdi, [path + mesh_dir_length]
    mov rsi, rbx
    call str_copy
    lea rdi, [path]
    call str_len
    cmp eax, 4
    jb .suffix
    cmp dword [path + rax - 4], '.msh'
    je .done
.suffix:
    lea rdi, [path + rax]
    lea rsi, [mesh_suffix]
    call str_copy
.done:
    pop rbx
    clc
    ret
.as_is:
    lea rdi, [path]
    mov rsi, rbx
    call str_copy
    pop rbx
    clc
    ret
.long:
    pop rbx
    stc
    ret

next_mesh:
    mov eax, [mesh_index]
    inc eax
    cmp eax, MESH_COUNT
    jb .index
    xor eax, eax
.index:
    mov [mesh_index], eax
    mov rdi, [mesh_paths + rax * 8]

load_mesh:
    push rbx
    push r12
    mov r12, rdi
    mov dword [mesh_data], 0
    mov dword [mesh_size], 0
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .named
    mov ebx, eax
    mov edi, ebx
    lea rsi, [mesh_data]
    mov edx, MESH_BYTES
    call sys_read
    mov [mesh_size], eax
    mov edi, ebx
    call sys_close
.named:
    mov rsi, r12
    call base_name
    mov [mesh_name], rax
    lea rdi, [mesh_data]
    mov esi, [mesh_size]
    call g3_mesh_check
    test eax, eax
    jnz .done
    lea rax, [missing_text]
    mov [mesh_name], rax
.done:
    mov byte [dirty], 1
    pop r12
    pop rbx
    ret

; the spin follows the clock, not the frame rate, so a slow frame turns it further
frame:
    call sys_getticks
    mov rdx, rax
    sub rdx, [last_frame]
    cmp rdx, FRAME_TICKS
    jb .draw
    mov [last_frame], rax
    cmp byte [spinning], 0
    je .draw
    cmp byte [dragging], 0
    jne .draw
    cmp rdx, STEP_MAX
    jbe .step
    mov edx, STEP_MAX
.step:
    cvtsi2ss xmm0, edx
    movss [steps], xmm0
    mulss xmm0, [x_rate]
    call turn_x
    movss xmm0, [steps]
    mulss xmm0, [z_rate]
    call turn_z
    mov byte [dirty], 1
.draw:
    cmp byte [dirty], 0
    je .done
    mov byte [dirty], 0
    call render
.done:
    ret

render:
    mov rdi, [draw_base]
    mov esi, [draw_stride]
    mov edx, [draw_width]
    mov ecx, [draw_height]
    mov r8, [zbuffer]
    call g3_begin
    mov edi, BACKGROUND
    call g3_clear
    xorps xmm0, xmm0
    xorps xmm1, xmm1
    movss xmm2, [distance]
    call g3_translate
    movss xmm0, [angle_z]
    call g3_rotate_z
    movss xmm0, [angle_x]
    call g3_rotate_x
    lea rdi, [mesh_data]
    call g3_fit
    lea rdi, [mesh_data]
    movzx esi, byte [wire]
    or esi, G3_NO_CULL
    call g3_draw_mesh

    mov edi, 8
    mov esi, 6
    mov rdx, [mesh_name]
    mov cl, TEXT_COLOR
    mov r8d, -1
    call draw_text
    mov edi, 8
    mov esi, [draw_height]
    sub esi, FONT_HEIGHT_PIXELS + 6
    lea rdx, [hint_text]
    mov cl, HINT_COLOR
    mov r8d, -1
    call draw_text
    jmp screen_present

window_title db 'Spin 3D', 0
hint_text db 'n next  w wires  space stop  drag to turn', 0
missing_text db 'no mesh', 0
apple_path db '/share/meshes/apple.msh', 0
cube_path db '/share/meshes/cube.msh', 0

align 8
mesh_paths dq apple_path, cube_path
x_rate dd 0x3C80ADFD
z_rate dd 0x3C45F7FB
drag_rate dd 0xBC23D70A
key_turn dd 0x3DCCCCCD
distance dd 0x40400000
pi dd 0x40490FDB
two_pi dd 0x40C90FDB

no_terminal_text db 'spin3d: no terminal to draw in, run spin3d alone for the window', 10
no_terminal_length = $ - no_terminal_text
no_mesh_text db 'spin3d: no such mesh', 10
no_mesh_length = $ - no_mesh_text
mesh_suffix db '.msh', 0
mesh_dir db '/share/meshes/'
mesh_dir_length = $ - mesh_dir
    db 0

include '../runtime/termgfx.inc'
include '../runtime/gfx3d.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'
include '../runtime/gfx3d_bss.inc'

align 8
zbuffer rq 1
mesh_name rq 1
last_frame rq 1
mesh_index rd 1
mesh_size rd 1
angle_x rd 1
angle_z rd 1
steps rd 1
drag_x rd 1
drag_y rd 1
running rb 1
dirty rb 1
wire rb 1
spinning rb 1
dragging rb 1
path rb PATH_MAX + 8
align 16
mesh_data rb MESH_BYTES
