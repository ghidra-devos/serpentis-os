; the desktop's plumbing: per process mailboxes, sys_event, the framebuffer blit and the shared
; memory window the gui server and its clients draw into

MAILBOX_ADDRESS = 0xA30000      ; one mailbox per slot, a 64 entry ring of 72 byte entries
MAILBOX_STRIDE = 0x1400
MAILBOX_HEAD = 0
MAILBOX_COUNT = 4
MAILBOX_ENTRIES = 64
MESSAGE_DAMAGE = 2
MAILBOX_ENTRY_SIZE = 72
MESSAGE_BYTES = 64

EVENT_NONE = 0
EVENT_KEY = 1
EVENT_MOUSE = 2
EVENT_MESSAGE = 3
EVENT_TYPE = 0
EVENT_CODE = 4
EVENT_X = 8
EVENT_Y = 12
EVENT_BUTTONS = 16
EVENT_WHEEL = 20
EVENT_PAYLOAD = 16
EVENT_BYTES = 80
EVENT_TICK = 5
EVENT_WANT_TICK = 2

GUI_NOTIFY_EXIT = 200

SHM_PHYSICAL = SHM_ADDRESS
USER_SHM_ADDRESS = SHM_PHYSICAL
SHM_PAGES = 4
SHM_BYTES = SHM_PAGES * 0x200000
SHM_PD_INDEX = USER_SHM_ADDRESS / 0x200000

ipc_init:
    push rax
    push rcx
    push rdi
    mov rdi, MAILBOX_ADDRESS
    xor eax, eax
    mov ecx, MAILBOX_STRIDE * PROCESS_MAX / 8
    rep stosq
    mov dword [shm_attached_mask], 0
    mov dword [gui_server_pid], 0
    pop rdi
    pop rcx
    pop rax
    ret

mailbox_address:
    imul eax, eax, MAILBOX_STRIDE
    add eax, MAILBOX_ADDRESS
    ret

ipc_release_slot:
    push rax
    push rcx
    push rdi
    movzx eax, word [process_current_index]
    mov ecx, eax
    call mailbox_address
    mov dword [rax + MAILBOX_HEAD], 0
    mov dword [rax + MAILBOX_COUNT], 0
    btr dword [shm_attached_mask], ecx
    jnc .not_client
    call notify_server_exit
.not_client:
    cmp cx, [input_claim_slot]
    jne .server
    cmp dword [input_claim_pid], 0
    je .server
    push rcx
    push rdx
    imul eax, ecx, PROCESS_SIZE
    movzx edi, word [process_table + rax + PROCESS_PARENT_PID]
    call process_slot_of_pid
    pop rdx
    pop rcx
    jc .release
    mov [input_claim_slot], ax
    mov [input_claim_pid], edi
    jmp .server
.release:
    mov dword [input_claim_pid], 0
    cmp dword [gui_server_pid], 0
    jne .server
    mov byte [mouse_gui_mode], 0
.server:
    cmp cx, [gui_server_slot]
    jne .done
    cmp dword [gui_server_pid], 0
    je .done
    mov dword [gui_server_pid], 0
    mov byte [mouse_gui_mode], 0
    cmp dword [input_claim_pid], 0
    je .done
    mov byte [mouse_gui_mode], 1
.done:
    pop rdi
    pop rcx
    pop rax
    ret

notify_server_exit:
    push rbx
    push rcx
    push rdx
    push rdi
    cmp dword [gui_server_pid], 0
    je .done
    movzx eax, word [gui_server_slot]
    cmp ax, [process_current_index]
    je .done
    mov edi, eax
    call mailbox_address
    mov rbx, rax
    mov ecx, [rbx + MAILBOX_COUNT]
    cmp ecx, MAILBOX_ENTRIES
    jae .done
    add ecx, [rbx + MAILBOX_HEAD]
    and ecx, MAILBOX_ENTRIES - 1
    imul ecx, ecx, MAILBOX_ENTRY_SIZE
    lea rdx, [rbx + 16 + rcx]
    mov dword [rdx], 0
    mov dword [rdx + 8], GUI_NOTIFY_EXIT
    call process_get_current_pid
    mov [rdx + 12], eax
    inc dword [rbx + MAILBOX_COUNT]
    call process_wake_slot
.done:
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

process_slot_of_pid:
    mov ecx, 1
.scan:
    cmp ecx, PROCESS_MAX
    jae .missing
    mov eax, ecx
    imul eax, PROCESS_SIZE
    cmp byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_FREE
    je .next
    cmp byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    je .next
    movzx edx, word [process_table + rax + PROCESS_PID]
    cmp edx, edi
    je .found
