MACRO_NAME_MAX = 32
MACRO_PARAM_MAX = 8

MACRO_SUB_MAX = 16
MACRO_ARG_MAX = 128
MACRO_LINE_MAX = 512
MACRO_MAX = 64
MACRO_DEPTH_MAX = 8

MACRO_NAME = 0
MACRO_PARAM_COUNT = MACRO_NAME_MAX
MACRO_PARAMS = MACRO_NAME_MAX + 8
MACRO_BODY_START = MACRO_PARAMS + MACRO_PARAM_MAX * MACRO_NAME_MAX
MACRO_BODY_END = MACRO_BODY_START + 8
MACRO_ENTRY = MACRO_BODY_END + 8

FRAME_COUNT = 0
FRAME_NAMES = 8
FRAME_VALUES = FRAME_NAMES + MACRO_SUB_MAX * MACRO_NAME_MAX
FRAME_LINE = FRAME_VALUES + MACRO_SUB_MAX * MACRO_ARG_MAX
FRAME_SIZE = FRAME_LINE + MACRO_LINE_MAX

macro_reset:
    mov dword [macro_count], 0
    mov dword [macro_depth], 0
    mov dword [macro_unique], 0
    lea rax, [macro_frames]
    mov [macro_active_frame], rax
    mov qword [rax + FRAME_COUNT], 0
    ret

