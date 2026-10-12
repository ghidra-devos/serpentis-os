cmd_ticks:
    call shell_use_output_color

    mov rsi, ticks_label
    call terminal_print

    mov rax, [pit_ticks]
    call terminal_print_u64
    call terminal_newline
    ret

cmd_uptime:
    call shell_use_output_color

    mov rax, [pit_ticks]
    xor edx, edx
    mov rcx, PIT_FREQUENCY
    div rcx

    xor edx, edx
    mov rcx, 86400
    div rcx
    mov r12, rax
    mov rbx, rdx

    mov rax, rbx
    xor edx, edx
    mov rcx, 3600
    div rcx
    mov r13, rax
    mov rbx, rdx

    mov rax, rbx
    xor edx, edx
    mov rcx, 60
    div rcx
    mov r14, rax
    mov r15, rdx

    mov rsi, uptime_label
    call terminal_print

    mov rax, r12
    call terminal_print_u64

    mov rsi, uptime_days_text
    call terminal_print

    mov al, r13b
    call terminal_print_two_digits

    mov al, ':'
    call terminal_putc

    mov al, r14b
    call terminal_print_two_digits

    mov al, ':'
    call terminal_putc

    mov al, r15b
    call terminal_print_two_digits
    call terminal_newline
    ret

cmd_cpuinfo:
    call shell_use_output_color

    mov rsi, cpu_vendor_label
    call terminal_print

    mov rsi, cpu_vendor
    call terminal_print_line

    mov rsi, cpu_brand_label
    call terminal_print

    mov rsi, cpu_brand
    call terminal_print_line

    mov rsi, cpu_basic_label
    call terminal_print

    mov eax, [cpu_max_basic]
    call terminal_print_hex64
    call terminal_newline

    mov rsi, cpu_extended_label
    call terminal_print

    mov eax, [cpu_max_extended]
    call terminal_print_hex64
    call terminal_newline
    ret

cmd_cpuid:
    cmp rdi, 2
    jb .usage

    mov r12, rdi
    mov r13, rsi

    mov rsi, [r13 + 8]
    call parse_u64
    jc .usage

    mov rdx, rax
    shr rdx, 32
    jnz .usage

    mov r14d, eax

    xor r15d, r15d

    cmp r12, 3
    jb .execute

    mov rsi, [r13 + 16]
    call parse_u64
    jc .usage

    mov rdx, rax
    shr rdx, 32
    jnz .usage

    mov r15d, eax

.execute:
    mov eax, r14d
    mov ecx, r15d
    cpuid

    mov [cpuid_result_eax], eax
    mov [cpuid_result_ebx], ebx
    mov [cpuid_result_ecx], ecx
    mov [cpuid_result_edx], edx

    call shell_use_output_color

    mov rsi, cpuid_eax_label
    call terminal_print

    mov eax, [cpuid_result_eax]
    call terminal_print_hex64

    mov rsi, cpuid_ebx_label
    call terminal_print

    mov eax, [cpuid_result_ebx]
    call terminal_print_hex64
    call terminal_newline

    mov rsi, cpuid_ecx_label
    call terminal_print

    mov eax, [cpuid_result_ecx]
    call terminal_print_hex64

    mov rsi, cpuid_edx_label
    call terminal_print

    mov eax, [cpuid_result_edx]
    call terminal_print_hex64
    call terminal_newline
    ret

.usage:
    mov rsi, cpuid_usage
    call shell_error
    ret

cmd_date:
    call rtc_read_datetime
    call shell_use_output_color

    movzx eax, word [rtc_year]
    call terminal_print_u64

    mov al, '-'
    call terminal_putc

    mov al, [rtc_month]
    call terminal_print_two_digits

    mov al, '-'
    call terminal_putc

    mov al, [rtc_day]
    call terminal_print_two_digits

cmd_time:
    call rtc_read_datetime
    call shell_use_output_color

    mov al, [rtc_hour]
    call terminal_print_two_digits

    mov al, ':'
    call terminal_putc

    mov al, [rtc_minute]
    call terminal_print_two_digits

    mov al, ':'
    call terminal_putc

    mov al, [rtc_second]
    call terminal_print_two_digits
    call terminal_newline
    ret

