; the text and the undo history. positions are byte offsets into text, lines end in 10

TEXT_MAX = 0x100000
UNDO_MAX = 0x80000
EDIT_INSERT = 1
EDIT_DELETE = 2
RECORD_HEADER = 13

line_start:
    mov eax, edi
.scan:
    test eax, eax
    jz .done
    cmp byte [text + rax - 1], 10
    je .done
    dec eax
    jmp .scan
.done:
    ret

line_end:
    mov eax, edi
.scan:
    cmp eax, [text_size]
    jae .done
    cmp byte [text + rax], 10
    je .done
    inc eax
    jmp .scan
.done:
    ret

next_line_start:
    call line_end
    cmp eax, [text_size]
    jae .last
    inc eax
    clc
    ret
.last:
    stc
    ret

previous_line_start:
    test edi, edi
    jz .first
    dec edi
    call line_start
    clc
    ret
.first:
    stc
    ret

line_of:
    xor eax, eax
    xor ecx, ecx
.scan:
    cmp ecx, edi
    jae .done
    cmp byte [text + rcx], 10
    jne .next
    inc eax
.next:
    inc ecx
    jmp .scan
.done:
    ret

; edi = line number -> eax = offset of its first byte
line_offset:
    xor eax, eax
    test edi, edi
    jz .done
    xor ecx, ecx
.scan:
    cmp eax, [text_size]
    jae .done
    cmp byte [text + rax], 10
    jne .next
    inc ecx
    cmp ecx, edi
    jne .next
    inc eax
    ret
.next:
    inc eax
    jmp .scan
.done:
    ret

line_count:
    mov edi, [text_size]
    call line_of
    inc eax
    ret

column_of:
    push rdi
    call line_start
    pop rdi
    neg eax
    add eax, edi
    ret

place_on_line:
    push rbx
    mov ebx, edi
    call line_end
    mov ecx, eax
    lea eax, [rbx + rsi]
    cmp eax, ecx
    jbe .done
    mov eax, ecx
.done:
    pop rbx
    ret

clamp_normal_cursor:
    mov edi, [cursor]
    call line_end
    cmp eax, [cursor]
    jne .done
    mov edi, [cursor]
    call line_start
    cmp eax, [cursor]
    je .done
    dec dword [cursor]
.done:
    ret

leading_blank_end:
    mov eax, edi
.scan:
    cmp eax, [text_size]
    jae .done
    mov cl, [text + rax]
    cmp cl, ' '
    je .next
    cmp cl, 9
    jne .done
.next:
    inc eax
    jmp .scan
.done:
    ret

text_move:
    push rdi
    push rsi
    push rcx
    cmp rdi, rsi
    jbe .forward
    lea rsi, [rsi + rcx - 1]
    lea rdi, [rdi + rcx - 1]
    std
    rep movsb
    cld
    jmp .done
.forward:
    cld
    rep movsb
.done:
    pop rcx
    pop rsi
    pop rdi
    ret

raw_insert:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13, rsi
    mov ebx, ecx
    mov eax, [text_size]
    add eax, ebx
    cmp eax, TEXT_MAX
    ja .full
    mov ecx, [text_size]
    sub ecx, r12d
    lea rsi, [text + r12]
    lea rdi, [text + r12 + rbx]
    call text_move
    lea rdi, [text + r12]
    mov rsi, r13
    mov ecx, ebx
    call text_move
    add [text_size], ebx
    mov byte [dirty], 1
    clc
    jmp .out
.full:
    stc
.out:
    pop r13
    pop r12
    pop rbx
    ret

raw_delete:
    push rbx
    mov ebx, ecx
    mov ecx, [text_size]
    sub ecx, edi
    sub ecx, ebx
    lea rsi, [text + rdi + rbx]
    lea rdi, [text + rdi]
    call text_move
    sub [text_size], ebx
    mov byte [dirty], 1
    pop rbx
    ret

