keyboard_init:
    mov byte [keyboard_queue_head], 0
    mov byte [keyboard_queue_tail], 0
    mov byte [keyboard_shift], 0
    mov byte [keyboard_control], 0
    mov byte [keyboard_super], 0
    mov byte [keyboard_caps_lock], 0
    mov byte [keyboard_extended], 0
    mov qword [keyboard_irq_count], 0
    ret

keyboard_isr:
    push rax
    push rbx
    push rcx
    push rdx

    in al, KEYBOARD_DATA_PORT
    mov dl, al

    inc qword [keyboard_irq_count]

    mov bl, [keyboard_queue_head]
    mov cl, bl
    inc cl
    and cl, KEYBOARD_QUEUE_SIZE - 1

    cmp cl, [keyboard_queue_tail]
    je .eoi

    movzx ebx, bl
    mov [keyboard_queue + rbx], dl
    mov [keyboard_queue_head], cl

    call process_wake_stdin

.eoi:
    mov al, PIC_EOI
    out PIC1_COMMAND, al

    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; next raw byte or 0. irqs off so the isr can't move head under us
keyboard_dequeue:
    pushfq
    cli

    movzx ecx, byte [keyboard_queue_tail]
    cmp cl, [keyboard_queue_head]
    je .empty

    movzx eax, byte [keyboard_queue + rcx]

    inc cl
    and cl, KEYBOARD_QUEUE_SIZE - 1
    mov [keyboard_queue_tail], cl

    popfq
    ret

.empty:
    xor eax, eax
    popfq
    ret

keyboard_read_char:
    call keyboard_read_key
    jc .done
    cmp eax, 0x100
    jae keyboard_read_char
    clc
.done:
    ret

keyboard_read_key:
    push rbx
    push rcx

.next:
    call keyboard_dequeue
    test eax, eax
    jz .none

    cmp al, 0xE0
    je .prefix

    cmp byte [keyboard_extended], 0
    jne .extended

    test al, 0x80
    jnz .release

    cmp al, 0x2A
    je .shift_down
    cmp al, 0x36
    je .shift_down
    cmp al, 0x1D
    je .control_down
    cmp al, 0x3A
    je .caps

    cmp al, 0x1C                ; enter
    jne .not_enter
    mov bl, 10
    cmp byte [keyboard_shift], 0
    je .emit_bl
    mov eax, KEY_SHIFT_ENTER
    jmp .emit_special
.not_enter:
    mov bl, 8
    cmp al, 0x0E
    je .emit_bl
    mov bl, 9
    cmp al, 0x0F
    jne .function_check
    cmp byte [keyboard_control], 0
    je .emit_bl
    mov eax, 9 or KEY_CTRL
    jmp .regular_ready

.function_check:
    cmp al, 0x3B
    jb .regular
    cmp al, 0x44
    jbe .function_key
    cmp al, 0x57
    je .f11
    cmp al, 0x58
    je .f12

.regular:
    movzx ebx, al
    cmp byte [keyboard_shift], 0
    jne .shifted
    mov bl, [keyboard_base_table + rbx]
    jmp .translated

.shifted:
    mov bl, [keyboard_shift_table + rbx]

.translated:
    test bl, bl
    jz .next

    mov al, bl
    or al, 0x20
    cmp al, 'a'
    jb .control_check
    cmp al, 'z'
    ja .control_check

    cmp byte [keyboard_caps_lock], 0
    je .control_check
    xor bl, 0x20

.control_check:
    cmp byte [keyboard_control], 0
    je .emit_bl
    cmp al, 'a'
    jb .next
    cmp al, 'z'
    ja .next
    and bl, 0x1F

.emit_bl:
    movzx eax, bl
    cmp byte [keyboard_super], 0
    je .regular_ready
    or eax, KEY_SUPER
.regular_ready:
    pop rcx
    pop rbx
    clc
    ret

.function_key:
    movzx eax, al
    sub eax, 0x3B - KEY_F1
    jmp .emit_special

.f11:
    mov eax, KEY_F1 + 10
    jmp .emit_special

