; reads current into entries, then sorts
load_directory:
    push rbx
    push r12
    mov dword [entry_count], 0
    mov dword [selected], 0
    mov dword [top], 0
    lea rdi, [current]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
.next:
    cmp dword [entry_count], ENTRY_MAX
    jae .close
    mov edi, ebx
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .close
    mov eax, [entry_count]
    call entry_address
    mov r12, rax
    mov eax, [dirent + SYS_DIRENT_FLAGS]
    and eax, FS_FLAG_DIRECTORY
    mov [r12 + ENTRY_FLAGS], eax
    mov eax, [dirent + SYS_DIRENT_SIZE]
    mov [r12 + ENTRY_SIZE], eax
    lea rdi, [r12 + ENTRY_NAME]
    lea rsi, [dirent + SYS_DIRENT_NAME]
    mov ecx, ENTRY_NAME_MAX
.copy:
    mov al, [rsi]
    test al, al
    jz .copied
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .copy
.copied:
    mov byte [rdi], 0
    inc dword [entry_count]
    jmp .next
.close:
    mov edi, ebx
    call sys_close
    call sort_entries
.done:
    pop r12
    pop rbx
    ret

; insertion sort, folders first, then names ignoring case
sort_entries:
    push rbx
    mov ebx, 1
.item:
    cmp ebx, [entry_count]
    jae .done
    mov eax, ebx
    call entry_address
    mov rsi, rax
    lea rdi, [swap]
    mov ecx, ENTRY_BYTES
    rep movsb
    mov edx, ebx
.shift:
    test edx, edx
    jz .place
    lea eax, [rdx - 1]
    call entry_address
    mov rsi, rax
    lea rdi, [swap]
    call entry_before
    jnc .place
    lea eax, [rdx - 1]
    call entry_address
    mov rsi, rax
    lea rdi, [rax + ENTRY_BYTES]
    mov ecx, ENTRY_BYTES
    rep movsb
    dec edx
    jmp .shift
.place:
    mov eax, edx
    call entry_address
    mov rdi, rax
    lea rsi, [swap]
    mov ecx, ENTRY_BYTES
    rep movsb
    inc ebx
    jmp .item
.done:
    pop rbx
    ret

entry_before:
    mov eax, [rdi + ENTRY_FLAGS]
    cmp eax, [rsi + ENTRY_FLAGS]
    je .names
    test eax, eax
    jnz .yes
    jmp .no
.names:
    xor ecx, ecx
.compare:
    mov al, [rdi + ENTRY_NAME + rcx]
    mov ah, [rsi + ENTRY_NAME + rcx]
    or al, 0x20
    or ah, 0x20
    cmp al, ah
    jb .yes
    ja .no
    cmp byte [rdi + ENTRY_NAME + rcx], 0
    je .no
    inc ecx
    jmp .compare
.yes:
    stc
    ret
.no:
    clc
    ret

enter_path:
    lea rdi, [current]
    call str_copy
    jmp load_directory

go_parent:
    lea rdi, [current]
    call str_len
    lea rcx, [current + rax]
.back:
    dec rcx
    cmp rcx, current
    jbe .root
    cmp byte [rcx], '/'
    jne .back
    mov byte [rcx], 0
    jmp load_directory
.root:
    mov word [current], '/'
    jmp load_directory

selected_path:
    lea rdi, [target]
    lea rsi, [current]
    call str_copy
    lea rdi, [target]
    call str_len
    lea rdi, [target + rax]
    cmp byte [rdi - 1], '/'
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    mov eax, [selected]
    call entry_address
    lea rsi, [rax + ENTRY_NAME]
    jmp str_copy

open_selected:
    cmp dword [entry_count], 0
    je .done
    call selected_path
    mov eax, [selected]
    call entry_address
    cmp dword [rax + ENTRY_FLAGS], 0
    je .file
    lea rsi, [target]
    jmp enter_path
.file:
    lea rdi, [target]
    call str_len
    cmp eax, 5
    jb .edit
    cmp dword [target + rax - 4], 'serp'
    jne .edit
    cmp byte [target + rax - 5], '.'
    jne .edit
    lea rdi, [target]
    xor esi, esi
    call sys_spawn
    lea rsi, [running_text]
    jmp set_message
.edit:
    lea rdi, [editor_path]
    lea rsi, [target]
    call sys_spawn
    lea rsi, [opening_text]
    jmp set_message
.done:
    ret

delete_selected:
    cmp dword [entry_count], 0
    je .done
    call selected_path
    lea rdi, [target]
    call sys_unlink
    test eax, eax
    js .failed
    mov eax, [selected]
    push rax
    call load_directory
    pop rax
    cmp eax, [entry_count]
    jb .keep
    mov eax, [entry_count]
    dec eax
    js .done
.keep:
    mov [selected], eax
    lea rsi, [deleted_text]
    jmp set_message
.failed:
    lea rsi, [delete_failed_text]
    jmp set_message
.done:
    ret

set_message:
    mov [message], rsi
    ret
