cmd_palette:
    xor r12d, r12d

.loop:
    cmp r12d, 16
    jae .done

    mov al, r12b
    test r12d, r12d
    jnz .color
    mov al, COLOR_LIGHT_GRAY
.color:
    call terminal_set_foreground

    mov rax, r12
    call terminal_print_u64

    mov al, ' '
    call terminal_putc

    mov rsi, [palette_name_table + r12 * 8]
    call terminal_print_line

    inc r12d
    jmp .loop

.done:
    jmp shell_use_output_color

cmd_rand:
    mov r12, rdi
    mov r13, rsi

    call random_next

    cmp r12, 2
    jb .print

    mov r14, rax

    mov rsi, [r13 + 8]
    call parse_u64
    jc .usage

    test rax, rax
    jz .usage

    mov rbx, rax
    mov rax, r14
    xor edx, edx
    div rbx
    mov rax, rdx

.print:
    call shell_use_output_color
    call terminal_print_u64
    call terminal_newline
    ret

.usage:
    mov rsi, rand_usage
    call shell_error
    ret

cmd_hex:
    cmp rdi, 2
    jb .usage

    mov rsi, [rsi + 8]
    call parse_u64
    jc .usage

    call shell_use_output_color
    call terminal_print_hex64
    call terminal_newline
    ret

.usage:
    mov rsi, hex_usage
    call shell_error
    ret

cmd_dec:
    cmp rdi, 2
    jb .usage

    mov rsi, [rsi + 8]
    call parse_u64
    jc .usage

    call shell_use_output_color
    call terminal_print_u64
    call terminal_newline
    ret

.usage:
    mov rsi, dec_usage
    call shell_error
    ret

cmd_beep:
    mov r12, rdi
    mov r13, rsi

    mov r14d, 800
    mov r15d, 150

    cmp r12, 2
    jb .start

    mov rsi, [r13 + 8]
    call parse_u64
    jc .usage

    cmp rax, 20
    jb .usage

    cmp rax, 20000
    ja .usage

    mov r14d, eax

    cmp r12, 3
    jb .start

    mov rsi, [r13 + 16]
    call parse_u64
    jc .usage

    cmp rax, 1
    jb .usage

    cmp rax, 5000
    ja .usage

    mov r15d, eax

.start:
    mov eax, r14d
    mov ebx, r15d
    call speaker_beep
    jc .usage

    call shell_use_output_color

    mov rsi, beep_started_text
    call terminal_print_line
    ret

.usage:
    mov rsi, beep_usage
    call shell_error
    ret

command_palette db 'palette', 0
command_rand db 'rand', 0
command_hex db 'hex', 0
command_dec db 'dec', 0
command_beep db 'beep', 0

rand_usage db 'Usage: rand [maximum]', 0
hex_usage db 'Usage: hex <decimal-or-hex-number>', 0
dec_usage db 'Usage: dec <decimal-or-hex-number>', 0
beep_usage db 'Usage: beep [20-20000] [1-5000ms]', 0
beep_started_text db 'Tone started.', 0

palette_name_0 db 'Black', 0
palette_name_1 db 'Blue', 0
palette_name_2 db 'Green', 0
palette_name_3 db 'Cyan', 0
palette_name_4 db 'Red', 0
palette_name_5 db 'Magenta', 0
palette_name_6 db 'Brown', 0
palette_name_7 db 'Light Gray', 0
palette_name_8 db 'Dark Gray', 0
palette_name_9 db 'Light Blue', 0
palette_name_10 db 'Light Green', 0
palette_name_11 db 'Light Cyan', 0
palette_name_12 db 'Light Red', 0
palette_name_13 db 'Light Magenta', 0
palette_name_14 db 'Yellow', 0
palette_name_15 db 'White', 0

palette_name_table:
    dq palette_name_0
    dq palette_name_1
    dq palette_name_2
    dq palette_name_3
    dq palette_name_4
    dq palette_name_5
    dq palette_name_6
    dq palette_name_7
    dq palette_name_8
    dq palette_name_9
    dq palette_name_10
    dq palette_name_11
    dq palette_name_12
    dq palette_name_13
    dq palette_name_14
    dq palette_name_15
; mouse on/off for the text cursor in the console
cmd_mouse:
    cmp rdi, 1
    je .status

    cmp rdi, 2
    jne .usage

    mov rsi, [rsi + 8]

    mov rdi, mouse_on_word
    call str_equal
    test eax, eax
    jnz .enable

    mov rdi, mouse_off_word
    call str_equal
    test eax, eax
    jnz .disable

    jmp .usage

.enable:
    cmp byte [mouse_device_active], 0
    je .unavailable

    mov al, 1
    call mouse_set_enabled

    mov byte [mouse_cursor_dirty], 1

    mov rsi, mouse_enabled_text
    call terminal_print_line
    ret

.disable:
    xor al, al
    call mouse_set_enabled

    mov byte [mouse_cursor_dirty], 1

    mov rsi, mouse_disabled_text
    call terminal_print_line
    ret

.status:
    cmp byte [mouse_device_active], 0
    je .unavailable

    cmp byte [mouse_cursor_enabled], 0
    je .show_disabled

    mov rsi, mouse_enabled_text
    call terminal_print_line
    ret

.show_disabled:
    mov rsi, mouse_disabled_text
    call terminal_print_line
    ret

.unavailable:
    mov rsi, mouse_unavailable_text
    call shell_error
    ret

.usage:
    mov rsi, mouse_usage_text
    call shell_error
    ret

cmd_mousepos:
    cmp byte [mouse_device_active], 0
    je .unavailable

    mov rsi, mouse_x_text
    call terminal_print

    movzx eax, word [mouse_x]
    call terminal_print_u64

    mov rsi, mouse_y_text
    call terminal_print

    movzx eax, word [mouse_y]
    call terminal_print_u64

    mov rsi, mouse_buttons_text
    call terminal_print

    movzx eax, byte [mouse_buttons]
    call terminal_print_u64

    call terminal_newline
    ret

.unavailable:
    mov rsi, mouse_unavailable_text
    call shell_error
    ret

command_mouse db "mouse", 0
command_mousepos db "mousepos", 0

mouse_on_word db "on", 0
mouse_off_word db "off", 0

mouse_usage_text db "Usage: mouse [on|off]", 0
mouse_enabled_text db "Mouse cursor enabled", 0
mouse_disabled_text db "Mouse cursor disabled", 0
mouse_unavailable_text db "Mouse device not available", 0

mouse_x_text db "X=", 0
mouse_y_text db " Y=", 0
mouse_buttons_text db " Buttons=", 0
