; flat gdt for long mode plus the one tss (we only need rsp0 out of it)

gdt_init:
    cli
    lgdt [gdt_descriptor]
    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    xor eax, eax
    mov ax, KERNEL_CODE_SELECTOR
    push rax
    lea rax, [.reload_cs]
    push rax
    retfq

.reload_cs:
    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

tss_init:
    mov qword [tss64_rsp0], TSS_STACK_TOP
    mov word [tss64_iomap], TSS64_BYTES
    mov ax, TSS_SELECTOR
    ltr ax
    ret

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AFFA000000FFFF
    dq 0x00CFF2000000FFFF

gdt_tss_descriptor:
    dw tss64_end - tss64 - 1
    dw tss64 and 0xFFFF
    db (tss64 shr 16) and 0xFF
    db 0x89                     ; present, type 9 = available 64 bit tss
    db ((tss64_end - tss64 - 1) shr 16) and 0x0F
    db (tss64 shr 24) and 0xFF
    dd (tss64 shr 32) and 0xFFFFFFFF
    dd 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

align 16
tss64:
    dd 0
tss64_rsp0:
    dq 0
tss64_rsp1:
    dq 0
tss64_rsp2:
    dq 0
    dq 0
tss64_ist1:
    dq 0
tss64_ist2:
    dq 0
tss64_ist3:
    dq 0
tss64_ist4:
    dq 0
tss64_ist5:
    dq 0
tss64_ist6:
    dq 0
tss64_ist7:
    dq 0
    dq 0
    dw 0
tss64_iomap:
    dw TSS64_BYTES
tss64_end:
