; process table. slot 0 is the kernel itself, user processes take 1..PROCESS_MAX-1.
; the slot index is what everything inside the kernel uses, the pid is only for userland

process_init:
    push rax
    push rcx
    push rdi

    xor eax, eax
    mov rdi, process_table
    mov ecx, PROCESS_MAX * PROCESS_SIZE
    cld
    rep stosb

    mov word [process_current_index], 0
    mov word [process_next_pid], 1

    mov byte [process_table + PROCESS_STATE], PROCESS_STATE_RUNNING
    mov byte [process_table + PROCESS_FLAGS], PROCESS_FLAG_KERNEL
    mov word [process_table + PROCESS_PID], 0
    mov word [process_table + PROCESS_PARENT_PID], 0
    mov dword [process_table + PROCESS_EXIT_CODE], 0

    mov rax, cr3
    mov [process_table + PROCESS_CR3], rax

    mov rax, kernel_entry
    mov [process_table + PROCESS_ENTRY], rax

    mov rax, rsp
    mov [process_table + PROCESS_KERNEL_RSP], rax

    mov rsi, process_kernel_name
    mov rdi, process_table + PROCESS_NAME
    mov ecx, process_kernel_name_end - process_kernel_name
    cld
    rep movsb

    call process_handles_init_current
    call process_cwd_init_current

    pop rdi
    pop rcx
    pop rax
    ret

process_find_free_slot:
    push rbx
    mov ebx, 1

.scan:
    cmp ebx, PROCESS_MAX
    jae .failed

    mov eax, ebx
    imul eax, PROCESS_SIZE
    cmp byte [process_table + eax + PROCESS_STATE], PROCESS_STATE_FREE
    je .found

    inc ebx
    jmp .scan

.found:
    mov eax, ebx
    call fpu_reset_slot
    pop rbx
    clc
    ret

.failed:
    xor eax, eax
    pop rbx
    stc
    ret

process_finish_current:
    push rax
    push rbx

    movzx eax, word [process_current_index]
    test eax, eax
    jz .restore_kernel

    cmp eax, PROCESS_MAX
    jae .restore_kernel

    mov ebx, eax
    imul ebx, PROCESS_SIZE

    cmp byte [process_table + ebx + PROCESS_STATE], PROCESS_STATE_FREE
    je .restore_kernel

    mov byte [process_table + ebx + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    mov dword [process_table + ebx + PROCESS_EXIT_CODE], edi

.restore_kernel:
    mov word [process_current_index], 0
    mov byte [process_table + PROCESS_STATE], PROCESS_STATE_RUNNING

    pop rbx
    pop rax
    ret

process_register_user:
    push rbx
    push rcx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    call process_find_free_slot
    jc .failed

    mov ebx, eax
    imul eax, PROCESS_SIZE
    lea rdi, [process_table + eax]

    mov byte [rdi + PROCESS_STATE], PROCESS_STATE_RUNNING
    mov byte [rdi + PROCESS_FLAGS], PROCESS_FLAG_USER

    movzx eax, word [process_next_pid]
    mov word [rdi + PROCESS_PID], ax
    inc word [process_next_pid]

    mov word [rdi + PROCESS_PARENT_PID], 0
    mov dword [rdi + PROCESS_EXIT_CODE], 0
    mov byte [rdi + PROCESS_TTY], 0
    mov byte [rdi + PROCESS_CHILD_TTY], 0

    mov rax, r14
    mov [rdi + PROCESS_CR3], rax

    mov rax, r12
    mov [rdi + PROCESS_ENTRY], rax

    mov rax, r13
    mov [rdi + PROCESS_USER_RSP], rax

    mov rax, TSS_STACK_TOP      ; all processes share the one kernel stack, a syscall never sleeps on it
    mov [rdi + PROCESS_KERNEL_RSP], rax

    mov rsi, r15
    lea rdi, [rdi + PROCESS_NAME]
    mov ecx, PROCESS_NAME_SIZE - 1
    cld

.copy_name:
    cmp ecx, 0
    je .terminate_name

    lodsb
    stosb

    test al, al
    jz .registered

    dec ecx
    jmp .copy_name

.terminate_name:
    mov al, 0
    stosb

.registered:
    mov word [process_current_index], bx

    call process_handles_init_current
    call process_cwd_init_current

    mov eax, ebx

    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    clc
    ret

.failed:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    xor eax, eax
    stc
    ret

process_get_current_pid:
    movzx eax, word [process_current_index]
    imul eax, PROCESS_SIZE
    movzx eax, word [process_table + eax + PROCESS_PID]
    ret

process_print_state:
    cmp al, PROCESS_STATE_READY
    je .ready
    cmp al, PROCESS_STATE_RUNNING
    je .running
    cmp al, PROCESS_STATE_BLOCKED
    je .blocked
    cmp al, PROCESS_STATE_ZOMBIE
    je .zombie

    mov rsi, process_free_text
    ret

.ready:
    mov rsi, process_ready_text
    ret

.running:
    mov rsi, process_running_text
    ret

.blocked:
    mov rsi, process_blocked_text
    ret

.zombie:
    mov rsi, process_zombie_text
    ret

process_print_table:
    push rbx
    push rcx
    push r12
    push r13

    call shell_use_output_color

    mov rsi, process_header_text
    call terminal_print_line

    xor ebx, ebx

.loop:
    cmp ebx, PROCESS_MAX
    jae .done

    mov eax, ebx
    imul eax, PROCESS_SIZE
    lea r12, [process_table + rax]

    cmp byte [r12 + PROCESS_STATE], PROCESS_STATE_FREE
    je .next

    mov rsi, process_pid_text
    call terminal_print

    movzx eax, word [r12 + PROCESS_PID]
    call terminal_print_u64

    mov rsi, process_state_text
    call terminal_print

    mov al, [r12 + PROCESS_STATE]
    call process_print_state
    call terminal_print

    mov rsi, process_name_text
    call terminal_print

    lea rsi, [r12 + PROCESS_NAME]
    call terminal_print

    cmp byte [r12 + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    jne .line_end

    mov rsi, process_exit_text
    call terminal_print

    mov eax, [r12 + PROCESS_EXIT_CODE]
    call terminal_print_u64

.line_end:
    call terminal_newline

.next:
    inc ebx
    jmp .loop

.done:
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

; frees the first zombie it finds. out: eax = pid, edx = exit code, CF if there was none
process_reap_one:
    push rbx
    push rcx
    push rdi
    push r12

    mov r12d, 1

.scan:
    cmp r12d, PROCESS_MAX
    jae .none

    mov eax, r12d
    imul eax, PROCESS_SIZE
    lea rbx, [process_table + rax]

    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    jne .next

    movzx eax, word [rbx + PROCESS_PID]
    mov [process_reap_pid], eax

    mov eax, [rbx + PROCESS_EXIT_CODE]
    mov [process_reap_exit_code], eax

    mov rdi, rbx
    xor eax, eax
    mov ecx, PROCESS_SIZE
    cld
    rep stosb

    mov eax, [process_reap_pid]
    mov edx, [process_reap_exit_code]

    pop r12
    pop rdi
    pop rcx
    pop rbx
    clc
    ret

.next:
    inc r12d
    jmp .scan

.none:
    pop r12
    pop rdi
    pop rcx
    pop rbx
    stc
    ret

process_reap_all:
    push rbx
    push r12

    xor r12d, r12d

.loop:
    call process_reap_one
    jc .finish

    inc r12d
    jmp .loop

.finish:
    mov eax, r12d

    pop r12
    pop rbx
    ret

process_reap_pid:
    dd 0

process_reap_exit_code:
    dd 0

process_table:
    times PROCESS_MAX * PROCESS_SIZE db 0

process_current_index dw 0
process_next_pid dw 1

process_kernel_name db 'kernel', 0
process_kernel_name_end:

process_free_text db 'FREE ', 0
process_ready_text db 'READY ', 0
process_running_text db 'RUNNING ', 0
process_blocked_text db 'BLOCKED ', 0
process_zombie_text db 'ZOMBIE ', 0

process_header_text db 'PID      STATE      NAME', 0
process_pid_text db 'PID=', 0
process_state_text db ' STATE=', 0
process_name_text db ' NAME=', 0
process_exit_text db ' EXIT=', 0

process_yield:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    movzx r12d, word [process_current_index]

    test r12d, r12d
    jz .done

    cmp dword [scheduler_ready_count], 0
    je .done

    call scheduler_ready_dequeue
    jc .done
    mov r13d, eax

    cmp r13d, r12d
    je .requeue_and_done

    mov eax, r12d
    imul eax, PROCESS_SIZE
    lea r14, [process_table + rax]

    mov eax, r13d
    imul eax, PROCESS_SIZE
    lea r15, [process_table + rax]

    mov rdi, rbx
    call process_context_save
    mov byte [r14 + PROCESS_STATE], PROCESS_STATE_READY

    mov edi, r12d
    call scheduler_ready_enqueue

    mov word [process_current_index], r13w
    mov byte [r15 + PROCESS_STATE], PROCESS_STATE_RUNNING

    mov rax, [r15 + PROCESS_CR3]
    mov cr3, rax

    mov rdi, rbx
    call process_context_restore

    jmp .done

.requeue_and_done:
    mov edi, r13d
    call scheduler_ready_enqueue

.done:
    mov eax, SYS_OK
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

process_exit_switch:
    push rbx
    push r12
    push r13

    mov rbx, rdi
    movzx r12d, word [process_current_index]

    mov eax, r12d
    imul eax, PROCESS_SIZE
    lea r13, [process_table + rax]

    mov [r13 + PROCESS_EXIT_CODE], esi
    mov byte [r13 + PROCESS_STATE], PROCESS_STATE_ZOMBIE

    call syscall_con_release_owner
    call ipc_release_slot
    call tty_release_current
    call pipe_release_current

    movzx edi, word [r13 + PROCESS_PARENT_PID]
    call process_wake_parent

    mov rdi, [r13 + PROCESS_CR3]
    push rdi
    mov rax, [process_table + PROCESS_CR3]
    mov cr3, rax
    pop rdi
    call address_space_destroy

    call scheduler_ready_dequeue
    jnc .next_ready

    call process_any_blocked
    jc .none_left

    call scheduler_dequeue_wait

.next_ready:
    mov r12d, eax
    imul eax, PROCESS_SIZE
    lea r13, [process_table + rax]

    mov word [process_current_index], r12w
    mov byte [r13 + PROCESS_STATE], PROCESS_STATE_RUNNING

    mov rax, [r13 + PROCESS_CR3]
    mov cr3, rax

    mov rdi, rbx
    call process_context_restore

    pop r13
    pop r12
    pop rbx
    clc
    ret

.none_left:
    mov word [process_current_index], 0
    mov byte [process_table + PROCESS_STATE], PROCESS_STATE_RUNNING

    pop r13
    pop r12
    pop rbx
    stc
    ret

; edi = pid, rdx -> frame, rsi = flags. reaps a zombie child and returns its exit code.
; if the child still runs we block, after rewinding rip over the 2 byte int 0x80 so the
; whole syscall runs again when the child exits and wakes us
process_waitpid:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi

    mov r12d, edi
    mov r15, rdx

    call process_get_current_pid
    mov r13d, eax

    mov r14d, 1

.scan_loop:
    cmp r14d, PROCESS_MAX
    jae .not_found

    mov eax, r14d
    imul eax, PROCESS_SIZE
    lea rbx, [process_table + rax]

    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_FREE
    je .next_slot

    movzx eax, word [rbx + PROCESS_PID]
    cmp eax, r12d
    jne .next_slot

    movzx eax, word [rbx + PROCESS_PARENT_PID]
    cmp eax, r13d
    jne .not_found

    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    jne .child_alive

    mov eax, [rbx + PROCESS_EXIT_CODE]
    push rax
    mov rdi, rbx
    xor eax, eax
    mov ecx, PROCESS_SIZE
    cld
    rep stosb
    pop rax
    jmp .done

.child_alive:
    test dword [rsp], WAIT_NOHANG
    jnz .busy

    sub qword [r15 + TRAP_RIP], 2
    mov rdi, r15
    call process_block_current
    jnc .switched
    add qword [r15 + TRAP_RIP], 2

.busy:
    mov eax, SYS_ERR_BUSY
    jmp .done

.next_slot:
    inc r14d
    jmp .scan_loop

.not_found:
    mov eax, SYS_ERR_NO_PROCESS

.done:
    clc
    jmp .out

.switched:
    stc

.out:
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

process_kill:
    push rbx
    mov ecx, 1
.scan:
    cmp ecx, PROCESS_MAX
    jae .missing
    imul eax, ecx, PROCESS_SIZE
    lea rbx, [process_table + rax]
    mov dl, [rbx + PROCESS_STATE]
    cmp dl, PROCESS_STATE_READY
    jb .next
    cmp dl, PROCESS_STATE_BLOCKED
    ja .next
    cmp [rbx + PROCESS_PID], di
    je .found
.next:
    inc ecx
    jmp .scan
.found:
    or byte [rbx + PROCESS_FLAGS], PROCESS_FLAG_KILLED
    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_BLOCKED
    jne .done
    mov byte [rbx + PROCESS_STATE], PROCESS_STATE_READY
    mov edi, ecx
    call scheduler_ready_enqueue
.done:
    xor eax, eax
    pop rbx
    ret
.missing:
    mov eax, SYS_ERR_NO_PROCESS
    pop rbx
    ret

process_block_current:
    push rbx
    push r12
    push r13

    mov rbx, rdi
    movzx r12d, word [process_current_index]
    test r12d, r12d
    jz .cannot

    mov rdi, rbx
    call process_context_save

    mov eax, r12d
    imul eax, PROCESS_SIZE
    mov byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_BLOCKED

    call scheduler_dequeue_wait
    mov r13d, eax

    mov word [process_current_index], r13w
    mov eax, r13d
    imul eax, PROCESS_SIZE
    lea r13, [process_table + rax]
    mov byte [r13 + PROCESS_STATE], PROCESS_STATE_RUNNING
    mov rax, [r13 + PROCESS_CR3]
    mov cr3, rax

    mov rdi, rbx
    call process_context_restore

    pop r13
    pop r12
    pop rbx
    clc
    ret

.cannot:
    pop r13
    pop r12
    pop rbx
    stc
    ret

process_wake_parent:
    push rbx
    push rcx

    mov ecx, 1

.scan:
    cmp ecx, PROCESS_MAX
    jae .out

    mov eax, ecx
    imul eax, PROCESS_SIZE
    lea rbx, [process_table + rax]

    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_BLOCKED
    jne .next
    movzx eax, word [rbx + PROCESS_PID]
    cmp eax, edi
    jne .next

    mov byte [rbx + PROCESS_STATE], PROCESS_STATE_READY
    push rdi
    mov edi, ecx
    call scheduler_ready_enqueue
    pop rdi
    jmp .out

.next:
    inc ecx
    jmp .scan

.out:
    pop rcx
    pop rbx
    ret

; CF if no process is blocked
process_any_blocked:
    push rax
    push rcx

    mov ecx, 1

.scan:
    cmp ecx, PROCESS_MAX
    jae .none

    mov eax, ecx
    imul eax, PROCESS_SIZE
    cmp byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_BLOCKED
    je .found

    inc ecx
    jmp .scan

.found:
    pop rcx
    pop rax
    clc
    ret

.none:
    pop rcx
    pop rax
    stc
    ret

process_wake_stdin:
    push rax
    push rdi
    movzx edi, word [stdin_waiter]
    test edi, edi
    jz .done
    mov word [stdin_waiter], 0
    call process_wake_slot
.done:
    pop rdi
    pop rax
    ret

stdin_waiter dw 0
