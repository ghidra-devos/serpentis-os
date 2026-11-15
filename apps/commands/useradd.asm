; useradd name. root only. next free uid from 1000 up, home in /home/name, empty password
; until passwd sets one

include '../runtime/program.inc'

NAME_MAX = 31

main:
    push rbx
    push r12
    push r13
    call arg_next
    test rax, rax
    jz .usage
    mov r12, rax
    mov rdi, rax
    call str_len
    test eax, eax
    jz .usage
    cmp eax, NAME_MAX
    ja .usage
    mov rsi, r12
.check:
    mov al, [rsi]
    test al, al
    jz .checked
    cmp al, ':'
    je .usage
    cmp al, '/'
    je .usage
    inc rsi
    jmp .check
.checked:
    call sys_getuid
    test eax, eax
    jnz .not_root
    mov rdi, r12
    call user_uid_of
    jnc .exists

    call next_uid
    mov ebx, eax

    lea rdi, [home]
    lea rsi, [home_prefix]
    call str_copy
    lea rdi, [home + 6]
    mov rsi, r12
    call str_copy

    lea rdi, [line]
    mov rsi, r12
    call str_copy
    lea rdi, [line]
    call str_len
    lea r13, [line + rax]
    mov byte [r13], ':'
    mov edi, ebx
    call u64_to_dec
    lea rdi, [r13 + 1]
    mov rsi, rax
    call str_copy
    lea rdi, [line]
    call str_len
    mov byte [line + rax], ':'
    lea rdi, [line + rax + 1]
    lea rsi, [home]
    call str_copy
    lea rdi, [line]
    call str_len
    mov word [line + rax], 10

    call users_load
    jc .failed
    lea rdi, [users_text]
    call str_len
    test eax, eax
    jz .append
    cmp byte [users_text + rax - 1], 10
    je .append
    mov word [users_text + rax], 10
.append:
    lea rdi, [users_text]
    call str_len
    mov r13d, eax
    lea rdi, [line]
    call str_len
    add eax, r13d
    cmp eax, USERS_TEXT_MAX - 1
    jae .failed
    lea rdi, [users_text + r13]
    lea rsi, [line]
    call str_copy

    lea rdi, [passwd_path]
    mov esi, OPEN_WRITE
    call sys_open
    test eax, eax
    js .failed
    mov r13d, eax
    lea rdi, [users_text]
    call str_len
    mov edx, eax
    mov edi, r13d
    lea rsi, [users_text]
    call sys_write
    mov edi, r13d
    call sys_close

    lea rdi, [home]
    call sys_mkdir
    lea rdi, [home]
    mov esi, ebx
    call sys_chown
    mov rdi, r12
    lea rsi, [empty]
    lea rdx, [empty]
    call sys_passwd

    lea rdi, [added_text]
    call print
    mov rdi, r12
    call print
    lea rdi, [added_rest]
    call print_line
    xor eax, eax
    jmp .out

.usage:
    lea rdi, [usage_text]
    call print_line
    mov eax, 1
    jmp .out
.not_root:
    lea rdi, [root_text]
    call print_line
    mov eax, 1
    jmp .out
.exists:
    lea rdi, [exists_text]
    call print_line
    mov eax, 1
    jmp .out
.failed:
    lea rdi, [failed_text]
    call print_line
    mov eax, 1
.out:
    pop r13
    pop r12
    pop rbx
    ret

; biggest uid in passwd + 1, never below 1000
next_uid:
    push rbx
    mov ebx, 999
    call users_load
    jc .done
    lea rsi, [users_text]
.line:
    cmp byte [rsi], 0
    je .done
    push rsi
    mov ecx, 1
    call users_field_of
    lea rdi, [users_field]
    call users_decimal
    pop rsi
    cmp eax, ebx
    jbe .next
    mov ebx, eax
.next:
    mov al, [rsi]
    inc rsi
    test al, al
    jz .done
    cmp al, 10
    jne .next
    jmp .line
.done:
    lea eax, [rbx + 1]
    pop rbx
    ret

passwd_path db '/etc/passwd', 0
home_prefix db '/home/', 0
empty db 0
usage_text db 'usage: useradd name', 0
root_text db 'useradd: only root can do that, try su root useradd name', 0
exists_text db 'useradd: that user already exists', 0
failed_text db 'useradd: could not write /etc/passwd', 0
added_text db 'added ', 0
added_rest db ', give it a password with passwd', 0

include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/users_bss.inc'

home rb 256
line rb 320
