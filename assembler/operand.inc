OP_NONE = 0
OP_REG = 1
OP_MEM = 2
OP_IMM = 3

OP_KIND = 0
OP_WIDTH = 4
OP_REGNUM = 8

OP_REGFLAGS = 12                ; REG_NEEDS_REX / REG_HIGH8
OP_BASE = 16
OP_INDEX = 20
OP_SCALE = 24
OP_KNOWN = 28
OP_VALUE = 32
OP_ESTIMATED = 40
OP_SIZE = 48

REG_NEEDS_REX = 1
XMM_WIDTH = 16
REG_HIGH8 = 2

register_lookup:
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .no
    mov ecx, [rdi + TOKEN_LENGTH]
    cmp ecx, 5
    ja .no
    mov r9, [rdi + TOKEN_TEXT]
    lea rsi, [register_table]

.entry:
    movzx edx, byte [rsi]
    test edx, edx
    jz .no
    cmp edx, ecx
    jne .skip

    xor r8d, r8d
.compare:
    mov al, [r9 + r8]
    or al, 0x20
    cmp al, [rsi + 1 + r8]
    jne .skip
    inc r8d
    cmp r8d, ecx
    jb .compare

    movzx eax, byte [rsi + 1 + rdx + 1]
    movzx ecx, byte [rsi + 1 + rdx + 2]
    movzx edx, byte [rsi + 1 + rdx]
    stc
    ret

.skip:
    lea rsi, [rsi + rdx + 4]
    jmp .entry

.no:
    clc
    ret

parse_operand:
    push r12
    mov r12, rbx

    mov rdi, r12
    xor eax, eax
    mov ecx, OP_SIZE / 4
    rep stosd
    mov dword [r12 + OP_BASE], -1
    mov dword [r12 + OP_INDEX], -1
    mov dword [r12 + OP_SCALE], 1
    mov dword [r12 + OP_KNOWN], 1

    lea rsi, [size_keywords]
.size_keyword:
    cmp byte [rsi], 0
    je .size_done
    mov rdi, [token_ptr]
    push rsi
    call token_equals
    pop rsi
    jc .size_found
    call .skip_string
    inc rsi
    jmp .size_keyword
.size_found:
    call .skip_string
    movzx eax, byte [rsi]
    mov [r12 + OP_WIDTH], eax
    call token_advance
    mov rdi, [token_ptr]
    lea rsi, [kw_ptr]
    call token_equals
    jnc .size_done
    call token_advance
.size_done:

    mov rdi, [token_ptr]
    mov sil, '['
    call token_is_punct
    jc .memory

    mov rdi, [token_ptr]
    call register_lookup
    jnc .immediate

    cmp dword [r12 + OP_WIDTH], 0
    je .register_width
    cmp [r12 + OP_WIDTH], edx
    jne .size_mismatch
.register_width:
    mov dword [r12 + OP_KIND], OP_REG
    mov [r12 + OP_WIDTH], edx
    mov [r12 + OP_REGNUM], eax
    mov [r12 + OP_REGFLAGS], ecx
    call token_advance
    jmp .done

.immediate:
    mov dword [r12 + OP_KIND], OP_IMM
    call expr_parse
    mov [r12 + OP_VALUE], rax
    movzx eax, byte [expr_known]
    mov [r12 + OP_KNOWN], eax
    movzx eax, byte [expr_estimated]
    mov [r12 + OP_ESTIMATED], eax
    jmp .done

.memory:
    mov dword [r12 + OP_KIND], OP_MEM
    call token_advance
    mov r8d, 1

.term:
    mov rdi, [token_ptr]
    push r8
    call register_lookup
    pop r8
    jnc .displacement

    cmp edx, 8
    jne .bad_memory
    cmp r8d, 1
    jne .bad_memory
    push rax
    call token_advance
    mov rdi, [token_ptr]
    mov sil, '*'
    call token_is_punct
    pop rax
    mov ecx, 1
    jnc .place_register

    push rax
    call token_advance
    mov byte [expr_known], 1
    call expr_unary
    mov ecx, eax
    pop rax
    cmp byte [expr_known], 0
    je .bad_memory
    cmp ecx, 1
    je .place_register
    cmp ecx, 2
    je .place_register
    cmp ecx, 4
    je .place_register
    cmp ecx, 8
    jne .bad_memory

.place_register:
    cmp ecx, 1
    jne .as_index
    cmp dword [r12 + OP_BASE], -1
    jne .as_index
    mov [r12 + OP_BASE], eax
    jmp .separator
.as_index:
    cmp dword [r12 + OP_INDEX], -1
    jne .bad_memory
    mov [r12 + OP_INDEX], eax
    mov [r12 + OP_SCALE], ecx
    jmp .separator

.displacement:
    mov byte [expr_known], 1
    push r8
    call expr_term
    pop r8
    imul rax, r8
    add [r12 + OP_VALUE], rax
    cmp byte [expr_known], 0
    jne .separator
    mov dword [r12 + OP_KNOWN], 0

.separator:
    mov rdi, [token_ptr]
    mov sil, ']'
    call token_is_punct
    jc .memory_done
    mov r8d, 1
    mov rdi, [token_ptr]
    mov sil, '+'
    call token_is_punct
    jc .next_term
    mov r8, -1
    mov rdi, [token_ptr]
    mov sil, '-'
    call token_is_punct
    jnc .bad_memory
.next_term:
    call token_advance
    jmp .term

.memory_done:
    call token_advance
    cmp dword [r12 + OP_BASE], -1
    jne .done
    cmp dword [r12 + OP_SCALE], 2
    jne .done
    mov eax, [r12 + OP_INDEX]
    mov [r12 + OP_BASE], eax
    mov dword [r12 + OP_SCALE], 1

