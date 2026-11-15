FSX_ERROR_NONE = 0
FSX_ERROR_RECURSIVE_REQUIRED = 1
FSX_ERROR_DESTINATION_EXISTS = 2
FSX_ERROR_INSIDE_SOURCE = 3
FSX_ERROR_ROOT = 4
FSX_ERROR_NO_SPACE = 5
FSX_ERROR_INVALID_DESTINATION = 6
FSX_ERROR_CORRUPT = 7
FSX_ERROR_PATH_TOO_LONG = 8
FSX_ERROR_OPERATION = 9
; cp -r and mv for whole subtrees. both refuse to start on a disk fsck does not like, check
; every target path first, and keep a copy of the entry table and bitmap so a failure half way
; can be undone instead of leaving half a tree behind

FSX_UNMAPPED = 0xFFFF

fsx_initialize_maps:
    push rax
    push rcx
    push rdi
    xor eax, eax
    mov rdi, fsx_selected
    mov ecx, FS_MAX_FILES
    cld
    rep stosb
    mov ax, FSX_UNMAPPED
    mov rdi, fsx_map
    mov ecx, FS_MAX_FILES
    rep stosw
    pop rdi
    pop rcx
    pop rax
    ret

fsx_select_subtree:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, eax
    call fsx_initialize_maps
    cmp r12d, FS_MAX_FILES
    jae .failed
    mov byte [fsx_selected + r12], 1
    mov r14d, 1
.repeat:
    xor r15d, r15d
    xor r13d, r13d
.scan:
    cmp r13d, FS_MAX_FILES
    jae .scan_done
    cmp byte [fsx_selected + r13], 0
    jne .next
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, FS_MAX_FILES
    jae .next
    cmp byte [fsx_selected + rax], 0
    je .next
    mov byte [fsx_selected + r13], 1
    inc r14d
    mov r15d, 1
.next:
    inc r13d
    jmp .scan
.scan_done:
    test r15d, r15d
    jnz .repeat
    mov [fsx_selected_count], r14d
    mov eax, r14d
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret
.failed:
    xor eax, eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    stc
    ret

fsx_count_free_entries:
    push rbx
    push rdx
    xor eax, eax
    xor edx, edx
