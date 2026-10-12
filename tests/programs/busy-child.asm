include '../../apps/runtime/program.inc'

main:
    mov ecx, 0x4000000
.spin:
    dec ecx
    jnz .spin
    lea rdi, [done_text]
    call print_line
    mov eax, 7
    ret

done_text db 'BUSY CHILD DONE', 0

include '../../apps/runtime/library.inc'
