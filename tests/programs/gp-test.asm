include '../../apps/runtime/program.inc'

main:
    lea rdi, [start_text]
    call print_line
    mov rax, cr3
    ud2

start_text db 'GP ERROR TEST START', 0

include '../../apps/runtime/library.inc'
