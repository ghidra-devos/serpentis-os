include '../../apps/runtime/program.inc'

main:
    mov eax, 0x2000000
.spin:
    dec eax
    jnz .spin
    mov eax, 9
    ret

include '../../apps/runtime/library.inc'
