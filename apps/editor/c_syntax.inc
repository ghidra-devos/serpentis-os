; C highlighting for .c and .h. same idea as syntax.inc plus two more states, and /* */ is
; the only thing that carries over to the next line

SYNTAX_BLOCK = 3
SYNTAX_CHAR = 4                 ; 'x', colored like a number

is_c_word_char:
    cmp al, '_'
    je .yes
    cmp al, '0'
    jb .no
    cmp al, '9'
    jbe .yes
    mov ah, al
    or ah, 0x20
    cmp ah, 'a'
    jb .no
    cmp ah, 'z'
    ja .no
.yes:
    stc
    ret
.no:
    clc
    ret

c_word_end:
    mov eax, edi
.scan:
    cmp eax, [text_size]
    jae .done
    push rax
    mov al, [text + rax]
    call is_c_word_char
    pop rax
    jnc .done
    inc eax
    jmp .scan
.done:
    ret

; case sensitive, C keywords are lower case
c_word_in_table:
    push rbx
    mov ebx, esi
.entry:
    movzx ecx, byte [rdx]
    test ecx, ecx
    jz .missing
    cmp ecx, ebx
    jne .skip
    xor r8d, r8d
.compare:
    mov al, [text + rdi + r8]
    cmp al, [rdx + 1 + r8]
    jne .skip
    inc r8d
    cmp r8d, ebx
    jb .compare
    pop rbx
    stc
    ret
.skip:
    lea rdx, [rdx + rcx + 1]
    jmp .entry
.missing:
    pop rbx
    clc
    ret

c_next_char:
    mov eax, edi
.scan:
    cmp eax, [text_size]
    jae .none
    mov cl, [text + rax]
    cmp cl, ' '
    je .next
    cmp cl, 9
    jne .found
.next:
    inc eax
    jmp .scan
.found:
    mov al, cl
    ret
.none:
    xor eax, eax
    ret

c_is_constant:
    xor ecx, ecx
    xor r8d, r8d
.scan:
    cmp ecx, esi
    jae .decide
    mov al, [text + rdi + rcx]
    cmp al, 'a'
    jb .not_lower
    cmp al, 'z'
    jbe .no
.not_lower:
    cmp al, 'A'
    jb .next
    cmp al, 'Z'
    ja .next
    inc r8d
.next:
    inc ecx
    jmp .scan
.decide:
    cmp esi, 2
    jb .no
    test r8d, r8d
    jz .no
    stc
    ret
.no:
    clc
    ret

c_classify_word:
    push rbx
    push r12
    mov r12d, edi
    movzx eax, byte [text + r12]
    cmp al, '0'
    jb .word
    cmp al, '9'
    ja .word
    call c_word_end
    mov [token_end], eax
    mov eax, ATTR_NUMBER
    jmp .done
.word:
    call c_word_end
    mov [token_end], eax
    mov ebx, eax
    sub ebx, r12d

    mov edi, r12d
    mov esi, ebx
    lea rdx, [c_control_words]
    call c_word_in_table
    jnc .not_control
    mov eax, ATTR_INSTRUCTION
    jmp .done
.not_control:
    mov edi, r12d
    mov esi, ebx
    lea rdx, [c_type_words]
    call c_word_in_table
    jnc .not_type
    mov eax, ATTR_REGISTER
    jmp .done
.not_type:
    mov edi, [token_end]
    call c_next_char
    cmp al, '('
    jne .not_call
    mov eax, ATTR_LABEL
    jmp .done
.not_call:
    mov edi, r12d
    mov esi, ebx
    call c_is_constant
    jnc .identifier
    mov eax, ATTR_NUMBER
    jmp .done
.identifier:
    mov eax, ATTR_IDENTIFIER
.done:
    mov dword [line_first_word], 0
    pop r12
    pop rbx
    ret

c_directive:
    lea edi, [r14 + 1]
.skip_blanks:
    cmp edi, [text_size]
    jae .end
    mov al, [text + rdi]
    cmp al, ' '
    je .blank
    cmp al, 9
    jne .word
.blank:
    inc edi
    jmp .skip_blanks
.word:
    mov r9d, edi
    call c_word_end
    mov [token_end], eax
    mov esi, eax
    sub esi, r9d
    mov edi, r9d
    lea rdx, [c_include_word]
    call c_word_in_table
    jnc .directive
    mov byte [c_include_line], 1
    jmp .directive
.end:
    mov [token_end], edi
.directive:
    mov dword [token_class], ATTR_DIRECTIVE
    mov dword [line_first_word], 0
    mov eax, ATTR_DIRECTIVE
    ret

c_classify_char:
    mov eax, [syntax_state]
    cmp eax, SYNTAX_COMMENT
    je .comment
    cmp eax, SYNTAX_BLOCK
    je .block
    cmp eax, SYNTAX_STRING
    je .string
    cmp eax, SYNTAX_CHAR
    je .string

    cmp r14d, [token_end]
    jb .inside_word

    movzx eax, byte [text + r14]
    cmp al, '/'
    jne .not_slash
    lea ecx, [r14 + 1]
    cmp ecx, [text_size]
    jae .punctuation
    cmp byte [text + rcx], '/'
    je .start_comment
    cmp byte [text + rcx], '*'
    je .start_block
    jmp .punctuation
.not_slash:
    cmp al, '"'
    je .start_string
    cmp al, 39
    je .start_char
    cmp al, '<'
    jne .not_header
    cmp byte [c_include_line], 0
    je .punctuation
    mov byte [string_quote], '>'
    mov dword [syntax_state], SYNTAX_STRING
    mov eax, ATTR_STRING
    ret
.not_header:
    cmp al, '#'
    jne .not_directive
    cmp dword [line_first_word], 0
    je .punctuation
    jmp c_directive
