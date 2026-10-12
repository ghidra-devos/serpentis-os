include '/lib/start.inc'

main:
if defined FLAG
    lea rdi, [label_text]
    call print
    mov edi, FLAG
    call print_u64
    mov edi, 10
    call print_char
else
    lea rdi, [none_text]
    call print_line
end if
    xor eax, eax
    ret

label_text db 'flag ', 0
none_text db 'no flag', 0

include '/lib/library.inc'
