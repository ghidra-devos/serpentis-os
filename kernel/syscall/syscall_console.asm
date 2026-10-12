; full screen console for programs that run without the desktop (editor, lds in console mode):
; they draw whole rows of (char, attr) cells themselves and take the keyboard raw

CON_COLUMNS = VGA_WIDTH / FONT_WIDTH
CON_ROWS = VGA_HEIGHT / FONT_HEIGHT

syscall_con_info:
    mov eax, CON_ROWS
    shl eax, 16
    or eax, CON_COLUMNS
    ret

syscall_con_draw:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    mov r13d, esi
    mov r14, rdx
    mov r15d, ecx

    cmp r12d, CON_COLUMNS
    jae .invalid
    cmp r13d, CON_ROWS
    jae .invalid

    mov eax, CON_COLUMNS
    sub eax, r12d
    cmp r15d, eax
    jbe .count_ready
    mov r15d, eax

.count_ready:
    mov rdi, r14
    mov edx, r15d
    shl edx, 1
    call syscall_user_range_valid
    jc .invalid_address

    mov ax, [process_current_index]
    mov [console_owner], ax

.next:
    test r15d, r15d
    jz .done

    movzx r8d, byte [r14 + 1]
    mov r9d, r8d
    shr r9d, 4
    and r8d, 0x0F
    mov ebx, r12d
    shl ebx, 3
    mov ecx, r13d
    shl ecx, 4
    mov al, [r14]
    call font_draw_char

    add r14, 2
    inc r12d
    dec r15d
    jmp .next

.done:
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

; ui_draw and terminal_redraw use rbx for coordinates. process_exit_switch holds the trap
; frame in rbx when it lands here, so rbx has to survive
syscall_con_restore:
    cmp word [console_owner], 0
    je .done
    push rbx
    mov word [console_owner], 0
    call ui_draw
    call terminal_redraw
    pop rbx
.done:
    xor eax, eax
    ret

syscall_con_release_owner:
    push rax
    movzx eax, word [process_current_index]
    cmp ax, [console_owner]
    jne .done
    call syscall_con_restore
.done:
    pop rax
    ret

syscall_getkey:
    push rbx
    mov rbx, rsi

    call tty_current
    test eax, eax
    jz .keyboard
    call tty_read_key
    jnc .got
    test edi, SYS_GETKEY_NOWAIT
    jnz .none
    jmp .block_only

.keyboard:
    call keyboard_read_key
    jnc .got

    test edi, SYS_GETKEY_NOWAIT
    jnz .none

    movzx eax, word [process_current_index]
    test eax, eax
    jz .none

    mov [stdin_waiter], ax
    call console_show_cursor
.block_only:
    sub qword [rbx + TRAP_RIP], 2
    mov rdi, rbx
    call process_block_current
    jnc .switched
    add qword [rbx + TRAP_RIP], 2
    mov word [stdin_waiter], 0

.none:
    xor eax, eax

.got:
    pop rbx
    clc
    ret

.switched:
    pop rbx
    stc
    ret

console_show_cursor:
    cmp word [console_owner], 0
    jne .done
    push rax
    push rbx
    push rcx
    push rdx
    mov byte [terminal_cursor_phase], 1
    call terminal_cursor_erase
    call terminal_cursor_draw_current
    pop rdx
    pop rcx
    pop rbx
    pop rax
.done:
    ret

console_owner dw 0

syscall_theme:
    cmp edi, THEME_CURRENT
    je .current
    cmp edi, THEME_SET
    je .set
    cmp edi, THEME_NAME
    je .name
    cmp edi, THEME_PALETTE
    je .palette
    jmp .invalid

.current:
    mov eax, [vga_theme_current]
    ret

.set:
    mov edi, esi
    call vga_set_theme
    jc .invalid
    xor eax, eax
    ret

.name:
    cmp esi, THEME_COUNT
    jae .invalid
    imul eax, esi, THEME_NAME_MAX
    lea rsi, [vga_theme_names + rax]
    mov rdi, rdx
    mov edx, THEME_NAME_MAX
    jmp .copy

.palette:
    cmp esi, THEME_COUNT
    jae .invalid
    imul eax, esi, THEME_PALETTE_BYTES
    lea rsi, [vga_themes + rax]
    mov rdi, rdx
    mov edx, THEME_PALETTE_BYTES

.copy:
    call syscall_copy_to_user
    jc .bad_address
    xor eax, eax
    ret

.invalid:
    mov eax, SYS_ERR_INVALID
    ret

.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    ret

; edi = first dac index, esi = count, rdx -> rgb (6 bit) triples. only PALETTE_FREE_FIRST and up,
; ui and art colors are off limits. the desktop puts its wallpaper colors here
syscall_palette:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13d, esi
    mov rbx, rdx
    cmp r12d, PALETTE_FREE_FIRST
    jb .invalid
    test r13d, r13d
    jz .invalid
    lea eax, [r12 + r13]
    cmp eax, 256
    ja .invalid
    lea edx, [r13 * 2 + r13]
    mov rdi, palette_upload
    mov rsi, rbx
    call syscall_copy_from_user
    jc .bad_address
    mov edi, r12d
    mov ecx, r13d
    mov rsi, palette_upload
    call vga_load_range
    xor eax, eax
    jmp .out
.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .out
.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
.out:
    pop r13
    pop r12
    pop rbx
    ret

palette_upload:
    times 256 * 3 db 0