.done:
    pop r12
    ret

.skip_string:
    cmp byte [rsi], 0
    je .skipped
    inc rsi
    jmp .skip_string
.skipped:
    inc rsi
    ret

.size_mismatch:
    lea rdi, [error_size_mismatch]
    jmp asm_error

.bad_memory:
    lea rdi, [error_bad_memory]
    jmp asm_error

parse_operands:
    push rbx
    mov dword [operand_count], 0
    lea rbx, [operand_1]
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .done

.next:
    call parse_operand
    inc dword [operand_count]
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .done
    mov sil, ','
    call token_is_punct
    jnc .junk
    cmp dword [operand_count], 3
    jae .junk
    call token_advance
    add rbx, OP_SIZE
    jmp .next

.done:
    pop rbx
    ret

.junk:
    lea rdi, [error_extra_tokens]
    jmp asm_error

size_keywords:
    db 'byte', 0, 1
    db 'word', 0, 2
    db 'dword', 0, 4
    db 'qword', 0, 8
    db 0

; len, name, width, number, flags
register_table:
    db 3, 'rax', 8, 0, 0
    db 3, 'rcx', 8, 1, 0
    db 3, 'rdx', 8, 2, 0
    db 3, 'rbx', 8, 3, 0
    db 3, 'rsp', 8, 4, 0
    db 3, 'rbp', 8, 5, 0
    db 3, 'rsi', 8, 6, 0
    db 3, 'rdi', 8, 7, 0
    db 2, 'r8', 8, 8, 0
    db 2, 'r9', 8, 9, 0
    db 3, 'r10', 8, 10, 0
    db 3, 'r11', 8, 11, 0
    db 3, 'r12', 8, 12, 0
    db 3, 'r13', 8, 13, 0
    db 3, 'r14', 8, 14, 0
    db 3, 'r15', 8, 15, 0
    db 3, 'eax', 4, 0, 0
    db 3, 'ecx', 4, 1, 0
    db 3, 'edx', 4, 2, 0
    db 3, 'ebx', 4, 3, 0
    db 3, 'esp', 4, 4, 0
    db 3, 'ebp', 4, 5, 0
    db 3, 'esi', 4, 6, 0
    db 3, 'edi', 4, 7, 0
    db 3, 'r8d', 4, 8, 0
    db 3, 'r9d', 4, 9, 0
    db 4, 'r10d', 4, 10, 0
    db 4, 'r11d', 4, 11, 0
    db 4, 'r12d', 4, 12, 0
    db 4, 'r13d', 4, 13, 0
    db 4, 'r14d', 4, 14, 0
    db 4, 'r15d', 4, 15, 0
    db 2, 'ax', 2, 0, 0
    db 2, 'cx', 2, 1, 0
    db 2, 'dx', 2, 2, 0
    db 2, 'bx', 2, 3, 0
    db 2, 'sp', 2, 4, 0
    db 2, 'bp', 2, 5, 0
    db 2, 'si', 2, 6, 0
    db 2, 'di', 2, 7, 0
    db 3, 'r8w', 2, 8, 0
    db 3, 'r9w', 2, 9, 0
    db 4, 'r10w', 2, 10, 0
    db 4, 'r11w', 2, 11, 0
    db 4, 'r12w', 2, 12, 0
    db 4, 'r13w', 2, 13, 0
    db 4, 'r14w', 2, 14, 0
    db 4, 'r15w', 2, 15, 0
    db 2, 'al', 1, 0, 0
    db 2, 'cl', 1, 1, 0
    db 2, 'dl', 1, 2, 0
    db 2, 'bl', 1, 3, 0
    db 3, 'spl', 1, 4, REG_NEEDS_REX
    db 3, 'bpl', 1, 5, REG_NEEDS_REX
    db 3, 'sil', 1, 6, REG_NEEDS_REX
    db 3, 'dil', 1, 7, REG_NEEDS_REX
    db 2, 'ah', 1, 4, REG_HIGH8
    db 2, 'ch', 1, 5, REG_HIGH8
    db 2, 'dh', 1, 6, REG_HIGH8
    db 2, 'bh', 1, 7, REG_HIGH8
    db 3, 'r8b', 1, 8, 0
    db 3, 'r9b', 1, 9, 0
    db 4, 'r10b', 1, 10, 0
    db 4, 'r11b', 1, 11, 0
    db 4, 'r12b', 1, 12, 0
    db 4, 'r13b', 1, 13, 0
    db 4, 'r14b', 1, 14, 0
    db 4, 'r15b', 1, 15, 0
    db 4, 'xmm0', XMM_WIDTH, 0, 0
    db 4, 'xmm1', XMM_WIDTH, 1, 0
    db 4, 'xmm2', XMM_WIDTH, 2, 0
    db 4, 'xmm3', XMM_WIDTH, 3, 0
    db 4, 'xmm4', XMM_WIDTH, 4, 0
    db 4, 'xmm5', XMM_WIDTH, 5, 0
    db 4, 'xmm6', XMM_WIDTH, 6, 0
    db 4, 'xmm7', XMM_WIDTH, 7, 0
    db 4, 'xmm8', XMM_WIDTH, 8, 0
    db 4, 'xmm9', XMM_WIDTH, 9, 0
    db 5, 'xmm10', XMM_WIDTH, 10, 0
    db 5, 'xmm11', XMM_WIDTH, 11, 0
    db 5, 'xmm12', XMM_WIDTH, 12, 0
    db 5, 'xmm13', XMM_WIDTH, 13, 0
    db 5, 'xmm14', XMM_WIDTH, 14, 0
    db 5, 'xmm15', XMM_WIDTH, 15, 0
    db 0
