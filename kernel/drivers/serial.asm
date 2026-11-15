SERIAL_PORT = 0x3F8


serial_init:
    push rax
    push rdx

    mov dx, SERIAL_PORT + 1
    mov al, 0x00
    out dx, al

    mov dx, SERIAL_PORT + 3
    mov al, 0x80
    out dx, al

    mov dx, SERIAL_PORT + 0
    mov al, 0x03                ; dlab is on, divisor 3 = 38400 baud
    out dx, al

    mov dx, SERIAL_PORT + 1
    mov al, 0x00
    out dx, al

    mov dx, SERIAL_PORT + 3
    mov al, 0x03                ; 8 bits, no parity, 1 stop, dlab off
    out dx, al

    mov dx, SERIAL_PORT + 2
    mov al, 0xC7
    out dx, al

    mov dx, SERIAL_PORT + 4
    mov al, 0x0B
    out dx, al

    pop rdx
    pop rax
    ret

serial_putc:
    push rax
    push rdx

    mov ah, al

.wait:
    mov dx, SERIAL_PORT + 5
    in al, dx
    test al, 0x20
    jz .wait

    mov dx, SERIAL_PORT
    mov al, ah
    out dx, al

    pop rdx
    pop rax
    ret

