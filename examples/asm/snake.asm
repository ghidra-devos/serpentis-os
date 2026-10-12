include '/lib/start.inc'

WINDOW_W = 78 * 8 + SCREEN_PAD_X * 2
WINDOW_H = 26 * 16 + SCREEN_PAD_Y * 2
SNAKE_MAX = 2048
BOARD_MAX = 80 * 30
START_LENGTH = 4
START_DELAY = 10
FASTEST_DELAY = 3

CELL_EMPTY = 0
CELL_SNAKE = 1
CELL_FOOD = 2

ATTR_FIELD = 0x00
ATTR_BODY = 0x2A
ATTR_HEAD = 0x2E
ATTR_FOOD = 0x0C
ATTR_WALL = 0x08
ATTR_STATUS = 0x70

main:
    push rbx
    lea rdi, [title]
    mov esi, WINDOW_W
    mov edx, WINDOW_H
    call screen_open
    call sys_getticks
    or eax, 1
    mov [seed], eax
    call new_game

.loop:
    cmp byte [quit], 0
    jne .exit
    mov esi, EVENT_WANT_TICK
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_CLOSE
    je .exit
    cmp eax, SCREEN_RESIZE
    je .resize
    call sys_getticks
    mov ecx, eax
    sub ecx, [last_step]
    cmp ecx, [delay]
    jb .loop
    mov [last_step], eax
    call step
    jmp .loop

.key:
    mov edi, [screen_key]
    call handle_key
    jmp .loop

.resize:
    call new_game
    call screen_acknowledge_resize
    jmp .loop

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

new_game:
    mov eax, [screen_columns]
    cmp eax, 80
    jbe .width
    mov eax, 80
.width:
    mov [board_w], eax
    mov eax, [screen_rows]
    dec eax
    cmp eax, 30
    jbe .height
    mov eax, 30
.height:
    mov [board_h], eax

    lea rdi, [board]
    xor esi, esi
    mov edx, BOARD_MAX
    call mem_set

    call screen_clear
    call draw_walls

    mov dword [head], START_LENGTH - 1
    mov dword [length], START_LENGTH
    mov dword [score], 0
    mov dword [delay], START_DELAY
    mov dword [direction_x], 1
    mov dword [direction_y], 0
    mov dword [next_x], 1
    mov dword [next_y], 0
    mov byte [dead], 0
    mov byte [paused], 0

    mov ecx, 0
.body:
    cmp ecx, START_LENGTH
    jae .placed
    mov eax, [board_w]
    shr eax, 2
    add eax, ecx
    mov edx, [board_h]
    shr edx, 1
    mov [snake_x + rcx], al
    mov [snake_y + rcx], dl
    push rcx
    mov edi, eax
    mov esi, edx
    mov dl, CELL_SNAKE
    call set_cell
    pop rcx
    inc ecx
    jmp .body
.placed:
    call place_food
    call draw_snake
    call draw_status
    jmp screen_present

draw_walls:
    push rbx
    xor ebx, ebx
.top:
    cmp ebx, [board_w]
    jae .sides
    mov edi, ebx
    xor esi, esi
    call wall_at
    mov edi, ebx
    mov esi, [board_h]
    dec esi
    call wall_at
    inc ebx
    jmp .top
.sides:
    xor ebx, ebx
.side:
    cmp ebx, [board_h]
    jae .done
    xor edi, edi
    mov esi, ebx
    call wall_at
    mov edi, [board_w]
    dec edi
    mov esi, ebx
    call wall_at
    inc ebx
    jmp .side
.done:
    pop rbx
    ret

wall_at:
    push rdi
    push rsi
    mov dl, CELL_SNAKE
    call set_cell
    pop rsi
    pop rdi
    mov dl, 0xB1
    mov cl, ATTR_WALL
    jmp screen_put

cell_index:
    mov eax, esi
    imul eax, [board_w]
    add eax, edi
    ret