.f12:
    mov eax, KEY_F1 + 11
    jmp .emit_special

.prefix:
    mov byte [keyboard_extended], 1
    jmp .next

.extended:
    mov byte [keyboard_extended], 0
    mov bl, al
    and bl, 0x7F
    cmp bl, 0x1D                ; right ctrl, e0 1d
    je .extended_control
    cmp bl, 0x5B
    je .left_super
    cmp bl, 0x5C
    je .right_super
    test al, 0x80
    jnz .next

    lea rcx, [keyboard_extended_map]
.map_scan:
    cmp byte [rcx], 0
    je .next
    cmp [rcx], bl
    je .mapped
    add rcx, 2
    jmp .map_scan

.left_super:
    mov cl, 1
    jmp .super_state

.right_super:
    mov cl, 2

.super_state:
    test al, 0x80
    jnz .super_up
    or [keyboard_super], cl
    jmp .next

.super_up:
    not cl
    and [keyboard_super], cl
    jmp .next

.mapped:
    movzx eax, byte [rcx + 1]
    or eax, 0x100

.emit_special:
    cmp byte [keyboard_control], 0
    je .special_ready
    or eax, KEY_CTRL
.special_ready:
    cmp byte [keyboard_super], 0
    je .special_modifiers_ready
    or eax, KEY_SUPER
.special_modifiers_ready:
    pop rcx
    pop rbx
    clc
    ret

.extended_control:
    test al, 0x80
    jnz .control_up

.control_down:
    mov byte [keyboard_control], 1
    jmp .next

.control_up:
    mov byte [keyboard_control], 0
    jmp .next

.release:
    and al, 0x7F
    cmp al, 0x1D
    je .control_up
    cmp al, 0x2A
    je .shift_up
    cmp al, 0x36
    jne .next

.shift_up:
    mov byte [keyboard_shift], 0
    jmp .next

.shift_down:
    mov byte [keyboard_shift], 1
    jmp .next

.caps:
    xor byte [keyboard_caps_lock], 1
    jmp .next

.none:
    pop rcx
    pop rbx
    stc
    ret

keyboard_extended_map:
    db 0x48, KEY_UP and 0xFF
    db 0x50, KEY_DOWN and 0xFF
    db 0x4B, KEY_LEFT and 0xFF
    db 0x4D, KEY_RIGHT and 0xFF
    db 0x47, KEY_HOME and 0xFF
    db 0x4F, KEY_END and 0xFF
    db 0x49, KEY_PAGE_UP and 0xFF
    db 0x51, KEY_PAGE_DOWN and 0xFF
    db 0x53, KEY_DELETE and 0xFF
    db 0x52, KEY_INSERT and 0xFF
    db 0

keyboard_base_table:
    db 0, 27, '1', '2', '3', '4', '5', '6'
    db '7', '8', '9', '0', '-', '=', 0, 0
    db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'
    db 'o', 'p', '[', ']', 0, 0, 'a', 's'
    db 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'
    db 39, '`', 0, 92, 'z', 'x', 'c', 'v'
    db 'b', 'n', 'm', ',', '.', '/', 0, '*'
    db 0, ' ', 0
    times 128 - ($ - keyboard_base_table) db 0

keyboard_shift_table:
    db 0, 27, '!', '@', '#', '$', '%', '^'
    db '&', '*', '(', ')', '_', '+', 0, 0
    db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'
    db 'O', 'P', '{', '}', 0, 0, 'A', 'S'
    db 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':'
    db 34, '~', 0, 124, 'Z', 'X', 'C', 'V'
    db 'B', 'N', 'M', '<', '>', '?', 0, '*'
    db 0, ' ', 0
    times 128 - ($ - keyboard_shift_table) db 0

keyboard_queue_head db 0
keyboard_queue_tail db 0
keyboard_shift db 0
keyboard_control db 0
keyboard_super db 0
keyboard_caps_lock db 0
keyboard_extended db 0
keyboard_irq_count dq 0

keyboard_queue:
times KEYBOARD_QUEUE_SIZE db 0
