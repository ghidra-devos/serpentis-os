; fasm highlighting, one pass left to right per line. no state crosses lines, ; comments
; and strings end at the newline anyway

SYNTAX_CODE = 0
SYNTAX_COMMENT = 1
SYNTAX_STRING = 2

is_word_char:
    cmp al, '_'
    je .yes
    cmp al, '.'
    je .yes
    cmp al, '@'
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

word_end:
    mov eax, edi
.scan:
    cmp eax, [text_size]
    jae .done
    push rax
    mov al, [text + rax]
    call is_word_char
    pop rax
    jnc .done
    inc eax
    jmp .scan
.done:
    ret

word_in_table:
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
    or al, 0x20
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

; edi = start of a word. sets token_end, eax = attr. the first word of a line is an
; instruction unless a directive follows it, then it is a data label (name db ...)
classify_word:
    push rbx
    push r12
    mov r12d, edi
    call word_end
    mov [token_end], eax
    mov ebx, eax
    sub ebx, r12d

    movzx eax, byte [text + r12]
    cmp al, '0'
    jb .not_number
    cmp al, '9'
    ja .not_number
    mov eax, ATTR_NUMBER
    jmp .done
.not_number:

    mov edi, [token_end]
    cmp edi, [text_size]
    jae .not_label
    cmp byte [text + rdi], ':'
    jne .not_label
    mov eax, ATTR_LABEL
    jmp .done
.not_label:

    mov edi, r12d
    mov esi, ebx
    lea rdx, [directive_words]
    call word_in_table
    jnc .not_directive
    mov eax, ATTR_DIRECTIVE
    jmp .done
.not_directive:

    mov edi, r12d
    mov esi, ebx
    lea rdx, [register_words]
    call word_in_table
    jnc .not_register
    mov eax, ATTR_REGISTER
    jmp .done
.not_register:

    cmp dword [line_first_word], 0
    je .identifier
    mov edi, [token_end]
    call leading_blank_end
    mov edi, eax
    call word_end
    mov esi, eax
    sub esi, edi
    jz .instruction
    lea rdx, [directive_words]
    call word_in_table
    jnc .instruction
    mov eax, ATTR_LABEL
    jmp .done
.instruction:
    mov eax, ATTR_INSTRUCTION
    jmp .done
.identifier:
    mov eax, ATTR_IDENTIFIER
.done:
    mov dword [line_first_word], 0
    pop r12
    pop rbx
    ret

classify_char:
    cmp byte [syntax_enabled], 0
    je .plain
    cmp byte [syntax_enabled], 2
    je c_classify_char
    cmp dword [syntax_state], SYNTAX_COMMENT
    je .comment
    cmp dword [syntax_state], SYNTAX_STRING
    je .string

    cmp r14d, [token_end]
    jb .inside_word

    movzx eax, byte [text + r14]
    cmp al, ';'
    je .start_comment
    cmp al, 39
    je .start_string
    cmp al, '"'
    je .start_string
    call is_word_char
    jnc .punctuation
    mov edi, r14d
    call classify_word
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

.start_string:
    mov dword [syntax_state], SYNTAX_STRING
    mov [string_quote], al
    mov eax, ATTR_STRING
    ret
.string:
    mov al, [text + r14]
    cmp al, [string_quote]
    jne .string_color
    mov dword [syntax_state], SYNTAX_CODE
.string_color:
    mov eax, ATTR_STRING
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

compute_selection:
    mov dword [selection_from], -1
    mov dword [selection_to], -1
    cmp byte [mode], MODE_VISUAL
    je .visual
    cmp byte [mode], MODE_VISUAL_LINE
    jne .done
.visual:
    mov eax, [anchor]
    mov ecx, [cursor]
    cmp eax, ecx
    jbe .ordered
    xchg eax, ecx
.ordered:
    cmp byte [mode], MODE_VISUAL_LINE
    je .lines
    inc ecx
    mov [selection_from], eax
    mov [selection_to], ecx
    ret
.lines:
    push rcx
    mov edi, eax
    call line_start
    mov [selection_from], eax
    pop rdi
    call line_end
    inc eax
    mov [selection_to], eax
.done:
    ret

selection_contains:
    cmp r14d, [selection_from]
    jb .no
    cmp r14d, [selection_to]
    jae .no
    stc
    ret
.no:
    clc
    ret

