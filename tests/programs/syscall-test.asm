include '../../apps/runtime/program.inc'

main:
    call sys_getpid
    lea rdi, [message]
    call print_line
    call sys_sync
    xor eax, eax
    ret

message db 'Syscalls are working.', 0

include '../../apps/runtime/library.inc'
