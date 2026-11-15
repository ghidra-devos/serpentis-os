include '../../apps/runtime/program.inc'

main:
    lea rdi, [message]
    call print_line
    mov eax, 42
    ret

message db 'CHILD RUNNING', 0

include '../../apps/runtime/library.inc'
