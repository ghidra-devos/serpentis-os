; second stage. still real mode: checks the cpu, reads the kernel header (lba 16) and the
; kernel itself to 0x10000, grabs the bios 8x16 font, then goes 32 bit and 64 bit and jumps in

format binary
use16
org 0x8000

LOADER_BYTES = 15 * 512
KERNEL_HEADER_LBA = 16
KERNEL_PAYLOAD_LBA = 17
KERNEL_HEADER_BUFFER = 0x6000
KERNEL_LOAD_ADDRESS = 0x10000
KERNEL_SEGMENT = 0x1000
KERNEL_MAX_BYTES = 0x80000
KERNEL_MAX_SECTORS = 1024
SECTORS_PER_READ = 64

KH_MAGIC = 0
KH_VERSION = 4
KH_HEADER_SIZE = 6
KH_KERNEL_BYTES = 8
KH_KERNEL_SECTORS = 12
KH_LOAD_ADDRESS = 16
KH_ENTRY_ADDRESS = 20
KH_FILESYSTEM_LBA = 24

KERNEL_MAGIC = 0x34364B53
KERNEL_VERSION = 1
KERNEL_HEADER_SIZE = 32
FILESYSTEM_LBA = 2048

; identity map for the first 1 GB with 2 MB pages, the kernel builds its own tables later
PML4_ADDRESS = 0x100000
PDPT_ADDRESS = 0x101000
PD_ADDRESS = 0x102000

PROTECTED_STACK = 0x200000
LONG_MODE_STACK = 0x300000
FONT_POINTER_ADDRESS = 0x500    ; kernel reads the bios font address from here

CODE32_SELECTOR = 0x08
DATA_SELECTOR = 0x10
CODE64_SELECTOR = 0x18

loader_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    sti

    mov [boot_drive], dl

    call enable_a20
    call check_extensions
    jc loader_failed
    call check_long_mode
    jc loader_failed
    call load_kernel_header
    jc loader_failed
    call validate_kernel_header
    jc loader_failed
    call load_kernel
    jc loader_failed
    call save_vga_font

    mov ax, 0x0012
    int 0x10

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE32_SELECTOR:protected_mode_entry

check_extensions:
    mov bx, 0x55AA
    mov ah, 0x41
    mov dl, [boot_drive]
    int 0x13
    jc .failed

    cmp bx, 0xAA55
    jne .failed

    test cx, 1
    jz .failed

    clc
    ret

.failed:
    stc
    ret

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .failed

    mov eax, 0x80000001
    cpuid
    test edx, 1 shl 29
    jz .failed

    clc
    ret

.failed:
    stc
    ret

load_kernel_header:
    mov byte [read_retries], 3

.retry:
    mov si, header_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc .success

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13

    dec byte [read_retries]
    jnz .retry

    stc
    ret

.success:
    clc
    ret

validate_kernel_header:
    cmp dword [KERNEL_HEADER_BUFFER + KH_MAGIC], KERNEL_MAGIC
    jne .failed

    cmp word [KERNEL_HEADER_BUFFER + KH_VERSION], KERNEL_VERSION
    jne .failed

    cmp word [KERNEL_HEADER_BUFFER + KH_HEADER_SIZE], KERNEL_HEADER_SIZE
    jne .failed

    cmp dword [KERNEL_HEADER_BUFFER + KH_LOAD_ADDRESS], KERNEL_LOAD_ADDRESS
    jne .failed

    cmp dword [KERNEL_HEADER_BUFFER + KH_FILESYSTEM_LBA], FILESYSTEM_LBA
    jne .failed

    mov eax, [KERNEL_HEADER_BUFFER + KH_KERNEL_BYTES]
    test eax, eax
    jz .failed

    cmp eax, KERNEL_MAX_BYTES
    ja .failed

    mov edx, eax
    add edx, 511
    shr edx, 9                  ; bytes -> sectors, must match what the image builder wrote

    cmp edx, [KERNEL_HEADER_BUFFER + KH_KERNEL_SECTORS]
    jne .failed

    test edx, edx
    jz .failed

    cmp edx, KERNEL_MAX_SECTORS
    ja .failed

    mov eax, [KERNEL_HEADER_BUFFER + KH_ENTRY_ADDRESS]
    cmp eax, KERNEL_LOAD_ADDRESS
    jb .failed

    mov edx, [KERNEL_HEADER_BUFFER + KH_KERNEL_BYTES]
    add edx, KERNEL_LOAD_ADDRESS
    jc .failed

    cmp eax, edx
    jae .failed

    clc
    ret

