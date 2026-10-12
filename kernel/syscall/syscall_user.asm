; users. /etc/passwd is name:uid:home and anyone can read it, /etc/shadow is
; name:salt:sha256(salt + password) in hex and only root can. the password check lives
; here in the kernel on purpose, login and su only ask, so a broken su cant let anyone in

USER_NAME_MAX = 32
USER_PASSWORD_MAX = 64
USER_TEXT_MAX = 4096
USER_SALT_BYTES = 8

users_read_file:
    push rax
    push rcx
    push rdi
    push rsi
    call fs_find_path
    jc .failed
    mov ecx, eax
    shl ecx, FS_ENTRY_SHIFT
    test byte [fs_entries + rcx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .failed
    call fs_load_file
    jc .failed
    mov ecx, [fs_file_size]
    cmp ecx, USER_TEXT_MAX - 1
    ja .failed
    mov [user_text_size], ecx
    mov rsi, fs_file_buffer
    mov rdi, user_text
    cld
    rep movsb
    mov byte [rdi], 0
    pop rsi
    pop rdi
    pop rcx
    pop rax
    clc
    ret
.failed:
    pop rsi
    pop rdi
    pop rcx
    pop rax
    stc
    ret

users_line_is:
    push rdi
.compare:
    mov al, [rsi]
    cmp al, ':'
    je .end
    cmp al, 10
    je .no
    test al, al
    jz .no
    cmp al, [rdi]
    jne .no
    inc rsi
    inc rdi
    jmp .compare
.end:
    cmp byte [rdi], 0
    jne .no
    inc rsi
    pop rdi
    clc
    ret
.no:
    pop rdi
    stc
    ret

users_next_line:
.scan:
    mov al, [rsi]
    test al, al
    jz .end
    inc rsi
    cmp al, 10
    jne .scan
    cmp byte [rsi], 0
.end:
    ret

users_copy_field:
    push rcx
.copy:
    mov al, [rsi]
    cmp al, ':'
    je .done
    cmp al, 10
    je .done
    cmp al, 13
    je .done
    test al, al
    jz .done
    cmp ecx, 1
    jbe .skip
    dec ecx
    mov [rdi], al
    inc rdi
.skip:
    inc rsi
    jmp .copy
.done:
    mov byte [rdi], 0
    cmp byte [rsi], ':'
    jne .out
    inc rsi
.out:
    pop rcx
    ret

users_parse_number:
    xor eax, eax
.digit:
    movzx ecx, byte [rsi]
    sub ecx, '0'
    cmp ecx, 9
    ja .done
    imul eax, 10
    add eax, ecx
    inc rsi
    jmp .digit
.done:
    cmp byte [rsi], ':'
    jne .out
    inc rsi
.out:
    ret

users_take_record:
    call users_parse_number
    mov [users_uid], eax
    mov rdi, users_home
    mov ecx, FS_PATH_MAX
    call users_copy_field
    ret

user_find_by_name:
    push rsi
    push rdi
    push rcx
    push r8
    mov r8, rdi
    mov rsi, passwd_path
    call users_read_file
    jc .missing
    mov rsi, user_text
.line:
    cmp byte [rsi], 0
    je .missing
    push rsi
    call users_line_is
    jc .other
    add rsp, 8
    call users_take_record
    mov rsi, r8
    mov rdi, users_name
    mov ecx, USER_NAME_MAX
.name:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jz .found
    dec ecx
    jnz .name
    mov byte [rdi - 1], 0
.found:
    pop r8
    pop rcx
    pop rdi
    pop rsi
    clc
    ret
.other:
    pop rsi
    call users_next_line
    jmp .line
.missing:
    pop r8
    pop rcx
    pop rdi
    pop rsi
    stc
    ret

user_name_of:
    push rax
    push rcx
    push rdx
    push rdi
    push r8
    mov r8d, eax
    mov rsi, passwd_path
    call users_read_file
    jc .number
    mov rsi, user_text
.line:
    cmp byte [rsi], 0
    je .number
    mov rdx, rsi
.skip_name:
    mov al, [rsi]
    cmp al, ':'
    je .uid
    cmp al, 10
    je .next
    test al, al
    jz .number
    inc rsi
    jmp .skip_name
.uid:
    inc rsi
    call users_parse_number
    cmp eax, r8d
    jne .next
    mov rsi, rdx
    mov rdi, users_name
    mov ecx, USER_NAME_MAX
    call users_copy_field
    mov rsi, users_name
    jmp .out
.next:
    call users_next_line
    jnz .line
.number:
    mov eax, r8d
    mov rsi, users_number_text + 11
    mov byte [rsi], 0
    mov ecx, 10
.digit:
    xor edx, edx
    div ecx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test eax, eax
    jnz .digit
.out:
    pop r8
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

user_check_password:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    mov rbx, rsi
    mov rsi, shadow_path
    call users_read_file
    jc .wrong
    mov rsi, user_text
.line:
    cmp byte [rsi], 0
    je .wrong
    push rsi
    call users_line_is
    jc .other
    add rsp, 8
    mov rdi, user_salt
    mov ecx, USER_SALT_BYTES
    call hex_to_bytes
    jc .wrong
    cmp byte [rsi], ':'
    jne .wrong
    inc rsi
    mov rdi, user_expected
    mov ecx, 32
    call hex_to_bytes
    jc .wrong
    mov rsi, rbx
    call user_hash_password
    mov rsi, sha_digest
    mov rdi, user_expected
    mov ecx, 32
    xor eax, eax
.compare:
    mov dl, [rsi]
    xor dl, [rdi]
    or al, dl
    inc rsi
    inc rdi
    dec ecx
    jnz .compare
    test al, al
    jnz .wrong
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    clc
    ret
.other:
    pop rsi
    call users_next_line
    jmp .line
.wrong:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    stc
    ret

user_hash_password:
    push rcx
    push rdi
    push rsi
    mov rdi, user_hash_input
    push rsi
    mov rsi, user_salt
    mov ecx, USER_SALT_BYTES
    cld
    rep movsb
    pop rsi
    mov ecx, USER_SALT_BYTES
.copy:
    lodsb
    test al, al
    jz .ready
    cmp ecx, USER_SALT_BYTES + USER_PASSWORD_MAX
    jae .ready
    stosb
    inc ecx
    jmp .copy
.ready:
    mov rsi, user_hash_input
    call sha256
    pop rsi
    pop rdi
    pop rcx
    ret

; rdi -> name, rsi -> new password. writes /etc/shadow again with a fresh salt for this
; user, the other lines are copied as they are. CF on failure
user_set_password:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    mov rbx, rsi
    mov r12, rdi
    call random_next
    mov [user_salt], rax
    mov rsi, rbx
    call user_hash_password
    mov rsi, shadow_path
    call users_read_file
    jnc .loaded
    mov byte [user_text], 0
.loaded:
    mov rdi, fs_file_buffer
    mov rsi, user_text
.line:
    cmp byte [rsi], 0
    je .append
    push rsi
    push rdi
    mov rdi, r12
    call users_line_is
    pop rdi
    pop rsi
    jnc .drop
.keep:
    lodsb
    stosb
    test al, al
    jz .text_end
    cmp al, 10
    jne .keep
    jmp .line
.text_end:
    dec rdi
    cmp byte [rdi - 1], 10
    je .append
    mov byte [rdi], 10
    inc rdi
    jmp .append
.drop:
    call users_next_line
    jmp .line
.append:
    mov rsi, r12
.name:
    lodsb
    test al, al
    jz .salt
    stosb
    jmp .name
.salt:
    mov al, ':'
    stosb
    mov rsi, user_salt
    mov ecx, USER_SALT_BYTES
    call bytes_to_hex
    mov al, ':'
    stosb
    mov rsi, sha_digest
    mov ecx, 32
    call bytes_to_hex
    mov al, 10
    stosb
    mov rax, rdi
    sub rax, fs_file_buffer
    mov [fs_file_size], eax
    mov rsi, shadow_path
    call fs_find_path
    jnc .store
    mov rsi, shadow_path
    mov rdi, fs_path_buffer
    call fs_copy_string
    mov dl, FS_FLAG_USED
    call fs_create_entry
    jc .failed
    mov ecx, eax
    shl ecx, FS_ENTRY_SHIFT
    mov word [fs_entries + rcx + FS_ENTRY_OWNER], 0
    mov word [fs_entries + rcx + FS_ENTRY_MODE], 600o
.store:
    call fs_store_file
    jc .failed
    call fs_save_entries
    jc .failed
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    clc
    ret
.failed:
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    stc
    ret

hex_to_bytes:
.byte:
    call .nibble
    jc .bad
    mov dl, al
    shl dl, 4
    call .nibble
    jc .bad
    or dl, al
    mov [rdi], dl
    inc rdi
    dec ecx
    jnz .byte
    clc
.bad:
    ret
.nibble:
    mov al, [rsi]
    inc rsi
    sub al, '0'
    cmp al, 9
    jbe .ok
    sub al, 'a' - '0'
    cmp al, 5
    ja .not_hex
    add al, 10
.ok:
    clc
    ret
.not_hex:
    stc
    ret

bytes_to_hex:
.byte:
    movzx eax, byte [rsi]
    shr eax, 4
    mov al, [hex_lower + rax]
    stosb
    movzx eax, byte [rsi]
    and eax, 15
    mov al, [hex_lower + rax]
    stosb
    inc rsi
    dec ecx
    jnz .byte
    ret


syscall_getuid:
    call process_current_uid
    ret

; rdi path, rsi args, rdx -> user name, r10 -> password or 0. starts the program as that
; user, in their home. without a password only root may do it, that is how login works
; after it checked the password itself, and how root's su skips the question
syscall_spawnas:
    push rbx
    push r12
    push r13
    push r14
    mov r12, rdi
    mov r13, rsi
    mov r14, r10
    mov rdi, rdx
    mov rsi, user_name_input
    call syscall_copy_user_string
    jc .invalid
    mov rdi, user_name_input
    call user_find_by_name
    jc .not_found
    test r14, r14
    jnz .password
    call need_root
    jc .denied
    jmp .allowed
.password:
    mov rdi, r14
    mov rsi, user_password_input
    call syscall_copy_user_string
    jc .invalid
    mov rdi, user_name_input
    mov rsi, user_password_input
    call user_check_password
    call user_forget_password
    jc .denied
.allowed:
    mov eax, [users_uid]
    mov [spawn_uid], eax
    mov rsi, users_home
    mov rdi, spawn_home
    call fs_copy_string
    mov rdi, r12
    mov rsi, r13
    call process_spawn
    mov dword [spawn_uid], -1
    mov byte [spawn_home], 0
    jmp .out
.invalid:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .out
.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .out
.denied:
    mov eax, SYS_ERR_ACCESS
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

user_forget_password:
    pushf
    push rax
    push rcx
    push rdi
    mov rdi, user_password_input
    mov ecx, FS_PATH_MAX
    xor eax, eax
    cld
    rep stosb
    mov rdi, user_hash_input
    mov ecx, USER_SALT_BYTES + USER_PASSWORD_MAX
    rep stosb
    pop rdi
    pop rcx
    pop rax
    popf
    ret

syscall_chmod:
    push rbx
    mov ebx, esi
    call syscall_user_path_resolve
    jc .done
    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found
    call fs_owns
    jc .denied
    shl eax, FS_ENTRY_SHIFT
    and ebx, 777o
    mov [fs_entries + rax + FS_ENTRY_MODE], bx
    call fs_save_entries
    jc .io
    xor eax, eax
    jmp .done
.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .done
.denied:
    mov eax, SYS_ERR_ACCESS
    jmp .done
.io:
    mov eax, SYS_ERR_IO
.done:
    pop rbx
    ret

; rdi path, esi uid. root only, giving files away would be a nice way to dodge things
syscall_chown:
    push rbx
    mov ebx, esi
    call syscall_user_path_resolve
    jc .done
    call need_root
    jc .denied
    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found
    shl eax, FS_ENTRY_SHIFT
    mov [fs_entries + rax + FS_ENTRY_OWNER], bx
    call fs_save_entries
    jc .io
    xor eax, eax
    jmp .done
.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .done
.denied:
    mov eax, SYS_ERR_ACCESS
    jmp .done
.io:
    mov eax, SYS_ERR_IO
.done:
    pop rbx
    ret

syscall_passwd:
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13, rdx
    mov rsi, user_name_input
    call syscall_copy_user_string
    jc .invalid
    mov rdi, user_name_input
    call user_find_by_name
    jc .not_found
    call process_current_uid
    test eax, eax
    jz .new
    cmp eax, [users_uid]
    jne .denied
    mov rdi, r12
    mov rsi, user_password_input
    call syscall_copy_user_string
    jc .invalid
    mov rdi, user_name_input
    mov rsi, user_password_input
    call user_check_password
    call user_forget_password
    jc .denied
.new:
    mov rdi, r13
    mov rsi, user_password_input
    call syscall_copy_user_string
    jc .invalid
    mov rdi, user_name_input
    mov rsi, user_password_input
    call user_set_password
    call user_forget_password
    jc .io
    xor eax, eax
    jmp .out
.invalid:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .out
.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .out
.denied:
    mov eax, SYS_ERR_ACCESS
    jmp .out
.io:
    mov eax, SYS_ERR_IO
.out:
    pop r13
    pop r12
    pop rbx
    ret

passwd_path db '/etc/passwd', 0
shadow_path db '/etc/shadow', 0
hex_lower db '0123456789abcdef'
users_number_text db 12 dup 0

align 8
spawn_uid dd -1
users_uid dd 0
user_text_size dd 0
user_salt dq 0
users_name:
    times USER_NAME_MAX db 0
users_home:
    times FS_PATH_MAX db 0
spawn_home:
    times FS_PATH_MAX db 0
user_name_input:
    times FS_PATH_MAX db 0
user_password_input:
    times FS_PATH_MAX db 0
user_expected:
    times 32 db 0
user_hash_input:
    times USER_SALT_BYTES + USER_PASSWORD_MAX db 0
user_text:
    times USER_TEXT_MAX db 0
