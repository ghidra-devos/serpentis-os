; primary master ata disk. polled, no irqs. bus master dma if the ide controller has it,
; through a 64 KB bounce buffer below 4 GB, otherwise plain pio

ata_init:
    mov byte [ata_present], 0
    mov byte [ata_dma], 0
    mov dx, ATA_DRIVE
    mov al, 0xA0
    out dx, al
    call ata_delay
    call ata_wait_ready
    jc .done
    mov byte [ata_present], 1
    mov dx, ATA_CONTROL
    mov al, ATA_INTERRUPTS_OFF
    out dx, al
    call ata_find_controller
.done:
    ret

ata_find_controller:
    push rbx
    push rcx
    push rdx
    xor ebx, ebx
.function:
    cmp ebx, 32 * 8
    jae .done
    mov ecx, ebx
    shl ecx, 8
    or ecx, 0x80000000
    lea eax, [rcx + 0x08]
    call pci_read
    shr eax, 16
    cmp eax, PCI_CLASS_IDE
    je .found
    inc ebx
    jmp .function
.found:
    lea eax, [rcx + 0x20]
    call pci_read
    test eax, 1
    jz .done
    and eax, 0xFFFC
    mov [ata_bm_base], ax
    lea eax, [rcx + 0x04]
    call pci_read
    or eax, 0x05
    mov edx, eax
    lea eax, [rcx + 0x04]
    call pci_write
    mov byte [ata_dma], 1
.done:
    pop rdx
    pop rcx
    pop rbx
    ret

pci_read:
    push rdx
    mov dx, PCI_ADDRESS_PORT
    out dx, eax
    mov dx, PCI_DATA_PORT
    in eax, dx
    pop rdx
    ret

pci_write:
    push rdx
    push rdx
    mov dx, PCI_ADDRESS_PORT
    out dx, eax
    pop rax
    mov dx, PCI_DATA_PORT
    out dx, eax
    pop rdx
    ret

ata_delay:
    push rax
    push rdx
    mov dx, ATA_STATUS
    in al, dx
    in al, dx
    in al, dx
    in al, dx
    pop rdx
    pop rax
    ret

ata_wait_ready:
    push rcx
    mov ecx, 1000000
.wait:
    mov dx, ATA_STATUS
    in al, dx
    test al, ATA_STATUS_BUSY
    jnz .retry
    test al, ATA_STATUS_ERROR
    jnz .failed
    test al, ATA_STATUS_DF
    jnz .failed
    pop rcx
    clc
    ret
.retry:
    dec ecx
    jnz .wait
.failed:
    pop rcx
    stc
    ret

ata_wait_drq:
    push rcx
    mov ecx, 1000000
.wait:
    mov dx, ATA_STATUS
    in al, dx
    test al, ATA_STATUS_ERROR
    jnz .failed
    test al, ATA_STATUS_DF
    jnz .failed
    test al, ATA_STATUS_BUSY
    jnz .retry
    test al, ATA_STATUS_DRQ
    jz .retry
    pop rcx
    clc
    ret
.retry:
    dec ecx
    jnz .wait
.failed:
    pop rcx
    stc
    ret

ata_select_lba:
    push rax
    push rdx
    push r8
    mov r8d, edi
    mov dx, ATA_DRIVE
    mov eax, r8d
    shr eax, 24
    and al, 0x0F
    or al, 0xE0
    out dx, al
    call ata_delay
    mov dx, ATA_SECTOR_COUNT
    mov al, cl
    out dx, al
    mov dx, ATA_LBA_LOW
    mov eax, r8d
    out dx, al
    mov dx, ATA_LBA_MID
    mov eax, r8d
    shr eax, 8
    out dx, al
    mov dx, ATA_LBA_HIGH
    mov eax, r8d
    shr eax, 16
    out dx, al
    pop r8
    pop rdx
    pop rax
    ret

ata_read_sector:
    push rcx
    mov ecx, 1
    call ata_read_sectors
    pop rcx
    ret

ata_write_sector:
    push rcx
    mov ecx, 1
    call ata_write_sectors
    pop rcx
    ret

; edi = lba, ecx = sectors, rsi = buffer. CF on error
ata_read_sectors:
    push rbx
    xor ebx, ebx
    call ata_transfer
    pop rbx
    ret

ata_write_sectors:
    push rbx
    mov ebx, 1
    call ata_transfer
    pop rbx
    ret

ata_transfer:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov r12d, edi
    mov r13d, ecx
    mov r14, rsi
