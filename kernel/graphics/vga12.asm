; display: bochs/qemu std vga in vbe mode 640x480x8, linear framebuffer. the name of this file
; is from the planar mode 12h days, the dac and attribute code still come from there

; maps the lfb as one 2 MB uncached page at FRAMEBUFFER_ADDRESS, keeping the offset inside
; the page, then programs the dispi registers
vga_init:
    push rax
    push rcx
    push rdx
    call pci_framebuffer
    mov edx, eax
    and edx, 0x1FFFFF
    and eax, not 0x1FFFFF
    or rax, FRAMEBUFFER_PAGE_FLAGS
    mov [PD_ADDRESS + FRAMEBUFFER_PD_INDEX * 8], rax
    mov rax, cr3
    mov cr3, rax
    add rdx, FRAMEBUFFER_ADDRESS
    mov [frame_base], rdx
    mov ecx, 4
    xor eax, eax
    call vbe_write
    mov ecx, 1
    mov eax, VGA_WIDTH
    call vbe_write
    mov ecx, 2
    mov eax, VGA_HEIGHT
    call vbe_write
    mov ecx, 3                  ; bpp
    mov eax, 8
    call vbe_write
    mov ecx, 4
    mov eax, 0x41
    call vbe_write
    pop rdx
    pop rcx
    pop rax
    ret

vbe_write:
    push rax
    mov dx, VBE_INDEX_PORT
    mov ax, cx
    out dx, ax
    pop rax
    mov dx, VBE_DATA_PORT
    out dx, ax
    ret

pci_framebuffer:
    push rbx
    push rcx
    push rdx
    ; any display controller, not just the bochs id. virtualbox has its own card with the
    ; same dispi ports but the lfb somewhere else, it went black looking at 0xFD000000.
    ; the first prefetchable memory bar is the framebuffer on all of them
    xor ebx, ebx
.scan:
    cmp ebx, 32
    jae .fallback
    mov ecx, ebx
    shl ecx, 11                 ; device number field
    or ecx, 0x80000000
    mov eax, ecx
    or eax, 0x08                ; class code in the top byte
    mov dx, PCI_ADDRESS_PORT
    out dx, eax
    mov dx, PCI_DATA_PORT
    in eax, dx
    shr eax, 24
    cmp eax, 0x03
    je .found
    inc ebx
    jmp .scan
.found:
    mov ebx, 0x10
.bar:
    cmp ebx, 0x28
    jae .fallback
    mov eax, ecx
    or eax, ebx
    mov dx, PCI_ADDRESS_PORT
    out dx, eax
    mov dx, PCI_DATA_PORT
    in eax, dx
    test eax, 1                 ; io bar
    jnz .next_bar
    test eax, 8
    jz .next_bar
    and eax, 0xFFFFFFF0
    jmp .done
.next_bar:
    add ebx, 4
    jmp .bar
.fallback:
    mov eax, 0xFD000000
.done:
    pop rdx
    pop rcx
    pop rbx
    ret

vga_clear:
    call screen_blocked
    jnc .allowed
    ret
.allowed:
    push rax
    push rcx
    push rdi
    movzx eax, al
    mov rdi, [frame_base]
    mov ecx, VGA_WIDTH * VGA_HEIGHT
    cld
    rep stosb
    pop rdi
    pop rcx
    pop rax
    ret

; eax = x, ebx = y, ecx = width, edx = height, r8b = color. x is read back from its push
vga_fill_rect_aligned:
    call screen_blocked
    jnc .allowed
    ret
.allowed:
    push rax
    push rcx
    push rdx
    push rdi
    push r9
    push r10
    test ecx, ecx
    jz .done
    test edx, edx
    jz .done
    mov r9d, ebx
    mov r10d, edx
.row:
    mov eax, r9d
    imul eax, FRAME_STRIDE
    add eax, [rsp + 40]
    mov rdi, [frame_base]
    add rdi, rax
    push rcx
    mov eax, r8d
    cld
    rep stosb
    pop rcx
    inc r9d
    dec r10d
    jnz .row
.done:
    pop r10
    pop r9
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

