expr_parse:
    mov byte [expr_known], 1
    mov byte [expr_estimated], 0

expr_or:
    push rbx
    call expr_and
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    lea rsi, [kw_or]
    call token_equals
    jc .or
    mov rdi, [token_ptr]
    mov sil, '|'
    call token_is_punct
    jc .or
    mov rdi, [token_ptr]
    lea rsi, [kw_xor]
    call token_equals
    jc .xor
    mov rax, rbx
    pop rbx
    ret
.or:
    call token_advance
    call expr_and
    or rbx, rax
    jmp .next
.xor:
    call token_advance
    call expr_and
    xor rbx, rax
    jmp .next

expr_and:
    push rbx
    call expr_shift
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    lea rsi, [kw_and]
    call token_equals
    jc .and
    mov rdi, [token_ptr]
    mov sil, '&'
    call token_is_punct
    jc .and
    mov rax, rbx
    pop rbx
    ret
.and:
    call token_advance
    call expr_shift
    and rbx, rax
    jmp .next

expr_shift:
    push rbx
    call expr_add
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    lea rsi, [kw_shl]
    call token_equals
    jc .shl
    mov rdi, [token_ptr]
    lea rsi, [kw_shr]
    call token_equals
    jc .shr
    mov rax, rbx
    pop rbx
    ret
.shl:
    call token_advance
    call expr_add
    mov ecx, eax
    shl rbx, cl
    jmp .next
.shr:
    call token_advance
    call expr_add
    mov ecx, eax
    shr rbx, cl
    jmp .next

expr_add:
    push rbx
    call expr_term
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    mov sil, '+'
    call token_is_punct
    jc .add
    mov rdi, [token_ptr]
    mov sil, '-'
    call token_is_punct
    jc .sub
    mov rax, rbx
    pop rbx
    ret
.add:
    call token_advance
    call expr_term
    add rbx, rax
    jmp .next
.sub:
    call token_advance
    call expr_term
    sub rbx, rax
    jmp .next

expr_term:
    push rbx
    call expr_unary
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    mov sil, '*'
    call token_is_punct
    jc .mul
    mov rdi, [token_ptr]
    mov sil, '/'
    call token_is_punct
    jc .div
    mov rdi, [token_ptr]
    lea rsi, [kw_mod]
    call token_equals
    jc .mod
    mov rax, rbx
    pop rbx
    ret
.mul:
    call token_advance
    call expr_unary
    imul rbx, rax
    jmp .next
.div:
    call token_advance
    call expr_unary
    call .divide
    mov rbx, rax
    jmp .next
.mod:
    call token_advance
    call expr_unary
    call .divide
    mov rbx, rdx
    jmp .next
.divide:
    mov rcx, rax
    test rcx, rcx
    jnz .nonzero
    cmp byte [expr_known], 0
    je .zero_result
    lea rdi, [error_divide_zero]
    jmp asm_error
.zero_result:
    xor eax, eax
    xor edx, edx
    ret
.nonzero:
    mov rax, rbx
    cqo
    idiv rcx
    ret

expr_unary:
    mov rdi, [token_ptr]
    mov sil, '-'
    call token_is_punct
    jc .negate
    mov rdi, [token_ptr]
    mov sil, '+'
    call token_is_punct
    jc .plus
    mov rdi, [token_ptr]
    mov sil, '~'
    call token_is_punct
    jc .invert
    mov rdi, [token_ptr]
    lea rsi, [kw_not]
    call token_equals
    jc .invert
    jmp expr_primary
.negate:
    call token_advance
    call expr_unary
    neg rax
    ret
.plus:
    call token_advance
    jmp expr_unary
.invert:
    call token_advance
    call expr_unary
    not rax
    ret

expr_primary:
    mov rdi, [token_ptr]
    mov eax, [rdi + TOKEN_TYPE]
    cmp eax, TOKEN_NUMBER
    je .number
    cmp eax, TOKEN_STRING
    je .string
    cmp eax, TOKEN_IDENT
    je .symbol
    cmp eax, TOKEN_PUNCT
    jne .expected

    mov sil, '('
    call token_is_punct
    jc .group
    mov rdi, [token_ptr]
    mov sil, '$'
    call token_is_punct
    jc .here
    mov rdi, [token_ptr]
    mov sil, '%'
    call token_is_punct
    jc .counter

.expected:
    lea rdi, [error_expression]
    jmp asm_error

.number:
    mov rax, [rdi + TOKEN_VALUE]
    jmp token_advance

.here:
    mov rax, [line_address]
    jmp token_advance

.counter:
    mov rax, [repeat_counter]
    jmp token_advance

.string:
    mov ecx, [rdi + TOKEN_LENGTH]
    cmp ecx, 8
    ja .expected
    mov rsi, [rdi + TOKEN_TEXT]
    xor eax, eax
.string_byte:
    test ecx, ecx
    jz token_advance
    dec ecx
    shl rax, 8
    mov al, [rsi + rcx]
    jmp .string_byte

.group:
    call token_advance
    call expr_or
    push rax
    mov rdi, [token_ptr]
    mov sil, ')'
    call token_is_punct
    pop rax
    jnc .expected
    jmp token_advance

.symbol:
    push rbx
    mov rsi, [rdi + TOKEN_TEXT]
    mov ecx, [rdi + TOKEN_LENGTH]
    call symbol_lookup
    mov rbx, rax
    mov eax, [current_pass]
    cmp [rbx + SYM_PASS], eax
    je .known
    test dword [rbx + SYM_FLAGS], SYM_FLAG_UNRESOLVED
    jnz .unresolved
    test dword [rbx + SYM_FLAGS], SYM_FLAG_CONSTANT
    jnz .known
    mov byte [expr_known], 0
    test dword [rbx + SYM_FLAGS], SYM_FLAG_LABEL
    jnz .estimated
.unresolved:
    mov byte [expr_known], 0
    cmp byte [final_pass], 0
    je .unknown
    jmp .undefined
.estimated:
    mov byte [expr_estimated], 1
.known:
    mov rax, [rbx + SYM_VALUE]
    pop rbx
    jmp token_advance
.unknown:
    xor eax, eax
    pop rbx
    jmp token_advance
.undefined:
    pop rbx
    lea rdi, [error_undefined]
    jmp asm_error

token_advance:
    add qword [token_ptr], TOKEN_SIZE
    ret
