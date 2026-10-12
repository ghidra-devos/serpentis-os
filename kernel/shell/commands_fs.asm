; file commands for the kernel side: ls cd mkdir rm cp mv stat tree hexdump wc fsinfo sync
; format find du df fsck. paths are relative to the kernel cwd, which syscall_kcommand sets to
; the caller's cwd for the duration of the command

; ls [-l] [path]. -l adds the rwx bits and the owner
cmd_ls:
    cmp byte [fs_ready], 0
    je fs_not_ready
    mov byte [ls_long], 0
    cmp rdi, 1
    je .current
    mov rax, [rsi + 8]
    cmp word [rax], '-l'
    jne .path
    cmp byte [rax + 2], 0
    jne .path
    mov byte [ls_long], 1
    add rsi, 8
    dec rdi
    cmp rdi, 1
    je .current
.path:
    mov rsi, [rsi + 8]
    call fs_make_path
    jc fs_invalid_path
    jmp .resolve
.current:
    mov rsi, fs_current_path
    mov rdi, fs_path_buffer
    call fs_copy_string
.resolve:
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r12d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz fs_not_directory
    mov eax, r12d
    mov cl, ACCESS_READ
    call fs_access
    jc fs_operation_failed
    xor r13d, r13d
.loop:
    cmp r13d, FS_MAX_FILES
    jae .done
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r12d
    jne .next
    cmp r13d, r12d
    je .next
    cmp byte [ls_long], 0
    je .short
    call ls_print_mode
    jmp .name
.short:
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .file
    mov rsi, ls_directory_text
    call terminal_print
    jmp .name
.file:
    mov rsi, ls_file_text
    call terminal_print
.name:
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fs_print_basename
    call terminal_newline
.next:
    inc r13d
    jmp .loop
.done:
    ret

; rbx -> entry. drwxr-xr-x owner, the owner padded to 9
ls_print_mode:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    mov rdi, ls_mode_text
    mov al, '-'
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .kind
    mov al, 'd'
.kind:
    stosb
    movzx edx, word [rbx + FS_ENTRY_MODE]
    mov ecx, 8
.bit:
    mov al, '-'
    bt edx, ecx
    jnc .put
    mov eax, ecx
    xor edx, edx
    push rcx
    mov ecx, 3
    div ecx
    pop rcx
    mov al, [ls_rwx + rdx]
    movzx edx, word [rbx + FS_ENTRY_MODE]
.put:
    stosb
    dec ecx
    jns .bit
    mov al, ' '
    stosb
    mov byte [rdi], 0
    mov rsi, ls_mode_text
    call terminal_print
    movzx eax, word [rbx + FS_ENTRY_OWNER]
    call user_name_of
    call str_length
    push rax
    call terminal_print
    pop rax
    mov ecx, 9
    sub ecx, eax
    jle .done
.pad:
    mov al, ' '
    call terminal_putc
    dec ecx
    jnz .pad
.done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

ls_long db 0
ls_rwx db 'xwr'
ls_mode_text db 12 dup 0

cmd_cd:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 1
    je .root
    mov r12, rsi
    mov rdi, [r12 + 8]
    mov rsi, fs_parent_command
    call str_equal
    test eax, eax
    jnz .parent
    mov rsi, [r12 + 8]
    call fs_cd
    jc fs_not_found
    ret
.root:
    call fs_set_root_path
    ret
.parent:
    call fs_cd_parent
    jc fs_not_found
    ret

cmd_mkdir:
    cmp byte [fs_ready], 0
    je fs_not_ready
    mov r13, rdi
    mov r12, rsi
    cmp r13, 2
    jb .usage
    xor r14d, r14d
    mov rdi, [r12 + 8]
    mov rsi, mkdir_parent_option
    call str_equal
    test eax, eax
    jz .single_directory
    cmp r13, 3
    jne .usage
    mov r14d, 1
    mov rsi, [r12 + 16]
    jmp .make_path
.single_directory:
    cmp r13, 2
    jne .usage
    mov rsi, [r12 + 8]
.make_path:
    call fs_make_path
    jc fs_invalid_path
    cmp r14d, 0
    jne .make_parents
    mov rsi, fs_path_buffer
    call fs_find_path
    jnc fs_already_exists
    mov dl, FS_FLAG_USED or FS_FLAG_DIRECTORY
    call fs_create_entry
    jc fs_create_failed
    mov rsi, mkdir_done_text
    call terminal_print_line
    ret
