format binary
use64
org 0x400000

value = 0x1234

start:
    mov rax, rbx
    mov eax, ebx
    mov ax, bx
    mov al, bl
    mov r8, r15
    mov r9d, eax
    mov sil, dil
    mov rax, 5
    mov rax, -1
    mov rax, 0x123456789
    mov eax, 5
    mov r10d, value
    mov ax, 7
    mov al, 1
    mov r12b, 3
    mov [rbx], rax
    mov [rbx + 8], eax
    mov [rbx + 0x100], ax
    mov [rsp], rcx
    mov [rsp + 16], rdx
    mov [rbp], rsi
    mov [rbp - 8], rdi
    mov [r12], r8
    mov [r13], r9
    mov [r13 + 4], r10d
    mov [rax + rbx*4], ecx
    mov [rax + rbx*8 + 16], rdx
    mov [r8 + r9*2 - 4], ax
    mov rax, [rbx]
    mov ecx, [rsi + rdi]
    mov dl, [rax + 1]
    mov byte [rbx], 7
    mov word [rbx + 2], 0x1234
    mov dword [rbx + 4], -1
    mov qword [rsp + 8], 100
    mov [data_word], eax
    mov eax, [data_word]
    lea rsi, [data_word]
    lea rdi, [rsi + rcx*4 + 12]
    lea eax, [rax + 1]
    add rax, rbx
    add eax, 1
    add rax, 1000
    add eax, 100000
    add al, 5
    add al, 200
    add bl, 5
    add rbx, 1000
    add qword [rbx], 1
    add dword [rbx + 4], 1000
    add [rbx], rcx
    add rcx, [rbx]
    or rax, rcx
    adc rax, 1
    sbb ecx, edx
    and rsi, -16
    sub rsp, 32
    xor eax, eax
    xor r15d, r15d
    cmp rax, 10
    cmp byte [rsi], 0
    cmp [rdi + 8], rax
    cmp eax, 0x10000
    test rax, rax
    test al, 1
    test eax, 0x100
    test ebx, 0x100
    test byte [rbx], 0x80
    test dword [rsi], 4
    not rax
    neg ecx
    mul rbx
    div rcx
    idiv r8
    inc rax
    dec dword [rbx]
    inc r12d
    imul rax, rbx
    imul eax, [rsi]
    imul rcx, rdx, 10
    imul rcx, rdx, 1000
    imul rax, 7
    shl rax, 1
    shl rax, 4
    shr ecx, cl
    sar rdx, 63
    rol eax, 3
    shl byte [rbx], 2
    push rax
    push r12
    pop rbx
    pop r15
    push 5
    push 1000
    movzx eax, byte [rsi]
    movzx ecx, al
    movzx rax, word [rbx]
    movsx rax, byte [rdi]
    movsx ecx, dx
    movsxd rax, dword [rbx]
    movsxd rdx, eax
    xchg rax, rbx
    xchg ecx, eax
    xchg [rsi], ecx
    xchg ecx, edx
    xchg al, cl
    xchg r8, rbx
    xchg ecx, [rax]
    lea esi, [rbx * 2]
    lea esi, [rbx * 2 + 1]
    mov eax, [r13 * 2 + 8]
    bt eax, ecx
    bts dword [rbx], eax
    btr rax, 5
    sete al
    setne byte [rbx]
    cmovz rax, rbx
    cmovl ecx, [rsi]
    int 0x80
    rep movsb
    rep stosq
    repe cmpsb
    cld
    std
    cqo
    cdq
    syscall
    nop
    hlt
back:
    jmp back
    je back
    jne start
    jz back
    call back
    call rax
    jmp rbx
    call qword [rsi]
    jmp qword [rax + 8]
    ret
    ret 8
sse:
    movsd xmm0, qword [rbp - 8]
    movsd qword [rbp - 16], xmm1
    movsd xmm2, xmm3
    movss xmm4, dword [rax]
    movss dword [rdi + rcx * 4], xmm5
    addsd xmm0, xmm1
    subsd xmm8, qword [rsp + 8]
    mulsd xmm9, xmm15
    divsd xmm1, xmm0
    sqrtsd xmm2, xmm2
    addss xmm3, xmm4
    ucomisd xmm0, xmm1
    ucomiss xmm2, dword [rbx]
    cvtsi2sd xmm0, rax
    cvtsi2sd xmm1, ecx
    cvtsi2ss xmm2, r9
    cvttsd2si rax, xmm0
    cvttsd2si ecx, qword [rsi]
    cvttss2si r10, xmm11
    cvtsd2ss xmm0, xmm1
    cvtss2sd xmm1, dword [rdx]
    movq xmm0, rax
    movq rax, xmm0
    movq xmm12, r13
    movq r14, xmm9
    movq xmm1, qword [rbp - 24]
    movq qword [rbp - 32], xmm2
    xorpd xmm0, xmm0
    andpd xmm1, xmm2
    pxor xmm3, xmm3
    movapd xmm4, xmm5
    movsd
data_word dd 0x11223344
data_bytes db 1, 2, 'abc', 0
data_quad dq 0x1122334455667788
data_words dw 1, 2
table dq back, start
    times 4 db 0x90
    db 'end'