.scan:
    cmp edx, FS_MAX_FILES
    jae .done
    mov ebx, edx
    shl ebx, FS_ENTRY_SHIFT
    test byte [fs_entries + rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jnz .next
    inc eax
.next:
    inc edx
    jmp .scan
.done:
    pop rdx
    pop rbx
    ret

; rdi -> source root, rsi -> path under it. fs_path_buffer = the same path moved under
; fsx_destination_path. CF + fsx_error on a path that would be too long
fsx_rebase_path:
    push rcx
    push rsi
    push rdi
    push r8
    push r10
    push r11
    push r12
    push r13
    push r14
    mov r12, rdi
    mov r13, rsi
    cmp byte [r12], '/'
    jne .invalid
    cmp byte [r13], '/'
    jne .invalid
    mov rsi, r12
    call str_length
    mov r10, rax
    mov rsi, r13
    call str_length
    mov r11, rax
    mov rsi, fsx_destination_path
    call str_length
    mov r8, rax
    cmp r8, 1
    jb .invalid
    mov rdi, r12
    mov rsi, r13
    call fst_path_under
    test eax, eax
    jz .invalid
    cmp r11, r10
    jb .invalid
    je .destination_only
    cmp byte [r13 + r10], '/'
    jne .invalid
    mov r14, r11
    sub r14, r10
    mov rax, r8
    add rax, r14
    cmp rax, FS_PATH_MAX - 1
    ja .too_long
    mov rdi, fs_path_buffer
    mov rsi, fsx_destination_path
    mov rcx, r8
    cld
    rep movsb
    lea rsi, [r13 + r10]
    mov rcx, r14
    rep movsb
    mov byte [rdi], 0
    jmp .success
.destination_only:
    cmp r8, FS_PATH_MAX - 1
    ja .too_long
    mov rdi, fs_path_buffer
    mov rsi, fsx_destination_path
    mov rcx, r8
    cld
    rep movsb
    mov byte [rdi], 0
    jmp .success
.too_long:
    mov byte [fsx_error], FSX_ERROR_PATH_TOO_LONG
    jmp .failed
.invalid:
    mov byte [fsx_error], FSX_ERROR_CORRUPT
.failed:
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r8
    pop rdi
    pop rsi
    pop rcx
    stc
    ret
.success:
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r8
    pop rdi
    pop rsi
    pop rcx
    clc
    ret

fsx_resolve_destination_parent:
    push rbx
    push rcx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov r12, rsi
    cmp byte [r12], '/'
    jne .failed
    mov rsi, r12
    call str_length
    mov r13, rax
    cmp r13, 1
    jbe .failed
    xor r14d, r14d
    xor ecx, ecx
.scan:
    cmp rcx, r13
    jae .parent_ready
    cmp byte [r12 + rcx], '/'
    jne .next
    mov r14, rcx
.next:
    inc rcx
    jmp .scan
.parent_ready:
    test r14, r14
    jnz .copy_parent
    mov byte [fsx_parent_path], '/'
    mov byte [fsx_parent_path + 1], 0
    jmp .find_parent
.copy_parent:
    mov rsi, r12
    mov rdi, fsx_parent_path
    mov rcx, r14
    cld
    rep movsb
    mov byte [rdi], 0
.find_parent:
    mov rsi, fsx_parent_path
    call fs_find_path
    jc .failed
    mov ebx, eax
    shl ebx, FS_ENTRY_SHIFT
    test byte [fs_entries + rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .failed
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    clc
    ret
.failed:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    stc
    ret

fsx_preflight_targets:
    push rbx
    push r12
    xor r12d, r12d
.scan:
    cmp r12d, FS_MAX_FILES
    jae .success
    cmp byte [fsx_selected + r12], 0
    je .next
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    mov rdi, fsx_source_path
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fsx_rebase_path
    jc .failed
    mov rsi, fs_path_buffer
    call fs_find_path
    jc .next
    mov byte [fsx_error], FSX_ERROR_DESTINATION_EXISTS
    jmp .failed
.next:
    inc r12d
    jmp .scan
.success:
    pop r12
    pop rbx
    clc
    ret
.failed:
    pop r12
    pop rbx
    stc
    ret

fsx_snapshot_metadata:
    push rcx
    push rsi
    push rdi
    mov rsi, fs_entries
    mov rdi, fsx_entries_backup
    mov ecx, FS_ENTRIES_BYTES / 8
    cld
    rep movsq
    mov rsi, fs_bitmap
    mov rdi, fsx_bitmap_backup
    mov ecx, FS_BITMAP_BYTES / 8
    rep movsq
    mov rsi, fs_current_path
    mov rdi, fsx_current_path_backup
    call fs_copy_string
    pop rdi
    pop rsi
    pop rcx
    ret

fsx_restore_metadata:
    push rcx
    push rsi
    push rdi
    mov rsi, fsx_entries_backup
    mov rdi, fs_entries
    mov ecx, FS_ENTRIES_BYTES / 8
    cld
    rep movsq
    mov rsi, fsx_bitmap_backup
    mov rdi, fs_bitmap
    mov ecx, FS_BITMAP_BYTES / 8
    rep movsq
    mov rsi, fsx_current_path_backup
    mov rdi, fs_current_path
    call fs_copy_string
    pop rdi
    pop rsi
    pop rcx
    ret

fsx_rollback_copy:
    push rax
    push rbx
    push r12
    mov bl, [fsx_error]
    mov r12d, [fsx_destination_root]
    cmp r12d, 0xFFFFFFFF
    je .restore_error
    cmp r12d, FS_MAX_FILES
    jae .restore_error
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .restore_error
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .clear_file
    mov eax, r12d
    call fs_delete_tree
    jmp .save
.clear_file:
    mov eax, r12d
    call fs_clear_entry_memory
.save:
    call fs_save_entries
.restore_error:
    mov [fsx_error], bl
    pop r12
    pop rbx
    pop rax
    ret

; copies fsx_source_path to fsx_destination_path. files are copied through fs_file_buffer one
; at a time, directories are created parent first. out: CF + fsx_error = FSX_ERROR_*
fsx_copy_tree:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov byte [fsx_error], FSX_ERROR_NONE
    mov dword [fsx_destination_root], 0xFFFFFFFF
    mov r12d, eax
    mov r13, rsi
    movzx r14d, dl
    cmp r12d, FS_MAX_FILES
    jae .corrupt
    mov rsi, r13
    mov rdi, fsx_destination_path
    call fs_copy_string
    call fst_run_fsck
    test eax, eax
    jnz .corrupt
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .corrupt
    lea rsi, [rbx + FS_ENTRY_PATH]
    mov rdi, fsx_source_path
    call fs_copy_string
    mov al, [rbx + FS_ENTRY_FLAGS]
    test al, FS_FLAG_DIRECTORY
    jz .source_file
    mov byte [fsx_source_directory], 1
    test r14d, r14d
    jz .recursive_required
    test r12d, r12d
    jz .root_error
    mov rdi, fsx_source_path
    mov rsi, fsx_destination_path
    call fst_path_under
    test eax, eax
    jnz .inside_source
    jmp .destination_check
.source_file:
    mov byte [fsx_source_directory], 0
.destination_check:
    mov rsi, fsx_destination_path
    call fs_find_path
    jnc .destination_exists
    mov rsi, fsx_destination_path
    call fsx_resolve_destination_parent
    jc .invalid_destination
    mov [fsx_destination_parent], ax
    mov eax, r12d
    call fsx_select_subtree
    jc .corrupt
    call fsx_count_free_entries
    cmp eax, [fsx_selected_count]
    jb .no_space
    call fsx_preflight_targets
    jc .failed
    call fs_find_free
    jc .no_space
    mov [fsx_destination_root], eax
    mov rsi, fsx_destination_path
    mov rdi, fs_path_buffer
    call fs_copy_string
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    mov dl, [fs_entries + rax + FS_ENTRY_FLAGS]
    call fs_create_entry
    jc .operation_rollback
    mov [fsx_destination_root], eax
    mov edx, r12d
    mov [fsx_map + rdx * 2], ax
    mov dword [fsx_copied_count], 1
    cmp byte [fsx_source_directory], 0
    jne .copy_children
    mov eax, r12d
    mov cl, ACCESS_READ
    call fs_access
    jc .operation_rollback
    call fs_load_file
    jc .operation_rollback
    mov eax, [fsx_destination_root]
    call fs_store_file
    jc .store_failed
    jmp .finalize
.copy_children:
.outer:
    mov eax, [fsx_copied_count]
    cmp eax, [fsx_selected_count]
    jae .finalize
    xor r15d, r15d
    xor r14d, r14d
.scan_children:
    cmp r14d, FS_MAX_FILES
    jae .scan_complete
    cmp byte [fsx_selected + r14], 0
    je .next_child
    cmp word [fsx_map + r14 * 2], FSX_UNMAPPED
    jne .next_child
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    movzx edx, word [rbx + FS_ENTRY_PARENT]
    cmp edx, FS_MAX_FILES
    jae .corrupt_rollback
    cmp word [fsx_map + rdx * 2], FSX_UNMAPPED
    je .next_child
    mov rdi, fsx_source_path
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fsx_rebase_path
    jc .rollback
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    mov dl, [fs_entries + rax + FS_ENTRY_FLAGS]
    call fs_create_entry
    jc .operation_rollback
    mov [fsx_map + r14 * 2], ax
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .child_created
    mov eax, r14d
    mov cl, ACCESS_READ
    call fs_access
    jc .operation_rollback
    call fs_load_file
    jc .operation_rollback
    movzx eax, word [fsx_map + r14 * 2]
    call fs_store_file
    jc .store_failed
.child_created:
    inc dword [fsx_copied_count]
    mov r15d, 1
.next_child:
    inc r14d
    jmp .scan_children
.scan_complete:
    test r15d, r15d
    jz .corrupt_rollback
    jmp .outer
.finalize:
    call fs_save_entries
    jc .operation_rollback
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    clc
    ret
.recursive_required:
    mov byte [fsx_error], FSX_ERROR_RECURSIVE_REQUIRED
    jmp .failed
.destination_exists:
    mov byte [fsx_error], FSX_ERROR_DESTINATION_EXISTS
    jmp .failed
.inside_source:
    mov byte [fsx_error], FSX_ERROR_INSIDE_SOURCE
    jmp .failed
.root_error:
    mov byte [fsx_error], FSX_ERROR_ROOT
    jmp .failed
.no_space:
    mov byte [fsx_error], FSX_ERROR_NO_SPACE
    jmp .failed
.invalid_destination:
    mov byte [fsx_error], FSX_ERROR_INVALID_DESTINATION
    jmp .failed
.corrupt:
    mov byte [fsx_error], FSX_ERROR_CORRUPT
    jmp .failed
.corrupt_rollback:
    mov byte [fsx_error], FSX_ERROR_CORRUPT
    jmp .rollback
.store_failed:
    cmp byte [fs_no_space], 0
    je .operation_rollback
    mov byte [fsx_error], FSX_ERROR_NO_SPACE
    jmp .rollback
.operation_rollback:
    mov byte [fsx_error], FSX_ERROR_OPERATION
.rollback:
    call fsx_rollback_copy
.failed:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

; eax = entry to move, rsi -> new path. a move never touches data sectors, it only rewrites
; the paths of the whole subtree and the parent of its root. the cwd moves along when it was
; inside. CF + fsx_error on failure, with the old table put back
fsx_move_tree:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov byte [fsx_error], FSX_ERROR_NONE
    mov r12d, eax
    mov r13, rsi
    cmp r12d, FS_MAX_FILES
    jae .corrupt
    test r12d, r12d
    jz .root_error
    mov rsi, r13
    mov rdi, fsx_destination_path
    call fs_copy_string
    call fst_run_fsck
    test eax, eax
    jnz .corrupt
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .corrupt
    lea rsi, [rbx + FS_ENTRY_PATH]
    mov rdi, fsx_source_path
    call fs_copy_string
    mov rsi, fsx_destination_path
    call fs_find_path
    jnc .destination_exists
    mov rdi, fsx_source_path
    mov rsi, fsx_destination_path
    call fst_path_under
    test eax, eax
    jnz .inside_source
    mov rsi, fsx_destination_path
    call fsx_resolve_destination_parent
    jc .invalid_destination
    mov [fsx_destination_parent], ax
    mov eax, r12d
    mov cl, ACCESS_WRITE or ACCESS_EXEC
    call fs_access_parent
    jc .denied
    movzx eax, word [fsx_destination_parent]
    call fs_access
    jc .denied
    mov eax, r12d
    call fsx_select_subtree
    jc .corrupt
    call fsx_preflight_targets
    jc .failed
    call fsx_snapshot_metadata
    xor r14d, r14d
.rewrite_loop:
    cmp r14d, FS_MAX_FILES
    jae .set_parent
    cmp byte [fsx_selected + r14], 0
    je .next_entry
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    mov rdi, fsx_source_path
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fsx_rebase_path
    jc .restore
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    lea rdi, [fs_entries + rax + FS_ENTRY_PATH]
    mov rsi, fs_path_buffer
    call fs_copy_string
.next_entry:
    inc r14d
    jmp .rewrite_loop
.set_parent:
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    mov dx, [fsx_destination_parent]
    mov [fs_entries + rax + FS_ENTRY_PARENT], dx
    mov rdi, fsx_source_path
    mov rsi, fs_current_path
    call fst_path_under
    test eax, eax
    jz .save
    mov rdi, fsx_source_path
    mov rsi, fs_current_path
    call fsx_rebase_path
    jc .restore
    mov rsi, fs_path_buffer
    mov rdi, fs_current_path
    call fs_copy_string
.save:
    call fs_save_entries
    jc .operation_restore
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    clc
    ret
.destination_exists:
    mov byte [fsx_error], FSX_ERROR_DESTINATION_EXISTS
    jmp .failed
.inside_source:
    mov byte [fsx_error], FSX_ERROR_INSIDE_SOURCE
    jmp .failed
.root_error:
    mov byte [fsx_error], FSX_ERROR_ROOT
    jmp .failed
.invalid_destination:
    mov byte [fsx_error], FSX_ERROR_INVALID_DESTINATION
    jmp .failed
.corrupt:
    mov byte [fsx_error], FSX_ERROR_CORRUPT
    jmp .failed
.denied:
    mov byte [fsx_error], FSX_ERROR_OPERATION
    jmp .failed
.operation_restore:
    mov byte [fsx_error], FSX_ERROR_OPERATION
.restore:
    call fsx_restore_metadata
    call fs_save_entries
.failed:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

align 16
fsx_selected:
    times FS_MAX_FILES db 0
fsx_map:
    times FS_MAX_FILES dw 0
fsx_source_path:
    times FS_PATH_MAX db 0
fsx_destination_path:
    times FS_PATH_MAX db 0
fsx_parent_path:
    times FS_PATH_MAX db 0
fsx_current_path_backup:
    times FS_PATH_MAX db 0
align 16
fsx_entries_backup = FS_ENTRIES_BACKUP_ADDRESS
fsx_bitmap_backup = FS_BITMAP_BACKUP_ADDRESS
fsx_selected_count dd 0
fsx_copied_count dd 0
fsx_destination_root dd 0xFFFFFFFF
fsx_destination_parent dw 0
fsx_source_directory db 0
fsx_error db 0
