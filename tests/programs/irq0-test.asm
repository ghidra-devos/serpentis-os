include '../../apps/runtime/program.inc'

main:
    push rbx
    push r12

    call sys_getticks
    mov rbx, rax
    mov r12d, 8

.window:
    mov ecx, 0x1000000
.delay:
    pause
    dec ecx
    jnz .delay

    call sys_getticks
    sub rax, rbx
    cmp rax, 2
    jae .ticking
    dec r12d
    jnz .window

    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    jmp .out

.ticking:
    lea rdi, [ok_text]
    call print_line
    xor eax, eax

.out:
    pop r12
    pop rbx
    ret

ok_text db 'IRQ0 RING3 OK', 0
fail_text db 'IRQ0 RING3 FAIL', 0

include '../../apps/runtime/library.inc'