set_cell:
    call cell_index
    mov [board + rax], dl
    ret

get_cell:
    call cell_index
    movzx eax, byte [board + rax]
    ret

random:
    mov eax, [seed]
    mov ecx, eax
    shl ecx, 13
    xor eax, ecx
    mov ecx, eax
    shr ecx, 17
    xor eax, ecx
    mov ecx, eax
    shl ecx, 5
    xor eax, ecx
    mov [seed], eax
    ret

place_food:
    push rbx
    push r12
.try:
    call random
    xor edx, edx
    mov ecx, [board_w]
    sub ecx, 2
    div ecx
    lea ebx, [rdx + 1]
    call random
    xor edx, edx
    mov ecx, [board_h]
    sub ecx, 2
    div ecx
    lea r12d, [rdx + 1]
    mov edi, ebx
    mov esi, r12d
    call get_cell
    cmp eax, CELL_EMPTY
    jne .try
    mov edi, ebx
    mov esi, r12d
    mov dl, CELL_FOOD
    call set_cell
    mov edi, ebx
    mov esi, r12d
    mov dl, 0x04
    mov cl, ATTR_FOOD
    call screen_put
    pop r12
    pop rbx
    ret

draw_snake:
    push rbx
    xor ebx, ebx
.segment:
    cmp ebx, [length]
    jae .done
    mov eax, [head]
    sub eax, ebx
    and eax, SNAKE_MAX - 1
    movzx edi, byte [snake_x + rax]
    movzx esi, byte [snake_y + rax]
    mov dl, 0xDB
    mov cl, ATTR_BODY
    test ebx, ebx
    jnz .put
    mov dl, 0x02
    mov cl, ATTR_HEAD
.put:
    call screen_put
    inc ebx
    jmp .segment
.done:
    pop rbx
    ret

step:
    cmp byte [dead], 0
    jne .done
    cmp byte [paused], 0
    jne .done
    push rbx
    push r12
    push r13

    mov eax, [next_x]
    mov [direction_x], eax
    mov eax, [next_y]
    mov [direction_y], eax

    mov eax, [head]
    movzx r12d, byte [snake_x + rax]
    movzx r13d, byte [snake_y + rax]
    add r12d, [direction_x]
    add r13d, [direction_y]

    mov edi, r12d
    mov esi, r13d
    call get_cell
    cmp eax, CELL_SNAKE
    je .crash
    mov ebx, eax

    cmp ebx, CELL_FOOD
    je .grow
    mov eax, [head]
    sub eax, [length]
    inc eax
    and eax, SNAKE_MAX - 1
    movzx edi, byte [snake_x + rax]
    movzx esi, byte [snake_y + rax]
    push rdi
    push rsi
    mov dl, CELL_EMPTY
    call set_cell
    pop rsi
    pop rdi
    mov dl, ' '
    mov cl, ATTR_FIELD
    call screen_put
    jmp .advance

.grow:
    cmp dword [length], SNAKE_MAX - 1
    jae .advance
    inc dword [length]
    add dword [score], 10
    cmp dword [delay], FASTEST_DELAY
    jbe .advance
    mov eax, [score]
    xor edx, edx
    mov ecx, 50
    div ecx
    test edx, edx
    jnz .advance
    dec dword [delay]

.advance:
    mov eax, [head]
    movzx edi, byte [snake_x + rax]
    movzx esi, byte [snake_y + rax]
    mov dl, 0xDB
    mov cl, ATTR_BODY
    call screen_put

    mov eax, [head]
    inc eax
    and eax, SNAKE_MAX - 1
    mov [head], eax
    mov [snake_x + rax], r12b
    mov [snake_y + rax], r13b
    mov edi, r12d
    mov esi, r13d
    mov dl, CELL_SNAKE
    call set_cell
    mov edi, r12d
    mov esi, r13d
    mov dl, 0x02
    mov cl, ATTR_HEAD
    call screen_put

    cmp ebx, CELL_FOOD
    jne .shown
    call place_food
    jmp .shown

