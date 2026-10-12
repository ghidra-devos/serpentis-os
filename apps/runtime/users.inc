; reading /etc/passwd from a program. needs users_bss.inc after library.inc

USERS_TEXT_MAX = 2048
USERS_FIELD_MAX = 240

users_load:
    push rbx
    lea rdi, [users_passwd_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax
    mov edi, ebx
    lea rsi, [users_text]
    mov edx, USERS_TEXT_MAX - 1
    call sys_read
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    test eax, eax
    js .failed
    mov byte [users_text + rax], 0
    pop rbx
    clc
    ret
.failed:
    mov byte [users_text], 0
    pop rbx
    stc
    ret

users_field_of:
    push rcx
.skip:
    test ecx, ecx
    jz .copy
.until_colon:
    mov al, [rsi]
    inc rsi
    cmp al, ':'
    je .next_field
    cmp al, 10
    je .empty
    test al, al
    jz .empty
    jmp .until_colon
.next_field:
    dec ecx
    jmp .skip
.copy:
    lea rdi, [users_field]
    xor ecx, ecx
.char:
    mov al, [rsi + rcx]
    cmp al, ':'
    je .done
    cmp al, 10
    je .done
    cmp al, 13
    je .done
    test al, al
    jz .done
    cmp ecx, USERS_FIELD_MAX - 1
    jae .done
    mov [rdi + rcx], al
    inc ecx
    jmp .char
.empty:
    xor ecx, ecx
    lea rdi, [users_field]
.done:
    mov byte [rdi + rcx], 0
    pop rcx
    ret

users_line_of_uid:
    push rbx
    mov ebx, edi
    call users_load
    jc .none
    lea rsi, [users_text]
.line:
    cmp byte [rsi], 0
    je .none
    push rsi
    mov ecx, 1
    call users_field_of
    lea rdi, [users_field]
    call users_decimal
    pop rsi
    cmp eax, ebx
    je .found
.next:
    mov al, [rsi]
    inc rsi
    test al, al
    jz .none
    cmp al, 10
    jne .next
    jmp .line
.found:
    pop rbx
    clc
    ret
.none:
    pop rbx
    stc
    ret

users_decimal:
    xor eax, eax
.digit:
    movzx ecx, byte [rdi]
    sub ecx, '0'
    cmp ecx, 9
    ja .done
    imul eax, 10
    add eax, ecx
    inc rdi
    jmp .digit
.done:
    ret

user_name:
    push rdi
    call users_line_of_uid
    pop rdi
    jc .number
    xor ecx, ecx
    call users_field_of
    lea rax, [users_field]
    ret
.number:
    jmp u64_to_dec

user_home:
    call users_line_of_uid
    jc .root
    mov ecx, 2
    call users_field_of
    cmp byte [users_field], 0
    je .root
    lea rax, [users_field]
    ret
.root:
    lea rax, [users_slash]
    ret

user_uid_of:
    push rbx
    mov rbx, rdi
    movzx eax, byte [rdi]
    sub eax, '0'
    cmp eax, 9
    ja .name
    call users_decimal
    cmp byte [rdi], 0
    jne .unknown
    pop rbx
    clc
    ret
.name:
    call users_load
    jc .unknown
    lea rsi, [users_text]
.line:
    cmp byte [rsi], 0
    je .unknown
    push rsi
    xor ecx, ecx
    call users_field_of
    lea rdi, [users_field]
    mov rsi, rbx
    call str_equal
    pop rsi
    test eax, eax
    jnz .found
.next:
    mov al, [rsi]
    inc rsi
    test al, al
    jz .unknown
    cmp al, 10
    jne .next
    jmp .line
.found:
    mov ecx, 1
    call users_field_of
    lea rdi, [users_field]
    call users_decimal
    pop rbx
    clc
    ret
.unknown:
    pop rbx
    stc
    ret

users_passwd_path db '/etc/passwd', 0
users_slash db '/', 0
