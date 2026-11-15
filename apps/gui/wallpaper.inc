load_wallpaper:
    call read_choice
    cmp byte [wallpaper_line], '/'
    jne .named
    call picture_wallpaper
    jnc .picture
.named:
    push rbx
    call wallpaper_choice
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    jns .opened
    lea rdi, [wallpaper_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
.opened:
    mov ebx, eax
    mov edi, ebx
    lea rsi, [wallpaper_header]
    mov edx, WALLPAPER_HEADER
    call sys_read
    cmp eax, WALLPAPER_HEADER
    jne .close
    cmp dword [wallpaper_header], WALLPAPER_MAGIC
    jne .close
    movzx eax, word [wallpaper_header + 6]
    cmp eax, WALLPAPER_COLORS
    ja .close
    imul edx, eax, 3
    mov edi, ebx
    lea rsi, [wallpaper_palette]
    call sys_read
    mov edi, ebx
    lea rsi, [wallpaper]
    mov edx, WALLPAPER_BYTES
    call sys_read
    cmp eax, WALLPAPER_BYTES
    jne .close
    movzx edi, word [wallpaper_header + 4]
    movzx esi, word [wallpaper_header + 6]
    lea rdx, [wallpaper_palette]
    call sys_palette
    mov byte [wallpaper_ready], 1
.close:
    mov edi, ebx
    call sys_close
.done:
    pop rbx
.picture:
    ret

; first line of /etc/wallpaper, empty when there is none. a name from /share/wallpapers
; or the full path of a .pic
read_choice:
    push rbx
    mov byte [wallpaper_line], 0
    lea rdi, [wallpaper_name_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
    mov edi, ebx
    lea rsi, [wallpaper_line]
    mov edx, WALLPAPER_LINE_MAX - 1
    call sys_read
    push rax
    mov edi, ebx
    call sys_close
    pop rcx
    test ecx, ecx
    jle .done
    lea rsi, [wallpaper_line]
.cut:
    cmp byte [rsi], ' '
    jb .end
    inc rsi
    dec ecx
    jnz .cut
.end:
    mov byte [rsi], 0
.done:
    pop rbx
    ret

; a .pic gets stretched over the screen. its colors are the fixed ones so there is no
; palette to load, the shade table just gets built over those. CF when it is no good
picture_wallpaper:
    push rbx
    push r12
    lea rdi, [wallpaper_line]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .no
    mov ebx, eax
    mov edi, ebx
    lea rsi, [picture_header]
    mov edx, PICTURE_HEADER
    call sys_read
    cmp eax, PICTURE_HEADER
    jne .close
    cmp dword [picture_header], PICTURE_MAGIC
    jne .close
    movzx r12d, word [picture_header + 4]
    movzx eax, word [picture_header + 6]
    test r12d, r12d
    jz .close
    cmp r12d, SCREEN_W
    ja .close
    test eax, eax
    jz .close
    cmp eax, WALLPAPER_H
    ja .close
    mov [picture_rows], eax
    mov edx, eax
    imul edx, r12d
    mov [picture_bytes], edx
    mov edi, ebx
    lea rsi, [picture_pixels]
    call sys_read
    cmp eax, [picture_bytes]
    jne .close

; stretched over the whole wallpaper, nearest pixel, 16.16 steps
    mov byte [wallpaper_ready], 0
    mov eax, r12d
    shl eax, 16
    xor edx, edx
    mov ecx, SCREEN_W
    div ecx
    mov r8d, eax
    mov eax, [picture_rows]
    shl eax, 16
    xor edx, edx
    mov ecx, WALLPAPER_H
    div ecx
    mov r9d, eax
    lea rdi, [wallpaper]
    xor r10d, r10d
    mov r11d, WALLPAPER_H
.row:
    mov eax, r10d
    shr eax, 16
    imul eax, r12d
    lea rsi, [picture_pixels + rax]
    xor edx, edx
    mov ecx, SCREEN_W
.column:
    mov eax, edx
    shr eax, 16
    mov al, [rsi + rax]
    stosb
    add edx, r8d
    dec ecx
    jnz .column
    add r10d, r9d
    dec r11d
    jnz .row

    mov word [wallpaper_header + 4], PALETTE_ART_FIRST
    mov word [wallpaper_header + 6], PALETTE_ART_COLORS
    lea rdi, [wallpaper_palette]
    lea rsi, [art_palette]
    mov ecx, PALETTE_ART_COLORS * 3
    rep movsb
    mov byte [wallpaper_ready], 1
    mov edi, ebx
    call sys_close
    pop r12
    pop rbx
    clc
    ret
.close:
    mov edi, ebx
    call sys_close
.no:
    pop r12
    pop rbx
    stc
    ret

; rdi -> /share/wallpapers/<name>.raw for the name read_choice found, or the default
; picture when there is no name or it is no good
wallpaper_choice:
    lea rsi, [wallpaper_line]
    xor ecx, ecx
.scan:
    mov dl, [rsi + rcx]
    test dl, dl
    jz .cut
    cmp dl, '/'                 ; a name, not a path out of the folder
    je .default
    inc ecx
    cmp ecx, WALLPAPER_NAME_MAX
    ja .default
    jmp .scan
.cut:
    test ecx, ecx
    jz .default
    lea rdi, [wallpaper_file + wallpaper_dir_length]
    call str_copy
    lea rdi, [wallpaper_file]
    call str_len
    lea rdi, [wallpaper_file + rax]
    lea rsi, [wallpaper_suffix]
    call str_copy
    lea rdi, [wallpaper_file]
    ret
.default:
    lea rdi, [wallpaper_path]
    ret

draw_wallpaper:
    cmp byte [wallpaper_ready], 0
    je .done
    lea rsi, [wallpaper]
    lea rdi, [scene + BAR_H * SCREEN_W]
    mov ecx, WALLPAPER_BYTES / 8
    rep movsq
.done:
    ret

build_shade:
    push rbx
    push r12
    push r13
    push r14
    lea rdi, [shade]
    xor eax, eax
    mov ecx, 256
    rep stosb
    cmp byte [wallpaper_ready], 0
    je .done
    movzx r12d, word [wallpaper_header + 4]
    movzx r13d, word [wallpaper_header + 6]
    xor ebx, ebx
.color:
    cmp ebx, r13d
    jae .done
    mov dword [best_distance], 0x7FFFFFFF
    mov r14d, ebx
    xor ecx, ecx
.candidate:
    cmp ecx, r13d
    jae .chosen
    xor r8d, r8d
    xor edx, edx
.channel:
    imul eax, ebx, 3
    add eax, edx
    movzx eax, byte [wallpaper_palette + rax]
    imul eax, SHADE_KEEP
    shr eax, 3
    imul r9d, ecx, 3
    add r9d, edx
    movzx r9d, byte [wallpaper_palette + r9]
    sub eax, r9d
    imul eax, eax
    add r8d, eax
    inc edx
    cmp edx, 3
    jb .channel
    cmp r8d, [best_distance]
    jae .worse
    mov [best_distance], r8d
    mov r14d, ecx
.worse:
    inc ecx
    jmp .candidate
.chosen:
    lea eax, [r12 + rbx]
    add r14d, r12d
    mov [shade + rax], r14b
    inc ebx
    jmp .color
.done:
    mov byte [shade + SLIDE_KEY], SLIDE_KEY  ; the key has to stay the key
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