.failed:
    stc
    ret

; reads in 64 sector chunks (32 KB). some bioses cap a call at 127 sectors and a chunk
; must not wrap the 64 KB segment, so 64 at offset 0 is safe
load_kernel:
    mov eax, [KERNEL_HEADER_BUFFER + KH_KERNEL_SECTORS]
    mov [kernel_remaining], ax

    mov word [kernel_segment], KERNEL_SEGMENT
    mov dword [kernel_lba], KERNEL_PAYLOAD_LBA
    mov dword [kernel_lba + 4], 0

.next_chunk:
    mov ax, [kernel_remaining]
    test ax, ax
    jz .success

    cmp ax, SECTORS_PER_READ
    jbe .count_ready

    mov ax, SECTORS_PER_READ

.count_ready:
    mov [kernel_chunk], ax
    mov [kernel_dap_count], ax

    mov ax, [kernel_segment]
    mov [kernel_dap_segment], ax

    mov eax, dword [kernel_lba]
    mov [kernel_dap_lba], eax

    mov eax, dword [kernel_lba + 4]
    mov [kernel_dap_lba + 4], eax

    mov byte [read_retries], 3

.retry:
    mov si, kernel_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc .loaded

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13

    dec byte [read_retries]
    jnz .retry

    stc
    ret

.loaded:
    mov ax, [kernel_chunk]
    sub [kernel_remaining], ax

    movzx eax, word [kernel_chunk]
    add dword [kernel_lba], eax
    adc dword [kernel_lba + 4], 0

    mov ax, [kernel_chunk]
    shl ax, 5
    add [kernel_segment], ax

    jmp .next_chunk

.success:
    clc
    ret

save_vga_font:
    push es
    push bp

    mov ax, 0x1130
    mov bh, 0x06
    int 0x10

    xor eax, eax
    mov ax, es
    shl eax, 4

    xor edx, edx
    mov dx, bp
    add eax, edx

    mov dword [FONT_POINTER_ADDRESS], eax
    mov dword [FONT_POINTER_ADDRESS + 4], 0

    pop bp
    pop es
    ret

enable_a20:
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    ret

loader_failed:
    xor ax, ax
    mov ds, ax
    mov si, loader_error
    call print_string

loader_halt:
    cli
    hlt
    jmp loader_halt

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
kernel_remaining dw 0
kernel_chunk dw 0
kernel_segment dw 0
kernel_lba dq 0

loader_error db 'SERPENTIS loader failure', 0

align 4
header_dap:
    db 0x10
    db 0
    dw 1
    dw KERNEL_HEADER_BUFFER
    dw 0
    dq KERNEL_HEADER_LBA

align 4
kernel_dap:
    db 0x10
    db 0
kernel_dap_count:
    dw 0
kernel_dap_offset:
    dw 0
kernel_dap_segment:
    dw KERNEL_SEGMENT
kernel_dap_lba:
    dq KERNEL_PAYLOAD_LBA

align 8
gdt_start:
    dq 0

gdt_code32:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0

gdt_data:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0

gdt_code64:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 00100000b
    db 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

use32

protected_mode_entry:
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, PROTECTED_STACK

    cld

    mov edi, PML4_ADDRESS
    xor eax, eax
    mov ecx, 0x3000 / 4
    rep stosd

    mov dword [PML4_ADDRESS], PDPT_ADDRESS or 3
    mov dword [PML4_ADDRESS + 4], 0

    mov dword [PDPT_ADDRESS], PD_ADDRESS or 3
    mov dword [PDPT_ADDRESS + 4], 0

    mov edi, PD_ADDRESS
    mov ecx, 512
    mov eax, 0x00000083
    xor edx, edx

.create_pages:
    mov [edi], eax
    mov [edi + 4], edx

    add eax, 0x00200000
    adc edx, 0

    add edi, 8
    dec ecx
    jnz .create_pages

    mov eax, cr4
    or eax, 0x30                ; pae + pse
    mov cr4, eax

    mov ecx, 0xC0000080         ; efer, bit 8 = long mode enable
    rdmsr
    or eax, 0x100
    wrmsr

    mov eax, PML4_ADDRESS
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000          ; paging on, with lme set this is long mode
    mov cr0, eax

    jmp CODE64_SELECTOR:long_mode_entry

use64

long_mode_entry:
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, LONG_MODE_STACK
    xor rbp, rbp

    mov eax, dword [KERNEL_HEADER_BUFFER + KH_ENTRY_ADDRESS]
    jmp rax

times LOADER_BYTES - ($ - $$) db 0
