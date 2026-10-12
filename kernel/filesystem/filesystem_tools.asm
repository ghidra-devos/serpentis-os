; find, du, fsck and friends, everything that walks the whole entry table

; rdi -> base path, rsi -> path. eax = 1 if path is base itself or somewhere below it.
; "/a" contains "/a/b" but not "/ab"
fst_path_under:
    push rcx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    mov r8, rdi
    mov r9, rsi
    mov rsi, r8
    call str_length
    mov r10, rax
    mov rsi, r9
    call str_length
    mov r11, rax
    cmp r11, r10
    jb .false
    cmp r10, 1
    jne .compare
    cmp byte [r8], '/'
    jne .false
    cmp byte [r9], '/'
    jne .false
    mov eax, 1
    jmp .done
.compare:
    mov rdi, r8
    mov rsi, r9
    mov rcx, r10
    cld
    repe cmpsb
    jne .false
    cmp r11, r10
    je .true
    cmp byte [r9 + r10], '/'
    jne .false
.true:
    mov eax, 1
    jmp .done
.false:
    xor eax, eax
.done:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rcx
    ret

fst_basename_equal:
    push r8
    push r9
    push rsi
    mov r8, rsi
    mov r9, rsi
.scan:
    mov al, [rsi]
    test al, al
    jz .compare
    cmp al, '/'
    jne .next
    lea r9, [rsi + 1]
.next:
    inc rsi
    jmp .scan
.compare:
    mov rsi, r9
    call str_equal
    pop rsi
    pop r9
    pop r8
    ret

fst_count_entries:
    push rdx
    push rdi
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edi, edi
.loop:
    cmp edi, FS_MAX_FILES
    jae .done
    mov edx, edi
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    inc eax
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .directory
    inc ecx
    jmp .next
.directory:
    inc ebx
.next:
    inc edi
    jmp .loop
.done:
    pop rdi
    pop rdx
    ret

fst_find_walk:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, [fst_find_name]
    mov r13d, eax
    call fs_tree_mark_visited
    jc .done
    xor r14d, r14d
.scan:
    cmp r14d, FS_MAX_FILES
    jae .done
    cmp r14d, r13d
    je .next
    mov eax, r14d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r13d
    jne .next
    mov r15d, r14d
    mov rdi, r12
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fst_basename_equal
    test eax, eax
    jz .check_directory
    inc dword [fst_find_matches]
    mov eax, r15d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .print_file
    mov rsi, find_directory_text
    call terminal_print
    jmp .print_path
.print_file:
    mov rsi, ls_file_text
    call terminal_print
.print_path:
    lea rsi, [rbx + FS_ENTRY_PATH]
    call terminal_print_line
.check_directory:
    mov eax, r15d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .next
    mov eax, r15d
    call fst_find_walk
.next:
    inc r14d
    jmp .scan
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

fst_du_walk:
    push rbx
    push r12
    push r13
    push r14
    mov r12d, eax
    cmp r12d, FS_MAX_FILES
    jae .zero
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .zero
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .file
    mov eax, r12d
    call fs_tree_mark_visited
    jc .zero
    xor r14d, r14d
    xor r13d, r13d
.scan:
    cmp r13d, FS_MAX_FILES
    jae .directory_done
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
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .child_directory
    mov eax, [rbx + FS_ENTRY_FILE_SIZE]
    add r14, rax
    jmp .next
.child_directory:
    mov eax, r13d
    call fst_du_walk
    add r14, rax
.next:
    inc r13d
    jmp .scan
.directory_done:
    mov rax, r14
    jmp .done
.file:
    mov eax, [rbx + FS_ENTRY_FILE_SIZE]
    jmp .done
.zero:
    xor eax, eax
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; rsi -> string. CF if it has no terminator within FS_PATH_MAX, eax = length otherwise
fst_bounded_strlen:
    xor eax, eax
    mov ecx, FS_PATH_MAX
.scan:
    cmp byte [rsi + rax], 0
    je .valid
    inc eax
    dec ecx
    jnz .scan
    stc
    ret
.valid:
    clc
    ret

fst_validate_parent_chain:
    push rbx
    push rcx
    push r12
    mov r12d, eax
    mov ecx, FS_MAX_FILES