; rdi -> stack descriptor, dl kind, esi position, rcx -> bytes, length from [rdi + 8].
; each record ends with its own total size so the stack can be walked back from the top.
; when a record doesn't fit the whole history is dropped, crude but it never fails
push_record:
    push rbx
    push r12
    mov r12, rdi
    mov ebx, [r12 + 8]
    lea eax, [rbx + RECORD_HEADER + 4]
    add eax, [r12 + 16]
    cmp eax, UNDO_MAX
    jbe .room
    mov dword [r12 + 16], 0
.room:
    mov r8, [r12]
    mov eax, [r12 + 16]
    lea r9, [r8 + rax]
    mov [r9], dl
    mov [r9 + 1], esi
    mov [r9 + 5], ebx
    mov eax, [undo_group]
    mov [r9 + 9], eax
    lea rdi, [r9 + RECORD_HEADER]
    mov rsi, rcx
    mov ecx, ebx
    cld
    rep movsb
    lea eax, [rbx + RECORD_HEADER + 4]
    mov [rdi], eax
    add [r12 + 16], eax
    pop r12
    pop rbx
    ret

edit_insert:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13, rsi
    mov ebx, ecx
    call raw_insert
    jc .done
    lea rdi, [undo_stack_descriptor]
    mov dword [undo_stack_descriptor + 8], ebx
    mov dl, EDIT_INSERT
    mov esi, r12d
    mov rcx, r13
    call push_record
    mov dword [redo_used], 0    ; a new edit kills the redo stack like everywhere else
    clc
.done:
    pop r13
    pop r12
    pop rbx
    ret

edit_delete:
    push rbx
    push r12
    mov r12d, edi
    mov ebx, ecx
    test ebx, ebx
    jz .done
    lea rdi, [undo_stack_descriptor]
    mov dword [undo_stack_descriptor + 8], ebx
    mov dl, EDIT_DELETE
    mov esi, r12d
    lea rcx, [text + r12]
    call push_record
    mov dword [redo_used], 0
    mov edi, r12d
    mov ecx, ebx
    call raw_delete
.done:
    pop r12
    pop rbx
    ret

apply_history:
    push rbx
    push r12
    push r13
    push r14
    mov r12, rdi
    mov r13, rsi
    mov r14d, edx
    mov eax, [r12 + 16]
    test eax, eax
    jz .empty
    mov r8, [r12]
    mov ecx, [r8 + rax - 4]
    sub eax, ecx
    mov ebx, [r8 + rax + 9]

.record:
    mov eax, [r12 + 16]
    test eax, eax
    jz .done
    mov r8, [r12]
    mov ecx, [r8 + rax - 4]
    sub eax, ecx
    lea r9, [r8 + rax]
    cmp [r9 + 9], ebx
    jne .done
    mov [r12 + 16], eax

    movzx edx, byte [r9]
    mov edi, [r9 + 1]
    mov ecx, [r9 + 5]
    mov [cursor], edi
    cmp edx, r14d
    jne .reinsert
    push r9
    call raw_delete
    pop r9
    jmp .mirror
.reinsert:
    lea rsi, [r9 + RECORD_HEADER]
    push r9
    call raw_insert
    pop r9

.mirror:
    mov eax, [r9 + 5]
    mov [r13 + 8], eax
    mov eax, [undo_group]
    push rax
    mov eax, [r9 + 9]
    mov [undo_group], eax
    mov rdi, r13
    movzx edx, byte [r9]
    mov esi, [r9 + 1]
    lea rcx, [r9 + RECORD_HEADER]
    call push_record
    pop rax
    mov [undo_group], eax
    jmp .record

.empty:
    stc
    jmp .out
.done:
    clc
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

undo:
    lea rdi, [undo_stack_descriptor]
    lea rsi, [redo_stack_descriptor]
    mov edx, EDIT_INSERT
    jmp apply_history

redo:
    lea rdi, [redo_stack_descriptor]
    lea rsi, [undo_stack_descriptor]
    mov edx, EDIT_DELETE
    jmp apply_history

begin_change:
    inc dword [undo_group]
    ret
