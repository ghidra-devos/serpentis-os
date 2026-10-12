COND_MAX = 32

COND_EQ = 0
COND_NE = 1
COND_LT = 2
COND_LE = 3
COND_GT = 4
COND_GE = 5


cond_reset:
    mov dword [cond_depth], 0
    ret

cond_emitting:
    mov eax, [cond_depth]
    test eax, eax
    jz .yes
    cmp byte [cond_active + rax - 1], 0
    je .no
.yes:
    stc
    ret
.no:
    clc
    ret

cond_parent:
    mov eax, [cond_depth]
    cmp eax, 1
    jbe .yes
    cmp byte [cond_active + rax - 2], 0
    je .no
.yes:
    stc
    ret
.no:
    clc
    ret

cond_finished:
    cmp dword [cond_depth], 0
    je .done
    cmp byte [final_pass], 0
    je .counted
    lea rdi, [error_cond_open]
    call print_line
.counted:
    inc dword [error_count]
    mov dword [cond_depth], 0
.done:
    ret

; inside a dead branch only if / else / end still matter, they keep the nesting right
cond_skip_line:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .done
    lea rsi, [cond_table]
    call keyword_lookup
    jnc .done
    push rdx
    call token_advance
    pop rdx
    jmp rdx
.done:
    ret

directive_if:
    mov eax, [cond_depth]
    cmp eax, COND_MAX
    jae .overflow
    call cond_emitting
    jnc .dead
    call cond_parse
    push rax
    call expect_end
    pop rax
    test rax, rax
    setnz cl
    mov eax, [cond_depth]
    mov [cond_active + rax], cl
    mov [cond_taken + rax], cl
    inc dword [cond_depth]
    ret
.dead:
    mov eax, [cond_depth]
    mov byte [cond_active + rax], 0
    mov byte [cond_taken + rax], 1
    inc dword [cond_depth]
    ret
.overflow:
    lea rdi, [error_cond_deep]
    jmp asm_error

directive_else:
    mov eax, [cond_depth]
    test eax, eax
    jz .orphan
    dec eax
    mov [cond_index], eax
    mov rdi, [token_ptr]
    lea rsi, [kw_if]
    call token_equals
    jc .else_if
    call expect_end
    call cond_parent
    jnc .dead
    mov eax, [cond_index]
    cmp byte [cond_taken + rax], 0
    jne .dead
    mov byte [cond_active + rax], 1
    mov byte [cond_taken + rax], 1
    ret
.dead:
    mov eax, [cond_index]
    mov byte [cond_active + rax], 0
    ret

.else_if:
    call token_advance
    call cond_parent
    jnc .dead
    mov eax, [cond_index]
    cmp byte [cond_taken + rax], 0
    jne .dead
    call cond_parse
    push rax
    call expect_end
    pop rax
    test rax, rax
    setnz cl
    mov eax, [cond_index]
    mov [cond_active + rax], cl
    or [cond_taken + rax], cl
    ret

.orphan:
    lea rdi, [error_cond_orphan]
    jmp asm_error

directive_end:
    mov rdi, [token_ptr]
    lea rsi, [kw_if]
    call token_equals
    jc .end_if
    mov rdi, [token_ptr]
    lea rsi, [kw_repeat]
    call token_equals
    jc .orphan
    lea rdi, [error_unknown_instruction]
    jmp asm_error
.end_if:
    call token_advance
    jmp directive_endif
.orphan:
    lea rdi, [error_block_orphan]
    jmp asm_error

cond_skip_end:
    mov rdi, [token_ptr]
    lea rsi, [kw_if]
    call token_equals
    jnc .done
    call token_advance
    jmp directive_endif
.done:
    ret

directive_endif:
    call expect_end
    mov eax, [cond_depth]
    test eax, eax
    jz directive_else.orphan
    dec eax
    mov [cond_depth], eax
    ret

directive_define:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .bad
    push rbx
    mov rbx, rdi
    call token_advance
    call expr_parse
    push rax
    call expect_end
    pop rdx
    mov rsi, [rbx + TOKEN_TEXT]
    mov ecx, [rbx + TOKEN_LENGTH]
    mov dword [symbol_define_flags], SYM_FLAG_CONSTANT
    call symbol_define
    and dword [rax + SYM_FLAGS], not SYM_FLAG_UNRESOLVED
    cmp byte [expr_known], 0
    jne .done
    or dword [rax + SYM_FLAGS], SYM_FLAG_UNRESOLVED
    mov dword [rax + SYM_PASS], 0
.done:
    pop rbx
    ret
.bad:
    lea rdi, [error_expression]
    jmp asm_error

cond_operand:
    jmp expr_shift

; conditions: or, and, ~, defined name, a < b and friends, (...). values are plain expressions
cond_parse:
    mov byte [expr_known], 1

cond_or:
    push rbx
    call cond_and
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    mov sil, '|'
    call token_is_punct
    jnc .done
    call token_advance
    call cond_and
    or rbx, rax
    jmp .next
.done:
    mov rax, rbx
    pop rbx
    ret

cond_and:
    push rbx
    call cond_not
    mov rbx, rax
.next:
    mov rdi, [token_ptr]
    mov sil, '&'
    call token_is_punct
    jnc .done
    call token_advance
    call cond_not
    and rbx, rax
    jmp .next