match_contains:
    cmp byte [search_highlight], 0
    je .no
    mov ecx, [search_length]
    test ecx, ecx
    jz .no
    mov edx, r14d
    sub edx, ecx
    inc edx
    jns .start
    xor edx, edx
.start:
    cmp edx, r14d
    ja .no
    mov edi, edx
    call pattern_at
    jc .yes
    inc edx
    jmp .start
.yes:
    stc
    ret
.no:
    clc
    ret

pattern_at:
    push rcx
    mov ecx, [search_length]
    mov eax, edi
    add eax, ecx
    cmp eax, [text_size]
    ja .no
    xor eax, eax
.compare:
    cmp eax, ecx
    jae .yes
    mov r11b, [text + rdi + rax]
    cmp r11b, [search_pattern + rax]
    jne .no
    inc eax
    jmp .compare
.yes:
    pop rcx
    stc
    ret
.no:
    pop rcx
    clc
    ret

; bracket under the cursor -> bracket_match = its partner, counting nesting. strings and
; comments are not skipped
find_bracket_match:
    mov dword [bracket_match], -1
    mov edi, [cursor]
    cmp edi, [text_size]
    jae .done
    mov al, [text + rdi]
    lea rsi, [bracket_pairs]
.pair:
    mov cl, [rsi]
    test cl, cl
    jz .done
    cmp al, cl
    je .forward
    cmp al, [rsi + 1]
    je .backward
    add rsi, 2
    jmp .pair

.forward:
    mov dl, [rsi + 1]
    xor r8d, r8d
.forward_scan:
    inc edi
    cmp edi, [text_size]
    jae .done
    mov r9b, [text + rdi]
    cmp r9b, cl
    jne .forward_close
    inc r8d
    jmp .forward_scan
.forward_close:
    cmp r9b, dl
    jne .forward_scan
    test r8d, r8d
    jz .found
    dec r8d
    jmp .forward_scan

.backward:
    mov dl, cl
    mov cl, [rsi + 1]
    xor r8d, r8d
.backward_scan:
    test edi, edi
    jz .done
    dec edi
    mov r9b, [text + rdi]
    cmp r9b, cl
    jne .backward_open
    inc r8d
    jmp .backward_scan
.backward_open:
    cmp r9b, dl
    jne .backward_scan
    test r8d, r8d
    jz .found
    dec r8d
    jmp .backward_scan

.found:
    mov [bracket_match], edi
.done:
    ret

bracket_pairs db '()[]{}', 0

directive_words:
    db 2, 'db', 2, 'dw', 2, 'dd', 2, 'dq', 2, 'rb', 2, 'rw', 2, 'rd', 2, 'rq'
    db 5, 'times', 7, 'include', 6, 'format', 5, 'use64', 3, 'org', 5, 'align'
    db 5, 'macro', 2, 'if', 3, 'end', 4, 'else', 5, 'local', 8, 'postpone'
    db 7, 'defined', 4, 'byte', 4, 'word', 5, 'dword', 5, 'qword', 3, 'shl', 3, 'shr'
    db 0

register_words:
    db 3, 'rax', 3, 'rbx', 3, 'rcx', 3, 'rdx', 3, 'rsi', 3, 'rdi', 3, 'rbp', 3, 'rsp'
    db 2, 'r8', 2, 'r9', 3, 'r10', 3, 'r11', 3, 'r12', 3, 'r13', 3, 'r14', 3, 'r15'
    db 3, 'eax', 3, 'ebx', 3, 'ecx', 3, 'edx', 3, 'esi', 3, 'edi', 3, 'ebp', 3, 'esp'
    db 3, 'r8d', 3, 'r9d', 4, 'r10d', 4, 'r11d', 4, 'r12d', 4, 'r13d', 4, 'r14d', 4, 'r15d'
    db 2, 'ax', 2, 'bx', 2, 'cx', 2, 'dx', 2, 'si', 2, 'di', 2, 'bp', 2, 'sp'
    db 2, 'al', 2, 'bl', 2, 'cl', 2, 'dl', 3, 'sil', 3, 'dil', 3, 'bpl', 3, 'spl'
    db 2, 'ah', 2, 'bh', 2, 'ch', 2, 'dh', 3, 'r8b', 3, 'r9b', 4, 'r10b', 4, 'r11b'
    db 4, 'r12b', 4, 'r13b', 4, 'r14b', 4, 'r15b', 3, 'rip', 3, 'cr0', 3, 'cr3'
    db 0