.next:
    inc ecx
    jmp .scan
.found:
    mov eax, ecx
    clc
    ret
.missing:
    stc
    ret

; edi = slot. makes it ready if it is blocked, anything else is left alone
process_wake_slot:
    push rax
    push rdi
    mov eax, edi
    imul eax, PROCESS_SIZE
    cmp byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_BLOCKED
    jne .done
    mov byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_READY
    call scheduler_ready_enqueue
.done:
    pop rdi
    pop rax
    ret

syscall_send:
    push rbx
    push r12
    push r13

    mov r12, rsi
    call process_slot_of_pid
    jc .missing
    mov r13d, eax

    mov rdi, r12
    mov edx, MESSAGE_BYTES
    call syscall_user_range_valid
    jc .invalid_address

    mov eax, r13d
    call mailbox_address
    mov rbx, rax
    cmp dword [r12], MESSAGE_DAMAGE
    jne .append
    call process_get_current_pid
    mov r8d, eax
    xor edx, edx
.queued:
    cmp edx, [rbx + MAILBOX_COUNT]
    jae .append
    mov eax, [rbx + MAILBOX_HEAD]
    add eax, edx
    and eax, MAILBOX_ENTRIES - 1
    imul eax, eax, MAILBOX_ENTRY_SIZE
    lea rdi, [rbx + 16 + rax]
    inc edx
    cmp [rdi], r8d
    jne .queued
    add rdi, 8
    mov rsi, r12
    mov ecx, MESSAGE_BYTES
    cld
    repe cmpsb
    jne .queued
    mov edi, r13d
    call process_wake_slot
    xor eax, eax
    jmp .out
.append:
    mov ecx, [rbx + MAILBOX_COUNT]
    cmp ecx, MAILBOX_ENTRIES
    jae .full

    add ecx, [rbx + MAILBOX_HEAD]
    and ecx, MAILBOX_ENTRIES - 1
    imul ecx, ecx, MAILBOX_ENTRY_SIZE
    lea rdi, [rbx + 16 + rcx]
    call process_get_current_pid
    mov [rdi], eax
    add rdi, 8
    mov rsi, r12
    mov ecx, MESSAGE_BYTES
    cld
    rep movsb
    inc dword [rbx + MAILBOX_COUNT]

    mov edi, r13d
    call process_wake_slot
    xor eax, eax
    jmp .out

.missing:
    mov eax, SYS_ERR_NO_PROCESS
    jmp .out
.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .out
.full:
    mov eax, SYS_ERR_FULL
.out:
    pop r13
    pop r12
    pop rbx
    ret

syscall_event:
    push rbx
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13d, esi
    mov r14, rdx

    mov edx, EVENT_BYTES
    call syscall_user_range_valid
    jc .invalid_address

    movzx eax, word [process_current_index]
    call mailbox_address
    mov rbx, rax
    cmp dword [rbx + MAILBOX_COUNT], 0
    je .keyboard

    mov ecx, [rbx + MAILBOX_HEAD]
    imul ecx, ecx, MAILBOX_ENTRY_SIZE
    lea rsi, [rbx + 16 + rcx]
    mov dword [r12 + EVENT_TYPE], EVENT_MESSAGE
    mov eax, [rsi]
    mov [r12 + EVENT_CODE], eax
    mov dword [r12 + EVENT_X], 0
    mov dword [r12 + EVENT_Y], 0
    add rsi, 8
    lea rdi, [r12 + EVENT_PAYLOAD]
    mov ecx, MESSAGE_BYTES
    cld
    rep movsb
    mov eax, [rbx + MAILBOX_HEAD]
    inc eax
    and eax, MAILBOX_ENTRIES - 1
    mov [rbx + MAILBOX_HEAD], eax
    dec dword [rbx + MAILBOX_COUNT]
    mov eax, EVENT_MESSAGE
    jmp .done

.keyboard:
    call event_is_input_owner
    jnc .idle

    call keyboard_read_key
    jc .mouse
    mov dword [r12 + EVENT_TYPE], EVENT_KEY
    mov [r12 + EVENT_CODE], eax
    mov eax, EVENT_KEY
    jmp .done

