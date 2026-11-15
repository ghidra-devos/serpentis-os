; pictures inside the terminal. tg_open asks lds for a block under the text, the program draws
; into the frame it got (width bytes a row, palette 16-63 so themes leave it alone) and calls
; tg_frame to show it. TERMGFX_CLEAR pixels are see through. the last frame stays in the
; scrollback after exit. needs gui.inc (and draw.inc under it) for the event layout

TG_WAIT_TICKS = PIT_FREQUENCY

; edi = width, esi = height. out: rax -> first frame, or a negative SYS_ERR_* when nobody
; answers (piped, or not running in lds) or the terminal says no
tg_open:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13d, esi
    mov edi, FD_STDOUT
    call sys_isatty
    test eax, eax
    jle .absent
    call sys_shm_attach

    lea rdi, [tg_text]
    lea rsi, [tg_open_text]
    call str_copy
    lea rbx, [rdi - 1]
    call sys_getpid
    mov edi, eax
    call tg_put_number
    mov byte [rbx], ';'
    inc rbx
    mov edi, r12d
    call tg_put_number
    mov byte [rbx], ';'
    inc rbx
    mov edi, r13d
    call tg_put_number
    call tg_send

    call sys_getticks
    lea r12, [rax + TG_WAIT_TICKS]
.wait:
    lea rdi, [tg_event]
    mov esi, EVENT_WANT_TICK
    call sys_event
    cmp eax, EVENT_MESSAGE
    jne .clock
    mov eax, [tg_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, TERMGFX_OPENED
    je .opened
    cmp eax, TERMGFX_REFUSED
    je .refused
.clock:
    call sys_getticks
    cmp rax, r12
    jb .wait
.absent:
    mov rax, SYS_ERR_NOT_FOUND
    jmp .done
.refused:
    mov eax, [tg_event + EVENT_PAYLOAD + MSG_A]
    cdqe
    jmp .done
.opened:
    mov eax, [tg_event + EVENT_PAYLOAD + MSG_B]
    add rax, SHM_ADDRESS
    mov [tg_shown], rax
    mov eax, [tg_event + EVENT_PAYLOAD + MSG_A]
    add rax, SHM_ADDRESS
    mov [tg_pixels], rax
    mov dword [tg_sequence], 0
.done:
    pop r13
    pop r12
    pop rbx
    ret

; out: rax -> frame to draw next, negative if the terminal stopped answering
tg_frame:
    push rbx
    push r12
    mov rax, SYS_ERR_INVALID
    cmp qword [tg_pixels], 0
    je .done
    inc dword [tg_sequence]
    lea rdi, [tg_text]
    lea rsi, [tg_frame_text]
    call str_copy
    lea rbx, [rdi - 1]
    mov edi, [tg_sequence]
    call tg_put_number
    call tg_send

    call sys_getticks
    lea r12, [rax + TG_WAIT_TICKS]
.ack:
    mov rax, [tg_shown]
    mov eax, [rax]
    cmp eax, [tg_sequence]
    je .taken
    call sys_yield
    call sys_getticks
    cmp rax, r12
    jb .ack
    mov rax, SYS_ERR_NO_PROCESS
    jmp .done
.taken:
    mov eax, [tg_sequence]
    and eax, 1
    imul eax, eax, TERMGFX_FRAME_BYTES
    add rax, [tg_pixels]
.done:
    pop r12
    pop rbx
    ret

; edi = ticks for a full turn around x, esi = around z, 0 holds that axis still. the frame
; tg_open gave holds a mesh, lds turns it from now on by itself, also after the program ends
tg_spin:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13d, esi
    cmp qword [tg_pixels], 0
    je .done
    lea rdi, [tg_text]
    lea rsi, [tg_spin_text]
    call str_copy
    lea rbx, [rdi - 1]
    mov edi, r12d
    call tg_put_number
    mov byte [rbx], ';'
    inc rbx
    mov edi, r13d
    call tg_put_number
    call tg_send
.done:
    pop r13
    pop r12
    pop rbx
    ret

tg_close:
    push rbx
    cmp qword [tg_pixels], 0
    je .done
    mov qword [tg_pixels], 0
    lea rdi, [tg_text]
    lea rsi, [tg_close_text]
    call str_copy
    lea rbx, [rdi - 1]
    call tg_send
.done:
    pop rbx
    ret

tg_put_number:
    call u64_to_dec
    mov rsi, rax
    mov rdi, rbx
    call str_copy
    lea rbx, [rdi - 1]
    ret

tg_send:
    mov byte [rbx], 27
    mov byte [rbx + 1], '\'
    lea rdx, [rbx + 2]
    lea rsi, [tg_text]
    sub rdx, rsi
    mov edi, FD_STDOUT
    jmp sys_write

tg_open_text db 27, '_Go', 0
tg_frame_text db 27, '_Gf', 0
tg_close_text db 27, '_Gc', 0
tg_spin_text db 27, '_Gs', 0

align 8
tg_pixels dq 0
tg_shown dq 0
tg_sequence dd 0
tg_event:
    times EVENT_BYTES db 0
tg_text:
    times 48 db 0
