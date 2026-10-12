; context switch helpers. a switch copies the trap frame on the shared kernel stack in and out
; of the process record, plus fxsave / fxrstor for the sse state

FPU_AREA_ADDRESS = 0xAA0000
FPU_STATE_BYTES = 512
FPU_DEFAULT_MXCSR = 0x1F80

fpu_init:
    mov rax, cr0
    and eax, not 4
    or eax, 2                   ; cr0.MP on
    mov cr0, rax
    mov rax, cr4
    or eax, 0x600
    mov cr4, rax
    fninit
    push FPU_DEFAULT_MXCSR
    ldmxcsr [rsp]
    add rsp, 8
    mov rax, FPU_AREA_ADDRESS
    fxsave [rax]
    ret

fpu_state:
    movzx eax, word [process_current_index]
    shl eax, 9
    add rax, FPU_AREA_ADDRESS
    ret

fpu_reset_slot:
    push rcx
    push rsi
    push rdi
    mov edi, eax
    shl edi, 9
    add rdi, FPU_AREA_ADDRESS
    mov rsi, FPU_AREA_ADDRESS
    mov ecx, FPU_STATE_BYTES / 8
    cld
    rep movsq
    pop rdi
    pop rsi
    pop rcx
    ret

process_context_save:
    push rcx
    push rsi
    push rdi

    call fpu_state
    fxsave [rax]

    mov rsi, rdi
    call process_current_record
    lea rdi, [rax + PROCESS_SAVED_FRAME]
    mov ecx, PROCESS_SAVED_FRAME_SIZE / 8
    cld
    rep movsq

    pop rdi
    pop rsi
    pop rcx
    ret

process_context_restore:
    push rcx
    push rsi
    push rdi

    call fpu_state
    fxrstor [rax]

    call process_current_record
    lea rsi, [rax + PROCESS_SAVED_FRAME]
    mov ecx, PROCESS_SAVED_FRAME_SIZE / 8
    cld
    rep movsq

    pop rdi
    pop rsi
    pop rcx
    ret
