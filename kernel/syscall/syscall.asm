; int 0x80. number in rax, args in rdi rsi rdx, result back in rax.
; the whole register file is saved as a TRAP_* frame first, handlers that block or switch
; processes work on that frame and the iretq returns into whoever owns it by then

syscall_entry:
    cld
    push 0
    push TRAP_VECTOR_SYSCALL

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call syscall_frame_dispatch

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16

    iretq

; rdi -> frame. a killed process doesn't get its syscall, it exits with KILLED_EXIT_CODE
; instead. handlers return CF when they already switched to another process, then the
; frame is not theirs to write a result into
syscall_frame_dispatch:
    push rbx
    sub rsp, 8
    mov [rsp], rdi              ; frame pointer, .store_result reads it back after the calls
    mov rbx, rdi

    movzx eax, word [process_current_index]
    imul eax, PROCESS_SIZE
    ; FIXME kill only lands here. something spinning in a loop without syscalls never dies,
    ; the timer should check this flag too
    test byte [process_table + rax + PROCESS_FLAGS], PROCESS_FLAG_KILLED
    jz .dispatch
    mov qword [rbx + TRAP_RDI], KILLED_EXIT_CODE
    jmp .exit

.dispatch:
    mov byte [fs_denied], 0
    mov eax, [rbx + TRAP_RAX]

    ; TODO this cmp chain is like 55 long, should be a jump table on the number.
    ; works fine so it stays for now
    cmp eax, SYS_GETPID
    je .getpid
    cmp eax, SYS_SYNC
    je .sync
    cmp eax, SYS_GETCWD
    je .getcwd
    cmp eax, SYS_CHDIR
    je .chdir
    cmp eax, SYS_OPEN
    je .open
    cmp eax, SYS_CLOSE
    je .close
    cmp eax, SYS_STAT
    je .stat
    cmp eax, SYS_READ
    je .read
    cmp eax, SYS_WRITE
    je .write
    cmp eax, SYS_EXIT
    je .exit
    cmp eax, SYS_GETTICKS
    je .getticks

    cmp eax, SYS_YIELD
    je .yield

    cmp eax, SYS_SPAWN
    je .spawn

    cmp eax, SYS_WAITPID
    je .waitpid

    cmp eax, SYS_READDIR
    je .readdir
    cmp eax, SYS_SEEK
    je .seek
    cmp eax, SYS_MKDIR
    je .mkdir
    cmp eax, SYS_UNLINK
    je .unlink
    cmp eax, SYS_RENAME
    je .rename
    cmp eax, SYS_GETKEY
    je .getkey
    cmp eax, SYS_CON_INFO
    je .con_info
    cmp eax, SYS_CON_DRAW
    je .con_draw
    cmp eax, SYS_CON_RESTORE
    je .con_restore
    cmp eax, SYS_FB_BLIT
    je .fb_blit
    cmp eax, SYS_EVENT
    je .event
    cmp eax, SYS_SEND
    je .send
    cmp eax, SYS_SHM_ATTACH
    je .shm_attach
    cmp eax, SYS_SERVICE
    je .service
    cmp eax, SYS_TTY_OPEN
    je .tty_open
    cmp eax, SYS_TTY_READ
    je .tty_read
    cmp eax, SYS_TTY_INPUT
    je .tty_input
    cmp eax, SYS_KCOMMAND
    je .kcommand
    cmp eax, SYS_LOG
    je .log
    cmp eax, SYS_DUP2
    je .dup2
    cmp eax, SYS_ALLOC
    je .alloc
    cmp eax, SYS_SYSINFO
    je .sysinfo
    cmp eax, SYS_KILL
    je .kill
    cmp eax, SYS_PIPE
    je .pipe
    cmp eax, SYS_TIME
    je .time
    cmp eax, SYS_THEME
    je .theme
    cmp eax, SYS_PALETTE
    je .palette
    cmp eax, SYS_ISATTY
    je .isatty
    cmp eax, SYS_GETUID
    je .getuid
    cmp eax, SYS_SPAWNAS
    je .spawnas
    cmp eax, SYS_CHMOD
    je .chmod
    cmp eax, SYS_CHOWN
    je .chown
    cmp eax, SYS_PASSWD
    je .passwd

    mov eax, SYS_ERR_INVALID
    jmp .store_result

.pipe:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_pipe
    jmp .store_result

.time:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_time
    jmp .store_result

.theme:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_theme
    jmp .store_result

.getuid:
    call syscall_getuid
    jmp .store_result

.spawnas:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    mov r10, [rbx + TRAP_R10]
    call syscall_spawnas
    jmp .store_result

.chmod:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_chmod
    jmp .store_result

.chown:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_chown
    jmp .store_result

.passwd:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_passwd
    jmp .store_result

.isatty:
    mov rdi, [rbx + TRAP_RDI]
    xor eax, eax
    cmp rdi, FD_STDERR
    ja .store_result
    call standard_stream_redirected
    mov eax, 0
    jc .store_result
    mov eax, 1
    jmp .store_result

