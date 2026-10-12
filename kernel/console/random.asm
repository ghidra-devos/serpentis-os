random_init:
    rdtsc
    shl rdx, 32
    or rax, rdx

    mov rbx, 0x9E3779B97F4A7C15  ; 2^64 / golden ratio, spreads the low tsc bits around
    xor rax, rbx

    test rax, rax
    jnz .store

    mov rax, 0xA5A5A5A55A5A5A5A

.store:
    mov [random_state], rax
    ret

random_next:
    mov rax, [random_state]

    mov rdx, rax
    shl rdx, 13
    xor rax, rdx

    mov rdx, rax
    shr rdx, 7
    xor rax, rdx

    mov rdx, rax
    shl rdx, 17
    xor rax, rdx

    mov [random_state], rax
    ret

random_state dq 0