.walk:
    test r12d, r12d
    jz .valid
    dec ecx
    jz .invalid
    cmp r12d, FS_MAX_FILES
    jae .invalid
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .invalid
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, r12d
    je .invalid
    cmp eax, FS_MAX_FILES
    jae .invalid
    mov edx, eax
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .invalid
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .invalid
    mov r12d, eax
    jmp .walk
.valid:
    pop r12
    pop rcx
    pop rbx
    clc
    ret
.invalid:
    pop r12
    pop rcx
    pop rbx
    stc
    ret

fst_validate_entry:
    push rbx
    push r12
    push r13
    mov r12d, eax
    cmp r12d, FS_MAX_FILES
    jae .invalid
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    movzx eax, byte [rbx + FS_ENTRY_FLAGS]
    cmp eax, FS_FLAG_USED
    je .regular
    cmp eax, FS_FLAG_USED or FS_FLAG_DIRECTORY
    je .directory
    jmp .invalid
.directory:
    cmp dword [rbx + FS_ENTRY_FILE_SIZE], 0
    jne .invalid
    cmp dword [rbx + FS_ENTRY_SECTOR_COUNT], 0
    jne .invalid
    cmp byte [rbx + FS_ENTRY_EXTENT_COUNT], 0
    jne .invalid
    jmp .common
.regular:
    mov eax, [rbx + FS_ENTRY_FILE_SIZE]
    cmp eax, FS_FILE_BYTES
    ja .invalid
    add eax, 511
    shr eax, 9
    cmp dword [rbx + FS_ENTRY_SECTOR_COUNT], eax
    jne .invalid
    call fst_extents_valid
    jc .invalid
.common:
    cmp byte [rbx + FS_ENTRY_PATH], '/'
    jne .invalid
    lea rsi, [rbx + FS_ENTRY_PATH]
    call fst_bounded_strlen
    jc .invalid
    test eax, eax
    jz .invalid
    test r12d, r12d
    jnz .not_root
    cmp word [rbx + FS_ENTRY_PARENT], 0
    jne .invalid
    cmp byte [rbx + FS_ENTRY_PATH + 1], 0
    jne .invalid
    jmp .chain
