include '../../apps/runtime/program.inc'

main:
    push rbx

    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov ebx, eax

.poll:
    call sys_yield
    mov edi, ebx
    mov esi, WAIT_NOHANG
    call sys_waitpid
    cmp eax, SYS_ERR_BUSY
    je .poll
    cmp eax, 9
    jne .failed

    lea rdi, [ok_text]
    jmp .report
.failed:
    lea rdi, [fail_text]
.report:
    call print_line
    xor eax, eax
    pop rbx
    ret

ok_text db 'RAX PRESERVED OK', 0
fail_text db 'RAX PRESERVED FAIL', 0
child_path db '/bin/rax-child.serp', 0

include '../../apps/runtime/library.inc'
