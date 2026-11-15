; a .pic (SPIC, u16 w, u16 h, pixels) drawn as a picture in the terminal, only when a gui
; terminal is there to show it. rdi -> path, rsi -> 16 KB to read it into. CF when not shown
LOGO_FILE_MAX = 0x4000

logo_show:
    push rbx
    push r12
    push r13
    mov r13, rsi
    push rdi
    mov edi, GUI_SERVICE_QUERY
    call sys_service
    pop rdi
    test eax, eax
    js .no
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .no
    mov ebx, eax
    mov edi, eax
    mov rsi, r13
    mov edx, LOGO_FILE_MAX
    call sys_read
    mov r12, rax
    mov edi, ebx
    call sys_close
    cmp r12, 8
    jl .no
    cmp dword [r13], 'SPIC'
    jne .no
    movzx edi, word [r13 + 4]
    movzx esi, word [r13 + 6]
    mov eax, edi
    imul eax, esi
    add eax, 8
    cmp rax, r12
    ja .no
    push rdi
    push rsi
    call tg_open
    pop rsi
    pop rdi
    test rax, rax
    js .no
    imul esi, edi
    mov ecx, esi
    mov rdi, rax
    lea rsi, [r13 + 8]
    cld
    rep movsb
    call tg_frame
    call tg_close
    pop r13
    pop r12
    pop rbx
    clc
    ret
.no:
    pop r13
    pop r12
    pop rbx
    stc
    ret