.done:
    mov rax, rbx
    pop rbx
    ret

cond_not:
    mov rdi, [token_ptr]
    mov sil, '~'
    call token_is_punct
    jc .invert
    mov rdi, [token_ptr]
    lea rsi, [kw_not]
    call token_equals
    jc .invert
    jmp cond_atom
.invert:
    call token_advance
    call cond_not
    xor eax, 1
    and eax, 1
    ret

cond_atom:
    mov rdi, [token_ptr]
    lea rsi, [kw_defined]
    call token_equals
    jc .defined
    mov rdi, [token_ptr]
    mov sil, '('
    call token_is_punct
    jnc cond_compare
    call cond_group_logical
    jnc cond_compare
    call token_advance
    call cond_or
    push rax
    mov rdi, [token_ptr]
    mov sil, ')'
    call token_is_punct
    jnc .bad
    call token_advance
    pop rax
    ret
.defined:
    call token_advance
    jmp cond_defined
.bad:
    pop rax
    lea rdi, [error_expression]
    jmp asm_error

cond_group_logical:
    push rbx
    mov rbx, [token_ptr]
    xor ecx, ecx
.scan:
    mov eax, [rbx + TOKEN_TYPE]
    cmp eax, TOKEN_END
    je .logical
    cmp eax, TOKEN_PUNCT
    jne .step
    mov rax, [rbx + TOKEN_VALUE]
    cmp al, '('
    jne .close
    inc ecx
    jmp .step
.close:
    cmp al, ')'
    jne .step
    dec ecx
    jz .after
.step:
    add rbx, TOKEN_SIZE
    jmp .scan
.after:
    add rbx, TOKEN_SIZE
    cmp dword [rbx + TOKEN_TYPE], TOKEN_PUNCT
    jne .logical
    mov rax, [rbx + TOKEN_VALUE]
    cmp al, '='
    je .numeric
    cmp al, '<'
    je .numeric
    cmp al, '>'
    je .numeric
    cmp al, '!'
    je .numeric
.logical:
    pop rbx
    stc
    ret
.numeric:
    pop rbx
    clc
    ret

cond_defined:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .bad
    mov rsi, [rdi + TOKEN_TEXT]
    mov ecx, [rdi + TOKEN_LENGTH]
    call symbol_lookup
    xor edx, edx
    test dword [rax + SYM_FLAGS], SYM_FLAG_CONSTANT or SYM_FLAG_LABEL
    jz .stored
    mov edx, 1
.stored:
    push rdx
    call token_advance
    pop rax
    ret
.bad:
    lea rdi, [error_expression]
    jmp asm_error

cond_compare:
    push rbx
    call cond_operand
    mov rbx, rax
    call cond_relop
    jnc .truth
    push rax
    call cond_operand
    pop rcx
    mov rdx, rax
    xor eax, eax
    cmp ecx, COND_EQ
    jne .try_ne
    cmp rbx, rdx
    sete al
    jmp .done
.try_ne:
    cmp ecx, COND_NE
    jne .try_lt
    cmp rbx, rdx
    setne al
    jmp .done
.try_lt:
    cmp ecx, COND_LT
    jne .try_le
    cmp rbx, rdx
    setl al
    jmp .done
.try_le:
    cmp ecx, COND_LE
    jne .try_gt
    cmp rbx, rdx
    setle al
    jmp .done
.try_gt:
    cmp ecx, COND_GT
    jne .ge
    cmp rbx, rdx
    setg al
    jmp .done
.ge:
    cmp rbx, rdx
    setge al
.done:
    pop rbx
    ret
.truth:
    xor eax, eax
    test rbx, rbx
    setnz al
    pop rbx
    ret

cond_relop:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_PUNCT
    jne .none
    mov rax, [rdi + TOKEN_VALUE]
    cmp al, '='
    je .equal
    cmp al, '!'
    je .bang
    cmp al, '<'
    je .less
    cmp al, '>'
    je .greater
.none:
    clc
    ret
.equal:
    call token_advance
    mov rdi, [token_ptr]
    mov sil, '='
    call token_is_punct
    jnc .equal_done
    call token_advance
.equal_done:
    mov eax, COND_EQ
    stc
    ret
.bang:
    call token_advance
    mov rdi, [token_ptr]
    mov sil, '='
    call token_is_punct
    jnc .bad
    call token_advance
    mov eax, COND_NE
    stc
    ret
.less:
    call token_advance
    mov rdi, [token_ptr]
    mov sil, '>'
    call token_is_punct
    jc .less_ne
    mov rdi, [token_ptr]
    mov sil, '='
    call token_is_punct
    jc .less_eq
    mov eax, COND_LT
    stc
    ret
.less_ne:
    call token_advance
    mov eax, COND_NE
    stc
    ret
.less_eq:
    call token_advance
    mov eax, COND_LE
    stc
    ret
.greater:
    call token_advance
    mov rdi, [token_ptr]
    mov sil, '='
    call token_is_punct
    jc .greater_eq
    mov eax, COND_GT
    stc
    ret
.greater_eq:
    call token_advance
    mov eax, COND_GE
    stc
    ret
.bad:
    lea rdi, [error_expression]
    jmp asm_error
