; cpuid vendor and brand strings for cpuinfo / neofetch

cpu_init:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8

    xor eax, eax
    cpuid

    mov [cpu_max_basic], eax

    mov dword [cpu_vendor], ebx
    mov dword [cpu_vendor + 4], edx
    mov dword [cpu_vendor + 8], ecx
    mov byte [cpu_vendor + 12], 0

    mov eax, 0x80000000
    cpuid

    mov [cpu_max_extended], eax

    cmp eax, 0x80000004
    jb .unknown_brand

    mov rdi, cpu_brand
    mov r8d, 0x80000002

.brand_loop:
    mov eax, r8d
    cpuid

    mov dword [rdi], eax
    mov dword [rdi + 4], ebx
    mov dword [rdi + 8], ecx
    mov dword [rdi + 12], edx

    add rdi, 16
    inc r8d
    cmp r8d, 0x80000005
    jb .brand_loop

    mov byte [cpu_brand + 48], 0
    jmp .done

.unknown_brand:
    mov rsi, cpu_unknown
    mov rdi, cpu_brand
    mov ecx, 12
    cld
    rep movsb

.done:
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

cpu_max_basic dd 0
cpu_max_extended dd 0
cpu_vendor: times 13 db 0
cpu_brand: times 49 db 0
cpu_unknown db 'Unknown CPU', 0
