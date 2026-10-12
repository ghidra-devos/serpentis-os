TOKEN_END = 0
TOKEN_IDENT = 1
TOKEN_NUMBER = 2
TOKEN_STRING = 3
TOKEN_PUNCT = 4

TOKEN_TYPE = 0
TOKEN_LENGTH = 4
TOKEN_TEXT = 8
TOKEN_VALUE = 16
TOKEN_SIZE = 24

TOKEN_MAX = 96

tokenize_line:
    push rbx
    push r12
    push r13

    mov rsi, [line_start]
    mov r12, [line_end]
    lea rbx, [tokens]
    xor r13d, r13d
    mov qword [string_pool_used], 0

.skip:
    cmp rsi, r12
    jae .finish
    mov al, [rsi]
    cmp al, ' '
    je .advance
    cmp al, 9
    je .advance
    cmp al, 13
    je .advance
    cmp al, ';'
    je .finish
    jmp .token

.advance:
    inc rsi
    jmp .skip

.token:
    cmp r13d, TOKEN_MAX - 1
    jae .too_many

    mov [rbx + TOKEN_TEXT], rsi
    mov qword [rbx + TOKEN_VALUE], 0

    cmp al, 39
    je .string
    cmp al, '"'
    je .string

    call is_ident_start
    jc .identifier

    cmp al, '0'
    jb .punct
    cmp al, '9'
    jbe .number

.punct:
    mov dword [rbx + TOKEN_TYPE], TOKEN_PUNCT
    mov dword [rbx + TOKEN_LENGTH], 1
    movzx eax, al
    mov [rbx + TOKEN_VALUE], rax
    inc rsi
    jmp .stored

.identifier:
    mov rdx, rsi
.identifier_next:
    inc rsi
    cmp rsi, r12
    jae .identifier_done
    mov al, [rsi]
    call is_ident_char
    jc .identifier_next
.identifier_done:
    mov dword [rbx + TOKEN_TYPE], TOKEN_IDENT
    mov rax, rsi
    sub rax, rdx
    mov [rbx + TOKEN_LENGTH], eax
    jmp .stored

.number:
    mov rdx, rsi
.number_next:
    inc rsi
    cmp rsi, r12
    jae .number_done
    mov al, [rsi]
    call is_ident_char
    jc .number_next
.number_done:
    mov dword [rbx + TOKEN_TYPE], TOKEN_NUMBER
    mov rax, rsi
    sub rax, rdx
    mov [rbx + TOKEN_LENGTH], eax
    push rsi
    mov rsi, rdx
    mov ecx, eax
    call parse_number
    pop rsi
    jc .bad_number
    mov [rbx + TOKEN_VALUE], rax
    jmp .stored

.string:
    mov dl, al
    inc rsi
    mov [rbx + TOKEN_TEXT], rsi
    lea rdi, [string_pool]
    add rdi, [string_pool_used]
    mov [rbx + TOKEN_TEXT], rdi
    xor ecx, ecx
.string_next:
    cmp rsi, r12
    jae .unterminated
    mov al, [rsi]
    inc rsi
    cmp al, dl
    jne .string_char
    cmp rsi, r12
    jae .string_done
    cmp [rsi], dl
    jne .string_done
    inc rsi
.string_char:
    cmp qword [string_pool_used], STRING_POOL_SIZE - 1
    jae .unterminated
    mov [rdi], al
    inc rdi
    inc qword [string_pool_used]
    inc ecx
    jmp .string_next
.string_done:
    mov dword [rbx + TOKEN_TYPE], TOKEN_STRING
    mov [rbx + TOKEN_LENGTH], ecx

.stored:
    add rbx, TOKEN_SIZE
    inc r13d
    jmp .skip

.finish:
    mov dword [rbx + TOKEN_TYPE], TOKEN_END
    mov [token_count], r13d
    pop r13
    pop r12
    pop rbx
    ret

.too_many:
    lea rdi, [error_line_too_long]
    jmp asm_error

.bad_number:
    lea rdi, [error_bad_number]
    jmp asm_error

.unterminated:
    lea rdi, [error_unterminated]
    jmp asm_error

is_ident_start:
    cmp al, '_'
    je .yes
    cmp al, '.'
    je .yes
    cmp al, '@'
    je .yes
    cmp al, '?'
    je .yes
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

is_ident_char:
    cmp al, '0'
    jb is_ident_start
    cmp al, '9'
    ja is_ident_start
    stc
    ret

parse_number:
    push rbx
    push r12
    mov r12d, ecx
    xor eax, eax

    cmp r12d, 2
    jbe .suffix
    cmp byte [rsi], '0'
    jne .suffix
    mov bl, [rsi + 1]
    or bl, 0x20
    cmp bl, 'x'
    jne .suffix
    add rsi, 2
    sub r12d, 2
    mov ecx, 16
    jmp .digits

.suffix:
    mov bl, [rsi + r12 - 1]
    or bl, 0x20
    mov ecx, 10
    cmp bl, 'h'
    jne .binary
    mov ecx, 16
    dec r12d
    jmp .digits
.binary:
    cmp bl, 'b'
    jne .digits
    mov ecx, 2
    dec r12d

.digits:
    test r12d, r12d
    jz .bad
.digit:
    movzx edx, byte [rsi]
    cmp dl, '_'
    je .skip_digit
    sub dl, '0'
    cmp dl, 9
    jbe .value
    mov dl, [rsi]
    or dl, 0x20
    sub dl, 'a' - 10
    cmp dl, 10
    jb .bad
.value:
    cmp edx, ecx
    jae .bad
    imul rax, rcx
    add rax, rdx
.skip_digit:
    inc rsi
    dec r12d
    jnz .digit

    pop r12
    pop rbx
    clc
    ret

.bad:
    pop r12
    pop rbx
    stc
    ret

token_is_punct:
    cmp dword [rdi + TOKEN_TYPE], TOKEN_PUNCT
    jne .no
    cmp [rdi + TOKEN_VALUE], sil
    jne .no
    stc
    ret
.no:
    clc
    ret

; rdi -> token, rsi -> lowercase keyword. CF on a case insensitive match
token_equals:
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .no
    push rcx
    push rdx
    push rdi
    push rsi
    mov ecx, [rdi + TOKEN_LENGTH]
    mov rdx, [rdi + TOKEN_TEXT]
.compare:
    mov al, [rsi]
    test ecx, ecx
    jz .end_of_token
    test al, al
    jz .differ
    mov ah, [rdx]
    or ax, 0x2020
    cmp al, ah
    jne .differ
    inc rsi
    inc rdx
    dec ecx
    jmp .compare
.end_of_token:
    test al, al
    jnz .differ
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    stc
    ret
.differ:
    pop rsi
    pop rdi
    pop rdx
    pop rcx
.no:
    clc
    ret