vga_map_palette:
    push rax
    push rcx
    push rdx
    mov dx, VGA_INPUT_STATUS
    in al, dx                   ; reading input status resets the index/data flip flop
    mov dx, VGA_ATTRIBUTE_INDEX
    xor ecx, ecx
.next:
    mov eax, ecx
    out dx, al
    mov eax, ecx
    out dx, al
    inc ecx
    cmp ecx, 16
    jb .next
    mov al, 0x20
    out dx, al
    pop rdx
    pop rcx
    pop rax
    ret

vga_load_palette:
    push rax
    push rcx
    push rdx
    push rsi
    mov dx, VGA_DAC_WRITE_INDEX
    xor al, al
    out dx, al
    mov dx, VGA_DAC_DATA
    mov rsi, vga_palette
    mov ecx, 16 * 3
.next:
    lodsb
    out dx, al
    dec ecx
    jnz .next
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

vga_palette:
    db 0, 0, 0
    db 0, 0, 42
    db 0, 42, 0
    db 0, 42, 42
    db 42, 0, 0
    db 42, 0, 42
    db 42, 21, 0
    db 42, 42, 42
    db 21, 21, 21
    db 21, 21, 63
    db 21, 63, 21
    db 21, 63, 63
    db 63, 21, 21
    db 63, 21, 63
    db 63, 63, 21
    db 63, 63, 63
THEME_COUNT = 6

vga_set_theme:
    cmp edi, THEME_COUNT
    jae .failed
    push rcx
    push rsi
    push rdi
    imul esi, edi, 48
    lea rsi, [vga_themes + rsi]
    lea rdi, [vga_palette]
    mov ecx, 48
    cld
    rep movsb
    pop rdi
    pop rsi
    pop rcx
    mov [vga_theme_current], edi
    call vga_load_palette
    clc
    ret
.failed:
    stc
    ret

theme_restore:
    push rbx
    mov rsi, theme_file_path
    call fs_find_path
    jc .done
    mov ebx, eax
    call fs_load_file
    jc .done
    mov eax, ebx
    shl eax, FS_ENTRY_SHIFT
    mov ecx, [fs_entries + rax + FS_ENTRY_FILE_SIZE]
    cmp ecx, THEME_NAME_MAX - 1
    ja .done
    mov rsi, fs_file_buffer
    mov rdi, theme_name_buffer
    xor edx, edx
.copy:
    cmp edx, ecx
    jae .copied
    mov al, [rsi + rdx]
    cmp al, ' '
    jb .copied
    mov [rdi + rdx], al
    inc edx
    jmp .copy
.copied:
    mov byte [rdi + rdx], 0
    xor ebx, ebx
.match:
    cmp ebx, THEME_COUNT
    jae .done
    imul eax, ebx, THEME_NAME_MAX
    lea rdi, [vga_theme_names + rax]
    mov rsi, theme_name_buffer
    call str_equal
    test eax, eax
    jnz .apply
    inc ebx
    jmp .match
.apply:
    mov edi, ebx
    call vga_set_theme
.done:
    pop rbx
    ret

theme_file_path db '/etc/theme', 0
theme_name_buffer:
    times THEME_NAME_MAX db 0

vga_theme_names:
    db 'python', 0, 0, 0, 0, 0, 0
    db 'viper', 0, 0, 0, 0, 0, 0, 0
    db 'cobra', 0, 0, 0, 0, 0, 0, 0
    db 'mamba', 0, 0, 0, 0, 0, 0, 0
    db 'boa', 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'taipan', 0, 0, 0, 0, 0, 0

