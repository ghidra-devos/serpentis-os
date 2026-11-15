include '../../apps/runtime/program.inc'

SEQUENTIAL = 100
CONCURRENT = 20

main:
    push rbx

    mov ebx, SEQUENTIAL
.sequential:
    call spawn_child
    js .failed
    mov edi, eax
    xor esi, esi
    call sys_waitpid
    cmp eax, 42
    jne .failed
    dec ebx
    jnz .sequential

    lea rdi, [sequential_text]
    call print_line

    xor ebx, ebx
.spawn_many:
    call spawn_child
    js .failed
    mov [children + rbx * 4], eax
    inc ebx
    cmp ebx, CONCURRENT
    jb .spawn_many

    xor ebx, ebx
.reap_many:
    mov edi, [children + rbx * 4]
    xor esi, esi
    call sys_waitpid
    cmp eax, 42
    jne .failed
    inc ebx
    cmp ebx, CONCURRENT
    jb .reap_many

    lea rdi, [concurrent_text]
    call print_line

    lea rdi, [fault_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    js .failed
    mov edi, eax
    xor esi, esi
    call sys_waitpid
    cmp eax, 0x8000000D
    jne .failed

    lea rdi, [ok_text]
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

spawn_child:
    lea rdi, [child_path]
    xor esi, esi
    call sys_spawn
    test eax, eax
    ret

sequential_text db 'STRESS SEQUENTIAL OK', 0
concurrent_text db 'STRESS CONCURRENT OK', 0
ok_text db 'STRESS OK', 0
fail_text db 'STRESS FAIL', 0
child_path db '/bin/child-test.serp', 0
fault_path db '/bin/fault-child.serp', 0

include '../../apps/runtime/library.inc'

children rd CONCURRENT
