; syscall stubs. args stay where the c abi puts them (rdi rsi rdx), the number goes in rax

; never returns, the jmp is only there so a broken kernel doesn't run into the next stub
sys_exit:
    mov eax, SYS_EXIT
    int 0x80
    jmp $

sys_write:
    mov eax, SYS_WRITE
    int 0x80
    ret

sys_read:
    mov eax, SYS_READ
    int 0x80
    ret

sys_open:
    mov eax, SYS_OPEN
    int 0x80
    ret

sys_close:
    mov eax, SYS_CLOSE
    int 0x80
    ret

sys_stat:
    mov eax, SYS_STAT
    int 0x80
    ret

sys_chdir:
    mov eax, SYS_CHDIR
    int 0x80
    ret

sys_getcwd:
    mov eax, SYS_GETCWD
    int 0x80
    ret

sys_getpid:
    mov eax, SYS_GETPID
    int 0x80
    ret

sys_getticks:
    mov eax, SYS_GETTICKS
    int 0x80
    ret

sys_yield:
    mov eax, SYS_YIELD
    int 0x80
    ret

sys_spawn:
    mov eax, SYS_SPAWN
    int 0x80
    ret

sys_waitpid:
    mov eax, SYS_WAITPID
    int 0x80
    ret

sys_sync:
    mov eax, SYS_SYNC
    int 0x80
    ret

sys_kill:
    mov eax, SYS_KILL
    int 0x80
    ret

sys_sysinfo:
    mov eax, SYS_SYSINFO
    int 0x80
    ret

sys_alloc:
    mov eax, SYS_ALLOC
    int 0x80
    ret

sys_dup2:
    mov eax, SYS_DUP2
    int 0x80
    ret

sys_pipe:
    mov eax, SYS_PIPE
    int 0x80
    ret

sys_time:
    mov eax, SYS_TIME
    int 0x80
    ret

sys_theme:
    mov eax, SYS_THEME
    int 0x80
    ret

sys_palette:
    mov eax, SYS_PALETTE
    int 0x80
    ret

sys_seek:
    mov eax, SYS_SEEK
    int 0x80
    ret

sys_readdir:
    mov eax, SYS_READDIR
    int 0x80
    ret

sys_mkdir:
    mov eax, SYS_MKDIR
    int 0x80
    ret

sys_unlink:
    mov eax, SYS_UNLINK
    int 0x80
    ret

sys_rename:
    mov eax, SYS_RENAME
    int 0x80
    ret

sys_getkey:
    mov eax, SYS_GETKEY
    int 0x80
    ret

sys_con_info:
    mov eax, SYS_CON_INFO
    int 0x80
    ret

sys_con_draw:
    mov r10, rcx
    mov eax, SYS_CON_DRAW
    int 0x80
    ret

sys_con_restore:
    mov eax, SYS_CON_RESTORE
    int 0x80
    ret

sys_fb_blit:
    mov r10, rcx
    mov eax, SYS_FB_BLIT
    int 0x80
    ret

sys_event:
    mov eax, SYS_EVENT
    int 0x80
    ret

sys_send:
    mov eax, SYS_SEND
    int 0x80
    ret

sys_shm_attach:
    mov eax, SYS_SHM_ATTACH
    int 0x80
    ret

sys_service:
    mov eax, SYS_SERVICE
    int 0x80
    ret

sys_tty_open:
    mov eax, SYS_TTY_OPEN
    int 0x80
    ret

sys_tty_read:
    mov eax, SYS_TTY_READ
    int 0x80
    ret

sys_tty_input:
    mov eax, SYS_TTY_INPUT
    int 0x80
    ret

sys_kcommand:
    mov eax, SYS_KCOMMAND
    int 0x80
    ret

sys_log:
    mov eax, SYS_LOG
    int 0x80
    ret

sys_getuid:
    mov eax, SYS_GETUID
    int 0x80
    ret

; rdi path, rsi args, rdx -> user, rcx -> password or 0
sys_spawnas:
    mov r10, rcx
    mov eax, SYS_SPAWNAS
    int 0x80
    ret

sys_chmod:
    mov eax, SYS_CHMOD
    int 0x80
    ret

sys_chown:
    mov eax, SYS_CHOWN
    int 0x80
    ret

sys_passwd:
    mov eax, SYS_PASSWD
    int 0x80
    ret
