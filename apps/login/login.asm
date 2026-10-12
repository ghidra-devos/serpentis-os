; pid 1. asks who you are, lets the kernel check the password and starts lds as that user.
; when the shell exits we are back here. on a fresh disk there is no /etc/passwd yet, then
; it first walks you through making your user and the root password, like an installer

SERP_STACK = 0x2000

include '../runtime/program.inc'

FIELD_MAX = 31
FIELD_BYTES = FIELD_MAX + 1
FIELD_COUNT = 5
BOX_W = 400
BOX_X = (640 - BOX_W) / 2
ROW_H = 28
FAIL_TICKS = 150
FIRST_UID = 1000

F_USER = 0
F_PASSWORD = 1
F_AGAIN = 2
F_ROOT = 3
F_ROOT_AGAIN = 4

main:
    push rbx
.session:
    lea rdi, [window_title]
    mov esi, 640
    mov edx, 480
    call screen_open
    call clear_fields
    mov byte [setup], 0
    lea rdi, [passwd_path]
    lea rsi, [stat_buffer]
    call sys_stat
    test eax, eax
    jns .loop
    mov byte [setup], 1

.loop:
    call draw
    xor esi, esi
    call screen_wait
    cmp eax, SCREEN_KEY
    jne .loop
    mov edi, [screen_key]
    call handle_key
    test eax, eax
    jz .loop
    cmp byte [setup], 0
    jne .install

    lea rdi, [shell_path]
    xor esi, esi
    lea rdx, [fields + F_USER * FIELD_BYTES]
    lea rcx, [fields + F_PASSWORD * FIELD_BYTES]
    call sys_spawnas
    mov ebx, eax
    call wipe_secrets
    test ebx, ebx
    js .refused

    mov qword [message], 0
    call screen_close
    mov edi, ebx
    xor esi, esi
    call sys_waitpid
    jmp .session

; a wrong password costs a second and a half, typing ahead during it goes nowhere
.refused:
    lea rax, [failed_text]
    mov [message], rax
    call clear_fields
    call draw
    call sys_getticks
    mov rbx, rax
.delay:
    call sys_yield
    call sys_getticks
    sub rax, rbx
    cmp rax, FAIL_TICKS
    jb .delay
.drain:
    lea rdi, [screen_event]
    mov esi, SYS_GETKEY_NOWAIT
    call sys_event
    test eax, eax
    jnz .drain
    jmp .loop

.install:
    call install
    jc .loop
    mov byte [setup], 0
    call wipe_secrets
    mov byte [focus], F_PASSWORD
    lea rax, [ready_text]
    mov [message], rax
    jmp .loop

clear_fields:
    lea rdi, [fields]
    xor esi, esi
    mov edx, FIELD_BYTES * FIELD_COUNT
    call mem_set
    lea rdi, [lengths]
    xor esi, esi
    mov edx, 4 * FIELD_COUNT
    call mem_set
    mov byte [focus], 0
    ret

wipe_secrets:
    lea rdi, [fields + F_PASSWORD * FIELD_BYTES]
    xor esi, esi
    mov edx, FIELD_BYTES * (FIELD_COUNT - 1)
    call mem_set
    lea rdi, [lengths + F_PASSWORD * 4]
    xor esi, esi
    mov edx, 4 * (FIELD_COUNT - 1)
    jmp mem_set

field_count:
    mov eax, 2
    cmp byte [setup], 0
    je .done
    mov eax, FIELD_COUNT
.done:
    ret

handle_key:
    mov qword [message], 0
    movzx eax, byte [focus]
    imul ecx, eax, FIELD_BYTES
    lea rsi, [fields + rcx]
    lea rdx, [lengths + rax * 4]
    cmp edi, 10
    je .enter
    cmp edi, 9
    je .next
    cmp edi, 27
    je .clear
    cmp edi, 8
    je .erase
    cmp edi, ' '
    jb .none
    cmp edi, 126
    ja .none
    mov ecx, [rdx]
    cmp ecx, FIELD_MAX
    jae .none
    mov [rsi + rcx], dil
    inc ecx
    mov byte [rsi + rcx], 0
    mov [rdx], ecx
    jmp .none
