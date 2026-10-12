; ps/2 mouse on the 8042 aux port. bytes are queued by the irq and turned into packets by
; whoever drains them: the gui through sys_event, or the console below for its own cursor

; 8042 status bit 1 = input buffer full, we may write only when it is clear. CF on timeout
mouse_wait_input_clear:
    push rcx

    mov ecx, 100000

.wait:
    in al, KEYBOARD_STATUS_PORT
    test al, 2
    jz .ready

    dec ecx
    jnz .wait

    pop rcx
    stc
    ret

.ready:
    pop rcx
    clc
    ret

mouse_wait_output_full:
    push rcx

    mov ecx, 100000

.wait:
    in al, KEYBOARD_STATUS_PORT
    test al, 1
    jnz .ready

    dec ecx
    jnz .wait

    pop rcx
    stc
    ret

.ready:
    pop rcx
    clc
    ret

; eat whatever the controller still has buffered from the bios
mouse_flush_output:
    push rax
    push rcx

    mov ecx, 64

.next:
    in al, KEYBOARD_STATUS_PORT
    test al, 1
    jz .done

    in al, KEYBOARD_DATA_PORT

    dec ecx
    jnz .next

.done:
    pop rcx
    pop rax
    ret

mouse_write_device:
    push rbx
    push rcx

    mov bl, al
    mov ecx, 3

.retry:
    call mouse_wait_input_clear
    jc .next_retry

    mov al, 0xD4
    out KEYBOARD_STATUS_PORT, al

    call mouse_wait_input_clear
    jc .next_retry

    mov al, bl
    out KEYBOARD_DATA_PORT, al

    call mouse_wait_output_full
    jc .next_retry

    in al, KEYBOARD_DATA_PORT
    cmp al, 0xFA
    je .success

.next_retry:
    dec ecx
    jnz .retry

    pop rcx
    pop rbx
    stc
    ret

.success:
    pop rcx
    pop rbx
    clc
    ret

mouse_init:
    mov byte [mouse_queue_head], 0
    mov byte [mouse_queue_tail], 0
    mov byte [mouse_packet_index], 0
    mov byte [mouse_packet_size], 3
    mov dword [mouse_wheel], 0
    mov byte [mouse_buttons], 0
    mov byte [mouse_device_active], 0
    mov byte [mouse_cursor_enabled], 0
    mov byte [mouse_cursor_drawn], 0
    mov byte [mouse_cursor_dirty], 1
    mov qword [mouse_irq_count], 0
    mov dword [mouse_x], 320
    mov dword [mouse_y], 240
    mov dword [mouse_drawn_x], 320
    mov dword [mouse_drawn_y], 240

    call mouse_flush_output

    call mouse_wait_input_clear
    jc .failed

    mov al, 0xA8                ; 8042: enable aux port
    out KEYBOARD_STATUS_PORT, al

    call mouse_wait_input_clear
    jc .failed

    mov al, 0x20
    out KEYBOARD_STATUS_PORT, al

    call mouse_wait_output_full
    jc .failed

    in al, KEYBOARD_DATA_PORT
    mov bl, al

    or bl, 2
    and bl, 0xDF

    call mouse_wait_input_clear
    jc .failed

    mov al, 0x60
    out KEYBOARD_STATUS_PORT, al

    call mouse_wait_input_clear
    jc .failed

    mov al, bl
    out KEYBOARD_DATA_PORT, al

    mov al, 0xF6
    call mouse_write_device
    jc .failed

    call mouse_enable_wheel

    mov al, 0xF4                ; mouse: start streaming
    call mouse_write_device
    jc .failed

    mov byte [mouse_device_active], 1
    mov byte [mouse_cursor_enabled], 1
    mov byte [mouse_cursor_dirty], 1

    clc
    ret

.failed:
    mov byte [mouse_device_active], 0
    mov byte [mouse_cursor_enabled], 0

    stc
    ret

mouse_enable_wheel:
    push rsi
    lea rsi, [mouse_wheel_rates]
    mov ecx, 3
