; the filesystem. flat table of 1024 entries kept in memory, every entry holds its full path,
; so lookups are a string compare over the table and directories are just entries other
; entries name as parent. files are read and written whole through one 1 MB buffer

; formats the disk when there is no superblock yet (fresh qemu image), refuses an unknown
; version instead of eating it
fs_init:
    mov byte [fs_ready], 0
    mov dword [fs_cached_file], -1  ; fs_file_buffer holds nothing yet
    mov word [fs_current_dir], 0
    cmp byte [ata_present], 0
    je .failed
    mov edi, FS_SUPERBLOCK_LBA
    mov rsi, fs_superblock
    call ata_read_sector
    jc .failed
    cmp dword [fs_superblock], FS_MAGIC
    jne .create
    cmp word [fs_superblock + 4], FS_VERSION
    jne .failed
    call fs_load_entries
    jc .failed
    call fs_bitmap_load
    jc .failed
    mov byte [fs_ready], 1
    call fs_set_root_path
    ret
.create:
    call fs_format_internal
    jc .failed
    mov byte [fs_ready], 1
    ret
.failed:
    mov byte [fs_ready], 0
    call fs_set_root_path
    ret

fs_format_internal:
    push rax
    push rcx
    push rdi
    mov dword [fs_cached_file], -1
    mov rdi, fs_superblock
    xor eax, eax
    mov ecx, FS_SUPERBLOCK_BYTES / 8
    cld
    rep stosq
    mov dword [fs_superblock], FS_MAGIC
    mov word [fs_superblock + 4], FS_VERSION
    mov word [fs_superblock + 6], FS_ENTRY_SIZE
    mov word [fs_superblock + 8], FS_MAX_FILES
    mov dword [fs_superblock + 10], FS_DATA_LBA
    mov dword [fs_superblock + 14], FS_DATA_SECTORS
    mov dword [fs_superblock + 18], FS_BITMAP_LBA
    mov word [fs_superblock + 22], FS_BITMAP_SECTORS
    mov rdi, fs_entries
    xor eax, eax
    mov ecx, FS_ENTRIES_BYTES / 8
    rep stosq
    mov rdi, fs_bitmap
    mov ecx, FS_BITMAP_BYTES / 8
    rep stosq
    ; shadows full of 0xff never match, so the first save writes every sector
    mov rax, -1
    mov rdi, FS_ENTRIES_SHADOW_ADDRESS
    mov ecx, FS_ENTRIES_BYTES / 8
    rep stosq
    mov rdi, FS_BITMAP_SHADOW_ADDRESS
    mov ecx, FS_BITMAP_BYTES / 8
    rep stosq
    mov byte [fs_entries + FS_ENTRY_FLAGS], FS_FLAG_USED or FS_FLAG_DIRECTORY
    mov word [fs_entries + FS_ENTRY_PARENT], 0
    mov dword [fs_entries + FS_ENTRY_FILE_SIZE], 0
    call fs_now
    mov [fs_entries + FS_ENTRY_MTIME], eax
    mov dword [fs_entries + FS_ENTRY_SECTOR_COUNT], 0
    mov byte [fs_entries + FS_ENTRY_PATH], '/'
    mov byte [fs_entries + FS_ENTRY_PATH + 1], 0
    mov word [fs_entries + FS_ENTRY_MODE], MODE_DIRECTORY
    mov edi, FS_SUPERBLOCK_LBA
    mov rsi, fs_superblock
    call ata_write_sector
    jc .failed
    call fs_save_entries
    jc .failed
    mov word [fs_current_dir], 0
    call fs_set_root_path
    pop rdi
    pop rcx
    pop rax
    clc
    ret
.failed:
    pop rdi
    pop rcx
    pop rax
    stc
    ret

fs_load_entries:
    mov rsi, fs_entries
    mov rdx, FS_ENTRIES_SHADOW_ADDRESS
    mov edi, FS_ENTRIES_LBA
    mov ecx, FS_ENTRIES_SECTORS
    jmp fs_read_table

fs_save_entries:
    push rsi
    push rdx
    push rdi
    push rcx
    mov rsi, fs_entries
    mov rdx, FS_ENTRIES_SHADOW_ADDRESS
    mov edi, FS_ENTRIES_LBA
    mov ecx, FS_ENTRIES_SECTORS
    call fs_write_table
    jc .done
    call fs_bitmap_save
.done:
    pop rcx
    pop rdi
    pop rdx
    pop rsi
    ret

fs_read_table:
    push rcx
    push rsi
    push rdi
    call ata_read_sectors
    jc .out
    mov rdi, rdx
    shl ecx, 6
    cld
    rep movsq
    clc
.out:
    pop rdi
    pop rsi
    pop rcx
    ret

fs_write_table:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    mov r12d, edi
    mov r13, rsi
    mov r14, rdx
    mov r15d, ecx
    xor ebx, ebx
