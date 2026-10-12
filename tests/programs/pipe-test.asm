include '../../apps/runtime/program.inc'

main:
    push rbx
    push r12
    push r13

    lea rdi, [fds]
    call sys_pipe
    test eax, eax
    js .failed

    mov edi, [fds + 4]
    lea rsi, [sample]
    mov edx, sample_end - sample
    call sys_write
    cmp eax, sample_end - sample
    jne .failed

    mov edi, [fds]
    lea rsi, [buffer]
    mov edx, 64
    call sys_read
    cmp eax, sample_end - sample
    jne .failed

    lea rdi, [buffer]
    lea rsi, [sample]
    mov ecx, sample_end - sample
    call compare
    jc .failed

    lea rdi, [self_text]
    call print_line

    mov edi, [fds + 4]
    call sys_close

    mov edi, [fds]
    lea rsi, [buffer]
    mov edx, 64
    call sys_read
    test eax, eax
    jnz .failed

    mov edi, [fds]
    call sys_close

    lea rdi, [eof_text]
    call print_line

    lea rdi, [fds]
    call sys_pipe
    test eax, eax
    js .failed

    mov edi, [fds + 4]
    mov esi, FD_STDOUT
    call sys_dup2

    mov edi, [fds + 4]
    call sys_close

    lea rdi, [child_path]
    lea rsi, [child_args]
    call sys_spawn
    test eax, eax
    js .restore_failed
    mov r12d, eax

    mov edi, FD_STDOUT
    call sys_close

    xor ebx, ebx
.drain:
    cmp ebx, 64
    jae .reaped
    mov edi, [fds]
    lea rsi, [buffer]
    add rsi, rbx
    mov edx, 64
    sub edx, ebx
    call sys_read
    test eax, eax
    jle .reaped
    add ebx, eax
    jmp .drain

.reaped:
    mov edi, [fds]
    call sys_close

    mov edi, r12d
    xor esi, esi
    call sys_waitpid

    cmp ebx, child_text_end - child_text
    jne .failed

    lea rdi, [buffer]
    lea rsi, [child_text]
    mov ecx, child_text_end - child_text
    call compare
    jc .failed

    lea rdi, [child_pipe_text]
    call print_line

    lea rdi, [done_text]
    call print_line

    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret

.restore_failed:
    mov edi, FD_STDOUT
    call sys_close

.failed:
    lea rdi, [failed_text]
    call print_line
    mov eax, 1
    pop r13
    pop r12
    pop rbx
    ret

compare:
    test ecx, ecx
    jz .same
    mov al, [rdi]
    cmp al, [rsi]
    jne .different
    inc rdi
    inc rsi
    dec ecx
    jmp compare
.same:
    clc
    ret
.different:
    stc
    ret

sample db 'PIPE SAMPLE', 10
sample_end:
child_text db 'piped hello', 10
child_text_end:
child_path db '/bin/echo.serp', 0
child_args db 'piped hello', 0
self_text db 'PIPE SELF OK', 0
eof_text db 'PIPE EOF OK', 0
child_pipe_text db 'PIPE CHILD OK', 0
done_text db 'PIPE OK', 0
failed_text db 'PIPE FAILED', 0

include '../../apps/runtime/library.inc'

align 8
fds rd 2
buffer rb 64