.not_root:
    movzx eax, word [rbx + FS_ENTRY_PARENT]
    cmp eax, FS_MAX_FILES
    jae .invalid
    mov edx, eax
    shl edx, FS_ENTRY_SHIFT
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .invalid
    test byte [fs_entries + rdx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .invalid
.chain:
    mov eax, r12d
    call fst_validate_parent_chain
    jc .invalid
    pop r13
    pop r12
    pop rbx
    clc
    ret
.invalid:
    pop r13
    pop r12
    pop rbx
    stc
    ret

; rdi -> "-rf" style argument. out: eax bit 0 = r, bit 1 = f. CF on anything else
fst_parse_rm_option:
    push rbx
    push rdx
    push rsi
    mov rsi, rdi
    cmp byte [rsi], '-'
    jne .invalid
    inc rsi
    xor eax, eax
    xor ebx, ebx
.scan:
    mov dl, [rsi]
    test dl, dl
    jz .finish
    cmp dl, 'r'
    je .recursive
    cmp dl, 'f'
    je .force
    jmp .invalid
.recursive:
    or eax, 1
    inc ebx
    inc rsi
    jmp .scan
.force:
    or eax, 2
    inc ebx
    inc rsi
    jmp .scan
.finish:
    test ebx, ebx
    jz .invalid
    pop rsi
    pop rdx
    pop rbx
    clc
    ret
.invalid:
    pop rsi
    pop rdx
    pop rbx
    stc
    ret

fst_run_fsck:
    push r12
    push r13
    push r15
    xor r15d, r15d
    xor r12d, r12d
.validate_loop:
    cmp r12d, FS_MAX_FILES
    jae .duplicates_start
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next_validate
    mov eax, r12d
    call fst_validate_entry
    jnc .next_validate
    inc r15d
.next_validate:
    inc r12d
    jmp .validate_loop
.duplicates_start:
    xor r12d, r12d
.duplicate_outer:
    cmp r12d, FS_MAX_FILES
    jae .finish
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next_outer
    mov r13d, r12d
    inc r13d
.duplicate_inner:
    cmp r13d, FS_MAX_FILES
    jae .next_outer
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + rax + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next_inner
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rdi, [fs_entries + rax + FS_ENTRY_PATH]
    mov eax, r13d
    shl eax, FS_ENTRY_SHIFT
    lea rsi, [fs_entries + rax + FS_ENTRY_PATH]
    call str_equal
    test eax, eax
    jz .next_inner
    inc r15d
.next_inner:
    inc r13d
    jmp .duplicate_inner
.next_outer:
    inc r12d
    jmp .duplicate_outer
.finish:
    call fst_check_extents
    add eax, r15d
    pop r15
    pop r13
    pop r12
    ret

; rebuilds the sector bitmap from the extents into FS_EXTENT_MAP_ADDRESS. counts files that
; share sectors with another file and bitmap dwords that disagree with the real one
fst_check_extents:
    push rbx
    push rcx
    push rdx
    push rdi
    push r12
    push r13
    mov rdi, FS_EXTENT_MAP_ADDRESS
    xor eax, eax
    mov ecx, FS_BITMAP_BYTES / 8
    cld
    rep stosq
    xor r13d, r13d
    xor r12d, r12d
.entry:
    cmp r12d, FS_MAX_FILES
    jae .compare
    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next_entry
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .next_entry
    call fst_extents_valid
    jc .next_entry
    xor edx, edx
    movzx edi, byte [rbx + FS_ENTRY_EXTENT_COUNT]
.extent:
    test edi, edi
    jz .counted
    dec edi
    mov eax, [rbx + FS_ENTRY_EXTENTS + rdi * FS_EXTENT_SIZE + FS_EXTENT_LBA]
    sub eax, FS_DATA_LBA
    mov ecx, [rbx + FS_ENTRY_EXTENTS + rdi * FS_EXTENT_SIZE + FS_EXTENT_SECTORS]
.sector:
    bts dword [FS_EXTENT_MAP_ADDRESS], eax
    jnc .claimed
    mov edx, 1
.claimed:
    inc eax
    dec ecx
    jnz .sector
    jmp .extent
.counted:
    add r13d, edx
.next_entry:
    inc r12d
    jmp .entry
.compare:
    xor ecx, ecx
.word:
    cmp ecx, FS_BITMAP_BYTES / 4
    jae .done
    mov eax, [FS_EXTENT_MAP_ADDRESS + rcx * 4]
    cmp eax, [fs_bitmap + rcx * 4]
    je .next_word
    inc r13d
.next_word:
    inc ecx
    jmp .word
.done:
    mov eax, r13d
    pop r13
    pop r12
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

fst_extents_valid:
    push rax
    push rcx
    push rdx
    push rsi
    movzx esi, byte [rbx + FS_ENTRY_EXTENT_COUNT]
    cmp esi, FS_EXTENT_MAX
    ja .invalid
    xor edx, edx
.extent:
    test esi, esi
    jz .total
    dec esi
    mov eax, [rbx + FS_ENTRY_EXTENTS + rsi * FS_EXTENT_SIZE + FS_EXTENT_LBA]
    mov ecx, [rbx + FS_ENTRY_EXTENTS + rsi * FS_EXTENT_SIZE + FS_EXTENT_SECTORS]
    test ecx, ecx
    jz .invalid
    cmp eax, FS_DATA_LBA
    jb .invalid
    sub eax, FS_DATA_LBA
    add eax, ecx
    jc .invalid
    cmp eax, FS_DATA_SECTORS
    ja .invalid
    add edx, ecx
    jmp .extent
.total:
    cmp edx, [rbx + FS_ENTRY_SECTOR_COUNT]
    jne .invalid
    pop rsi
    pop rdx
    pop rcx
    pop rax
    clc
    ret
.invalid:
    pop rsi
    pop rdx
    pop rcx
    pop rax
    stc
    ret

fst_find_name dq 0
fst_find_matches dd 0
fst_count_used dd 0
fst_count_directories dd 0
fst_count_files dd 0

