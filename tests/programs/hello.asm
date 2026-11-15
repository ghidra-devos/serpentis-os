include '../../apps/runtime/program.inc'

main:
    lea rdi, [greeting]
    call print_line
    mov eax, 5
    ret

greeting db 'Hello from /bin/hello.serp', 0

include '../../apps/runtime/library.inc'
