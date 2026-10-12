include '/lib/start.inc'

define WIDTH 4
HEIGHT equ 3

main:
    lea rdi, [banner]
    call print_line

if WIDTH = 4
    lea rdi, [text_a]
    call print_line
else
    lea rdi, [text_bad]
    call print_line
end if

if WIDTH > 10
    lea rdi, [text_bad]
    call print_line
else if HEIGHT = 3
    lea rdi, [text_b]
    call print_line
else
    lea rdi, [text_bad]
    call print_line
end if

if defined WIDTH
    lea rdi, [text_c]
    call print_line
end if

if ~ defined NOT_THERE
    lea rdi, [text_d]
    call print_line
end if

if (WIDTH = 4) & (HEIGHT <> 9)
    lea rdi, [text_e]
    call print_line
end if

if WIDTH * HEIGHT = 12
    lea rdi, [text_f]
    call print_line
end if

if WIDTH = 4
    if HEIGHT >= 3
        lea rdi, [text_g]
        call print_line
    else
        lea rdi, [text_bad]
        call print_line
    end if
end if

if 0
    this line never reaches the assembler
    include 'no such file at all'
    lea rdi, [text_bad]
    call print_line
else
    lea rdi, [text_h]
    call print_line
end if

if WIDTH < 4 | HEIGHT = 3
    lea rdi, [text_i]
    call print_line
end if

    xor eax, eax
    ret

banner db 'conditionals:', 0
text_a db 'a ok', 0
text_b db 'b ok', 0
text_c db 'c ok', 0
text_d db 'd ok', 0
text_e db 'e ok', 0
text_f db 'f ok', 0
text_g db 'g ok', 0
text_h db 'h ok', 0
text_i db 'i ok', 0
text_bad db 'BAD', 0

include '/lib/library.inc'