.scan:
    cmp ebx, r15d
    jae .written
    call .changed
    jne .run
    inc ebx
    jmp .scan
.run:
    mov ebp, ebx
.extend:
    inc ebx
    cmp ebx, r15d
    jae .flush
    mov eax, ebx
    sub eax, ebp
    cmp eax, ATA_DMA_SECTORS
    jae .flush
    call .changed
    jne .extend
.flush:
    ; this one gets overwritten two lines down, harmless
    lea edi, [r12 + rbp]
    mov eax, ebp
    shl eax, 9
    lea rsi, [r13 + rax]
    lea rdi, [r14 + rax]
    push rdi
    lea edi, [r12 + rbp]
    mov ecx, ebx
    sub ecx, ebp
    call ata_write_sectors
    pop rdi
    jc .out
    shl ecx, 6
    cld
    rep movsq
    jmp .scan
.written:
    clc
.out:
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.changed:
    mov eax, ebx
    shl eax, 9
    lea rsi, [r13 + rax]
    lea rdi, [r14 + rax]
    mov ecx, 64
    cld
    repe cmpsq
    ret

fs_set_root_path:
    mov byte [fs_current_path], '/'
    mov byte [fs_current_path + 1], 0
    mov word [fs_current_dir], 0
    ret

fs_copy_string:
    push rax
    push rcx
    push rsi
    push rdi
    call str_length
    inc eax
    mov ecx, eax
    cld
    rep movsb
    pop rdi
    pop rsi
    pop rcx
    pop rax
    ret

; rsi -> path (absolute or relative to the cwd). normalises it into fs_path_buffer: no
; double slashes, no . or .., no trailing slash. CF if it gets longer than FS_PATH_MAX
fs_make_path:
    push rbx
    push rcx
    push r12
    push r14
    push r15
    mov r12, rsi
    cmp byte [r12], 0
    je .failed
    cmp byte [r12], '/'
    je .start_root
    mov rsi, fs_current_path
    call str_length
    mov r14, rax
    cmp r14, FS_PATH_MAX - 1
    ja .failed
    mov rdi, fs_path_buffer
    mov rsi, fs_current_path
    mov rcx, r14
    cld
    rep movsb
    jmp .parse
.start_root:
    mov byte [fs_path_buffer], '/'
    mov r14, 1
.parse:
.skip_slashes:
    cmp byte [r12], '/'
    jne .component_start
    inc r12
    jmp .skip_slashes
.component_start:
    cmp byte [r12], 0
    je .finish
    mov r15, r12
.find_component_end:
    mov al, [r12]
    test al, al
    jz .component_end
    cmp al, '/'
    je .component_end
    inc r12
    jmp .find_component_end
.component_end:
    mov rbx, r12
    sub rbx, r15
    cmp rbx, 1
    jne .check_parent_component
    cmp byte [r15], '.'
    je .after_component
    jmp .append_component
.check_parent_component:
    cmp rbx, 2
    jne .append_component
    cmp byte [r15], '.'
    jne .append_component
    cmp byte [r15 + 1], '.'
    jne .append_component
.remove_parent:
    cmp r14, 1
    jbe .after_component
    dec r14
    cmp byte [fs_path_buffer + r14], '/'
    jne .remove_parent
    jmp .after_component
.append_component:
    mov rax, r14
    cmp r14, 1
    je .length_without_separator
    inc rax
.length_without_separator:
    add rax, rbx
    cmp rax, FS_PATH_MAX - 1
    ja .failed
    mov rdi, fs_path_buffer
    add rdi, r14
    cmp r14, 1
    je .copy_component
    mov byte [rdi], '/'
    inc rdi
    inc r14
.copy_component:
    mov rsi, r15
    mov rcx, rbx
    cld
    rep movsb
    add r14, rbx
.after_component:
    jmp .parse
.finish:
    mov byte [fs_path_buffer + r14], 0
    pop r15
    pop r14
    pop r12
    pop rcx
    pop rbx
    clc
    ret
.failed:
    pop r15
    pop r14
    pop r12
    pop rcx
    pop rbx
    stc
    ret

fs_find_path:
    push rbx
    push r12
    push r13
    mov r13, rsi
    xor r12d, r12d
.loop:
    cmp r12d, FS_MAX_FILES
    jae .failed
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    mov rdi, r13
    lea rsi, [rbx + FS_ENTRY_PATH]
    call str_equal
    test eax, eax
    jnz .found
.next:
    inc r12d
    jmp .loop
.found:
    mov eax, r12d
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

fs_find_free:
    xor eax, eax
.loop:
    cmp eax, FS_MAX_FILES
    jae .failed
    mov edx, eax
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .found
    inc eax
    jmp .loop
.found:
    clc
    ret