; rsi..rdx: if rsi sits on a comment or a string, skip it (braces in there don't count).
; CF if something was skipped
macro_skip_literal:
    cmp rsi, rdx
    jae .none
    mov al, [rsi]
    cmp al, ';'
    je .comment
    cmp al, 39
    je .string
    cmp al, '"'
    je .string
.none:
    clc
    ret
.comment:
    cmp rsi, rdx
    jae .done
    cmp byte [rsi], 10
    je .done
    inc rsi
    jmp .comment
.string:
    mov ah, al
    inc rsi
.next:
    cmp rsi, rdx
    jae .done
    mov al, [rsi]
    inc rsi
    cmp al, ah
    jne .next
    cmp rsi, rdx
    jae .done
    cmp [rsi], ah
    jne .done
    inc rsi
    jmp .next
.done:
    stc
    ret

macro_scan_body:
    push rbx
    push r12
    mov rsi, rdi
    call block_file_end
    mov r12, rax
    mov rdx, rax
.find_open:
    cmp rsi, rdx
    jae .missing
    call macro_skip_literal
    jc .find_open
    cmp rsi, rdx
    jae .missing
    mov al, [rsi]
    inc rsi
    cmp al, '{'
    jne .find_open
    mov [macro_body_start], rsi
    mov ebx, 1
.find_close:
    cmp rsi, rdx
    jae .missing
    call macro_skip_literal
    jc .find_close
    cmp rsi, rdx
    jae .missing
    mov al, [rsi]
    cmp al, '{'
    je .deeper
    cmp al, '}'
    je .shallower
    inc rsi
    jmp .find_close
.deeper:
    inc ebx
    inc rsi
    jmp .find_close
.shallower:
    dec ebx
    jz .closed
    inc rsi
    jmp .find_close
.closed:
    mov [macro_body_end], rsi
    inc rsi
.tail:
    cmp rsi, r12
    jae .ready
    cmp byte [rsi], 10
    je .ready
    inc rsi
    jmp .tail
.ready:
    mov [macro_body_tail], rsi
    pop r12
    pop rbx
    stc
    ret
.missing:
    pop r12
    pop rbx
    clc
    ret

directive_macro:
    push rbx
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .bad
    mov eax, [macro_count]
    cmp eax, MACRO_MAX
    jae .full
    imul rax, rax, MACRO_ENTRY
    lea rbx, [macro_table + rax]

    mov ecx, [rdi + TOKEN_LENGTH]
    cmp ecx, MACRO_NAME_MAX - 1
    jae .bad
    mov rsi, [rdi + TOKEN_TEXT]
    lea rdi, [rbx + MACRO_NAME]
    cld
    rep movsb
    mov byte [rdi], 0
    call token_advance
    mov qword [rbx + MACRO_PARAM_COUNT], 0

.param:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .body
    mov rax, [rbx + MACRO_PARAM_COUNT]
    cmp rax, MACRO_PARAM_MAX
    jae .bad
    mov ecx, [rdi + TOKEN_LENGTH]
    cmp ecx, MACRO_NAME_MAX - 1
    jae .bad
    imul rax, rax, MACRO_NAME_MAX
    mov rsi, [rdi + TOKEN_TEXT]
    lea rdi, [rbx + MACRO_PARAMS]
    add rdi, rax
    rep movsb
    mov byte [rdi], 0
    inc qword [rbx + MACRO_PARAM_COUNT]
    call token_advance
    mov rdi, [token_ptr]
    mov sil, ','
    call token_is_punct
    jnc .body
    call token_advance
    jmp .param

.body:
    mov rdi, [line_start]
    call macro_scan_body
    jnc .unterminated
    mov rax, [macro_body_start]
    mov [rbx + MACRO_BODY_START], rax
    mov rax, [macro_body_end]
    mov [rbx + MACRO_BODY_END], rax
    mov rax, [macro_body_tail]
    mov [line_end], rax
    inc dword [macro_count]
    pop rbx
    ret

.bad:
    pop rbx
    lea rdi, [error_macro_header]
    jmp asm_error
.full:
    pop rbx
    lea rdi, [error_macro_full]
    jmp asm_error
.unterminated:
    pop rbx
    lea rdi, [error_macro_open]
    jmp asm_error

macro_name_equal:
    test ecx, ecx
    jz .differ
.compare:
    mov al, [rdi]
    test al, al
    jz .end
    test ecx, ecx
    jz .differ
    mov ah, [rsi]
    or ax, 0x2020
    cmp al, ah
    jne .differ
    inc rdi
    inc rsi
    dec ecx
    jmp .compare
.end:
    test ecx, ecx
    jnz .differ
    stc
    ret
.differ:
    clc
    ret

macro_lookup:
    push rbx
    push r12
    push r13
    mov r12d, [rdi + TOKEN_LENGTH]
    mov r13, [rdi + TOKEN_TEXT]
    xor ebx, ebx
.entry:
    cmp ebx, [macro_count]
    jae .missing
    mov rax, rbx
    imul rax, rax, MACRO_ENTRY
    lea rax, [macro_table + rax]
    push rax
    mov rdi, rax
    mov rsi, r13
    mov ecx, r12d
    call macro_name_equal
    pop rax
    jc .found
    inc ebx
    jmp .entry
.found:
    pop r13
    pop r12
    pop rbx
    stc
    ret
.missing:
    pop r13
    pop r12
    pop rbx
    clc
    ret

macro_frame:
    mov eax, [macro_depth]
    imul rax, rax, FRAME_SIZE
    lea rax, [macro_frames + rax]
    ret

macro_sub_name:
    imul rax, rax, MACRO_NAME_MAX
    add rax, rbx
    add rax, FRAME_NAMES
    ret

macro_sub_value:
    imul rax, rax, MACRO_ARG_MAX
    add rax, rbx
    add rax, FRAME_VALUES
    ret

macro_find_sub:
    push rbx
    push r12
    push r13
    push r14
    mov r12, rsi
    mov r13d, ecx
    mov rbx, [macro_active_frame]
    xor r14d, r14d
.entry:
    cmp r14, [rbx + FRAME_COUNT]
    jae .missing
    mov rax, r14
    call macro_sub_name
    mov rdi, rax
    mov rsi, r12
    mov ecx, r13d
    call macro_name_equal
    jc .found
    inc r14
    jmp .entry
.found:
    mov rax, r14
    call macro_sub_value
    pop r14
    pop r13
    pop r12
    pop rbx
    stc
    ret
.missing:
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret

macro_store_name:
    push rbx
    push r12
    mov rbx, [macro_active_frame]
    mov rax, [rbx + FRAME_COUNT]
    cmp rax, MACRO_SUB_MAX
    jae .full
    call macro_sub_name
    mov r12, rax
    mov rax, [rbx + FRAME_COUNT]
    call macro_sub_value
    mov byte [rax], 0
    inc qword [rbx + FRAME_COUNT]
    mov rax, r12
    pop r12
    pop rbx
    clc
    ret
.full:
    pop r12
    pop rbx
    stc
    ret

macro_value_slot:
    push rbx
    mov rbx, [macro_active_frame]
    mov rax, [rbx + FRAME_COUNT]
    dec rax
    call macro_sub_value
    pop rbx
    ret

macro_arg_span:
    push rbx
    push r12
    push r13
    mov r13, rdx
    xor ebx, ebx
.lead:
    cmp rsi, r13
    jae .empty
    mov al, [rsi]
    cmp al, ' '
    je .lead_next
    cmp al, 9
    je .lead_next
    jmp .begin
.lead_next:
    inc rsi
    jmp .lead
.empty:
    mov rax, rsi
    mov rdx, rsi
    pop r13
    pop r12
    pop rbx
    ret
.begin:
    mov r12, rsi
.scan:
    cmp rsi, r13
    jae .finish
    mov rdx, r13
    call macro_skip_literal
    jc .scan
    cmp rsi, r13
    jae .finish
    mov al, [rsi]
    cmp al, '('
    je .deeper
    cmp al, '['
    je .deeper
    cmp al, ')'
    je .shallower
    cmp al, ']'
    je .shallower
    cmp al, ','
    jne .step
    test ebx, ebx
    jz .finish
    jmp .step
.deeper:
    inc ebx
    jmp .step
.shallower:
    dec ebx
.step:
    inc rsi
    jmp .scan
.finish:
    mov rax, r12
    mov rdx, rsi
.trim:
    cmp rdx, rax
    jbe .comma
    mov cl, [rdx - 1]
    cmp cl, ' '
    je .trim_step
    cmp cl, 9
    jne .comma
.trim_step:
    dec rdx
    jmp .trim
.comma:
    cmp rsi, r13
    jae .out
    cmp byte [rsi], ','
    jne .out
    inc rsi
.out:
    pop r13
    pop r12
    pop rbx
    ret

macro_copy_span:
    push rbx
    mov rbx, rdi
    xor ecx, ecx
.next:
    cmp rsi, rdx
    jae .done
    cmp ecx, MACRO_ARG_MAX - 1
    jae .done
    mov al, [rsi]
    mov [rbx + rcx], al
    inc rsi
    inc ecx
    jmp .next
.done:
    mov byte [rbx + rcx], 0
    pop rbx
    ret

macro_parse_args:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rsi
    mov r13, rdx
    mov r14, [macro_entry]
    xor r15d, r15d
.argument:
    cmp r15, [r14 + MACRO_PARAM_COUNT]
    jae .fill
    mov rsi, r12
    mov rdx, r13
    call macro_arg_span
    mov [macro_arg_start], rax
    mov [macro_arg_end], rdx
    mov r12, rsi

    mov rbx, [macro_active_frame]
    mov rax, r15
    call macro_sub_name
    mov rdi, rax
    mov rax, r15
    imul rax, rax, MACRO_NAME_MAX
    lea rsi, [r14 + MACRO_PARAMS]
    add rsi, rax
    call str_copy

    mov rbx, [macro_active_frame]
    mov rax, r15
    call macro_sub_value
    mov rdi, rax
    mov rsi, [macro_arg_start]
    mov rdx, [macro_arg_end]
    call macro_copy_span
    inc r15
    jmp .argument
.fill:
    mov rbx, [macro_active_frame]
    mov [rbx + FRAME_COUNT], r15
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

macro_substitute_line:
    push rbx
    push r12
    push r13
    push r14
    mov [macro_line_src], rsi
    mov [macro_line_stop], rdx
    mov r12, rdi
    mov r13, rdi
    add r13, MACRO_LINE_MAX - 1
.next:
    mov rsi, [macro_line_src]
    cmp rsi, [macro_line_stop]
    jae .done
    mov r14, rsi
    mov rdx, [macro_line_stop]
    call macro_skip_literal
    mov [macro_line_src], rsi
    jc .copy_span
    mov al, [r14]
    call is_ident_start
    jnc .single
    mov rsi, r14
.ident:
    inc rsi
    cmp rsi, [macro_line_stop]
    jae .ident_done
    mov al, [rsi]
    call is_ident_char
    jc .ident
.ident_done:
    mov [macro_line_src], rsi
    mov rcx, rsi
    sub rcx, r14
    mov rsi, r14
    call macro_find_sub
    jnc .copy_span
    mov rsi, rax
.value:
    mov al, [rsi]
    test al, al
    jz .next
    cmp r12, r13
    jae .next
    mov [r12], al
    inc r12
    inc rsi
    jmp .value
.single:
    lea rsi, [r14 + 1]
    mov [macro_line_src], rsi
.copy_span:
    mov rsi, [macro_line_src]
.copy:
    cmp r14, rsi
    jae .next
    cmp r12, r13
    jae .next
    mov al, [r14]
    mov [r12], al
    inc r14
    inc r12
    jmp .copy
.done:
    mov rax, r12
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

macro_local:
    push rbx
    push r12
    push r13
.name:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .done
    mov r12, [rdi + TOKEN_TEXT]
    mov r13d, [rdi + TOKEN_LENGTH]
    cmp r13d, MACRO_NAME_MAX - 1
    jae .bad
    call macro_store_name
    jc .full
    mov rdi, rax
    mov rsi, r12
    mov ecx, r13d
    cld
    rep movsb
    mov byte [rdi], 0

    call macro_value_slot
    mov rdi, rax
    mov rsi, r12
    mov ecx, r13d
    rep movsb
    mov byte [rdi], '?'
    inc rdi
    push rdi
    mov edi, [macro_unique]
    inc dword [macro_unique]
    call u64_to_dec
    mov rsi, rax
    pop rdi
    call str_copy

    call token_advance
    mov rdi, [token_ptr]
    mov sil, ','
    call token_is_punct
    jnc .done
    call token_advance
    jmp .name
.done:
    pop r13
    pop r12
    pop rbx
    ret
.bad:
    pop r13
    pop r12
    pop rbx
    lea rdi, [error_name_too_long]
    jmp asm_error
.full:
    pop r13
    pop r12
    pop rbx
    lea rdi, [error_macro_locals]
    jmp asm_error

; rax -> macro. saves the line state, binds the arguments, runs the body, restores everything.
; asm_error inside the body comes back to .line_done here, not into the caller's line
macro_expand:
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 80

    mov [rsp + 56], rax
    mov eax, [macro_depth]
    cmp eax, MACRO_DEPTH_MAX
    jae .deep

    mov rax, [macro_active_frame]
    mov [rsp + 0], rax
    mov rax, [line_start]
    mov [rsp + 8], rax
    mov rax, [line_end]
    mov [rsp + 16], rax
    mov rax, [line_rsp]
    mov [rsp + 24], rax
    mov rax, [line_resume]
    mov [rsp + 32], rax

    call macro_frame
    mov [rsp + 48], rax
    mov rbx, rax
    mov qword [rbx + FRAME_COUNT], 0
    mov [macro_active_frame], rbx
    mov rax, [rsp + 56]
    mov [macro_entry], rax

    mov rdi, [token_ptr]
    mov rsi, [rdi + TOKEN_TEXT]
    mov ecx, [rdi + TOKEN_LENGTH]
    add rsi, rcx
    mov rdx, [rsp + 16]
    cmp rsi, rdx
    jbe .args
    mov rsi, rdx
.args:
    call macro_parse_args

    inc dword [macro_depth]
    mov rax, [rsp + 56]
    mov rcx, [rax + MACRO_BODY_START]
    mov [rsp + 40], rcx
    mov rcx, [rax + MACRO_BODY_END]
    mov [rsp + 56], rcx

.body_line:
    mov rsi, [rsp + 40]
    cmp rsi, [rsp + 56]
    jae .finished
    mov rdi, rsi
.scan:
    cmp rdi, [rsp + 56]
    jae .ready
    cmp byte [rdi], 10
    je .ready
    inc rdi
    jmp .scan
.ready:
    mov [rsp + 64], rdi
    mov rbx, [rsp + 48]
    mov [macro_active_frame], rbx
    mov rsi, [rsp + 40]
    mov rdx, rdi
    lea rdi, [rbx + FRAME_LINE]
    call macro_substitute_line
    mov rbx, [rsp + 48]
    lea rdi, [rbx + FRAME_LINE]
    mov [line_start], rdi
    mov [line_end], rax
    mov [line_rsp], rsp
    lea rax, [.line_done]
    mov [line_resume], rax
    call macro_body_line
.line_done:
    mov rax, [rsp + 64]
    inc rax
    mov [rsp + 40], rax
    jmp .body_line

.finished:
    dec dword [macro_depth]
    mov rax, [rsp + 0]
    mov [macro_active_frame], rax
    mov rax, [rsp + 8]
    mov [line_start], rax
    mov rax, [rsp + 16]
    mov [line_end], rax
    mov rax, [rsp + 24]
    mov [line_rsp], rax
    mov rax, [rsp + 32]
    mov [line_resume], rax
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.deep:
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    lea rdi, [error_macro_deep]
    jmp asm_error

macro_body_line:
    call tokenize_line
    lea rax, [tokens]
    mov [token_ptr], rax
    cmp dword [tokens + TOKEN_TYPE], TOKEN_IDENT
    jne .plain
    mov rdi, [token_ptr]
    lea rsi, [kw_local]
    call token_equals
    jnc .plain
    call cond_emitting
    jnc .done
    call token_advance
    jmp macro_local
.done:
    ret
.plain:
    jmp process_line