.mouse:
    call mouse_drain_gui
    cmp byte [mouse_event_pending], 0
    je .idle
    mov byte [mouse_event_pending], 0
    mov dword [r12 + EVENT_TYPE], EVENT_MOUSE
    xor eax, eax
    cmp byte [keyboard_super], 0
    setne al
    mov [r12 + EVENT_CODE], eax
    mov eax, [mouse_x]
    mov [r12 + EVENT_X], eax
    mov eax, [mouse_y]
    mov [r12 + EVENT_Y], eax
    movzx eax, byte [mouse_buttons]
    mov [r12 + EVENT_BUTTONS], eax
    xor eax, eax
    xchg eax, [mouse_wheel]
    mov [r12 + EVENT_WHEEL], eax
    mov eax, EVENT_MOUSE
    jmp .done

.idle:
    call tty_event
    jc .quiet
    mov dword [r12 + EVENT_TYPE], EVENT_TTY
    mov [r12 + EVENT_CODE], eax
    mov eax, EVENT_TTY
    jmp .done

; EVENT_WANT_TICK: one event per pit tick, at most. no new tick yet = arm the bit so the
; timer irq wakes us
.quiet:
    test r13d, EVENT_WANT_TICK
    jz .no_tick
    movzx ecx, word [process_current_index]
    mov rax, [pit_ticks]
    cmp rax, [tick_seen + rcx * 8]
    je .arm_tick
    mov [tick_seen + rcx * 8], rax
    mov dword [r12 + EVENT_TYPE], EVENT_TICK
    mov [r12 + EVENT_CODE], eax
    mov eax, EVENT_TICK
    jmp .done
.arm_tick:
    bts dword [tick_waiters], ecx

.no_tick:
    test r13d, SYS_GETKEY_NOWAIT
    jnz .nothing

    movzx eax, word [process_current_index]
    test eax, eax
    jz .nothing
    call event_is_input_owner
    jnc .block
    movzx eax, word [process_current_index]
    mov [stdin_waiter], ax
.block:
    sub qword [r14 + TRAP_RIP], 2
    mov rdi, r14
    call process_block_current
    jnc .switched
    add qword [r14 + TRAP_RIP], 2

.nothing:
    mov dword [r12 + EVENT_TYPE], EVENT_NONE
    xor eax, eax
    jmp .done

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS

.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret

.switched:
    pop r14
    pop r13
    pop r12
    pop rbx
    stc
    ret

mouse_drain_gui:
    mov byte [mouse_buttons_changed], 0
.next:
    call mouse_dequeue
    jc .done
    call mouse_accept_byte
    cmp byte [mouse_buttons_changed], 0
    je .next
.done:
    ret

mouse_process_gui_packet:
    cmp byte [mouse_packet_size], 4
    jne .buttons
    mov cl, [mouse_packet + 3]
    shl cl, 4
    sar cl, 4
    movsx ecx, cl
    test ecx, ecx
    jz .buttons
    add [mouse_wheel], ecx
    mov byte [mouse_event_pending], 1
    mov byte [mouse_buttons_changed], 1
.buttons:
    mov bl, al
    and bl, 7
    cmp bl, [mouse_buttons]
    je .motion
    mov [mouse_buttons], bl
    mov byte [mouse_event_pending], 1
    mov byte [mouse_buttons_changed], 1

.motion:
    movsx eax, byte [mouse_packet + 1]
    mov ecx, [mouse_x]
    add ecx, eax
    jns .x_low
    xor ecx, ecx
.x_low:
    cmp ecx, VGA_WIDTH - 1
    jle .x_ready
    mov ecx, VGA_WIDTH - 1
.x_ready:
    movsx eax, byte [mouse_packet + 2]
    mov edx, [mouse_y]
    sub edx, eax
    jns .y_low
    xor edx, edx
.y_low:
    cmp edx, VGA_HEIGHT - 1
    jle .y_ready
    mov edx, VGA_HEIGHT - 1
.y_ready:
    cmp ecx, [mouse_x]
    jne .moved
    cmp edx, [mouse_y]
    je .done
.moved:
    mov [mouse_x], ecx
    mov [mouse_y], edx
    mov byte [mouse_event_pending], 1
.done:
    ret

syscall_fb_blit:
    call screen_blocked
    jnc .allowed
    xor eax, eax
    ret
