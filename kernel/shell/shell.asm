shell_use_output_color:
    mov al, [shell_output_foreground]
    call terminal_set_foreground
    ret

; a check deep in the fs said no, that beats whatever the command wanted to say
shell_error:
    cmp byte [fs_denied], 0
    je .print
    mov byte [fs_denied], 0
    mov rsi, permission_denied_text
.print:
    mov al, COLOR_LIGHT_RED
    call terminal_set_foreground

    call terminal_print_line
    call shell_use_output_color
    ret

permission_denied_text db 'Permission denied.', 0

shell_parse:
    mov qword [shell_argc], 0

    mov rdi, shell_command_buffer

.skip_spaces:
    mov al, [rdi]

    cmp al, ' '
    je .advance_space

    cmp al, 9
    je .advance_space

    test al, al
    jz .done

    cmp qword [shell_argc], SHELL_MAX_ARGUMENTS
    jae .done

    cmp al, '"'
    je .quoted

    cmp al, 39
    je .quoted

    mov rcx, [shell_argc]
    mov [shell_argv + rcx * 8], rdi

    inc qword [shell_argc]

.scan_plain:
    mov al, [rdi]

    test al, al
    jz .done

    cmp al, ' '
    je .terminate_plain

    cmp al, 9
    je .terminate_plain

    inc rdi
    jmp .scan_plain

.terminate_plain:
    mov byte [rdi], 0
    inc rdi
    jmp .skip_spaces

.quoted:
    mov dl, al
    inc rdi

    mov rcx, [shell_argc]
    mov [shell_argv + rcx * 8], rdi

    inc qword [shell_argc]

.scan_quoted:
    mov al, [rdi]

    test al, al
    jz .done

    cmp al, dl
    je .terminate_quoted

    inc rdi
    jmp .scan_quoted

.terminate_quoted:
    mov byte [rdi], 0
    inc rdi
    jmp .skip_spaces

.advance_space:
    inc rdi
    jmp .skip_spaces

.done:
    ret

shell_print_arguments:
    push rbx
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13, rsi
    mov ebx, edx

    xor r14d, r14d

.loop:
    cmp rbx, r12
    jae .done

    test r14d, r14d
    jz .print

    mov al, ' '
    call terminal_putc

.print:
    mov rsi, [r13 + rbx * 8]
    call terminal_print

    mov r14d, 1
    inc ebx
    jmp .loop

.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

shell_command_table:
    dq command_help, cmd_help
    dq command_echo, cmd_echo
    dq command_version, cmd_version
    dq command_about, cmd_about
    dq command_banner, cmd_banner
    dq command_logo, cmd_logo
    dq command_pwd, cmd_pwd
    dq command_whoami, cmd_whoami
    dq command_hostname, cmd_hostname
    dq command_uname, cmd_uname
    dq command_strlen, cmd_strlen
    dq command_repeat, cmd_repeat
    dq command_ticks, cmd_ticks
    dq command_uptime, cmd_uptime
    dq command_cpuinfo, cmd_cpuinfo
    dq command_cpuid, cmd_cpuid
    dq command_date, cmd_date
    dq command_time, cmd_time
    dq command_palette, cmd_palette
    dq command_rand, cmd_rand
    dq command_hex, cmd_hex
    dq command_dec, cmd_dec
    dq command_beep, cmd_beep
    dq command_irqinfo, cmd_irqinfo
    dq command_ls, cmd_ls
    dq command_cd, cmd_cd
    dq command_mkdir, cmd_mkdir
    dq command_rmdir, cmd_rmdir
    dq command_touch, cmd_touch
    dq command_write, cmd_write
    dq command_append, cmd_append
    dq command_cat, cmd_cat
    dq command_rm, cmd_rm
    dq command_cp, cmd_cp
    dq command_mv, cmd_mv
    dq command_stat, cmd_stat
    dq command_tree, cmd_tree
    dq command_hexdump, cmd_hexdump
    dq command_wc, cmd_wc
    dq command_fsinfo, cmd_fsinfo
    dq command_sync, cmd_sync
    dq command_format, cmd_format
    dq command_reboot, cmd_reboot
    dq command_shutdown, cmd_shutdown
    dq command_halt, cmd_halt
    dq command_mouse, cmd_mouse
    dq command_mousepos, cmd_mousepos
    dq command_find, cmd_find
    dq command_du, cmd_du
    dq command_df, cmd_df
    dq command_fsck, cmd_fsck
    dq command_ps, cmd_ps
    dq command_ringinfo, cmd_ringinfo
    dq command_wait, cmd_wait
    dq command_reap, cmd_reap
    dq 0, 0

shell_output_foreground db COLOR_WHITE

shell_argc dq 0

shell_argv:
times SHELL_MAX_ARGUMENTS dq 0

shell_command_buffer:
times SHELL_BUFFER_SIZE db 0