.make_parents:
    call fs_mkdir_p
    jc fs_create_failed
    test eax, eax
    jz .already_exists
    mov rsi, mkdir_done_text
    call terminal_print_line
    ret
.already_exists:
    mov rsi, mkdir_exists_text
    call terminal_print_line
    ret
.usage:
    mov rsi, mkdir_usage
    call shell_error
    ret

cmd_rmdir:
    cmp byte [fs_ready], 0
    je fs_not_ready
    mov r13, rdi
    mov r12, rsi
    cmp r13, 2
    je .normal
    cmp r13, 3
    jne .usage
    mov rdi, [r12 + 8]
    mov rsi, rmdir_recursive_option
    call str_equal
    test eax, eax
    jz .usage
    mov r14d, 1
    mov rsi, [r12 + 16]
    jmp .resolve
.normal:
    xor r14d, r14d
    mov rsi, [r12 + 8]
.resolve:
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz fs_not_directory
    test r13d, r13d
    jz fs_cannot_remove_root
    mov eax, r13d
    call fs_is_current_inside
    jc fs_cannot_remove_current
    cmp r14d, 0
    jne .recursive
    mov eax, r13d
    call fs_directory_has_children
    jc fs_directory_not_empty
    mov eax, r13d
    call fs_delete_entry
    jc fs_operation_failed
    mov rsi, rmdir_done_text
    call terminal_print_line
    ret
.recursive:
    mov eax, r13d
    call fs_delete_tree
    jc fs_operation_failed
    call fs_save_entries
    jc fs_operation_failed
    mov rsi, rmdir_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, rmdir_usage
    call shell_error
    ret

cmd_touch:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .exists
    mov rdx, FS_FLAG_USED
    call fs_create_entry
    jc fs_create_failed
.exists:
    mov rsi, touch_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, touch_usage
    call shell_error
    ret