.crash:
    mov byte [dead], 1
.shown:
    call draw_status
    call screen_present
    pop r13
    pop r12
    pop rbx
.done:
    ret

handle_key:
    mov eax, edi
    cmp eax, 'q'
    je .quit
    cmp eax, 27
    je .quit
    cmp byte [dead], 0
    je .playing
    cmp eax, 'r'
    je new_game
    cmp eax, ' '
    je new_game
    ret
.playing:
    cmp eax, 'p'
    je .pause
    cmp eax, KEY_UP
    je .up
    cmp eax, 'w'
    je .up
    cmp eax, 'k'
    je .up
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 's'
    je .down
    cmp eax, 'j'
    je .down
    cmp eax, KEY_LEFT
    je .left
    cmp eax, 'a'
    je .left
    cmp eax, 'h'
    je .left
    cmp eax, KEY_RIGHT
    je .right
    cmp eax, 'd'
    je .right
    cmp eax, 'l'
    je .right
    ret
.quit:
    mov byte [quit], 1
    ret
.pause:
    xor byte [paused], 1
    call draw_status
    jmp screen_present
.up:
    cmp dword [direction_y], 1
    je .ignored
    mov dword [next_x], 0
    mov dword [next_y], -1
    ret
.down:
    cmp dword [direction_y], -1
    je .ignored
    mov dword [next_x], 0
    mov dword [next_y], 1
    ret
.left:
    cmp dword [direction_x], 1
    je .ignored
    mov dword [next_x], -1
    mov dword [next_y], 0
    ret
.right:
    cmp dword [direction_x], -1
    je .ignored
    mov dword [next_x], 1
    mov dword [next_y], 0
.ignored:
    ret

draw_status:
    push rbx
    mov ebx, [screen_rows]
    dec ebx
    xor edi, edi
.clear:
    cmp edi, [screen_columns]
    jae .text
    push rdi
    mov esi, ebx
    mov dl, ' '
    mov cl, ATTR_STATUS
    call screen_put
    pop rdi
    inc edi
    jmp .clear
.text:
    lea rdi, [status]
    lea rsi, [score_text]
    call str_copy
    mov edi, [score]
    call u64_to_dec
    mov rsi, rax
    lea rdi, [status]
    call append
    lea rsi, [keys_text]
    cmp byte [paused], 0
    je .state
    lea rsi, [paused_text]
.state:
    cmp byte [dead], 0
    je .message
    lea rsi, [over_text]
.message:
    lea rdi, [status]
    call append
    lea rbx, [status]
    mov edi, 1
.put:
    mov dl, [rbx]
    test dl, dl
    jz .done
    push rdi
    mov esi, [screen_rows]
    dec esi
    mov cl, ATTR_STATUS
    call screen_put
    pop rdi
    inc edi
    inc rbx
    jmp .put
.done:
    pop rbx
    ret

append:
    push rsi
    call str_len
    add rdi, rax
    pop rsi
    jmp str_copy

title db 'Snake', 0
score_text db 'Score ', 0
keys_text db '   arrows or wasd to steer, p pause, q quit', 0
paused_text db '   paused, p to continue', 0
over_text db '   game over, r to play again, q to quit', 0
quit db 0

include '/lib/draw.inc'
include '/lib/gui.inc'
include '/lib/screen.inc'
include '/lib/library.inc'
include '/lib/screen_bss.inc'

seed rd 1
board_w rd 1
board_h rd 1
head rd 1
length rd 1
score rd 1
delay rd 1
last_step rd 1
direction_x rd 1
direction_y rd 1
next_x rd 1
next_y rd 1
dead rb 1
paused rb 1
status rb 96
snake_x rb SNAKE_MAX
snake_y rb SNAKE_MAX
board rb BOARD_MAX