cmd_irqinfo:
    in al, PIC1_DATA
    mov [irq_master_mask], al

    in al, PIC2_DATA
    mov [irq_slave_mask], al

    call shell_use_output_color

    mov rsi, irq_timer_label
    call terminal_print

    mov rax, [pit_ticks]
    call terminal_print_u64
    call terminal_newline

    mov rsi, irq_keyboard_label
    call terminal_print

    mov rax, [keyboard_irq_count]
    call terminal_print_u64
    call terminal_newline

    mov rsi, irq_master_mask_label
    call terminal_print

    movzx eax, byte [irq_master_mask]
    call terminal_print_hex64
    call terminal_newline

    mov rsi, irq_slave_mask_label
    call terminal_print

    movzx eax, byte [irq_slave_mask]
    call terminal_print_hex64
    call terminal_newline
    ret

cmd_reboot:
    mov al, COLOR_YELLOW
    call terminal_set_foreground

    mov rsi, reboot_message
    call terminal_print_line

    cmp byte [fs_ready], 0
    je .reset

    call fs_save_entries
    jc .sync_failed

    cmp byte [ata_present], 0
    je .reset

    call ata_flush
    jc .sync_failed

.reset:
    cli

    call reboot_cf9
    call reboot_8042
    call reboot_triple_fault

.halt:
    hlt
    jmp .halt

.sync_failed:
    mov al, COLOR_LIGHT_RED
    call terminal_set_foreground

    mov rsi, reboot_sync_failed_message
    call terminal_print_line

    mov al, COLOR_WHITE
    call terminal_set_foreground
    ret

; FIXME reboot saves the entries and flushes ata before going down, this doesn't.
; don't shutdown right after writing something
cmd_shutdown:
    mov al, COLOR_YELLOW
    call terminal_set_foreground

    mov rsi, shutdown_message
    call terminal_print_line

    cli

    mov dx, 0x604
    mov ax, 0x2000
    out dx, ax

    mov dx, 0xB004              ; bochs, older qemu
    mov ax, 0x2000
    out dx, ax

    mov dx, 0x4004
    mov ax, 0x3400
    out dx, ax

.halt:
    hlt
    jmp .halt

cmd_halt:
    mov al, COLOR_YELLOW
    call terminal_set_foreground

    mov rsi, halt_message
    call terminal_print_line

    cli

.loop:
    hlt
    jmp .loop

reboot_cf9:
    push rax
    push rdx

    mov dx, REBOOT_CONTROL_PORT
    mov al, 0x02
    out dx, al

    call reboot_delay

    mov al, 0x06
    out dx, al

    call reboot_delay

    pop rdx
    pop rax
    stc
    ret

reboot_8042:
    push rax
    push rcx
    push rdx

    mov ecx, 3

.retry:
    call reboot_wait_input
    jc .next_try

    mov al, 0xAD                ; keyboard off so nothing answers in between
    out KEYBOARD_STATUS_PORT, al

    call reboot_wait_input
    jc .next_try

    mov al, 0xFE
    out KEYBOARD_STATUS_PORT, al

    call reboot_delay

.next_try:
    dec ecx
    jnz .retry

    pop rdx
    pop rcx
    pop rax
    stc
    ret

reboot_wait_input:
    push rcx

    mov ecx, REBOOT_WAIT_LOOPS

.wait:
    in al, KEYBOARD_STATUS_PORT
    test al, 2
    jz .ready

    pause
    dec ecx
    jnz .wait

    pop rcx
    stc
    ret

.ready:
    pop rcx
    clc
    ret

reboot_delay:
    push rcx

    mov ecx, REBOOT_DELAY_LOOPS

.delay:
    pause
    dec ecx
    jnz .delay

    pop rcx
    ret

; empty idt + any exception = triple fault = the cpu resets. the last resort
reboot_triple_fault:
    lidt [reboot_empty_idt]
    ud2

.reboot_halt:
    cli
    hlt
    jmp .reboot_halt