.rate:
    push rcx
    mov al, 0xF3
    call mouse_write_device
    jc .refused
    mov al, [rsi]
    call mouse_write_device
    jc .refused
    pop rcx
    inc rsi
    dec ecx
    jnz .rate

    mov al, 0xF2
    call mouse_write_device
    jc .done
    call mouse_wait_output_full
    jc .done
    in al, KEYBOARD_DATA_PORT
    cmp al, 3
    jne .done
    mov byte [mouse_packet_size], 4
.done:
    pop rsi
    ret
.refused:
    pop rcx
    pop rsi
    ret

mouse_isr:
    push rax
    push rbx
    push rcx
    push rdx

    in al, KEYBOARD_DATA_PORT
    mov dl, al

    inc qword [mouse_irq_count]

    mov bl, [mouse_queue_head]
    mov cl, bl
    inc cl
    and cl, MOUSE_QUEUE_SIZE - 1

    cmp cl, [mouse_queue_tail]
    je .eoi

    movzx ebx, bl
    mov [mouse_queue + rbx], dl
    mov [mouse_queue_head], cl

    cmp byte [mouse_gui_mode], 0
    je .eoi
    call process_wake_stdin

.eoi:
    mov al, PIC_EOI
    out PIC2_COMMAND, al
    out PIC1_COMMAND, al

    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

mouse_dequeue:
    pushfq
    cli

    movzx ecx, byte [mouse_queue_tail]
    cmp cl, [mouse_queue_head]
    je .empty

    movzx eax, byte [mouse_queue + rcx]

    inc cl
    and cl, MOUSE_QUEUE_SIZE - 1
    mov [mouse_queue_tail], cl

    popfq
    clc
    ret

.empty:
    xor eax, eax
    popfq
    stc
    ret

; first byte of a packet always has bit 3 set, so a lost byte can't shift us forever
mouse_accept_byte:
    movzx ebx, byte [mouse_packet_index]

    test ebx, ebx
    jnz .store

    test al, 8
    jz .done

.store:
    mov [mouse_packet + rbx], al

    inc bl
    mov [mouse_packet_index], bl

    cmp bl, [mouse_packet_size]
    jb .done

    mov byte [mouse_packet_index], 0

    call mouse_process_packet

.done:
    ret

mouse_process_packet:
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9

    mov al, [mouse_packet]
    test al, 0xC0               ; x/y overflow bits, the deltas are junk
    jnz .done

    cmp byte [mouse_gui_mode], 0
    je .classic
    call mouse_process_gui_packet
    jmp .done

.classic:

    mov r8b, [mouse_buttons]

    mov bl, al
    and bl, 7
    mov [mouse_buttons], bl

    movsx eax, byte [mouse_packet + 1]
    mov ecx, [mouse_x]
    add ecx, eax

    cmp ecx, TERMINAL_X
    jge .x_min_ready

    mov ecx, TERMINAL_X

.x_min_ready:
    cmp ecx, TERMINAL_X + TERMINAL_WIDTH - 1
    jle .x_ready

    mov ecx, TERMINAL_X + TERMINAL_WIDTH - 1

.x_ready:
    movsx eax, byte [mouse_packet + 2]
    mov edx, [mouse_y]
    sub edx, eax

    cmp edx, TERMINAL_Y
    jge .y_min_ready

    mov edx, TERMINAL_Y

.y_min_ready:
    cmp edx, TERMINAL_Y + TERMINAL_HEIGHT - 1
    jle .y_ready

    mov edx, TERMINAL_Y + TERMINAL_HEIGHT - 1

.y_ready:
    cmp ecx, [mouse_x]
    jne .moved

    cmp edx, [mouse_y]
    je .buttons

.moved:
    mov [mouse_x], ecx
    mov [mouse_y], edx

    mov byte [mouse_cursor_dirty], 1

.buttons:
    test bl, 1
    jz .done

    test r8b, 1
    jnz .done

    call mouse_place_text_cursor

.done:
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