.not_directive:
    call is_c_word_char
    jnc .punctuation
    mov edi, r14d
    call c_classify_word
    mov [token_class], eax
    ret

.inside_word:
    mov eax, [token_class]
    ret

.start_comment:
    mov dword [syntax_state], SYNTAX_COMMENT
.comment:
    mov eax, ATTR_COMMENT
    ret

.start_block:
    mov dword [syntax_state], SYNTAX_BLOCK
    lea eax, [r14 + 2]
    mov [c_block_start], eax
    mov eax, ATTR_COMMENT
    ret
.block:
    cmp byte [text + r14], '/'
    jne .comment
    cmp r14d, [c_block_start]   ; the * of the /* itself can't close it
    jbe .comment
    cmp byte [text + r14 - 1], '*'
    jne .comment
    mov dword [syntax_state], SYNTAX_CODE
    mov eax, ATTR_COMMENT
    ret

.start_char:
    mov dword [syntax_state], SYNTAX_CHAR
    mov [string_quote], al
    mov byte [c_escaped], 0
    mov eax, ATTR_NUMBER
    ret
.start_string:
    mov dword [syntax_state], SYNTAX_STRING
    mov [string_quote], al
    mov byte [c_escaped], 0
    mov eax, ATTR_STRING
    ret
.string:
    cmp byte [c_escaped], 0
    jne .escaped
    mov al, [text + r14]
    cmp al, 92
    je .escape
    cmp al, [string_quote]
    jne .string_color
    mov eax, [syntax_state]
    mov dword [syntax_state], SYNTAX_CODE
    jmp .string_kind
.escape:
    mov byte [c_escaped], 1
    jmp .string_color
.escaped:
    mov byte [c_escaped], 0
.string_color:
    mov eax, [syntax_state]
.string_kind:
    cmp eax, SYNTAX_CHAR
    je .char_color
    mov eax, ATTR_STRING
    ret
.char_color:
    mov eax, ATTR_NUMBER
    ret

.punctuation:
    cmp al, ' '
    je .plain
    cmp al, 9
    je .plain
    mov dword [line_first_word], 0
.plain:
    mov eax, ATTR_TEXT
    ret

c_line_state:
    xor eax, eax
    cmp byte [c_block_open], 0
    je .ready
    mov eax, SYNTAX_BLOCK
    mov dword [c_block_start], 0
.ready:
    mov [syntax_state], eax
    mov byte [c_include_line], 0
    ret

c_line_finished:
    xor eax, eax
    cmp dword [syntax_state], SYNTAX_BLOCK
    jne .store
    inc eax
.store:
    mov [c_block_open], al
    ret

; edi = first visible offset. scans everything above it once per frame to know if it
; starts inside a /* */. strings and // matter here so a "/*" in a string doesn't count
c_block_before:
    mov byte [c_block_open], 0
    xor eax, eax
    xor ecx, ecx
.next:
    cmp ecx, edi
    jae .done
    mov dl, [text + rcx]
    cmp eax, SYNTAX_COMMENT
    je .line_comment
    cmp eax, SYNTAX_BLOCK
    je .block
    cmp eax, SYNTAX_STRING
    je .string
    cmp dl, '/'
    jne .quote
    lea r8d, [rcx + 1]
    cmp r8d, [text_size]
    jae .advance
    cmp byte [text + r8], '/'
    je .open_line
    cmp byte [text + r8], '*'
    jne .advance
    mov eax, SYNTAX_BLOCK
    add ecx, 2
    jmp .next
.open_line:
    mov eax, SYNTAX_COMMENT
    jmp .advance
.quote:
    cmp dl, '"'
    je .open_string
    cmp dl, 39
    jne .advance
.open_string:
    mov eax, SYNTAX_STRING
    mov r9b, dl
    jmp .advance
.line_comment:
    cmp dl, 10
    jne .advance
    xor eax, eax
    jmp .advance
.block:
    cmp dl, '*'
    jne .advance
    lea r8d, [rcx + 1]
    cmp r8d, [text_size]
    jae .advance
    cmp byte [text + r8], '/'
    jne .advance
    xor eax, eax
    add ecx, 2
    jmp .next
.string:
    cmp dl, 92
    jne .string_end
    add ecx, 2
    jmp .next
.string_end:
    cmp dl, 10
    je .close_string
    cmp dl, r9b
    jne .advance
.close_string:
    xor eax, eax
.advance:
    inc ecx
    jmp .next
.done:
    cmp eax, SYNTAX_BLOCK
    jne .out
    mov byte [c_block_open], 1
.out:
    ret

c_include_word db 7, 'include', 0

c_control_words:
    db 2, 'if', 4, 'else', 3, 'for', 5, 'while', 2, 'do', 6, 'switch', 4, 'case', 7, 'default'
    db 5, 'break', 8, 'continue', 6, 'return', 4, 'goto', 6, 'sizeof', 8, '_Alignof'
    db 0

c_type_words:
    db 3, 'int', 4, 'char', 4, 'void', 4, 'long', 5, 'short', 8, 'unsigned', 6, 'signed'
    db 6, 'struct', 5, 'union', 4, 'enum', 7, 'typedef', 6, 'static', 6, 'extern', 5, 'const'
    db 8, 'volatile', 6, 'inline', 8, 'register', 4, 'auto', 5, '_Bool', 4, 'bool', 6, 'size_t'
    db 4, 'FILE', 7, 'uint8_t', 8, 'uint16_t', 8, 'uint32_t', 8, 'uint64_t', 6, 'int8_t'
    db 7, 'int16_t', 7, 'int32_t', 7, 'int64_t', 7, 'va_list', 8, 'restrict', 5, 'float', 6, 'double'
    db 0
