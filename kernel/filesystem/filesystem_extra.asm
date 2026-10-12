; tree helpers for the recursive commands: rm -r, mkdir -p, tree

; eax = directory entry. CF if the kernel's cwd is that directory or somewhere under it.
; walks parents up to FS_MAX_FILES steps, so a corrupt parent loop can't hang us
fs_is_current_inside:
    push rbx
    push rcx
    push r12
    mov r12d, eax
    movzx eax, word [fs_current_dir]
    mov ecx, FS_MAX_FILES
.walk:
    cmp eax, r12d
    je .inside
    test eax, eax
    jz .outside
    cmp eax, FS_MAX_FILES
    jae .outside
    dec ecx
    jz .inside
    mov ebx, eax
    shl ebx, FS_ENTRY_SHIFT
    movzx eax, word [fs_entries + rbx + FS_ENTRY_PARENT]
    jmp .walk
.inside:
    pop r12
    pop rcx
    pop rbx
    stc
    ret
.outside:
    pop r12
    pop rcx
    pop rbx
    clc
    ret

fs_tree_clear_visited:
    push rax
    push rcx
    push rdi
    xor eax, eax
    mov rdi, fs_tree_visited
    mov ecx, FS_TREE_VISITED_BYTES
    cld
    rep stosb
    pop rdi
    pop rcx
    pop rax
    ret

; eax = entry. CF if it was marked already, otherwise marks it
fs_tree_mark_visited:
    push rbx
    push rcx
    push rdx
    push r8
    push rdi
    mov ebx, eax
    cmp ebx, FS_MAX_FILES
    jae .seen
    mov r8d, ebx
    shr r8d, 3
    mov edx, ebx
    and edx, 7
    mov eax, 1
    mov cl, dl
    shl eax, cl
    mov rdi, fs_tree_visited
    add rdi, r8
    test byte [rdi], al
    jnz .seen
    or byte [rdi], al
    pop rdi
    pop r8
    pop rdx
    pop rcx
    pop rbx
    clc
    ret
.seen:
    pop rdi
    pop r8
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

fs_directory_has_children:
    push rbx
    push r12
    push r13
    mov r12d, eax
    cmp r12d, FS_MAX_FILES
    jae .empty
    xor r13d, r13d
.scan:
    cmp r13d, FS_MAX_FILES
    jae .empty
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    cmp r13d, r12d
    je .next
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r12d
    je .found
.next:
    inc r13d
    jmp .scan
.found:
    pop r13
    pop r12
    pop rbx
    stc
    ret
.empty:
    pop r13
    pop r12
    pop rbx
    clc
    ret

fs_clear_entry_memory:
    push rax
    push rcx
    push rdi
    cmp eax, FS_MAX_FILES
    jae .done
    call fs_entry_release_extent
    shl eax, FS_ENTRY_SHIFT
    lea rdi, [fs_entries + rax]
    xor eax, eax
    mov ecx, FS_ENTRY_SIZE / 8
    cld
    rep stosq
.done:
    pop rdi
    pop rcx
    pop rax
    ret

; eax = directory. depth first delete of everything below it and itself, in memory only
fs_delete_tree:
    push rbx
    push r12
    push r13
    mov r12d, eax
    test r12d, r12d
    jz .failed
    cmp r12d, FS_MAX_FILES
    jae .failed
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .failed
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .failed
    push rcx
    mov eax, r12d
    mov cl, ACCESS_WRITE or ACCESS_EXEC
    call fs_access_parent
    pop rcx
    jc .failed
    xor r13d, r13d
.scan_children:
    cmp r13d, FS_MAX_FILES
    jae .delete_directory
    cmp r13d, r12d
    je .next_child
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next_child
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r12d
    jne .next_child
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .delete_file_child
    mov eax, r13d
    call fs_delete_tree
    jc .failed
    jmp .next_child
.delete_file_child:
    mov eax, r13d
    call fs_clear_entry_memory
.next_child:
    inc r13d
    jmp .scan_children
.delete_directory:
    mov eax, r12d
    call fs_clear_entry_memory
    pop r13
    pop r12
    pop rbx
    clc
    ret
.failed:
    pop r13
    pop r12
    pop rbx
    stc
    ret

