include '/lib/start.inc'
include 'greeting.inc'

main:
    lea rdi, [label_text]
    call print
    mov edi, greeting_value
    call print_u64
    mov edi, 10
    call print_char
    xor eax, eax
    ret

label_text db 'searched ', 0

include '/lib/library.inc'