cmd_write:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 3
    jb .usage
    mov r12, rdi
    mov r13, rsi
    mov rsi, [r13 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .existing
    mov rdx, FS_FLAG_USED
    call fs_create_entry
    jc fs_create_failed
    mov r14d, eax
    jmp .prepare
.existing:
    mov r14d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz fs_not_file
.prepare:
    call fs_clear_file_buffer
    mov rdi, r12
    mov rsi, r13
    mov edx, 2
    call fs_join_arguments
    jc fs_file_too_large
    mov eax, r14d
    mov cl, ACCESS_WRITE
    call fs_access
    jc fs_operation_failed
    call fs_store_file
    jc fs_operation_failed
    mov rsi, write_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, write_usage
    call shell_error
    ret

cmd_append:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 3
    jb .usage
    mov r12, rdi
    mov r13, rsi
    mov rsi, [r13 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r14d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz fs_not_file
    mov eax, r14d
    mov cl, ACCESS_READ or ACCESS_WRITE
    call fs_access
    jc fs_operation_failed
    call fs_load_file
    jc fs_operation_failed
    mov rdi, r12
    mov rsi, r13
    mov edx, 2
    call fs_append_arguments
    jc fs_file_too_large
    mov eax, r14d
    call fs_store_file
    jc fs_operation_failed
    mov rsi, append_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, append_usage
    call shell_error
    ret

cmd_cat:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz fs_not_file
    mov eax, r13d
    mov cl, ACCESS_READ
    call fs_access
    jc fs_operation_failed
    call fs_load_file
    jc fs_operation_failed
    xor r14d, r14d
.print:
    cmp r14d, [fs_file_size]
    jae .finish
    mov al, [fs_file_buffer + r14]
    call terminal_putc
    inc r14d
    jmp .print
.finish:
    call terminal_newline
    ret
.usage:
    mov rsi, cat_usage
    call shell_error
    ret

; rm [-r] [-f] path. -r for directories, never the root or a directory the cwd sits in,
; -f makes a missing path quiet
cmd_rm:
    cmp byte [fs_ready], 0
    je fs_not_ready
    mov r13, rdi
    mov r12, rsi
    xor r14d, r14d
    xor r15d, r15d
    cmp r13, 2
    je .normal
    cmp r13, 3
    jne .usage
    mov rdi, [r12 + 8]
    call fst_parse_rm_option
    jc .usage
    mov r14d, eax
    test r14d, 1
    jz .no_recursive
    mov r15d, 1
.no_recursive:
    mov rsi, [r12 + 16]
    jmp .resolve
.normal:
    mov rsi, [r12 + 8]
.resolve:
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .found
    test r14d, 2
    jnz .silent_success
    jmp fs_not_found
.found:
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .regular_file
    test r15d, r15d
    jz fs_is_directory
    test r13d, r13d
    jz fs_cannot_remove_root
    mov eax, r13d
    call fs_is_current_inside
    jc fs_cannot_remove_current
    mov eax, r13d
    call fs_delete_tree
    jc fs_operation_failed
    call fs_save_entries
    jc fs_operation_failed
    mov rsi, rm_done_text
    call terminal_print_line
    ret
.regular_file:
    mov eax, r13d
    call fs_delete_entry
    jc fs_operation_failed
    mov rsi, rm_done_text
    call terminal_print_line
    ret
.silent_success:
    ret
.usage:
    mov rsi, rm_usage
    call shell_error
    ret

; cp [-r] src dst, the work and the rollback are in filesystem_transfer.asm
cmd_cp:
    cmp byte [fs_ready], 0
    je fs_not_ready
    mov r13, rdi
    mov r12, rsi
    mov byte [fsx_command_recursive], 0
    cmp r13, 3
    je .plain
    cmp r13, 4
    jne .usage
    mov rdi, [r12 + 8]
    mov rsi, rmdir_recursive_option
    call str_equal
    test eax, eax
    jnz .recursive
    mov rdi, [r12 + 8]
    mov rsi, cp_recursive_upper_option
    call str_equal
    test eax, eax
    jz .usage
.recursive:
    mov byte [fsx_command_recursive], 1
    mov r14, [r12 + 16]
    mov r15, [r12 + 24]
    jmp .resolve_source
.plain:
    mov r14, [r12 + 8]
    mov r15, [r12 + 16]
.resolve_source:
    mov rsi, r14
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r14d, eax
    mov rsi, r15
    call fs_make_path
    jc fs_invalid_path
    mov eax, r14d
    mov rsi, fs_path_buffer
    mov dl, [fsx_command_recursive]
    call fsx_copy_tree
    jc fsx_report_error
    mov rsi, cp_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, cp_usage
    call shell_error
    ret

cmd_mv:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 3
    jne .usage
    mov r12, rsi
    mov r14, [r12 + 8]
    mov r15, [r12 + 16]
    mov rsi, r14
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r14d, eax
    mov rsi, r15
    call fs_make_path
    jc fs_invalid_path
    mov eax, r14d
    mov rsi, fs_path_buffer
    call fsx_move_tree
    jc fsx_report_error
    mov rsi, mv_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, mv_usage
    call shell_error
    ret

cmd_stat:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .directory
    mov rsi, stat_file_text
    call terminal_print
    jmp .path
.directory:
    mov rsi, stat_directory_text
    call terminal_print
.path:
    mov rsi, stat_path_text
    call terminal_print
    lea rsi, [rbx + FS_ENTRY_PATH]
    call terminal_print_line
    mov rsi, stat_size_text
    call terminal_print
    mov eax, [rbx + FS_ENTRY_FILE_SIZE]
    call terminal_print_u64
    call terminal_newline
    mov rsi, stat_extents_text
    call terminal_print
    movzx eax, byte [rbx + FS_ENTRY_EXTENT_COUNT]
    call terminal_print_u64
    call terminal_newline
    mov rsi, stat_modified_text
    call terminal_print
    mov r12d, [rbx + FS_ENTRY_MTIME]
    mov eax, r12d
    shr eax, 26
    add eax, 2000
    call terminal_print_u64
    mov al, '-'
    call terminal_putc
    mov ecx, 22
    mov edx, 15
    call stat_two
    mov al, '-'
    call terminal_putc
    mov ecx, 17
    mov edx, 31
    call stat_two
    mov al, ' '
    call terminal_putc
    mov ecx, 12
    mov edx, 31
    call stat_two
    mov al, ':'
    call terminal_putc
    mov ecx, 6
    mov edx, 63
    call stat_two
    mov al, ':'
    call terminal_putc
    xor ecx, ecx
    mov edx, 63
    call stat_two
    jmp terminal_newline
.usage:
    mov rsi, stat_usage
    call shell_error
    ret

stat_two:
    mov eax, r12d
    shr eax, cl
    and eax, edx
    cmp eax, 10
    jae .print
    push rax
    mov al, '0'
    call terminal_putc
    pop rax
.print:
    jmp terminal_print_u64

cmd_tree:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 1
    je .current
    cmp rdi, 2
    jne .usage
    mov rsi, [rsi + 8]
    call fs_make_path
    jc fs_invalid_path
    jmp .resolve
.current:
    mov rsi, fs_current_path
    mov rdi, fs_path_buffer
    call fs_copy_string
.resolve:
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r12d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz fs_not_directory
    mov rsi, tree_header_text
    call terminal_print
    mov rsi, fs_path_buffer
    call terminal_print_line
    call fs_tree_clear_visited
    mov eax, r12d
    xor ecx, ecx
    call fs_tree_print
    ret
.usage:
    mov rsi, tree_usage
    call shell_error
    ret

cmd_hexdump:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    mov cl, ACCESS_READ
    call fs_access
    jc fs_operation_failed
    call fs_load_file
    jc fs_operation_failed
    xor r14d, r14d
.loop:
    cmp r14d, [fs_file_size]
    jae .done
    test r14d, 15
    jnz .byte
    mov rax, r14
    call terminal_print_hex64
    mov al, ':'
    call terminal_putc
    mov al, ' '
    call terminal_putc
.byte:
    mov al, [fs_file_buffer + r14]
    call fs_print_hex_byte
    mov al, ' '
    call terminal_putc
    inc r14d
    test r14d, 15
    jnz .loop
    call terminal_newline
    jmp .loop
.done:
    test r14d, 15
    jz .newline
    call terminal_newline
.newline:
    ret
.usage:
    mov rsi, hexdump_usage
    call shell_error
    ret

cmd_wc:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz fs_not_file
    mov eax, r13d
    mov cl, ACCESS_READ
    call fs_access
    jc fs_operation_failed
    call fs_load_file
    jc fs_operation_failed
    xor r14d, r14d
    xor r15d, r15d
    xor ebx, ebx
    xor ecx, ecx
.scan:
    cmp r14d, [fs_file_size]
    jae .finish
    mov al, [fs_file_buffer + r14]
    cmp al, 10
    jne .check_carriage_return
    inc ebx
    jmp .whitespace
.check_carriage_return:
    cmp al, 13
    je .whitespace
    cmp al, ' '
    je .whitespace
    cmp al, 9
    je .whitespace
    mov ecx, 1
    jmp .next
.whitespace:
    test ecx, ecx
    jz .next
    inc r15d
    xor ecx, ecx
.next:
    inc r14d
    jmp .scan
.finish:
    test ecx, ecx
    jz .print
    inc r15d
.print:
    call shell_use_output_color
    mov rsi, wc_bytes_text
    call terminal_print
    mov eax, [fs_file_size]
    call terminal_print_u64
    mov rsi, wc_words_text
    call terminal_print
    mov eax, r15d
    call terminal_print_u64
    mov rsi, wc_lines_text
    call terminal_print
    mov eax, ebx
    call terminal_print_u64
    call terminal_newline
    ret
.usage:
    mov rsi, wc_usage
    call shell_error
    ret

cmd_fsinfo:
    cmp byte [fs_ready], 0
    je fs_not_ready
    xor r12d, r12d
    xor r13d, r13d
.count:
    cmp r13d, FS_MAX_FILES
    jae .print
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    inc r12d
.next:
    inc r13d
    jmp .count
.print:
    mov rsi, fsinfo_magic_text
    call terminal_print
    mov eax, FS_MAGIC
    call terminal_print_hex64
    call terminal_newline
    mov rsi, fsinfo_entries_text
    call terminal_print
    mov eax, r12d
    call terminal_print_u64
    call terminal_newline
    mov rsi, fsinfo_capacity_text
    call terminal_print
    mov eax, FS_MAX_FILES
    call terminal_print_u64
    call terminal_newline
    mov rsi, fsinfo_file_size_text
    call terminal_print
    mov eax, FS_FILE_BYTES
    call terminal_print_u64
    call terminal_newline
    mov rsi, fsinfo_transfer_text
    call terminal_print
    mov rsi, fsinfo_pio_text
    cmp byte [ata_dma], 0
    je .mode
    mov rsi, fsinfo_dma_text
.mode:
    jmp terminal_print_line

cmd_sync:
    cmp byte [fs_ready], 0
    je fs_not_ready
    call fs_save_entries
    jc fs_operation_failed
    call ata_flush
    jc fs_operation_failed
    mov rsi, sync_done_text
    call terminal_print_line
    ret

; "format yes" or nothing happens, it wipes the whole disk
cmd_format:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    mov rdi, [r12 + 8]
    mov rsi, format_confirm_text
    call str_equal
    test eax, eax
    jz .usage
    call need_root
    jc fs_operation_failed
    call fs_format_internal
    jc fs_operation_failed
    mov byte [fs_ready], 1
    call fs_set_root_path
    call terminal_clear
    mov rsi, format_done_text
    call terminal_print_line
    ret
.usage:
    mov rsi, format_usage
    call shell_error
    ret

fs_print_basename:
    push rax
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13, rsi
.scan:
    mov al, [rsi]
    test al, al
    jz .print
    cmp al, '/'
    jne .next
    lea r13, [rsi + 1]
.next:
    inc rsi
    jmp .scan
.print:
    mov rsi, r13
    call terminal_print
    pop r13
    pop r12
    pop rbx
    pop rax
    ret

fs_print_hex_byte:
    push rax
    push rbx
    movzx ebx, al
    mov eax, ebx
    shr eax, 4
    and eax, 0x0F
    mov al, [numbers_hex_chars + rax]
    call terminal_putc
    mov eax, ebx
    and eax, 0x0F
    mov al, [numbers_hex_chars + rax]
    call terminal_putc
    pop rbx
    pop rax
    ret


fs_not_ready:
    mov rsi, fs_not_ready_text
    call shell_error
    ret

fs_invalid_path:
    mov rsi, fs_invalid_path_text
    call shell_error
    ret

fs_not_found:
    mov rsi, fs_not_found_text
    call shell_error
    ret

fs_not_directory:
    mov rsi, fs_not_directory_text
    call shell_error
    ret

fs_not_file:
    mov rsi, fs_not_file_text
    call shell_error
    ret

fs_is_directory:
    mov rsi, fs_is_directory_text
    call shell_error
    ret

fs_create_failed:
    mov rsi, fs_create_failed_text
    call shell_error
    ret

fs_operation_failed:
    mov rsi, fs_operation_failed_text
    call shell_error
    ret

fs_file_too_large:
    mov rsi, fs_file_too_large_text
    call shell_error
    ret

fs_already_exists:
    mov rsi, fs_already_exists_text
    call shell_error
    ret

fs_cannot_remove_root:
    mov rsi, fs_cannot_remove_root_text
    call shell_error
    ret

fs_cannot_remove_current:
    mov rsi, fs_cannot_remove_current_text
    call shell_error
    ret

fs_directory_not_empty:
    mov rsi, fs_directory_not_empty_text
    call shell_error
    ret

command_ls db 'ls', 0
command_cd db 'cd', 0
command_mkdir db 'mkdir', 0
command_rmdir db 'rmdir', 0
command_touch db 'touch', 0
command_write db 'write', 0
command_append db 'append', 0
command_cat db 'cat', 0
command_rm db 'rm', 0
command_cp db 'cp', 0
command_mv db 'mv', 0
command_stat db 'stat', 0
command_tree db 'tree', 0
command_hexdump db 'hexdump', 0
command_wc db 'wc', 0
command_fsinfo db 'fsinfo', 0
command_sync db 'sync', 0
command_format db 'format', 0

fs_parent_command db '..', 0

ls_directory_text db '[DIR] ', 0
ls_file_text db '[FILE] ', 0

mkdir_usage db 'Usage: mkdir [-p] <path>', 0
rmdir_usage db 'Usage: rmdir [-r] <path>', 0
touch_usage db 'Usage: touch <file>', 0
write_usage db 'Usage: write <file> <text>', 0
append_usage db 'Usage: append <file> <text>', 0
cat_usage db 'Usage: cat <file>', 0
rm_usage db 'Usage: rm [-r] [-f] <path>', 0
cp_usage db 'Usage: cp [-r] <source> <destination>', 0
mv_usage db 'Usage: mv <source> <destination>', 0
stat_usage db 'Usage: stat <path>', 0
hexdump_usage db 'Usage: hexdump <file>', 0
wc_usage db 'Usage: wc <file>', 0
format_usage db 'Usage: format yes', 0

mkdir_done_text db 'Directory created.', 0
mkdir_exists_text db 'Directory already exists.', 0
rmdir_done_text db 'Directory removed.', 0
touch_done_text db 'File ready.', 0
write_done_text db 'File written.', 0
append_done_text db 'File extended.', 0
rm_done_text db 'File removed.', 0
cp_done_text db 'File copied.', 0
mv_done_text db 'File moved.', 0

mkdir_parent_option db '-p', 0
rmdir_recursive_option db '-r', 0
tree_header_text db 'Tree: ', 0
tree_usage db 'Usage: tree [path]', 0
fs_cannot_remove_root_text db 'Cannot remove the root directory.', 0
fs_cannot_remove_current_text db 'Cannot remove the current directory or one of its parents.', 0
fs_directory_not_empty_text db 'Directory is not empty.', 0

stat_file_text db 'Type: file', 10, 0
stat_directory_text db 'Type: directory', 10, 0
stat_path_text db 'Path: ', 0
stat_size_text db 'Size: ', 0
stat_extents_text db 'Extents: ', 0
stat_modified_text db 'Modified: ', 0

wc_bytes_text db 'Bytes: ', 0
wc_words_text db ' Words: ', 0
wc_lines_text db ' Lines: ', 0

fsinfo_transfer_text db 'Disk transfer: ', 0
fsinfo_dma_text db 'DMA', 0
fsinfo_pio_text db 'PIO', 0
fsinfo_magic_text db 'Magic: ', 0
fsinfo_entries_text db 'Used entries: ', 0
fsinfo_capacity_text db 'Capacity: ', 0
fsinfo_file_size_text db 'Maximum file size: ', 0

sync_done_text db 'Filesystem synchronized.', 0
format_confirm_text db 'yes', 0
format_done_text db 'Filesystem formatted.', 0

fs_not_ready_text db 'Filesystem is not available.', 0
fs_invalid_path_text db 'Invalid path.', 0
fs_not_found_text db 'Path not found.', 0
fs_not_directory_text db 'Not a directory.', 0
fs_not_file_text db 'Not a regular file.', 0
fs_is_directory_text db 'Use rmdir for directories.', 0
fs_create_failed_text db 'Cannot create filesystem entry.', 0
fs_operation_failed_text db 'Disk operation failed.', 0
fs_file_too_large_text db 'File exceeds 4096 bytes.', 0
fs_already_exists_text db 'Destination already exists.', 0

cmd_find:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 2
    jb .usage
    mov r12, rsi
    cmp rdi, 2
    jne .explicit_path
    mov r14, [r12 + 8]
    mov rsi, fs_current_path
    mov rdi, fs_path_buffer
    call fs_copy_string
    jmp .resolve_base
.explicit_path:
    cmp rdi, 3
    jne .usage
    mov r14, [r12 + 16]
    mov rsi, [r12 + 8]
    call fs_make_path
    jc fs_invalid_path
.resolve_base:
    mov [fst_find_name], r14
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz fs_not_directory
    mov dword [fst_find_matches], 0
    call fs_tree_clear_visited
    mov eax, r13d
    call fst_find_walk
    cmp dword [fst_find_matches], 0
    jne .done
    mov rsi, find_no_matches_text
    call terminal_print_line
.done:
    ret
.usage:
    mov rsi, find_usage
    call shell_error
    ret

cmd_du:
    cmp byte [fs_ready], 0
    je fs_not_ready
    cmp rdi, 1
    je .current
    cmp rdi, 2
    jne .usage
    mov rsi, [rsi + 8]
    call fs_make_path
    jc fs_invalid_path
    jmp .resolve
.current:
    mov rsi, fs_current_path
    mov rdi, fs_path_buffer
    call fs_copy_string
.resolve:
    mov rsi, fs_path_buffer
    call fs_find_path
    jc fs_not_found
    mov r13d, eax
    call fs_tree_clear_visited
    mov eax, r13d
    call fst_du_walk
    mov r12, rax
    mov rsi, stat_size_text
    call terminal_print
    mov rax, r12
    call terminal_print_u64
    mov rsi, du_bytes_text
    call terminal_print_line
    ret
.usage:
    mov rsi, du_usage
    call shell_error
    ret

; only counts entries. the free space is right there in the bitmap, someday
cmd_df:
    cmp byte [fs_ready], 0
    je fs_not_ready
    call fst_count_entries
    mov [fst_count_used], eax
    mov [fst_count_directories], ebx
    mov [fst_count_files], ecx
    mov rsi, fsinfo_entries_text
    call terminal_print
    mov eax, [fst_count_used]
    call terminal_print_u64
    mov rsi, df_capacity_entries_text
    call terminal_print
    mov eax, FS_MAX_FILES
    call terminal_print_u64
    call terminal_newline
    mov rsi, df_directories_text
    call terminal_print
    mov eax, [fst_count_directories]
    call terminal_print_u64
    call terminal_newline
    mov rsi, df_files_text
    call terminal_print
    mov eax, [fst_count_files]
    call terminal_print_u64
    call terminal_newline
    mov rsi, df_free_entries_text
    call terminal_print
    mov eax, FS_MAX_FILES
    sub eax, [fst_count_used]
    call terminal_print_u64
    call terminal_newline
    ret

cmd_fsck:
    cmp byte [fs_ready], 0
    je fs_not_ready
    call fst_run_fsck
    test eax, eax
    jz .clean
    mov r12d, eax
    mov rsi, fsck_errors_text
    call terminal_print
    mov eax, r12d
    call terminal_print_u64
    call terminal_newline
    ret
.clean:
    mov rsi, fsck_clean_text
    call terminal_print_line
    ret

command_find db 'find', 0
command_du db 'du', 0
command_df db 'df', 0
command_fsck db 'fsck', 0

find_usage db 'Usage: find [path] <name>', 0
find_no_matches_text db 'No matching entries.', 0
find_directory_text db '[DIR]  ', 0

du_usage db 'Usage: du [path]', 0
du_bytes_text db ' bytes', 0

df_capacity_entries_text db ' / ', 0
df_directories_text db 'Directories: ', 0
df_files_text db 'Files: ', 0
df_free_entries_text db 'Free entries: ', 0

fsck_clean_text db 'Filesystem is clean.', 0
fsck_errors_text db 'Filesystem errors: ', 0

; prints the FSX_ERROR_* left by a failed cp or mv
fsx_report_error:
    mov al, [fsx_error]
    cmp al, FSX_ERROR_RECURSIVE_REQUIRED
    je .recursive_required
    cmp al, FSX_ERROR_DESTINATION_EXISTS
    je .destination_exists
    cmp al, FSX_ERROR_INSIDE_SOURCE
    je .inside_source
    cmp al, FSX_ERROR_ROOT
    je .root
    cmp al, FSX_ERROR_NO_SPACE
    je .no_space
    cmp al, FSX_ERROR_INVALID_DESTINATION
    je .invalid_destination
    cmp al, FSX_ERROR_CORRUPT
    je .corrupt
    cmp al, FSX_ERROR_PATH_TOO_LONG
    je .path_too_long
    mov rsi, fsx_operation_error_text
    call shell_error
    ret
.recursive_required:
    mov rsi, fsx_recursive_required_text
    call shell_error
    ret
.destination_exists:
    mov rsi, fs_already_exists_text
    call shell_error
    ret
.inside_source:
    mov rsi, fsx_inside_source_text
    call shell_error
    ret
.root:
    mov rsi, fsx_root_text
    call shell_error
    ret
.no_space:
    mov rsi, fsx_no_space_text
    call shell_error
    ret
.invalid_destination:
    mov rsi, fsx_invalid_destination_text
    call shell_error
    ret
.corrupt:
    mov rsi, fsx_corrupt_text
    call shell_error
    ret
.path_too_long:
    mov rsi, fsx_path_too_long_text
    call shell_error
    ret

cp_recursive_upper_option db '-R', 0
fsx_recursive_required_text db 'Directory copy requires cp -r.', 0
fsx_inside_source_text db 'Cannot place a directory inside itself.', 0
fsx_root_text db 'Cannot copy or move the root directory.', 0
fsx_no_space_text db 'Not enough free space on the filesystem.', 0
fsx_invalid_destination_text db 'Destination parent does not exist or is not a directory.', 0
fsx_corrupt_text db 'Filesystem is inconsistent. Run fsck.', 0
fsx_path_too_long_text db 'A resulting path is too long.', 0
fsx_operation_error_text db 'Transfer failed. Filesystem metadata was rolled back.', 0
fsx_command_recursive db 0
