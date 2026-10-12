include '/lib/start.inc'

macro say text
{
    lea rdi, [text]
    call print_line
}

macro twice text
{
    say text
    say text
}

macro sized name, value
{
    local .start, .stop
    db .stop - .start
.start:
    db name
.stop:
    db value
}

main:
    say banner
    twice echoed

    xor edi, edi
    movzx eax, byte [first]
    add edi, eax
    movzx eax, byte [second]
    add edi, eax
    call print_u64
    mov edi, 10
    call print_char

    lea rsi, [counted]
    mov ecx, counted_end - counted
    xor eax, eax
.sum:
    test ecx, ecx
    jz .shown
    movzx edx, byte [rsi]
    add eax, edx
    inc rsi
    dec ecx
    jmp .sum
.shown:
    mov edi, eax
    call print_u64
    mov edi, 10
    call print_char

    mov edi, pair_end - pair
    call print_u64
    mov edi, 10
    call print_char

if defined pair
    say tail
end if

    xor eax, eax
    ret

banner db 'macros:', 0
tail db 'tail ok', 0
echoed db 'echo', 0

first:
sized 'abc', 9
second:
sized 'de', 4

pair:
sized 'xy', 1
sized 'z', 2
pair_end:

counted:
repeat 3
    db %
end repeat
counted_end:

include '/lib/library.inc'
