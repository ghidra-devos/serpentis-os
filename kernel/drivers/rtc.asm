; cmos real time clock. qemu keeps it in utc

; wait while status a bit 7 (update in progress) is set, the registers are garbage then
rtc_wait_ready:
    push rax
    push rcx
    push rdx

    mov ecx, 100000

.wait:
    mov al, 0x0A
    mov dx, 0x70
    out dx, al

    mov dx, 0x71
    in al, dx

    test al, 0x80
    jz .done

    dec ecx
    jnz .wait

.done:
    pop rdx
    pop rcx
    pop rax
    ret

rtc_read_register:
    push rdx

    mov dx, 0x70
    out dx, al

    mov dx, 0x71
    in al, dx

    pop rdx
    ret

rtc_bcd_to_binary:
    push rbx

    mov bl, al
    and eax, 0x0F

    and bl, 0xF0
    shr bl, 4
    movzx ebx, bl

    imul ebx, 10
    add eax, ebx

    pop rbx
    ret

rtc_read_datetime:
.retry:
    call rtc_wait_ready

    mov al, 0x00
    call rtc_read_register
    mov [rtc_second], al
    mov r8b, al

    mov al, 0x02
    call rtc_read_register
    mov [rtc_minute], al

    mov al, 0x04
    call rtc_read_register
    mov [rtc_hour], al
    mov r9b, al

    mov al, 0x07
    call rtc_read_register
    mov [rtc_day], al

    mov al, 0x08
    call rtc_read_register
    mov [rtc_month], al

    mov al, 0x09
    call rtc_read_register
    mov [rtc_year_byte], al

    mov al, 0x0B                ; status b: bit 1 = 24h, bit 2 = binary instead of bcd
    call rtc_read_register
    mov [rtc_status_b], al

    mov al, 0x00
    call rtc_read_register
    cmp al, r8b
    jne .retry

    test byte [rtc_status_b], 0x04
    jnz .binary_ready

    mov al, [rtc_second]
    call rtc_bcd_to_binary
    mov [rtc_second], al

    mov al, [rtc_minute]
    call rtc_bcd_to_binary
    mov [rtc_minute], al

    mov al, r9b
    and al, 0x7F
    call rtc_bcd_to_binary
    mov [rtc_hour], al

    mov al, [rtc_day]
    call rtc_bcd_to_binary
    mov [rtc_day], al

    mov al, [rtc_month]
    call rtc_bcd_to_binary
    mov [rtc_month], al

    mov al, [rtc_year_byte]
    call rtc_bcd_to_binary
    mov [rtc_year_byte], al

    jmp .hour_mode

.binary_ready:
    and byte [rtc_hour], 0x7F

.hour_mode:
    test byte [rtc_status_b], 0x02
    jnz .year

    cmp byte [rtc_hour], 12
    jne .check_pm

    mov byte [rtc_hour], 0

.check_pm:
    test r9b, 0x80
    jz .year

    add byte [rtc_hour], 12

.year:
    movzx eax, byte [rtc_year_byte]
    add eax, 2000
    mov [rtc_year], ax

    ret

rtc_second db 0
rtc_minute db 0
rtc_hour db 0
rtc_day db 0
rtc_month db 0
rtc_year_byte db 0
rtc_status_b db 0
rtc_year dw 0