.failed:
    stc
    ret

; dl = flags. creates fs_path_buffer as a new entry under its parent (which has to exist and
; be a directory). out: eax = entry, CF if it exists already or there is no room
fs_create_entry:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    movzx r14d, dl
    mov rsi, fs_path_buffer
    mov rdi, fs_aux_path
    call fs_copy_string
    mov rsi, fs_aux_path
    xor r12d, r12d
    xor r13d, r13d
.scan_path:
    mov al, [rsi + r13]
    test al, al
    jz .parent_ready
    cmp al, '/'
    jne .next_character
    mov r12d, r13d
.next_character:
    inc r13d
    jmp .scan_path
.parent_ready:
    cmp r12d, 0
    jne .copy_parent
    mov byte [fs_parent_path], '/'
    mov byte [fs_parent_path + 1], 0
    jmp .find_parent
.copy_parent:
    mov rsi, fs_aux_path
    mov rdi, fs_parent_path
    mov ecx, r12d
    cld
    rep movsb
    mov byte [rdi], 0
.find_parent:
    mov rsi, fs_parent_path
    call fs_find_path
    jc .failed
    mov r15d, eax
    mov edx, eax
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .failed
    mov eax, r15d
    mov cl, ACCESS_WRITE or ACCESS_EXEC
    call fs_access
    jc .failed
    mov rsi, fs_aux_path
    call fs_find_path
    jnc .failed
    call fs_find_free
    jc .failed
    mov r12d, eax
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    mov rdi, rbx
    xor eax, eax
    mov ecx, FS_ENTRY_SIZE / 8
    cld
    rep stosq
    mov byte [rbx + FS_ENTRY_FLAGS], r14b
    mov word [rbx + FS_ENTRY_PARENT], r15w
    call fs_now
    mov [rbx + FS_ENTRY_MTIME], eax
    mov rsi, fs_aux_path
    lea rdi, [rbx + FS_ENTRY_PATH]
    call fs_copy_string
    call fs_stamp_owner
    call fs_save_entries
    jc .failed
    mov eax, r12d
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

fs_delete_entry:
    push rbx
    push r12
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
    push rcx
    mov eax, r12d
    mov cl, ACCESS_WRITE or ACCESS_EXEC
    call fs_access_parent
    pop rcx
    jc .failed
    mov eax, r12d
    call fs_entry_release_extent
    mov byte [rbx + FS_ENTRY_FLAGS], 0
    call fs_save_entries
    jc .failed
    pop r12
    pop rbx
    clc
    ret
.failed:
    pop r12
    pop rbx
    stc
    ret

