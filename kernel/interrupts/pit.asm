pit_init:
    mov qword [pit_ticks], 0

    mov al, 0x36                ; channel 0, low then high byte, mode 3
    out PIT_COMMAND, al

    mov ax, PIT_DIVISOR
    out PIT_CHANNEL0, al

    mov al, ah
    out PIT_CHANNEL0, al

    ret

pit_isr:
    cld
    push 0
    push TRAP_VECTOR_IRQ0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call pit_frame_dispatch

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq

pit_frame_dispatch:
    inc qword [pit_ticks]

    mov al, PIC_EOI
    out PIC1_COMMAND, al

    xor eax, eax
    xchg eax, [tick_waiters]
    test eax, eax
    jz .preempt
    push rdi
    push rcx
    mov ecx, eax
.wake:
    bsf edi, ecx
    btr ecx, edi
    call process_wake_slot
    test ecx, ecx
    jnz .wake
    pop rcx
    pop rdi

.preempt:
    call speaker_service
    test byte [rdi + TRAP_CS], 3     ; only ring 3 gets preempted, a syscall runs to its end
    jz .done

    dec dword [scheduler_quantum_left]
    jnz .done

    mov dword [scheduler_quantum_left], SCHEDULER_QUANTUM
    call process_yield

.done:
    ret

pit_ticks dq 0