mouse_place_text_cursor:
    push rax
    push rbx
    push rcx
    push rdx

    mov eax, [mouse_x]
    sub eax, TERMINAL_X

    xor edx, edx
    mov ecx, FONT_WIDTH
    div ecx

    mov ebx, eax

    mov eax, [mouse_y]
    sub eax, TERMINAL_Y

    xor edx, edx
    mov ecx, FONT_HEIGHT
    div ecx

    xchg eax, ebx

    call document_set_cursor_screen

    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

mouse_set_enabled:
    cmp byte [terminal_capture], 0
    je .live
    ret
.live:
    push rbx

    mov bl, al
    call mouse_cursor_erase

    mov al, bl
    and al, 1
    mov [mouse_cursor_enabled], al

    mov byte [mouse_cursor_dirty], 1

    pop rbx
    ret

mouse_cursor_erase:
    cmp byte [mouse_cursor_drawn], 0
    je .done

    push rax
    push rbx
    push rcx
    push rdx
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov byte [mouse_cursor_drawn], 0

    mov eax, [mouse_drawn_x]
    sub eax, TERMINAL_X

    xor edx, edx
    mov ecx, FONT_WIDTH
    div ecx

    mov r12d, eax

    mov eax, [mouse_drawn_x]
    add eax, MOUSE_CURSOR_WIDTH - 1
    sub eax, TERMINAL_X

    xor edx, edx
    mov ecx, FONT_WIDTH
    div ecx

    mov r13d, eax

    cmp r13d, TERMINAL_COLUMNS - 1
    jbe .column_ready

    mov r13d, TERMINAL_COLUMNS - 1

.column_ready:
    mov eax, [mouse_drawn_y]
    sub eax, TERMINAL_Y

    xor edx, edx
    mov ecx, FONT_HEIGHT
    div ecx

    mov r14d, eax

    mov eax, [mouse_drawn_y]
    add eax, MOUSE_CURSOR_HEIGHT - 1
    sub eax, TERMINAL_Y

    xor edx, edx
    mov ecx, FONT_HEIGHT
    div ecx

    mov r15d, eax

    cmp r15d, TERMINAL_ROWS - 1
    jbe .row_ready

    mov r15d, TERMINAL_ROWS - 1

.row_ready:
    mov r10d, r14d

.redraw_row:
    cmp r10d, r15d
    ja .cursor_check

    mov r11d, r12d

.redraw_column:
    cmp r11d, r13d
    ja .next_redraw_row

    mov eax, r11d
    mov ebx, r10d
    call terminal_draw_cell
    inc r11d
    jmp .redraw_column

.next_redraw_row:
    inc r10d
    jmp .redraw_row

.cursor_check:
    cmp byte [terminal_cursor_drawn], 0
    je .restore

    movzx eax, word [terminal_cursor_drawn_line]
    movzx ebx, word [document_view_top]

    cmp eax, ebx
    jb .restore

    sub eax, ebx

    cmp eax, r14d
    jb .restore

    cmp eax, r15d
    ja .restore

    movzx eax, word [terminal_cursor_drawn_column]

    cmp eax, r12d
    jb .restore

    cmp eax, r13d
    ja .restore

    mov byte [terminal_cursor_drawn], 0
    mov byte [terminal_cursor_dirty], 1

.restore:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop rdx
    pop rcx
    pop rbx
    pop rax

.done:
    ret

mouse_device_active db 0
mouse_cursor_enabled db 0
mouse_cursor_drawn db 0
mouse_cursor_dirty db 1
mouse_packet_index db 0
mouse_packet_size db 3
mouse_buttons db 0
mouse_queue_head db 0
mouse_queue_tail db 0

mouse_x dd 320
mouse_y dd 240
mouse_drawn_x dd 320
mouse_drawn_y dd 240

mouse_irq_count dq 0
mouse_wheel dd 0

mouse_wheel_rates db 200, 100, 80

mouse_packet:
times 4 db 0

mouse_queue:
times MOUSE_QUEUE_SIZE db 0
