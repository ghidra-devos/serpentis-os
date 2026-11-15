include '../runtime/program.inc'

NAME_MAX = 32
PICTURE_ARG_MAX = 120
CWD_MAX = 240
GUI_WALLPAPER = 6
MSG_OP = 0
MSG_BYTES = 64

main:
    push rbx
    push r12
    call collect_args
    call read_current
    cmp dword [arg_count], 0
    je .list
    cmp dword [arg_count], 1
    jne .usage

    mov rax, [arg_list]
    mov [choice], rax
    mov rdi, rax
    call picture_name
    jc .name
    call full_path
    lea rax, [picture]
    mov [choice], rax
    lea rdi, [picture]
    jmp .check
.name:
    mov rdi, [arg_list]
    call make_path
    jc .unknown
    lea rdi, [path]
.check:
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .unknown
    test dword [status + USER_STAT_FLAGS], FS_FLAG_DIRECTORY
    jnz .unknown

    call save_choice
    test eax, eax
    js .cannot_save
    lea rdi, [set_text]
    call print
    mov rdi, [arg_list]
    call print
    call tell_gui
    test eax, eax
    jns .told
    lea rdi, [later_text]
    call print
.told:
    mov edi, 10
    call print_char
    xor eax, eax
    jmp .out

.list:
    call list_names
    jmp .out

.unknown:
    lea rdi, [unknown_text]
    call print
    mov rdi, [arg_list]
    call print_line
    call list_names
    mov eax, 1
    jmp .out

.cannot_save:
    lea rdi, [cannot_save_text]
    call print_line
    mov eax, 1
    jmp .out

.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

.out:
    pop r12
    pop rbx
    ret

; rdi -> name. path = /share/wallpapers/<name>.raw, CF set for an empty name, a slash
; or one that is too long
make_path:
    mov rsi, rdi
    call str_len
    test eax, eax
    jz .bad
    cmp eax, NAME_MAX
    ja .bad
    mov rdx, rsi
.check:
    mov cl, [rdx]
    test cl, cl
    jz .copy
    cmp cl, '/'
    je .bad
    inc rdx
    jmp .check
.copy:
    lea rdi, [path + dir_length]
    call str_copy
    lea rdi, [path + dir_length]
    call str_len
    lea rdi, [path + dir_length + rax]
    lea rsi, [raw_suffix]
    call str_copy
    clc
    ret
.bad:
    stc
    ret

; a .pic from paint can be the wallpaper too, gui gets its full path
picture_name:
    push rdi
    call str_len
    pop rdi
    cmp eax, 5
    jb .no
    cmp eax, PICTURE_ARG_MAX
    ja .no
    cmp dword [rdi + rax - 4], '.pic'
    jne .no
    clc
    ret
.no:
    stc
    ret

full_path:
    mov rsi, [arg_list]
    lea rdi, [picture]
    cmp byte [rsi], '/'
    je .copy
    mov esi, CWD_MAX
    call sys_getcwd
    lea rdi, [picture]
    call str_len
    cmp byte [picture + rax - 1], '/'
    je .join
    mov byte [picture + rax], '/'
    inc eax
.join:
    lea rdi, [picture + rax]
    mov rsi, [arg_list]
.copy:
    jmp str_copy

; current = the name in /etc/wallpaper, cut at the newline. day when there is none,
; that is what gui shows without the file
read_current:
    push rbx
    lea rdi, [current]
    lea rsi, [default_name]
    call str_copy
    lea rdi, [choice_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
    mov edi, ebx
    lea rsi, [line]
    mov edx, NAME_MAX
    call sys_read
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    test eax, eax
    jle .done
    mov byte [line + rax], 0
    lea rsi, [line]
.cut:
    mov cl, [rsi]
    test cl, cl
    jz .copy
    cmp cl, 10
    je .end
    cmp cl, 13
    je .end
    inc rsi
    jmp .cut
.end:
    mov byte [rsi], 0
.copy:
    cmp byte [line], 0
    je .done
    lea rdi, [current]
    lea rsi, [line]
    call str_copy
.done:
    pop rbx
    ret

; every .raw in the folder, the one in use gets a star
list_names:
    push rbx
    push r12
    lea rdi, [dir_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .empty
    mov ebx, eax
.next:
    mov edi, ebx
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .done
    test dword [dirent + SYS_DIRENT_FLAGS], FS_FLAG_DIRECTORY
    jnz .next
    lea rdi, [dirent + SYS_DIRENT_NAME]
    call str_len
    cmp eax, 5
    jb .next
    lea r12, [dirent + SYS_DIRENT_NAME + rax - 4]
    mov rdi, r12
    lea rsi, [raw_suffix]
    call str_equal
    test eax, eax
    jz .next
    mov byte [r12], 0
    lea rdi, [dirent + SYS_DIRENT_NAME]
    lea rsi, [current]
    call str_equal
    lea rdi, [plain_mark]
    test eax, eax
    jz .mark
    lea rdi, [current_mark]
.mark:
    call print
    lea rdi, [dirent + SYS_DIRENT_NAME]
    call print_line
    jmp .next
.done:
    mov edi, ebx
    call sys_close
    pop r12
    pop rbx
    ret
.empty:
    lea rdi, [no_folder_text]
    call print_line
    pop r12
    pop rbx
    ret

; eax < 0 when /etc/wallpaper cannot be written
save_choice:
    push rbx
    lea rdi, [choice_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
    mov rdi, [choice]
    call str_len
    mov edx, eax
    mov edi, ebx
    mov rsi, [choice]
    call sys_write
    mov edi, ebx
    lea rsi, [newline_byte]
    mov edx, 1
    call sys_write
    mov edi, ebx
    call sys_close
    xor eax, eax
.done:
    pop rbx
    ret

; eax < 0 when gui is not running, then it picks the file up next time it starts
tell_gui:
    xor edi, edi
    call sys_service
    test eax, eax
    js .done
    mov dword [message + MSG_OP], GUI_WALLPAPER
    mov edi, eax
    lea rsi, [message]
    call sys_send
.done:
    ret

program_name db 'wallpaper', 0
usage_text db 'usage: wallpaper [name | picture.pic]', 0
dir_path db '/share/wallpapers', 0
choice_path db '/etc/wallpaper', 0
default_name db 'day', 0
raw_suffix db '.raw', 0
newline_byte db 10
current_mark db '* ', 0
plain_mark db '  ', 0
set_text db 'wallpaper: ', 0
later_text db ', shows up next time gui starts', 0
unknown_text db 'wallpaper: no picture called ', 0
cannot_save_text db 'wallpaper: cannot write /etc/wallpaper', 0
no_folder_text db 'wallpaper: /share/wallpapers is missing', 0
path db '/share/wallpapers/'
dir_length = $ - path
    rb NAME_MAX + 8

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
message rb MSG_BYTES
choice rq 1
picture rb CWD_MAX + PICTURE_ARG_MAX + 8
status rb USER_STAT_SIZE
dirent rb SYS_DIRENT_BYTES
current rb NAME_MAX + 1
line rb NAME_MAX + 1