reboot_empty_idt:
    dw 0
    dq 0

command_ticks db 'ticks', 0
command_uptime db 'uptime', 0
command_cpuinfo db 'cpuinfo', 0
command_cpuid db 'cpuid', 0
command_date db 'date', 0
command_time db 'time', 0
command_irqinfo db 'irqinfo', 0
command_reboot db 'reboot', 0
command_shutdown db 'shutdown', 0
command_halt db 'halt', 0

ticks_label db 'Timer ticks: ', 0
uptime_label db 'Uptime: ', 0
uptime_days_text db 'd ', 0
cpu_vendor_label db 'Vendor: ', 0
cpu_brand_label db 'CPU: ', 0
cpu_basic_label db 'Maximum basic CPUID leaf: ', 0
cpu_extended_label db 'Maximum extended CPUID leaf: ', 0
cpuid_usage db 'Usage: cpuid <leaf> [subleaf]', 0
cpuid_eax_label db 'EAX=', 0
cpuid_ebx_label db '  EBX=', 0
cpuid_ecx_label db 'ECX=', 0
cpuid_edx_label db '  EDX=', 0
irq_timer_label db 'IRQ0 timer count: ', 0
irq_keyboard_label db 'IRQ1 keyboard count: ', 0
irq_master_mask_label db 'Master PIC mask: ', 0
irq_slave_mask_label db 'Slave PIC mask: ', 0
reboot_message db 'Rebooting SERPENTIS...', 0
reboot_sync_failed_message db 'Cannot reboot: filesystem synchronization failed.', 0
shutdown_message db 'Shutting down SERPENTIS...', 0
halt_message db 'Processor halted.', 0

cpuid_result_eax dd 0
cpuid_result_ebx dd 0
cpuid_result_ecx dd 0
cpuid_result_edx dd 0
irq_master_mask db 0
irq_slave_mask db 0
cmd_ringinfo:
    push rax
    push rbx
    push rsi

    mov ax, cs
    mov [ringinfo_cs], ax

    str ax
    mov [ringinfo_tr], ax

    mov rsi, ringinfo_cs_text
    call terminal_print

    movzx eax, word [ringinfo_cs]
    call terminal_print_hex64
    call terminal_newline

    mov rsi, ringinfo_cpl_text
    call terminal_print

    movzx eax, word [ringinfo_cs]
    and eax, 3
    call terminal_print_u64
    call terminal_newline

    mov rsi, ringinfo_tr_text
    call terminal_print

    movzx eax, word [ringinfo_tr]
    call terminal_print_hex64
    call terminal_newline

    pop rsi
    pop rbx
    pop rax
    ret

command_ringinfo db 'ringinfo', 0
ringinfo_cs dw 0
ringinfo_tr dw 0
ringinfo_cs_text db 'CS: ', 0
ringinfo_cpl_text db 'CPL: ', 0
ringinfo_tr_text db 'TR: ', 0

cmd_ps:
    call process_print_table
    ret

command_ps db 'ps', 0

cmd_wait:
    push r12
    push r13

    call process_reap_one
    jc .none

    mov r12d, eax
    mov r13d, edx

    mov rsi, wait_reaped_pid_text
    call terminal_print

    mov eax, r12d
    call terminal_print_u64

    mov rsi, wait_reaped_exit_text
    call terminal_print

    mov eax, r13d
    call terminal_print_u64

    call terminal_newline

    pop r13
    pop r12
    ret

.none:
    mov rsi, wait_no_zombie_text
    call terminal_print_line

    pop r13
    pop r12
    ret

cmd_reap:
    push r12

    call process_reap_all

    mov r12d, eax

    mov rsi, reap_count_text
    call terminal_print

    mov eax, r12d
    call terminal_print_u64

    call terminal_newline

    pop r12
    ret

command_wait db 'wait', 0
command_reap db 'reap', 0

wait_reaped_pid_text db 'Reaped PID: ', 0
wait_reaped_exit_text db ' Exit code: ', 0
wait_no_zombie_text db 'No zombie processes.', 0
reap_count_text db 'Reaped processes: ', 0
