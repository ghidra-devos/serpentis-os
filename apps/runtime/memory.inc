; rdi = dst, rsi = src, rdx = bytes. out: rax = dst, like memcpy
mem_copy:
    mov rax, rdi
    mov rcx, rdx
    cld
    rep movsb
    ret

mem_set:
    mov r8, rdi
    mov eax, esi
    mov rcx, rdx
    cld
    rep stosb
    mov rax, r8
    ret
