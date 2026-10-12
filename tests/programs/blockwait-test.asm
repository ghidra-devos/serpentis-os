include '../../apps/runtime/program.inc'

main:
    push rbx
    push r12

    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov ebx, eax

    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov r12d, eax

    mov edi, ebx
    xor esi, esi
    call sys_waitpid
    cmp eax, 7
    jne .failed

    mov edi, r12d
    xor esi, esi
    call sys_waitpid
    cmp eax, 7
    jne .failed

    mov edi, r12d
    xor esi, esi
    call sys_waitpid
    cmp eax, SYS_ERR_NO_PROCESS
    jne .failed

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

ok_text db 'BLOCKWAIT OK', 0
fail_text db 'BLOCKWAIT FAIL', 0
child_path db '/bin/busy-child.serp', 0

include '../../apps/runtime/library.inc'
