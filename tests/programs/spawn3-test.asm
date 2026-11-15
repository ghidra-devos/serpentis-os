include '../../apps/runtime/program.inc'

CHILDREN = 3

main:
    push rbx
    push r12

    lea rdi, [start_text]
    call print_line

    xor ebx, ebx
.spawn:
    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov [children + rbx * 4], eax
    inc ebx
    cmp ebx, CHILDREN
    jb .spawn

    mov ebx, 6
.yield:
    call sys_yield
    dec ebx
    jnz .yield

    xor ebx, ebx
.reap:
    mov edi, [children + rbx * 4]
    xor esi, esi
    call sys_waitpid
    cmp eax, 42
    jne .failed
    inc ebx
    cmp ebx, CHILDREN
    jb .reap

    lea rdi, [ok_text]
    jmp .report
.failed:
    lea rdi, [fail_text]
.report:
    call print_line
    xor eax, eax
    pop r12
    pop rbx
    ret

start_text db 'PARENT3 START', 0
ok_text db 'REAP3 OK', 0
fail_text db 'REAP3 FAIL', 0
child_path db '/bin/child-test.serp', 0

include '../../apps/runtime/library.inc'

children rd CHILDREN