.erase:
    mov ecx, [rdx]
    test ecx, ecx
    jz .back
    dec ecx
    mov byte [rsi + rcx], 0
    mov [rdx], ecx
    jmp .none
.back:
    cmp byte [focus], 0
    je .none
    dec byte [focus]
    jmp .none
.clear:
    call clear_fields
    jmp .none
.enter:
    call field_count
    dec eax
    cmp [focus], al
    jae .done
    cmp byte [focus], F_USER
    jne .next
    cmp dword [lengths], 0
    je .none
.next:
    call field_count
    movzx ecx, byte [focus]
    inc ecx
    cmp ecx, eax
    jb .store
    xor ecx, ecx
.store:
    mov [focus], cl
.none:
    xor eax, eax
    ret
.done:
    mov eax, 1
    ret

; first boot: checks what was typed, writes /etc/passwd, sets both passwords through the
; kernel and makes the home directory. CF with a message when something is off
install:
    push rbx
    lea rsi, [fields + F_USER * FIELD_BYTES]
    cmp byte [rsi], 0
    je .bad_name
.name:
    mov al, [rsi]
    test al, al
    jz .name_ok
    cmp al, 'a'
    jb .digit
    cmp al, 'z'
    jbe .name_next
.digit:
    cmp al, '0'
    jb .bad_name
    cmp al, '9'
    ja .bad_name
.name_next:
    inc rsi
    jmp .name
