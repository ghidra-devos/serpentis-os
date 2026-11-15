; sha-256 for the password hashes, straight from FIPS 180-4. input is short (salt plus a
; password), at most 119 bytes so the padding never needs a third block

SHA_INPUT_MAX = 119

sha256:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    cmp ecx, SHA_INPUT_MAX
    jbe .length_ok
    mov ecx, SHA_INPUT_MAX
.length_ok:
    mov ebp, ecx
    mov rdi, sha_block
    push rcx
    mov ecx, 128 / 8
    xor eax, eax
    cld
    rep stosq
    pop rcx
    mov rdi, sha_block
    rep movsb
    mov byte [sha_block + rbp], 0x80
    mov ebx, 64
    cmp ebp, 55
    jbe .one_block
    mov ebx, 128
.one_block:
    mov rax, rbp
    shl rax, 3
    bswap rax
    mov [sha_block + rbx - 8], rax
    mov [sha_blocks_end], ebx

    mov rsi, sha_initial
    mov rdi, sha_hash
    mov ecx, 8
    rep movsd

    xor ebx, ebx
.block:
    cmp ebx, [sha_blocks_end]
    jae .output

    xor ecx, ecx
.load:
    mov eax, [sha_block + rbx + rcx * 4]
    bswap eax
    mov [sha_schedule + rcx * 4], eax
    inc ecx
    cmp ecx, 16
    jb .load
.expand:
    mov eax, [sha_schedule + rcx * 4 - 15 * 4]
    mov edx, eax
    ror edx, 7
    mov esi, eax
    ror esi, 18
    xor edx, esi
    shr eax, 3
    xor edx, eax
    mov eax, [sha_schedule + rcx * 4 - 2 * 4]
    mov esi, eax
    ror esi, 17
    mov edi, eax
    ror edi, 19
    xor esi, edi
    shr eax, 10
    xor esi, eax
    mov eax, [sha_schedule + rcx * 4 - 16 * 4]
    add eax, edx
    add eax, [sha_schedule + rcx * 4 - 7 * 4]
    add eax, esi
    mov [sha_schedule + rcx * 4], eax
    inc ecx
    cmp ecx, 64
    jb .expand

    mov r8d, [sha_hash]
    mov r9d, [sha_hash + 4]
    mov r10d, [sha_hash + 8]
    mov r11d, [sha_hash + 12]
    mov r12d, [sha_hash + 16]
    mov r13d, [sha_hash + 20]
    mov r14d, [sha_hash + 24]
    mov r15d, [sha_hash + 28]

    xor ecx, ecx
.round:
    mov eax, r12d
    ror eax, 6
    mov edx, r12d
    ror edx, 11
    xor eax, edx
    mov edx, r12d
    ror edx, 25
    xor eax, edx                ; S1(e)
    mov edx, r12d
    and edx, r13d
    mov esi, r12d
    not esi
    and esi, r14d
    xor edx, esi
    add eax, edx
    add eax, r15d
    add eax, [sha_constants + rcx * 4]
    add eax, [sha_schedule + rcx * 4]
    mov edi, eax

    mov eax, r8d
    ror eax, 2
    mov edx, r8d
    ror edx, 13
    xor eax, edx
    mov edx, r8d
    ror edx, 22
    xor eax, edx                ; S0(a)
    mov edx, r8d
    and edx, r9d
    mov esi, r8d
    and esi, r10d
    xor edx, esi
    mov esi, r9d
    and esi, r10d
    xor edx, esi
    add eax, edx

    mov r15d, r14d
    mov r14d, r13d
    mov r13d, r12d
    lea r12d, [r11 + rdi]
    mov r11d, r10d
    mov r10d, r9d
    mov r9d, r8d
    lea r8d, [rdi + rax]

    inc ecx
    cmp ecx, 64
    jb .round

    add [sha_hash], r8d
    add [sha_hash + 4], r9d
    add [sha_hash + 8], r10d
    add [sha_hash + 12], r11d
    add [sha_hash + 16], r12d
    add [sha_hash + 20], r13d
    add [sha_hash + 24], r14d
    add [sha_hash + 28], r15d
    add ebx, 64
    jmp .block

.output:
    xor ecx, ecx
.store:
    mov eax, [sha_hash + rcx * 4]
    bswap eax
    mov [sha_digest + rcx * 4], eax
    inc ecx
    cmp ecx, 8
    jb .store

    mov rdi, sha_block
    mov ecx, 128 / 8
    xor eax, eax
    rep stosq

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

align 4
sha_initial:
    dd 0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A
    dd 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
sha_constants:
    dd 0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5
    dd 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174
    dd 0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA
    dd 0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967
    dd 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85
    dd 0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070
    dd 0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3
    dd 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
sha_hash:
    times 8 dd 0
sha_digest:
    times 32 db 0
sha_blocks_end dd 0
sha_schedule:
    times 64 dd 0
align 8
sha_block:
    times 128 db 0
