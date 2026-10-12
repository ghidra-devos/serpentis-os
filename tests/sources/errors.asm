include '/lib/start.inc'

main:
    mov eax, [rbx + rsp*2]
    mov al, 1000
    frobnicate eax
    jmp missing_label
    mov eax,
    ret

include '/lib/library.inc'
