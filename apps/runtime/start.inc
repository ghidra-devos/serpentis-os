include 'defs.inc'

_start:
    call main
    mov edi, eax
    call sys_exit