fs_cd:
    push rbx
    push r12
    call fs_make_path
    jc .failed
    mov rsi, fs_path_buffer
    call fs_find_path
    jc .failed
    mov r12d, eax
    mov ebx, eax
    shl ebx, FS_ENTRY_SHIFT
    test byte [fs_entries + rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .failed
    push rcx
    mov eax, r12d
    mov cl, ACCESS_EXEC
    call fs_access
    pop rcx
    jc .failed
    mov rsi, fs_path_buffer
    mov rdi, fs_current_path
    call fs_copy_string
    mov word [fs_current_dir], r12w
    pop r12
    pop rbx
    clc
    ret
.failed:
    pop r12
    pop rbx
    stc
    ret

fs_cd_parent:
    push rax
    push rcx
    push rsi
    push rdi
    movzx eax, word [fs_current_dir]
    test eax, eax
    jz .success
    shl eax, FS_ENTRY_SHIFT
    movzx ecx, word [fs_entries + rax + FS_ENTRY_PARENT]
    mov word [fs_current_dir], cx
    mov eax, ecx
    shl eax, FS_ENTRY_SHIFT
    lea rsi, [fs_entries + rax + FS_ENTRY_PATH]
    mov rdi, fs_current_path
    call fs_copy_string
.success:
    pop rdi
    pop rsi
    pop rcx
    pop rax
    clc
    ret

fs_load_file:
    push rbx
    push rcx
    push rdx
    push r12
    mov r12d, eax
    call fs_file_entry
    jc .failed
    mov eax, [rbx + FS_ENTRY_FILE_SIZE]
    cmp eax, FS_FILE_BYTES
    ja .failed
    mov [fs_file_size], eax
    cmp r12d, [fs_cached_file]
    je .loaded
    mov dword [fs_cached_file], -1
    add eax, 511
    shr eax, 9
    mov dl, 0
    call fs_transfer_extents
    jc .failed
    mov [fs_cached_file], r12d
.loaded:
    pop r12
    pop rdx
    pop rcx
    pop rbx
    clc
    ret
.failed:
    mov dword [fs_cached_file], -1
    pop r12
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

; eax = entry, writes fs_file_size bytes from fs_file_buffer. grows or shrinks the extents
; first. CF with fs_no_space set when the disk is full
fs_store_file:
    push rbx
    push rcx
    push rdx
    push r12
    push r13
    mov byte [fs_no_space], 0
    mov dword [fs_cached_file], -1
    mov r12d, eax
    call fs_file_entry
    jc .failed
    mov eax, [fs_file_size]
    cmp eax, FS_FILE_BYTES
    ja .failed
    add eax, 511
    shr eax, 9
    mov r13d, eax
    mov ecx, eax
    mov eax, r12d
    call fs_entry_resize_extent
    jc .full
    mov eax, [fs_file_size]
    mov [rbx + FS_ENTRY_FILE_SIZE], eax
    call fs_now
    mov [rbx + FS_ENTRY_MTIME], eax
    mov dl, 1
    call fs_transfer_extents
    jc .failed
    call fs_save_entries
    jc .failed
    mov [fs_cached_file], r12d
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    clc
    ret
.full:
    mov byte [fs_no_space], 1
.failed:
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

fs_transfer_extents:
    push rcx
    push rsi
    push rdi
    push r8
    mov rsi, fs_file_buffer
    xor r8d, r8d
.extent:
    movzx eax, byte [rbx + FS_ENTRY_EXTENT_COUNT]
    cmp r8d, eax
    jae .done
    mov edi, [rbx + FS_ENTRY_EXTENTS + r8 * FS_EXTENT_SIZE + FS_EXTENT_LBA]
    mov ecx, [rbx + FS_ENTRY_EXTENTS + r8 * FS_EXTENT_SIZE + FS_EXTENT_SECTORS]
    test dl, dl
    jnz .write
    call ata_read_sectors
    jmp .moved
.write:
    call ata_write_sectors
.moved:
    jc .out
    shl ecx, 9
    add rsi, rcx
    inc r8d
    jmp .extent
.done:
    clc
.out:
    pop r8
    pop rdi
    pop rsi
    pop rcx
    ret

fs_now:
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    call rtc_read_datetime
    movzx eax, word [rtc_year]
    sub eax, 2000
    and eax, 63
    shl eax, 4
    movzx ecx, byte [rtc_month]
    or eax, ecx
    shl eax, 5
    movzx ecx, byte [rtc_day]
    or eax, ecx
    shl eax, 5
    movzx ecx, byte [rtc_hour]
    or eax, ecx
    shl eax, 6
    movzx ecx, byte [rtc_minute]
    or eax, ecx
    shl eax, 6
    movzx ecx, byte [rtc_second]
    or eax, ecx
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    ret

fs_file_entry:
    cmp eax, FS_MAX_FILES
    jae .invalid
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .invalid
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .invalid
    clc
    ret
.invalid:
    stc
    ret

fs_clear_file_buffer:
    push rax
    push rcx
    push rdi
    mov dword [fs_cached_file], -1
    xor eax, eax
    mov rdi, fs_file_buffer
    mov ecx, FS_FILE_BYTES / 8
    cld
    rep stosq
    mov dword [fs_file_size], 0
    pop rdi
    pop rcx
    pop rax
    ret

fs_join_arguments:
    mov dword [fs_file_size], 0

fs_append_arguments:
    mov dword [fs_cached_file], -1
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi
    mov r13, rsi
    mov r14d, edx
    mov r15d, [fs_file_size]
.argument_loop:
    cmp r14, r12
    jae .success
    test r15d, r15d
    jz .argument_pointer
    cmp r15d, FS_FILE_BYTES
    jae .failed
    mov byte [fs_file_buffer + r15], ' '
    inc r15d
.argument_pointer:
    mov rsi, [r13 + r14 * 8]
.character_loop:
    lodsb
    test al, al
    jz .next_argument
    cmp r15d, FS_FILE_BYTES
    jae .failed
    mov [fs_file_buffer + r15], al
    inc r15d
    jmp .character_loop
.next_argument:
    inc r14d
    jmp .argument_loop
.success:
    mov [fs_file_size], r15d
    pop r15
    pop r14
    pop r13
    pop r12
    clc
    ret
.failed:
    pop r15
    pop r14
    pop r13
    pop r12
    stc
    ret

fs_current_path:
    times FS_PATH_MAX db 0
fs_path_buffer:
    times FS_PATH_MAX db 0
fs_aux_path:
    times FS_PATH_MAX db 0
fs_parent_path:
    times FS_PATH_MAX db 0
fs_superblock:
    times FS_SUPERBLOCK_BYTES db 0
fs_entries = FS_ENTRIES_ADDRESS
fs_bitmap = FS_BITMAP_ADDRESS
fs_file_buffer = FS_FILE_BUFFER_ADDRESS
fs_file_size dd 0
fs_cached_file dd -1
fs_current_dir dw 0
fs_ready db 0
fs_no_space db 0
