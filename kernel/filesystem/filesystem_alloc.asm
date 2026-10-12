fs_bitmap_load:
    push rax
    mov rax, fs_read_table
    jmp fs_bitmap_transfer

fs_bitmap_save:
    push rax
    mov rax, fs_write_table

fs_bitmap_transfer:
    push rsi
    push rdx
    push rdi
    push rcx
    mov rsi, fs_bitmap
    mov rdx, FS_BITMAP_SHADOW_ADDRESS
    mov edi, FS_BITMAP_LBA
    mov ecx, FS_BITMAP_SECTORS
    call rax
    pop rcx
    pop rdi
    pop rdx
    pop rsi
    pop rax
    ret

fs_extent_mark:
    push rax
    push rcx
    sub eax, FS_DATA_LBA
    jb .done
.loop:
    test ecx, ecx
    jz .done
    cmp eax, FS_DATA_SECTORS
    jae .done
    test dl, dl
    jz .clear
    bts dword [fs_bitmap], eax
    jmp .next
.clear:
    btr dword [fs_bitmap], eax
.next:
    inc eax
    dec ecx
    jmp .loop
.done:
    pop rcx
    pop rax
    ret

; ecx = sectors. first fit search for one contiguous run, marks it used.
; out: eax = lba, CF if no run is long enough
fs_extent_alloc:
    push rbx
    push rdx
    push r8
    xor eax, eax
    xor ebx, ebx
    xor r8d, r8d
.scan:
    cmp eax, FS_DATA_SECTORS
    jae .failed
    bt dword [fs_bitmap], eax
    jc .used
    test r8d, r8d
    jnz .extend
    mov ebx, eax
.extend:
    inc r8d
    cmp r8d, ecx
    je .found
    inc eax
    jmp .scan
.used:
    xor r8d, r8d
    inc eax
    jmp .scan
.found:
    lea eax, [rbx + FS_DATA_LBA]
    mov dl, 1
    call fs_extent_mark
    pop r8
    pop rdx
    pop rbx
    clc
    ret
.failed:
    pop r8
    pop rdx
    pop rbx
    stc
    ret

fs_entry_release_extent:
    push rax
    push rbx
    push rcx
    push rdx
    cmp eax, FS_MAX_FILES
    jae .done
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    xor dl, dl
    call fs_mark_extents
    mov byte [rbx + FS_ENTRY_EXTENT_COUNT], 0
    mov dword [rbx + FS_ENTRY_SECTOR_COUNT], 0
    call fs_clear_extents
.done:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

fs_mark_extents:
    push rax
    push rcx
    push rsi
    movzx esi, byte [rbx + FS_ENTRY_EXTENT_COUNT]
    cmp esi, FS_EXTENT_MAX
    jbe .walk
    mov esi, FS_EXTENT_MAX
.walk:
    test esi, esi
    jz .done
    dec esi
    mov eax, [rbx + FS_ENTRY_EXTENTS + rsi * FS_EXTENT_SIZE + FS_EXTENT_LBA]
    mov ecx, [rbx + FS_ENTRY_EXTENTS + rsi * FS_EXTENT_SIZE + FS_EXTENT_SECTORS]
    call fs_extent_mark
    jmp .walk
.done:
    pop rsi
    pop rcx
    pop rax
    ret

fs_clear_extents:
    push rax
    push rcx
    push rdi
    lea rdi, [rbx + FS_ENTRY_EXTENTS]
    xor eax, eax
    mov ecx, FS_EXTENT_MAX * FS_EXTENT_SIZE / 8
    cld
    rep stosq
    pop rdi
    pop rcx
    pop rax
    ret

fs_free_run:
    push rbx
.find:
    cmp edi, FS_DATA_SECTORS
    jae .none
    bt dword [fs_bitmap], edi
    jnc .start
    inc edi
    jmp .find
.start:
    mov eax, edi
    xor ebx, ebx
.extend:
    cmp ebx, ecx
    jae .found
    lea edx, [rax + rbx]
    cmp edx, FS_DATA_SECTORS
    jae .found
    bt dword [fs_bitmap], edx
    jc .found
    inc ebx
    jmp .extend
.found:
    mov ecx, ebx
    pop rbx
    clc
    ret
.none:
    pop rbx
    stc
    ret

fs_entry_resize_extent:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    mov r12d, ecx
    lea rsi, [rbx + FS_ENTRY_EXTENTS]
    lea rdi, [fs_resize_saved]
    mov ecx, FS_EXTENT_MAX * FS_EXTENT_SIZE / 8
    cld
    rep movsq
    mov al, [rbx + FS_ENTRY_EXTENT_COUNT]
    mov [fs_resize_saved_count], al
    xor dl, dl
    call fs_mark_extents
    call fs_clear_extents
    mov byte [rbx + FS_ENTRY_EXTENT_COUNT], 0
    xor r14d, r14d
    test r12d, r12d
    jz .success
    mov ecx, r12d
    call fs_extent_alloc
    jc .scatter
    mov [rbx + FS_ENTRY_EXTENTS + FS_EXTENT_LBA], eax
    mov [rbx + FS_ENTRY_EXTENTS + FS_EXTENT_SECTORS], r12d
    mov r14d, 1
    jmp .success
.scatter:
    mov r13d, r12d
    xor r15d, r15d
.piece:
    test r13d, r13d
    jz .success
    cmp r14d, FS_EXTENT_MAX
    jae .undo
    mov edi, r15d
    mov ecx, r13d
    call fs_free_run
    jc .undo
    lea r15d, [rax + rcx]
    add eax, FS_DATA_LBA
    mov [rbx + FS_ENTRY_EXTENTS + r14 * FS_EXTENT_SIZE + FS_EXTENT_LBA], eax
    mov [rbx + FS_ENTRY_EXTENTS + r14 * FS_EXTENT_SIZE + FS_EXTENT_SECTORS], ecx
    mov dl, 1
    call fs_extent_mark
    sub r13d, ecx
    inc r14d
    mov [rbx + FS_ENTRY_EXTENT_COUNT], r14b
    jmp .piece
.undo:
    mov [rbx + FS_ENTRY_EXTENT_COUNT], r14b
    xor dl, dl
    call fs_mark_extents
    lea rsi, [fs_resize_saved]
    lea rdi, [rbx + FS_ENTRY_EXTENTS]
    mov ecx, FS_EXTENT_MAX * FS_EXTENT_SIZE / 8
    rep movsq
    mov al, [fs_resize_saved_count]
    mov [rbx + FS_ENTRY_EXTENT_COUNT], al
    mov dl, 1
    call fs_mark_extents
    stc
    jmp .out
.success:
    mov [rbx + FS_ENTRY_EXTENT_COUNT], r14b
    mov [rbx + FS_ENTRY_SECTOR_COUNT], r12d
    clc
.out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

fs_resize_saved_count db 0
align 8
fs_resize_saved:
    times FS_EXTENT_MAX * FS_EXTENT_SIZE db 0