.palette:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_palette
    jmp .store_result

.getpid:
    call process_get_current_pid
    jmp .store_result

.sync:
    call syscall_sync
    jmp .store_result

.getcwd:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_getcwd
    jmp .store_result

.chdir:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_chdir
    jmp .store_result

.open:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_open
    jmp .store_result

.close:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_close
    jmp .store_result

.stat:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_stat
    jmp .store_result

.read:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    cmp edi, FD_STDIN
    jne .file_read
    call standard_stream_redirected
    jnc .stdin_read
.file_read:
    mov rcx, rbx
    call syscall_read
    jc .epilogue
    jmp .store_result

.stdin_read:
    mov rcx, rbx
    call syscall_read_stdin
    jc .epilogue
    jmp .store_result

.write:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    cmp edi, FD_STDOUT
    jb .file_write
    cmp edi, FD_STDERR
    ja .file_write
    call standard_stream_redirected
    jnc .console_write
.file_write:
    mov rcx, rbx
    call syscall_write_file
    jc .epilogue
    jmp .store_result

; tty full: block and redo the write once the reader drained some
.console_write:
    call tty_current
    test eax, eax
    jz .console_output
    call tty_write
    jnc .store_result
    sub qword [rbx + TRAP_RIP], 2
    mov rdi, rbx
    call process_block_current
    jnc .epilogue
    add qword [rbx + TRAP_RIP], 2
    xor eax, eax
    jmp .store_result

.console_output:
    call syscall_user_write_buffer
    jmp .store_result

.exit:
    mov edi, [rbx + TRAP_RDI]
    cmp byte [user_test_active], 0
    jne .test_exit

    movzx eax, word [process_current_index]
    test eax, eax
    jz .exit_invalid

    mov esi, edi
    mov rdi, rbx
    call process_exit_switch
    jc exec_return_to_kernel
    jmp .epilogue

.exit_invalid:
    mov eax, SYS_ERR_INVALID
    jmp .store_result

.test_exit:
    jmp user_test_exit

.getticks:
    mov rdx, [rsp]
    mov rax, [pit_ticks]
    mov [rdx + TRAP_RAX], rax
    jmp .store_result

.yield:
    mov qword [rbx + TRAP_RAX], SYS_OK
    mov rdi, rbx
    call process_yield
    jmp .epilogue

.spawn:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call process_spawn
    jmp .store_result

.waitpid:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, rbx
    call process_waitpid
    jc .epilogue
    jmp .store_result

.readdir:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_readdir
    jmp .store_result

.seek:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_seek
    jmp .store_result

.mkdir:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_mkdir
    jmp .store_result

.unlink:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_unlink
    jmp .store_result

.rename:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_rename
    jmp .store_result

.getkey:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, rbx
    call syscall_getkey
    jc .epilogue
    jmp .store_result

.con_info:
    call syscall_con_info
    jmp .store_result

.con_draw:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    mov rcx, [rbx + TRAP_R10]
    call syscall_con_draw
    jmp .store_result

.con_restore:
    call syscall_con_restore
    jmp .store_result

.fb_blit:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    mov rcx, [rbx + TRAP_R10]
    call syscall_fb_blit
    jmp .store_result

.event:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, rbx
    call syscall_event
    jc .epilogue
    jmp .store_result

.send:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_send
    jmp .store_result

.shm_attach:
    call syscall_shm_attach
    jmp .store_result

.service:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_service
    jmp .store_result

.tty_open:
    call syscall_tty_open
    jmp .store_result

.tty_read:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_tty_read
    jmp .store_result

.tty_input:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_tty_input
    jmp .store_result

.kill:
    mov edi, [rbx + TRAP_RDI]
    call process_kill
    jmp .store_result

.sysinfo:
    mov rdi, [rbx + TRAP_RDI]
    call syscall_sysinfo
    jmp .store_result

.alloc:
    mov rdi, [rbx + TRAP_RDI]
    call heap_grow
    mov rdx, [rsp]
    mov [rdx + TRAP_RAX], rax
    jmp .epilogue

.dup2:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_dup2
    jmp .store_result

.log:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    call syscall_log
    jmp .store_result

.kcommand:
    mov rdi, [rbx + TRAP_RDI]
    mov rsi, [rbx + TRAP_RSI]
    mov rdx, [rbx + TRAP_RDX]
    call syscall_kcommand

.store_result:
    mov rdx, [rsp]
    mov [rdx + TRAP_RAX], eax
.epilogue:
    add rsp, 8
    pop rbx
    ret

syscall_user_write_buffer:
    push rdx
    mov [syscall_user_pointer], rsi
    mov [syscall_user_length], rdx

    mov rdi, rsi
    call syscall_user_range_valid
    jc .failed