fs_mkdir_p:
    mov byte [fs_mkdir_created], 0
    mov rsi, fs_path_buffer
    mov rdi, fs_mkdir_source_path
    call fs_copy_string
    mov byte [fs_path_buffer], '/'
    mov byte [fs_path_buffer + 1], 0
    mov dword [fs_mkdir_work_length], 1
    mov dword [fs_mkdir_source_offset], 1
    cmp byte [fs_mkdir_source_path + 1], 0
    je .success
.next_component:
.skip_slashes:
    mov edx, [fs_mkdir_source_offset]
    mov rsi, fs_mkdir_source_path
    add rsi, rdx
    cmp byte [rsi], '/'
    jne .component_start
    inc edx
    mov [fs_mkdir_source_offset], edx
    jmp .skip_slashes
.component_start:
    cmp byte [rsi], 0
    je .success
    mov [fs_mkdir_component_start], edx
.find_end:
    mov edx, [fs_mkdir_source_offset]
    mov rsi, fs_mkdir_source_path
    add rsi, rdx
    mov al, [rsi]
    test al, al
    jz .component_end
    cmp al, '/'
    je .component_end
    inc edx
    mov [fs_mkdir_source_offset], edx
    jmp .find_end
.component_end:
    mov eax, [fs_mkdir_source_offset]
    sub eax, [fs_mkdir_component_start]
    mov [fs_mkdir_component_length], eax
    test eax, eax
    jz .advance
    mov eax, [fs_mkdir_work_length]
    mov r8d, [fs_mkdir_component_length]
    cmp eax, 1
    je .append_component
    mov rdi, fs_path_buffer
    add rdi, rax
    mov byte [rdi], '/'
    inc eax
.append_component:
    mov edx, [fs_mkdir_component_start]
    mov rsi, fs_mkdir_source_path
    add rsi, rdx
    mov rdi, fs_path_buffer
    add rdi, rax
    mov ecx, r8d
    cld
    rep movsb
    add eax, r8d
    mov [fs_mkdir_work_length], eax
    mov rdi, fs_path_buffer
    add rdi, rax
    mov byte [rdi], 0
    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .existing_component
    mov dl, FS_FLAG_USED or FS_FLAG_DIRECTORY
    call fs_create_entry
    jc .failed
    mov byte [fs_mkdir_created], 1
    jmp .advance
.existing_component:
    mov edx, eax
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .failed
.advance:
    mov edx, [fs_mkdir_source_offset]
    mov rsi, fs_mkdir_source_path
    add rsi, rdx
    cmp byte [rsi], 0
    je .success
    inc edx
    mov [fs_mkdir_source_offset], edx
    jmp .next_component
.success:
    movzx eax, byte [fs_mkdir_created]
    clc
    ret
.failed:
    stc
    ret

fs_tree_print:
    cmp ecx, FS_MAX_FILES
    jae .return
    push rax
    push rcx
    call fs_tree_mark_visited
    pop rcx
    pop rax
    jc .return
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, eax
    mov r14d, ecx
    xor r13d, r13d
.scan:
    cmp r13d, FS_MAX_FILES
    jae .done
    cmp r13d, r12d
    je .next
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r12d
    jne .next
    mov r15d, r13d
    mov ebx, r14d
.indent:
    test ebx, ebx
    jz .entry_type
    mov al, ' '
    call terminal_putc
    mov al, ' '
    call terminal_putc
    dec ebx
    jmp .indent
.entry_type:
    mov eax, r15d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .file_entry
    mov rsi, ls_directory_text
    call terminal_print
    jmp .entry_name
.file_entry:
    mov rsi, ls_file_text
    call terminal_print
.entry_name:
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fs_print_basename
    call terminal_newline
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .next
    cmp r14d, FS_MAX_FILES - 1
    jae .next
    mov eax, r15d
    mov ecx, r14d
    inc ecx
    call fs_tree_print
.next:
    inc r13d
    jmp .scan
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
.return:
    ret

align 8
fs_tree_visited:
    times FS_TREE_VISITED_BYTES db 0
fs_mkdir_source_path:
    times FS_PATH_MAX db 0
fs_mkdir_source_offset dd 0
fs_mkdir_component_start dd 0
fs_mkdir_component_length dd 0
fs_mkdir_work_length dd 0
fs_mkdir_created db 0
