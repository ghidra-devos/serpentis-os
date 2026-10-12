cmd_help:
    call shell_use_output_color
    mov rsi, help_text
    call terminal_print
    ret

cmd_echo:
    call shell_use_output_color
    mov edx, 1
    call shell_print_arguments
    call terminal_newline
    ret

cmd_version:
    call shell_use_output_color
    mov rsi, version_text
    call terminal_print_line
    ret

cmd_about:
    call shell_use_output_color
    mov rsi, about_text
    call terminal_print
    ret

cmd_banner:
    mov al, COLOR_LIGHT_GREEN
    call terminal_set_foreground
    mov rsi, banner_text
    call terminal_print
    call shell_use_output_color
    ret

cmd_logo:
    mov al, COLOR_LIGHT_GREEN
    call terminal_set_foreground
    mov rsi, snake_logo_text
    call terminal_print
    call shell_use_output_color
    ret

cmd_pwd:
    call shell_use_output_color
    mov rsi, fs_current_path
    call terminal_print_line
    ret

cmd_whoami:
    call shell_use_output_color
    call process_current_uid
    call user_name_of
    call terminal_print_line
    ret

cmd_hostname:
    call shell_use_output_color
    mov rsi, hostname_text
    call terminal_print_line
    ret

cmd_uname:
    call shell_use_output_color
    mov rsi, uname_text
    call terminal_print_line
    ret

cmd_strlen:
    cmp rdi, 2
    jb .usage

    mov r12, rdi
    mov r13, rsi

    mov ebx, 1
    xor r14, r14

.count:
    cmp rbx, r12
    jae .print

    mov rsi, [r13 + rbx * 8]
    call str_length

    add r14, rax
    inc rbx

    cmp rbx, r12
    jae .count

    inc r14
    jmp .count

.print:
    call shell_use_output_color

    mov rsi, strlen_result_text
    call terminal_print

    mov rax, r14
    call terminal_print_u64
    call terminal_newline
    ret

.usage:
    mov rsi, strlen_usage_text
    call shell_error
    ret

cmd_repeat:
    cmp rdi, 3
    jb .usage

    mov r12, rdi
    mov r13, rsi

    mov rsi, [r13 + 8]
    call parse_u64
    jc .usage

    cmp rax, 1
    jb .usage

    cmp rax, 50
    ja .usage

    mov r14, rax
    xor r15d, r15d

    call shell_use_output_color

.loop:
    cmp r15, r14
    jae .done

    mov rdi, r12
    mov rsi, r13
    mov edx, 2
    call shell_print_arguments
    call terminal_newline

    inc r15
    jmp .loop

.done:
    ret

.usage:
    mov rsi, repeat_usage_text
    call shell_error
    ret

command_help db 'help', 0
command_echo db 'echo', 0
command_version db 'version', 0
command_about db 'about', 0
command_banner db 'banner', 0
command_logo db 'logo', 0
command_pwd db 'pwd', 0
command_whoami db 'whoami', 0
command_hostname db 'hostname', 0
command_uname db 'uname', 0
command_strlen db 'strlen', 0
command_repeat db 'repeat', 0

help_text:
    db 'SERPENTIS commands:', 10
    db ' help clear echo version about banner logo pwd', 10
    db ' whoami hostname uname history strlen repeat', 10
    db ' ticks uptime cpuinfo cpuid date time irqinfo', 10
    db ' palette rand hex dec beep', 10
    db ' ls cd mkdir mkdir -p rmdir rmdir -r', 10
    db ' touch write append cat rm rm -r rm -f', 10
    db ' cp mv stat tree hexdump wc fsinfo sync format', 10
    db ' find du df fsck', 10
    db ' ps wait reap ringinfo run', 10
    db ' programs: lds apple cc gui edit neofetch theme', 10
    db ' text: head tail grep sort uniq cut rev tac nl cmp diff less more', 10
    db '       tr tee wc paste comm fold strings xxd cksum xargs', 10
    db ' files: split truncate dd realpath mktemp basename dirname which file', 10
    db ' utils: seq sleep true false', 10
    db ' users: id su passwd useradd chmod chown, ls -l shows owners', 10
    db ' desktop: files clock graphcalc paint calc demo (Super+Space)', 10
    db ' reboot shutdown halt', 10, 0

version_text db 'serpentis-os x86-64', 0

about_text:
    db 'serpentis-os is a 64-bit operating system written in assembly.', 10
    db 'It comes with its own assembler, a C compiler and a tiling desktop.', 10, 0

banner_text:
    db 'serpentis-os', 10
    db 'The serpent is awake.', 10, 0

snake_logo_text:
    db '              __', 10
    db '     ________/  \___', 10
    db '  **/               \**', 10
    db ' <__   SERPENTIS      _>', 10
    db '    \___/\___/\______/', 10, 0

hostname_text db 'serpentis', 0
uname_text db 'serpentis-os x86_64', 0
strlen_result_text db 'Length: ', 0
strlen_usage_text db 'Usage: strlen <text>', 0
repeat_usage_text db 'Usage: repeat <1-50> <text>', 0