.write_loop:
    cmp qword [syscall_user_length], 0
    je .success

    mov rax, [syscall_user_pointer]
    mov al, [rax]

    inc qword [syscall_user_pointer]
    dec qword [syscall_user_length]

    cmp al, 8
    je .backspace

    call terminal_putc
    jmp .write_loop

.backspace:
    call terminal_backspace
    jmp .write_loop

.success:
    mov rax, [rsp]
    add rsp, 8
    ret

.failed:
    add rsp, 8
    mov eax, SYS_ERR_INVALID_ADDRESS
    ret

syscall_read_stdin:
    push rbx
    push r12
    push r13
    push r14

    mov r12, rsi
    mov r13, rdx
    mov r14, rcx

    mov rdi, rsi
    call syscall_user_range_valid
    jc .invalid

    xor ebx, ebx

    call tty_current
    test eax, eax
    jnz .tty

.next:
    cmp rbx, r13
    jae .done

    call keyboard_read_char
    jc .empty
    call stdin_control
    jc .done

    mov [r12 + rbx], al
    inc rbx
    cmp al, 10
    jne .next
    jmp .done

.empty:
    test rbx, rbx
    jnz .done

    movzx eax, word [process_current_index]
    test eax, eax
    jz .done

    mov [stdin_waiter], ax
    call console_show_cursor
    sub qword [r14 + TRAP_RIP], 2
    mov rdi, r14
    call process_block_current
    jnc .switched

    add qword [r14 + TRAP_RIP], 2
    mov word [stdin_waiter], 0

.done:
    mov rax, rbx
    clc
    jmp .out

.invalid:
    mov rax, SYS_ERR_INVALID_ADDRESS
    clc
    jmp .out

.tty:
    mov [tty_reading], eax
.tty_next:
    cmp rbx, r13
    jae .done
    mov eax, [tty_reading]
    call tty_read_key
    jc .tty_empty
    cmp eax, 0x100              ; arrows and such, a byte stream has no use for them
    jae .tty_next
    call stdin_control
    jc .done
    mov [r12 + rbx], al
    inc rbx
    cmp al, 10
    jne .tty_next
    jmp .done
.tty_empty:
    test rbx, rbx
    jnz .done
    sub qword [r14 + TRAP_RIP], 2
    mov rdi, r14
    call process_block_current
    jnc .switched
    add qword [r14 + TRAP_RIP], 2
    jmp .done

.switched:
    stc

.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

stdin_control:
    cmp al, KEY_END_OF_INPUT
    je .stop
    cmp al, KEY_INTERRUPT
    jne .keep
    push rax
    call process_current_record
    or byte [rax + PROCESS_FLAGS], PROCESS_FLAG_KILLED
    pop rax
.stop:
    stc
    ret
.keep:
    clc
    ret

syscall_user_range_valid:
    test rdx, rdx
    jz .valid

    mov rax, rdi
    add rax, rdx
    jc .invalid

    dec rax

    cmp byte [exec_active], 0
    je .test_range

    push rcx
    call shm_range_valid
    pop rcx
    jc .valid

    push rcx
    push rax
    call process_current_record
    mov rcx, rax
    pop rax

    cmp rdi, USER_CODE_ADDRESS
    jb .exec_stack

    push rcx
    mov ecx, [rcx + PROCESS_EXEC_CODE_PAGES]
    shl rcx, 12
    add rcx, USER_CODE_ADDRESS - 1
    cmp rax, rcx
    pop rcx
    jbe .exec_valid

    push rdx
    mov rdx, HEAP_BASE
    cmp rdi, rdx
    pop rdx
    jb .exec_stack
    cmp rax, [rcx + PROCESS_HEAP_TOP]
    jb .exec_valid

.exec_stack:
    mov rcx, [rcx + PROCESS_EXEC_STACK_SIZE]
    neg rcx
    add rcx, USER_STACK_TOP
    cmp rdi, rcx
    jb .exec_invalid
    cmp rax, USER_STACK_TOP - 1
    ja .exec_invalid

.exec_valid:
    pop rcx
    clc
    ret

.exec_invalid:
    pop rcx
    stc
    ret

; dead since the old r3test command is gone, exec_active is always 1 now
.test_range:
    cmp rdi, USER_CODE_ADDRESS
    jb .check_stack

    cmp rax, USER_CODE_ADDRESS + USER_CODE_PAGE_SIZE - 1
    jbe .valid

.check_stack:
    cmp rdi, USER_STACK_PAGE
    jb .invalid

    cmp rax, USER_STACK_PAGE + USER_CODE_PAGE_SIZE - 1
    jbe .valid

.invalid:
    stc
    ret

.valid:
    clc
    ret

tty_reading dd 0
syscall_user_pointer dq 0
syscall_user_length dq 0
syscall_sync:
    cmp byte [fs_ready], 0
    je .io_error

    call fs_save_entries
    jc .io_error

    call ata_flush
    jc .io_error

    mov eax, SYS_OK
    ret

.io_error:
    mov eax, SYS_ERR_IO
    ret
