; pic [-b] [picture.pic]. shows a .pic (the snake by default) as a picture in the gui terminal.
; -b also prints the big name under it, lds runs that when a terminal opens

include '../runtime/program.inc'

main:
    push rbx
    lea rbx, [snake_path]
    mov byte [with_banner], 0
.argument:
    call arg_next
    test rax, rax
    jz .show
    mov rdi, rdx
    cmp word [rax], '-b'
    jne .path
    cmp byte [rax + 2], 0
    jne .path
    mov byte [with_banner], 1
    jmp .argument
.path:
    mov rbx, rax
    jmp .argument
.show:
    mov rdi, rbx
    lea rsi, [logo_file]
    call logo_show
    cmp byte [with_banner], 0
    je .done
    lea rdi, [banner_text]
    call print
.done:
    xor eax, eax
    pop rbx
    ret

snake_path db '/share/logos/snake.pic', 0
banner_text:
    db ' SSSSS EEEEE RRRR  PPPP  EEEEE N   N TTTTT IIIII SSSSS', 10
    db ' S     E     R   R P   P E     NN  N   T     I   S', 10
    db ' SSSSS EEEE  RRRR  PPPP  EEEE  N N N   T     I   SSSSS', 10
    db '     S E     R  R  P     E     N  NN   T     I       S', 10
    db ' SSSSS EEEEE R   R P     EEEEE N   N   T   IIIII SSSSS', 10, 10, 0

include '../runtime/logo.inc'
include '../runtime/termgfx.inc'
include '../runtime/gui.inc'
include '../runtime/draw.inc'
include '../runtime/library.inc'

align 8
with_banner rb 1
logo_file rb LOGO_FILE_MAX
