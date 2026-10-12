format binary
use16
org 0x7C00

LOADER_ADDRESS = 0x8000
LOADER_SECTORS = 15

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    sti
    mov [boot_drive], dl
    ; int 13h extensions there? bx comes back swapped and cx bit 0 means ah=42h works
    mov bx, 0x55AA
    mov ah, 0x41
    mov dl, [boot_drive]
    int 0x13
    jc boot_failed
    cmp bx, 0xAA55
    jne boot_failed
    test cx, 1
    jz boot_failed
    mov byte [read_retries], 3

read_loader:
    mov si, loader_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc loader_ready
    xor ah, ah                  ; reset the disk and try again, real drives need this sometimes
    mov dl, [boot_drive]
    int 0x13
    dec byte [read_retries]
    jnz read_loader

boot_failed:
    mov si, boot_error
    call print_string

boot_halt:
    cli
    hlt
    jmp boot_halt

loader_ready:
    mov dl, [boot_drive]
    jmp 0x0000:LOADER_ADDRESS

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x000C
    int 0x10
    jmp print_string
.done:
    ret

boot_drive db 0
read_retries db 0
boot_error db 'SERPENTIS boot failure', 0

align 4
loader_dap:
    db 0x10
    db 0
    dw LOADER_SECTORS
    dw LOADER_ADDRESS
    dw 0
    dq 1

times 510 - ($ - $$) db 0
dw 0xAA55
