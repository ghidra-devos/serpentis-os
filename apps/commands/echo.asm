include '../runtime/program.inc'

main:
    test rdi, rdi
    jz .empty
    call print_line
    xor eax, eax
    ret

.empty:
    mov edi, 10
    call print_char
    xor eax, eax
    ret

include '../runtime/library.inc'