.chunk:
    test r13d, r13d
    jz .flush
    mov ecx, r13d
    cmp ecx, ATA_DMA_SECTORS
    jbe .sized
    mov ecx, ATA_DMA_SECTORS
.sized:
    mov edi, r12d
    mov rsi, r14
    cmp byte [ata_dma], 0
    je .pio
    call ata_dma_chunk
    jmp .moved
.pio:
    call ata_pio_chunk
.moved:
    jc .failed
    add r12d, ecx
    sub r13d, ecx
    mov eax, ecx
    shl rax, 9
    add r14, rax
    jmp .chunk
.flush:
    test ebx, ebx
    jz .success
    call ata_flush
    jc .failed
.success:
    clc
.failed:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

; one sector per command. slow but it works on anything
ata_pio_chunk:
    push rcx
    push r8
    push r9
    mov r8d, ecx
    xor r9d, r9d
.sector:
    cmp r9d, r8d
    jae .done
    call ata_wait_ready
    jc .out
    push rcx
    mov ecx, 1
    call ata_select_lba
    pop rcx
    mov dx, ATA_COMMAND
    mov al, ATA_CMD_READ
    test ebx, ebx
    jz .command
    mov al, ATA_CMD_WRITE
.command:
    out dx, al
    call ata_wait_drq
    jc .out
    mov dx, ATA_DATA
    mov ecx, 256
    cld
    test ebx, ebx
    jnz .write
    xchg rdi, rsi               ; insw wants the buffer in rdi, edi was the lba
    rep insw
    xchg rdi, rsi
    jmp .sent
.write:
    rep outsw
.sent:
    call ata_wait_ready
    jc .out
    inc edi
    inc r9d
    jmp .sector
.done:
    clc
.out:
    pop r9
    pop r8
    pop rcx
    ret

ata_dma_chunk:
    push rcx
    push rsi
    push rdi
    push r8
    mov r8d, ecx
    shl r8d, 9
    test ebx, ebx
    jz .table
    push rdi
    mov rdi, ATA_DMA_BUFFER
    mov ecx, r8d
    cld
    rep movsb
    pop rdi
.table:
    mov dword [ATA_PRD_TABLE], ATA_DMA_BUFFER
    mov eax, r8d
    and eax, 0xFFFF
    or eax, 0x80000000
    mov dword [ATA_PRD_TABLE + 4], eax
    mov dx, [ata_bm_base]
    xor eax, eax
    out dx, al
    add dx, ATA_BM_PRD
    mov eax, ATA_PRD_TABLE
    out dx, eax
    mov dx, [ata_bm_base]
    add dx, ATA_BM_STATUS
    mov al, ATA_BM_ERROR or ATA_BM_DONE
    out dx, al
    call ata_wait_ready
    jc .out
    mov ecx, r8d
    shr ecx, 9
    call ata_select_lba
    mov dx, ATA_COMMAND
    mov al, ATA_CMD_READ_DMA
    mov ah, ATA_BM_START or ATA_BM_TO_MEMORY
    test ebx, ebx
    jz .command
    mov al, ATA_CMD_WRITE_DMA
    mov ah, ATA_BM_START
.command:
    out dx, al
    mov dx, [ata_bm_base]
    mov al, ah
    out dx, al
    mov ecx, 50000000           ; give up eventually, a stuck transfer should fail not hang
.poll:
    mov dx, [ata_bm_base]
    add dx, ATA_BM_STATUS
    in al, dx
    test al, ATA_BM_ERROR
    jnz .stop_failed
    test al, ATA_BM_ACTIVE
    jz .stop
    dec ecx
    jnz .poll
.stop_failed:
    mov dx, [ata_bm_base]
    xor eax, eax
    out dx, al
    stc
    jmp .out
.stop:
    mov dx, [ata_bm_base]
    xor eax, eax
    out dx, al
    mov dx, ATA_STATUS
    in al, dx
    test al, ATA_STATUS_ERROR or ATA_STATUS_DF
    jnz .failed
    call ata_wait_ready
    jc .out
    test ebx, ebx
    jnz .done
    mov rdi, [rsp + 16]
    mov rsi, ATA_DMA_BUFFER
    mov ecx, r8d
    cld
    rep movsb
.done:
    clc
    jmp .out
.failed:
    stc
.out:
    pop r8
    pop rdi
    pop rsi
    pop rcx
    ret

ata_flush:
    mov dx, ATA_COMMAND
    mov al, ATA_CMD_CACHE_FLUSH
    out dx, al
    jmp ata_wait_ready

ata_present db 0
ata_dma db 0
ata_bm_base dw 0
