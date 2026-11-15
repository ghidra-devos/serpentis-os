include '../../apps/runtime/program.inc'

main:
    push rbx
    push r12

    lea rdi, [start_text]
    call print_line

    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov r12d, eax

    mov ebx, 6
.yield:
    call sys_yield
    dec ebx
    jnz .yield

    mov edi, r12d
    xor esi, esi
    call sys_waitpid
    test eax, eax
    js .failed

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    jmp .out

.failed:
    lea rdi, [fail_text]
    call print_line
    xor eax, eax

.out:
    pop r12
    pop rbx
    ret

start_text db 'PARENT START', 0
ok_text db 'REAP OK', 0
fail_text db 'REAP FAIL', 0
child_path db '/bin/child-test.serp', 0

include '../../apps/runtime/library.inc'
