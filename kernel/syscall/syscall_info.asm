SYSINFO_MEMORY_TOTAL = 0
SYSINFO_MEMORY_USED = 8
SYSINFO_TICKS = 16
SYSINFO_PROCESSES = 24
SYSINFO_BYTES = 32
LOW_MEMORY_BYTES = 0x1000000

syscall_sysinfo:
    push rbx
    push r12
    mov r12, rdi

    mov al, 0x35                ; cmos 0x34/0x35: ram above 16 MB in 64 KB units, bochs and qemu fill it in
    call rtc_read_register
    movzx ebx, al
    shl ebx, 8
    mov al, 0x34
    call rtc_read_register
    mov bl, al
    shl rbx, 16
    add rbx, LOW_MEMORY_BYTES
    mov [sysinfo_buffer + SYSINFO_MEMORY_TOTAL], rbx

    mov eax, [as_next_page]
    sub eax, AS_POOL_START
    mov ecx, [as_free_count]
    shl ecx, 12
    sub eax, ecx
    add rax, LOW_MEMORY_BYTES
    mov [sysinfo_buffer + SYSINFO_MEMORY_USED], rax

    mov rax, [pit_ticks]
    mov [sysinfo_buffer + SYSINFO_TICKS], rax

    xor eax, eax
    xor ecx, ecx
.count:
    cmp ecx, PROCESS_MAX
    jae .counted
    imul edx, ecx, PROCESS_SIZE
    mov bl, [process_table + rdx + PROCESS_STATE]
    cmp bl, PROCESS_STATE_READY
    jb .next
    cmp bl, PROCESS_STATE_BLOCKED
    ja .next
    inc eax
.next:
    inc ecx
    jmp .count
.counted:
    mov [sysinfo_buffer + SYSINFO_PROCESSES], rax

    mov rdi, r12
    lea rsi, [sysinfo_buffer]
    mov edx, SYSINFO_BYTES
    call syscall_copy_to_user
    mov eax, SYS_OK
    jnc .out
    mov eax, SYS_ERR_INVALID_ADDRESS
.out:
    pop r12
    pop rbx
    ret

align 8
sysinfo_buffer:
    times SYSINFO_BYTES db 0
