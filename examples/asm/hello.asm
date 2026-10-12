include '/lib/start.inc'

main:
    lea rdi, [message]
    call print_line
    xor eax, eax
    ret

message db 'Hello from a program assembled inside Serpentis', 0

include '/lib/library.inc'
