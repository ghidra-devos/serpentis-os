; 8259 pair. remapped to 32-47 so irqs don't land on the cpu exception vectors

; write to the unused post code port, gives the old pics time to settle between writes
io_wait:
    push rax
    push rdx

    xor eax, eax
    mov dx, 0x80
    out dx, al

    pop rdx
    pop rax
    ret

pic_init:
    push rax

    ; icw1 init, icw2 vector base, icw3 slave on irq 2, icw4 8086 mode
    mov al, 0x11
    out PIC1_COMMAND, al
    call io_wait

    mov al, 0x11
    out PIC2_COMMAND, al
    call io_wait

    mov al, 0x20
    out PIC1_DATA, al
    call io_wait

    mov al, 0x28
    out PIC2_DATA, al
    call io_wait

    mov al, 4
    out PIC1_DATA, al
    call io_wait

    mov al, 2
    out PIC2_DATA, al
    call io_wait

    mov al, 1
    out PIC1_DATA, al
    call io_wait

    mov al, 1
    out PIC2_DATA, al
    call io_wait

    mov al, 11111000b           ; unmask timer, keyboard and the cascade
    out PIC1_DATA, al

    mov al, 11101111b
    out PIC2_DATA, al

    pop rax
    ret
