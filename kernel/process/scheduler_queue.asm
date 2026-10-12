SCHEDULER_QUEUE_EMPTY = 0xFFFFFFFF

SCHEDULER_QUANTUM = 2           ; ticks a process runs before the timer moves on

scheduler_init:
    mov dword [scheduler_ready_head], 0
    mov dword [scheduler_ready_tail], 0
    mov dword [scheduler_ready_count], 0
    mov dword [scheduler_quantum_left], SCHEDULER_QUANTUM
    mov rdi, scheduler_ready_queue
    mov eax, SCHEDULER_QUEUE_EMPTY
    mov ecx, PROCESS_MAX
    cld
    rep stosd
    ret

scheduler_ready_contains:
    push rbx
    push rcx
    push rdx
    mov ebx, edi
    xor ecx, ecx

.scan:
    cmp ecx, [scheduler_ready_count]
    jae .not_found
    mov edx, [scheduler_ready_head]
    add edx, ecx
    cmp edx, PROCESS_MAX
    jb .index_ready
    sub edx, PROCESS_MAX

.index_ready:
    cmp [scheduler_ready_queue + rdx * 4], ebx
    je .found
    inc ecx
    jmp .scan

.found:
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.not_found:
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

scheduler_ready_enqueue:
    push rax
    push rbx
    push rcx
    mov ebx, edi

    cmp ebx, PROCESS_MAX
    jae .failed

    call scheduler_ready_contains
    jnc .already_present

    cmp dword [scheduler_ready_count], PROCESS_MAX
    jae .failed

    mov ecx, [scheduler_ready_tail]
    mov [scheduler_ready_queue + rcx * 4], ebx

    inc ecx
    cmp ecx, PROCESS_MAX
    jb .tail_ready
    xor ecx, ecx

.tail_ready:
    mov [scheduler_ready_tail], ecx
    inc dword [scheduler_ready_count]

.already_present:
    pop rcx
    pop rbx
    pop rax
    clc
    ret

.failed:
    pop rcx
    pop rbx
    pop rax
    stc
    ret

scheduler_ready_dequeue:
    push rbx
    push rcx

    cmp dword [scheduler_ready_count], 0
    je .empty

    mov ecx, [scheduler_ready_head]
    mov eax, [scheduler_ready_queue + rcx * 4]
    mov dword [scheduler_ready_queue + rcx * 4], SCHEDULER_QUEUE_EMPTY

    inc ecx
    cmp ecx, PROCESS_MAX
    jb .head_ready
    xor ecx, ecx

.head_ready:
    mov [scheduler_ready_head], ecx
    dec dword [scheduler_ready_count]

    pop rcx
    pop rbx
    clc
    ret

.empty:
    mov eax, SCHEDULER_QUEUE_EMPTY
    pop rcx
    pop rbx
    stc
    ret

scheduler_dequeue_wait:
    call scheduler_ready_dequeue
    jnc .done
    sti
    hlt
    cli
    jmp scheduler_dequeue_wait

.done:
    ret

align 4
scheduler_ready_head dd 0
scheduler_ready_tail dd 0
scheduler_ready_count dd 0
scheduler_quantum_left dd SCHEDULER_QUANTUM

align 4
scheduler_ready_queue:
    times PROCESS_MAX dd SCHEDULER_QUEUE_EMPTY