.allowed:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi
    mov r13, rsi
    movzx r14d, dx
    shr edx, 16
    mov r15d, edx
    movzx ebx, cx
    shr ecx, 16
    mov [blit_height], ecx
    mov [blit_width], ebx

    test r14d, 7
    jnz .invalid
    test ebx, 7
    jnz .invalid
    test ebx, ebx
    jz .invalid
    test ecx, ecx
    jz .invalid
    lea eax, [r14 + rbx]
    cmp eax, VGA_WIDTH
    ja .invalid
    lea eax, [r15 + rcx]
    cmp eax, VGA_HEIGHT
    ja .invalid

    lea eax, [rcx - 1]
    imul rax, r13
    add rax, rbx
    mov rdx, rax
    mov rdi, r12
    call syscall_user_range_valid
    jc .invalid_address

    mov ax, [process_current_index]
    mov [console_owner], ax

.row:
    mov eax, r15d
    imul eax, FRAME_STRIDE
    add eax, r14d
    mov rdi, [frame_base]
    add rdi, rax
    mov rsi, r12
    mov ecx, [blit_width]
    cld
    rep movsb
    add r12, r13
    inc r15d
    dec dword [blit_height]
    jnz .row

    xor eax, eax
    jmp .out

.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .out
.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
.out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

syscall_shm_attach:
    mov rax, cr3
    and rax, not 0xFFF
    mov rax, [rax]
    and rax, not 0xFFF
    mov rax, [rax]
    and rax, not 0xFFF
    lea rax, [rax + SHM_PD_INDEX * 8]
    mov rdx, SHM_PHYSICAL or 0x87
    mov ecx, SHM_PAGES
.map:
    mov [rax], rdx
    add rax, 8
    add rdx, 0x200000
    dec ecx
    jnz .map
    mov rax, cr3
    mov cr3, rax
    movzx eax, word [process_current_index]
    bts dword [shm_attached_mask], eax
    mov eax, USER_SHM_ADDRESS
    ret

shm_range_valid:
    movzx ecx, word [process_current_index]
    bt dword [shm_attached_mask], ecx
    jnc .no
    cmp rdi, USER_SHM_ADDRESS
    jb .no
    cmp rax, USER_SHM_ADDRESS + SHM_BYTES - 1
    ja .no
    stc
    ret
.no:
    clc
    ret

syscall_service:
    cmp edi, 3
    je .claim
    cmp edi, 2
    je .font
    test edi, edi
    jnz .register
    mov eax, [gui_server_pid]
    test eax, eax
    jnz .done
    mov eax, SYS_ERR_NOT_FOUND
.done:
    ret
.font:
    push rdi
    push rsi
    mov rdi, rsi
    mov edx, 256 * FONT_HEIGHT
    call syscall_user_range_valid
    pop rdi
    pop rax
    jc .font_invalid
    mov rsi, [FONT_POINTER_ADDRESS]
    test rsi, rsi
    jz .font_invalid
    mov ecx, 256 * FONT_HEIGHT
    cld
    rep movsb
    xor eax, eax
    ret
.font_invalid:
    mov eax, SYS_ERR_INVALID
    ret
.claim:
    call process_get_current_pid
    mov [input_claim_pid], eax
    mov ax, [process_current_index]
    mov [input_claim_slot], ax
    mov byte [mouse_gui_mode], 1
    mov byte [mouse_event_pending], 1
    xor eax, eax
    ret
.register:
    call process_get_current_pid
    mov [gui_server_pid], eax
    mov ax, [process_current_index]
    mov [gui_server_slot], ax
    mov byte [mouse_gui_mode], 1
    mov byte [mouse_event_pending], 1
    xor eax, eax
    ret

screen_blocked:
    cmp dword [gui_server_pid], 0
    je .free
    push rax
    movzx eax, word [process_current_index]
    cmp ax, [gui_server_slot]
    pop rax
    je .free
    stc
    ret
.free:
    clc
    ret

; CF if the current process gets keyboard and mouse events: the gui server, or the claimer
; when no gui runs
event_is_input_owner:
    push rax
    movzx eax, word [process_current_index]
    cmp dword [gui_server_pid], 0
    je .claimed
    cmp ax, [gui_server_slot]
    jne .no
    jmp .yes
.claimed:
    cmp dword [input_claim_pid], 0
    je .no
    cmp ax, [input_claim_slot]
    jne .no
.yes:
    pop rax
    stc
    ret
.no:
    pop rax
    clc
    ret

tick_seen:
    times PROCESS_MAX dq 0
tick_waiters dd 0
input_claim_pid dd 0
input_claim_slot dw 0
shm_attached_mask dd 0
gui_server_pid dd 0
gui_server_slot dw 0
blit_width dd 0
blit_height dd 0
mouse_gui_mode db 0
mouse_event_pending db 0
mouse_buttons_changed db 0
