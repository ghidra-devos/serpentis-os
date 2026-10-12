load_wallpaper:
    push rbx
    lea rdi, [wallpaper_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
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