.name_ok:
    lea rdi, [fields + F_USER * FIELD_BYTES]
    lea rsi, [root_name]
    call str_equal
    test eax, eax
    jnz .bad_name

    lea rdi, [fields + F_PASSWORD * FIELD_BYTES]
    lea rsi, [fields + F_AGAIN * FIELD_BYTES]
    call str_equal
    test eax, eax
    jz .mismatch
    lea rdi, [fields + F_ROOT * FIELD_BYTES]
    lea rsi, [fields + F_ROOT_AGAIN * FIELD_BYTES]
    call str_equal
    test eax, eax
    jz .mismatch
    cmp dword [lengths + F_ROOT * 4], 0
    je .no_root

    lea rdi, [home]
    lea rsi, [home_prefix]
    call str_copy
    lea rdi, [home + 6]
    lea rsi, [fields + F_USER * FIELD_BYTES]
    call str_copy

    lea rdi, [line]
    lea rsi, [root_line]
    call str_copy
    lea rdi, [line]
    call str_len
    lea rdi, [line + rax]
    lea rsi, [fields + F_USER * FIELD_BYTES]
    call str_copy
    lea rdi, [line]
    call str_len
    lea rdi, [line + rax]
    lea rsi, [user_uid_text]
    call str_copy
    lea rdi, [line]
    call str_len
    lea rdi, [line + rax]
    lea rsi, [home]
    call str_copy
    lea rdi, [line]
    call str_len
    mov word [line + rax], 10

    lea rdi, [passwd_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax
    lea rdi, [line]
    call str_len
    mov edx, eax
    mov edi, ebx
    lea rsi, [line]
    call sys_write
    mov edi, ebx
    call sys_close

    lea rdi, [root_name]
    lea rsi, [empty]
    lea rdx, [fields + F_ROOT * FIELD_BYTES]
    call sys_passwd
    test eax, eax
    js .failed
    lea rdi, [fields + F_USER * FIELD_BYTES]
    lea rsi, [empty]
    lea rdx, [fields + F_PASSWORD * FIELD_BYTES]
    call sys_passwd
    test eax, eax
    js .failed

    lea rdi, [home_root]
    call sys_mkdir
    lea rdi, [home]
    call sys_mkdir
    lea rdi, [home]
    mov esi, FIRST_UID
    call sys_chown
    pop rbx
    clc
    ret

.bad_name:
    lea rax, [bad_name_text]
    jmp .complain
.mismatch:
    lea rax, [mismatch_text]
    jmp .complain
.no_root:
    lea rax, [no_root_text]
    jmp .complain
.failed:
    lea rax, [install_failed_text]
.complain:
    mov [message], rax
    call wipe_secrets
    mov byte [focus], F_PASSWORD
    pop rbx
    stc
    ret

draw:
    push rbx
    push r12
    call screen_clear
    call field_count
    mov r12d, eax
    imul ecx, eax, ROW_H
    add ecx, 96
    mov [box_h], ecx
    mov eax, 480
    sub eax, ecx
    shr eax, 1
    mov [box_y], eax

    mov edi, BOX_X - 2
    mov esi, [box_y]
    sub esi, 2
    mov edx, BOX_W + 4
    mov ecx, [box_h]
    add ecx, 4
    mov r8d, COLOR_DARK_GRAY
    call draw_fill
    mov edi, BOX_X
    mov esi, [box_y]
    mov edx, BOX_W
    mov ecx, [box_h]
    mov r8d, COLOR_BLACK
    call draw_fill

    lea rdx, [banner_text]
    cmp byte [setup], 0
    je .banner
    lea rdx, [setup_text]
.banner:
    mov edi, BOX_X + 24
    mov esi, [box_y]
    add esi, 18
    mov cl, COLOR_LIGHT_CYAN
    mov r8d, -1
    call draw_text

    xor ebx, ebx
.row:
    cmp ebx, r12d
    jae .message
    mov edi, BOX_X + 24
    imul esi, ebx, ROW_H
    add esi, [box_y]
    add esi, 58
    mov rdx, [labels + rbx * 8]
    cmp byte [setup], 0
    jne .label
    mov rdx, [login_labels + rbx * 8]
.label:
    mov cl, COLOR_LIGHT_GRAY
    mov r8d, -1
    call draw_text
    mov edi, ebx
    call field_line
    inc ebx
    jmp .row

.message:
    mov rdx, [message]
    test rdx, rdx
    jz .present
    mov edi, BOX_X + 24
    mov esi, [box_y]
    add esi, [box_h]
    sub esi, 30
    mov cl, COLOR_LIGHT_RED
    lea rax, [ready_text]
    cmp rdx, rax
    jne .colored
    mov cl, COLOR_LIGHT_GREEN
.colored:
    mov r8d, -1
    call draw_text
.present:
    pop r12
    pop rbx
    jmp screen_present

field_line:
    push rbx
    mov ebx, edi
    imul eax, ebx, FIELD_BYTES
    lea rsi, [fields + rax]
    mov ecx, [lengths + rbx * 4]
    lea rdi, [line]
    test ecx, ecx
    jz .cursor
.char:
    mov al, [rsi]
    test ebx, ebx
    jz .shown
    mov al, '*'
.shown:
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .char
.cursor:
    movzx eax, byte [focus]
    cmp eax, ebx
    jne .end
    mov byte [rdi], '_'
    inc rdi
.end:
    mov byte [rdi], 0
    mov edi, BOX_X + 184
    imul esi, ebx, ROW_H
    add esi, [box_y]
    add esi, 58
    lea rdx, [line]
    mov cl, COLOR_WHITE
    mov r8d, -1
    call draw_text
    pop rbx
    ret

window_title db 'login', 0
shell_path db '/bin/lds.serp', 0
passwd_path db '/etc/passwd', 0
home_prefix db '/home/', 0
home_root db '/home', 0
root_name db 'root', 0
root_line db 'root:0:/root', 10, 0
user_uid_text db ':1000:', 0
empty db 0
banner_text db 'serpentis-os   who are you', 0
setup_text db 'serpentis-os   first boot, lets set you up', 0
user_label db 'user', 0
password_label db 'password', 0
again_label db 'again', 0
root_label db 'root password', 0
failed_text db 'nope. wrong user or password', 0
bad_name_text db 'name: lowercase letters and digits, not root', 0
mismatch_text db 'the two passwords dont match', 0
no_root_text db 'root needs a password', 0
install_failed_text db 'could not write /etc, disk full?', 0
ready_text db 'done. now log in', 0

align 8
labels dq user_label, password_label, again_label, root_label, again_label
login_labels dq user_label, password_label

include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'

align 8
message rq 1
box_y rd 1
box_h rd 1
lengths rd FIELD_COUNT
focus rb 1
setup rb 1
stat_buffer rb USER_STAT_SIZE
fields rb FIELD_BYTES * FIELD_COUNT
home rb 64
line rb 128