vga_themes:
    db 0, 0, 0
    db 0, 0, 42
    db 0, 42, 0
    db 0, 42, 42
    db 42, 0, 0
    db 42, 0, 42
    db 42, 21, 0
    db 42, 42, 42
    db 21, 21, 21
    db 21, 21, 63
    db 21, 63, 21
    db 21, 63, 63
    db 63, 21, 21
    db 63, 21, 63
    db 63, 63, 21
    db 63, 63, 63

    db 0, 0, 0
    db 27, 18, 45
    db 12, 45, 22
    db 20, 45, 50
    db 50, 16, 16
    db 49, 21, 38
    db 50, 35, 20
    db 50, 50, 53
    db 21, 22, 26
    db 47, 36, 62
    db 20, 62, 30
    db 63, 30, 49
    db 63, 21, 21
    db 63, 36, 55
    db 60, 62, 35
    db 61, 61, 60

    db 0, 0, 0
    db 45, 29, 5
    db 38, 37, 6
    db 26, 39, 26
    db 50, 9, 7
    db 44, 24, 33
    db 53, 23, 3
    db 53, 48, 40
    db 23, 22, 21
    db 63, 32, 6
    db 45, 46, 9
    db 62, 47, 12
    db 62, 18, 13
    db 52, 33, 38
    db 55, 43, 16
    db 62, 60, 49

    db 0, 0, 0
    db 23, 32, 42
    db 31, 38, 26
    db 23, 37, 42
    db 47, 24, 26
    db 44, 35, 43
    db 51, 33, 28
    db 53, 55, 58
    db 21, 22, 26
    db 32, 40, 48
    db 40, 47, 35
    db 34, 47, 51
    db 55, 30, 33
    db 50, 41, 48
    db 58, 50, 34
    db 58, 59, 60

    db 0, 0, 0
    db 8, 27, 17
    db 16, 40, 19
    db 12, 41, 38
    db 47, 14, 11
    db 35, 27, 43
    db 38, 30, 12
    db 45, 53, 43
    db 20, 23, 21
    db 20, 48, 26
    db 31, 59, 34
    db 22, 59, 40
    db 63, 26, 22
    db 49, 38, 57
    db 57, 56, 26
    db 58, 63, 59

    db 0, 0, 0
    db 9, 34, 52
    db 33, 38, 0
    db 10, 40, 38
    db 54, 12, 12
    db 52, 13, 32
    db 45, 34, 0
    db 42, 46, 46
    db 12, 23, 26
    db 27, 28, 48
    db 41, 48, 0
    db 13, 51, 48
    db 50, 19, 5
    db 55, 23, 40
    db 55, 43, 0
    db 63, 61, 56

vga_theme_current dd 0

; 16-47 fixed art colors, then a gray ramp for whatever the gui does not set itself
vga_load_art_palette:
    push rax
    push rcx
    push rdx
    push rsi
    mov dx, VGA_DAC_WRITE_INDEX
    mov al, PALETTE_ART_FIRST
    out dx, al
    mov dx, VGA_DAC_DATA
    lea rsi, [art_palette]
    mov ecx, PALETTE_ART_COLORS * 3
.art:
    lodsb
    out dx, al
    dec ecx
    jnz .art
    mov ecx, PALETTE_FREE_FIRST
.free:
    mov eax, ecx
    sub eax, PALETTE_FREE_FIRST
    shr eax, 2
    out dx, al
    out dx, al
    out dx, al
    inc ecx
    cmp ecx, 256
    jb .free
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

vga_load_range:
    push rax
    push rcx
    push rdx
    push rsi
    mov dx, VGA_DAC_WRITE_INDEX
    mov eax, edi
    out dx, al
    mov dx, VGA_DAC_DATA
    imul ecx, ecx, 3
.next:
    lodsb
    out dx, al
    dec ecx
    jnz .next
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

art_palette:
    db 47, 18, 12
    db 53, 29, 17
    db 58, 52, 42
    db 56, 41, 28
    db 45, 27, 20
    db 28, 15, 14
    db 15, 10, 12
    db 40, 9, 13
    db 56, 15, 17
    db 61, 29, 8
    db 63, 43, 13
    db 63, 57, 24
    db 24, 49, 19
    db 15, 34, 18
    db 9, 23, 16
    db 6, 15, 15
    db 4, 19, 34
    db 0, 38, 54
    db 11, 57, 61
    db 63, 63, 63
    db 47, 50, 54
    db 34, 38, 44
    db 22, 26, 34
    db 14, 17, 25
    db 9, 11, 17
    db 6, 5, 9
    db 63, 0, 17
    db 26, 14, 27
    db 45, 20, 34
    db 61, 29, 30
    db 57, 45, 37
    db 48, 33, 26

frame_base dq FRAMEBUFFER_ADDRESS
