; 256 interrupt gates. 0-31 cpu exceptions, 32-47 the two pics after remap, 0x80 syscalls

idt_init:
    xor ecx, ecx

.fill_all:
    mov eax, ecx
    mov rdx, interrupt_ignore
    call idt_set_gate

    inc ecx
    cmp ecx, 256
    jb .fill_all

    xor ecx, ecx

.fill_exceptions:
    mov eax, ecx
    mov rdx, [exception_handler_table + rcx * 8]
    call idt_set_gate

    inc ecx
    cmp ecx, 32
    jb .fill_exceptions

    mov ecx, 32

.fill_master_irqs:
    mov eax, ecx
    mov rdx, irq_master_default
    call idt_set_gate

    inc ecx
    cmp ecx, 40
    jb .fill_master_irqs

    mov ecx, 40

.fill_slave_irqs:
    mov eax, ecx
    mov rdx, irq_slave_default
    call idt_set_gate

    inc ecx
    cmp ecx, 48
    jb .fill_slave_irqs

    mov eax, 32
    mov rdx, pit_isr
    call idt_set_gate

    mov eax, 33
    mov rdx, keyboard_isr
    call idt_set_gate

    mov eax, 44
    mov rdx, mouse_isr
    call idt_set_gate

    mov eax, 0x80               ; int 0x80 has to be callable from ring 3
    mov rdx, syscall_entry
    call idt_set_user_gate

    lidt [idt_descriptor]
    ret

idt_set_gate:
    push rax
    push rbx
    push rcx

    mov ecx, eax
    shl rcx, 4

    mov rbx, idt_table
    add rbx, rcx

    mov word [rbx], dx
    mov word [rbx + 2], KERNEL_CODE_SELECTOR
    mov byte [rbx + 4], 0
    mov byte [rbx + 5], 0x8E

    mov rax, rdx
    shr rax, 16
    mov word [rbx + 6], ax

    shr rax, 16
    mov dword [rbx + 8], eax

    mov dword [rbx + 12], 0

    pop rcx
    pop rbx
    pop rax
    ret

idt_set_user_gate:
    push rax
    push rbx
    push rcx
    push r8

    mov r8d, eax
    call idt_set_gate

    mov eax, r8d
    mov ecx, eax
    shl rcx, 4
    mov rbx, idt_table
    add rbx, rcx

    mov byte [rbx + 5], 0xEE

    pop r8
    pop rcx
    pop rbx
    pop rax
    ret

interrupt_ignore:
    iretq

irq_master_default:
    push rax

    mov al, PIC_EOI
    out PIC1_COMMAND, al

    pop rax
    iretq

irq_slave_default:
    push rax

    mov al, PIC_EOI
    out PIC2_COMMAND, al
    out PIC1_COMMAND, al

    pop rax
    iretq

align 16
idt_table:
times 256 * 16 db 0
idt_table_end:

idt_descriptor:
dw idt_table_end - idt_table - 1
dq idt_table
