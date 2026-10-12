include '../../apps/runtime/program.inc'

main:
    push rbx
    lea rdi, [before_text]
    call print_line
    mov ebx, 8
.yield:
    call sys_yield
    test eax, eax
    js .failed
    dec ebx
    jnz .yield
    lea rdi, [after_text]
    call print_line
    xor eax, eax
    pop rbx
    ret
.failed:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

before_text db 'YIELD BEFORE', 0
after_text db 'YIELD AFTER', 0
fail_text db 'YIELD FAIL', 0

include '../../apps/runtime/library.inc'
