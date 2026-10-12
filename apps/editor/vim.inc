; the vim part. normal mode reads [count] [operator] motion, like real vim but no registers,
; no marks, no dot repeat. one unnamed yank buffer

MODE_NORMAL = 0
MODE_INSERT = 1
MODE_VISUAL = 2
MODE_VISUAL_LINE = 3
MODE_COMMAND = 4
MODE_SEARCH = 5
MODE_FILES = 6
MODE_FINDER = 7

YANK_MAX = 0x10000
INDENT = 4                      ; spaces, the editor never writes tabs
KEY_ESCAPE = 27
KEY_CTRL_B = 2
KEY_CTRL_C = 3
KEY_CTRL_Q = 17
KEY_CTRL_S = 19
KEY_CTRL_T = 20
KEY_CTRL_W = 23
KEY_NEXT_TAB = KEY_CTRL or 9
KEY_NEXT_PAGE_TAB = KEY_CTRL or KEY_PAGE_DOWN
KEY_PREVIOUS_PAGE_TAB = KEY_CTRL or KEY_PAGE_UP
KEY_CTRL_D = 4
KEY_CTRL_R = 18
KEY_CTRL_U = 21

; no count typed means 1
effective_count:
    mov eax, [count]
    test eax, eax
    jnz .done
    inc eax
.done:
    ret

char_class:
    cmp al, ' '
    je .blank
    cmp al, 9
    je .blank
    cmp al, 10
    je .blank
    call is_word_char
    jc .word
    mov eax, 2
    ret
.blank:
    xor eax, eax
    ret
.word:
    mov eax, 1
    ret

class_at:
    cmp edi, [text_size]
    jae .blank
    mov al, [text + rdi]
    jmp char_class
.blank:
    xor eax, eax
    ret

word_forward:
    mov ecx, edi
    call class_at
    mov edx, eax
    mov edi, ecx
.skip_same:
    cmp edi, [text_size]
    jae .done
    test edx, edx
    jz .skip_blank
    push rdx
    call class_at
    pop rdx
    cmp eax, edx
    jne .skip_blank
    inc edi
    jmp .skip_same
.skip_blank:
    cmp edi, [text_size]
    jae .done
    call class_at
    test eax, eax
    jnz .done
    inc edi
    jmp .skip_blank
.done:
    mov eax, edi
    ret

word_backward:
.skip_blank:
    test edi, edi
    jz .done
    dec edi
    call class_at
    test eax, eax
    jz .skip_blank
    mov edx, eax
.skip_same:
    test edi, edi
    jz .done
    dec edi
    push rdx
    call class_at
    pop rdx
    cmp eax, edx
    je .skip_same
    inc edi
.done:
    mov eax, edi
    ret

word_end_forward:
    inc edi
.skip_blank:
    cmp edi, [text_size]
    jae .last
    call class_at
    test eax, eax
    jnz .word
    inc edi
    jmp .skip_blank
.word:
    mov edx, eax
.same:
    lea eax, [rdi + 1]
    cmp eax, [text_size]
    jae .done
    push rdi
    mov edi, eax
    push rdx
    call class_at
    pop rdx
    pop rdi
    cmp eax, edx
    jne .done
    inc edi
    jmp .same
.last:
    mov edi, [text_size]
    test edi, edi
    jz .done
    dec edi
.done:
    mov eax, edi
    ret

; $ lands on the last char, not on the newline
line_last_char:
    push rdi
    call line_end
    pop rdi
    push rax
    call line_start
    pop rcx
    cmp eax, ecx
    je .empty
    lea eax, [rcx - 1]
    ret
.empty:
    mov eax, ecx
    ret

vertical_target:
    push rbx
    push r12
    mov r12d, esi
    mov edi, [cursor]
    call line_start
    mov ebx, eax
.step:
    test r12d, r12d
    jz .place
    mov edi, ebx
    cmp dword [vertical_direction], 0
    jl .up
    call next_line_start
    jc .place
    jmp .moved
.up:
    call previous_line_start
    jc .place
.moved:
    mov ebx, eax
    dec r12d
    jmp .step
.place:
    mov edi, ebx
    mov esi, [want_column]
    call place_on_line
    pop r12
    pop rbx
    ret

paragraph_target:
    mov edi, [cursor]
    call line_start
    mov ecx, eax
.step:
    mov edi, ecx
    cmp dword [vertical_direction], 0
    jl .up
    call next_line_start
    jc .end
    jmp .check
.up:
    call previous_line_start
    jc .start
.check:
    mov ecx, eax
    mov edi, eax
    call line_end
    cmp eax, ecx
    jne .step
    mov eax, ecx
    ret
.end:
    mov eax, [text_size]
    ret
.start:
    xor eax, eax
    ret

; f F t T on the current line only, t and T stop one short
find_in_line:
    mov edi, [cursor]
    mov r8b, [find_char]
    cmp byte [find_kind], 'f'
    je .forward
    cmp byte [find_kind], 't'
    je .forward
.backward:
    test edi, edi
    jz .missing
    dec edi
    cmp byte [text + rdi], 10
    je .missing
    cmp [text + rdi], r8b
    jne .backward
    cmp byte [find_kind], 'T'
    jne .found
    inc edi
    jmp .found
.forward:
    inc edi
    cmp edi, [text_size]
    jae .missing
    cmp byte [text + rdi], 10
    je .missing
    cmp [text + rdi], r8b
    jne .forward
    cmp byte [find_kind], 't'
    jne .found
    dec edi
.found:
    mov eax, edi
    clc
    ret
.missing:
    stc
    ret

motion:
    mov byte [motion_linewise], 0
    mov byte [motion_inclusive], 0
    mov ecx, eax
    call effective_count
    mov r12d, eax
    mov eax, ecx

    cmp eax, 'h'
    je .left
    cmp eax, KEY_LEFT
    je .left
    cmp eax, 'l'
    je .right
    cmp eax, KEY_RIGHT
    je .right
    cmp eax, ' '
    je .right
    cmp eax, 'j'
    je .down
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 'k'
    je .up
    cmp eax, KEY_UP
    je .up
    cmp eax, 'w'
    je .word
    cmp eax, 'W'
    je .word
    cmp eax, 'b'
    je .back
    cmp eax, 'B'
    je .back
    cmp eax, 'e'
    je .end_word
    cmp eax, 'E'
    je .end_word
    cmp eax, '0'
    je .line_start
    cmp eax, KEY_HOME
    je .line_start
    cmp eax, '^'
    je .first_blank
    cmp eax, '$'
    je .line_end
    cmp eax, KEY_END
    je .line_end
    cmp eax, 'G'
    je .last_line
    cmp eax, '}'
    je .paragraph_down
    cmp eax, '{'
    je .paragraph_up
    cmp eax, '%'
    je .bracket
    cmp eax, KEY_PAGE_DOWN
    je .page_down
    cmp eax, KEY_PAGE_UP
    je .page_up
    cmp eax, KEY_CTRL_D
    je .half_down
    cmp eax, KEY_CTRL_U
    je .half_up
    cmp eax, 'n'
    je .search_next
    cmp eax, 'N'
    je .search_previous
    stc
    ret

.left:
    mov edi, [cursor]
    call line_start
    mov ecx, [cursor]
    sub ecx, r12d
    cmp ecx, eax
    jge .left_ready
    mov ecx, eax
.left_ready:
    mov eax, ecx
    jmp .found

.right:
    mov edi, [cursor]
    call line_end
    mov ecx, [cursor]
    add ecx, r12d
    cmp ecx, eax
    jbe .found_ecx
    mov ecx, eax
.found_ecx:
    mov eax, ecx
    jmp .found

.down:
    mov dword [vertical_direction], 1
    jmp .vertical
.up:
    mov dword [vertical_direction], -1
.vertical:
    mov esi, r12d
    call vertical_target
    mov byte [motion_linewise], 1
    mov byte [motion_vertical], 1
    jmp .found

.word:
    cmp byte [operator], 'c'
    je .end_word
    mov edi, [cursor]
.word_repeat:
    call word_forward
    mov edi, eax
    dec r12d
    jnz .word_repeat
    ; dw on the last word of a line must not eat the newline
    cmp byte [operator], 0
    je .found
    push rax
    mov edi, [cursor]
    call line_end
    pop rcx
    cmp ecx, eax
    jbe .found_ecx
    jmp .found

.back:
    mov edi, [cursor]
.back_repeat:
    call word_backward
    mov edi, eax
    dec r12d
    jnz .back_repeat
    jmp .found

.end_word:
    mov edi, [cursor]
.end_repeat:
    call word_end_forward
    mov edi, eax
    dec r12d
    jnz .end_repeat
    mov byte [motion_inclusive], 1
    jmp .found

.line_start:
    mov edi, [cursor]
    call line_start
    jmp .found

.first_blank:
    mov edi, [cursor]
    call line_start
    mov edi, eax
    call leading_blank_end
    jmp .found

.line_end:
    mov edi, [cursor]
    call line_last_char
    mov byte [motion_inclusive], 1
    mov dword [want_column], 0x7FFFFFFF
    mov byte [motion_vertical], 1
    jmp .found

.last_line:
    mov edi, [count]
    test edi, edi
    jnz .goto_line
    call line_count
    mov edi, eax
.goto_line:
    dec edi
    call line_offset
    mov edi, eax
    call leading_blank_end
    mov byte [motion_linewise], 1
    jmp .found

.paragraph_down:
    mov dword [vertical_direction], 1
    call paragraph_target
    jmp .found
.paragraph_up:
    mov dword [vertical_direction], -1
    call paragraph_target
    jmp .found

.bracket:
    call find_bracket_match
    mov eax, [bracket_match]
    cmp eax, -1
    je .missing
    mov byte [motion_inclusive], 1
    jmp .found

.page_down:
    call text_rows
    mov r12d, eax
    jmp .down
.page_up:
    call text_rows
    mov r12d, eax
    jmp .up
.half_down:
    call text_rows
    shr eax, 1
    mov r12d, eax
    jmp .down
.half_up:
    call text_rows
    shr eax, 1
    mov r12d, eax
    jmp .up

.search_next:
    mov al, [search_direction]
    jmp .search
.search_previous:
    mov al, [search_direction]
    xor al, 1
.search:
    call search_from_cursor
    jc .missing
    jmp .found

.missing:
    mov eax, [cursor]
.found:
    mov [motion_target], eax
    clc
    ret

goto_motion:
    mov byte [motion_vertical], 0
    call motion
    jc .done
    mov eax, [motion_target]
    mov [cursor], eax
    cmp byte [motion_vertical], 0
    jne .done_moved
    mov edi, eax
    call column_of
    mov [want_column], eax
.done_moved:
    clc
.done:
    ret

operator_range:
    mov eax, [cursor]
    mov ecx, [motion_target]
    cmp eax, ecx
    jbe .ordered
    xchg eax, ecx
.ordered:
    cmp byte [motion_linewise], 0
    jne .lines
    cmp byte [motion_inclusive], 0
    je .store
    cmp ecx, [text_size]
    jae .store
    inc ecx
.store:
    mov [range_from], eax
    mov [range_to], ecx
    mov byte [range_linewise], 0
    ret
.lines:
    push rcx
    mov edi, eax
    call line_start
    mov [range_from], eax
    pop rdi
    call next_line_start
    jnc .line_end_ready
    mov eax, [text_size]
.line_end_ready:
    mov [range_to], eax
    mov byte [range_linewise], 1
    ret

yank_range:
    mov ecx, [range_to]
    sub ecx, [range_from]
    cmp ecx, YANK_MAX
    jbe .size
    mov ecx, YANK_MAX
.size:
    mov [yank_length], ecx
    mov al, [range_linewise]
    mov [yank_linewise], al
    mov esi, [range_from]
    lea rsi, [text + rsi]
    lea rdi, [yank_buffer]
    cld
    rep movsb
    cmp byte [range_linewise], 0
    je .done
    mov ecx, [yank_length]
    test ecx, ecx
    jz .done
    cmp byte [yank_buffer + rcx - 1], 10
    je .done
    cmp ecx, YANK_MAX
    jae .done
    mov byte [yank_buffer + rcx], 10
    inc dword [yank_length]
.done:
    ret

delete_range:
    cmp byte [range_linewise], 0
    je .delete
    mov eax, [range_to]
    cmp eax, [text_size]
    jb .delete
    mov eax, [range_from]
    test eax, eax
    jz .delete
    cmp byte [text + rax - 1], 10
    jne .delete
    dec dword [range_from]
.delete:
    mov edi, [range_from]
    mov ecx, [range_to]
    sub ecx, edi
    call edit_delete
    mov eax, [range_from]
    mov [cursor], eax
    cmp byte [range_linewise], 0
    je .done
    mov edi, eax
    call line_start
    mov edi, eax
    call leading_blank_end
    mov [cursor], eax
.done:
    ret

indent_range:
    push rbx
    push r12
    mov edi, [range_from]
    call line_start
    mov ebx, eax
.line:
    cmp ebx, [range_to]
    jae .done
    cmp ebx, [text_size]
    jae .done
    mov edi, ebx
    call line_end
    cmp eax, ebx
    je .next
    cmp byte [indent_direction], 0
    jne .outdent
    mov edi, ebx
    lea rsi, [indent_spaces]
    mov ecx, INDENT
    call edit_insert
    add dword [range_to], INDENT
    jmp .next
.outdent:
    xor ecx, ecx
.count_blanks:
    cmp ecx, INDENT
    jae .remove
    lea eax, [rbx + rcx]
    cmp eax, [text_size]
    jae .remove
    cmp byte [text + rax], ' '
    jne .remove
    inc ecx
    jmp .count_blanks
.remove:
    test ecx, ecx
    jz .next
    sub [range_to], ecx
    mov edi, ebx
    call edit_delete
.next:
    mov edi, ebx
    call next_line_start
    jc .done
    mov ebx, eax
    jmp .line
.done:
    mov edi, [range_from]
    call line_start
    mov edi, eax
    call leading_blank_end
    mov [cursor], eax
    pop r12
    pop rbx
    ret

apply_operator:
    call begin_change
    movzx eax, byte [operator]
    cmp eax, 'y'
    je .yank
    cmp eax, '>'
    je .indent
    cmp eax, '<'
    je .outdent
    call yank_range
    cmp byte [operator], 'c'
    je .change
    jmp delete_range
.yank:
    call yank_range
    mov eax, [range_from]
    mov [cursor], eax
    ret
.indent:
    mov byte [indent_direction], 0
    jmp indent_range
.outdent:
    mov byte [indent_direction], 1
    jmp indent_range
.change:
    cmp byte [range_linewise], 0
    je .change_chars
    mov eax, [range_to]
    cmp eax, [range_from]
    je .change_chars
    dec eax
    cmp byte [text + rax], 10
    jne .change_chars
    mov [range_to], eax
.change_chars:
    mov edi, [range_from]
    mov ecx, [range_to]
    sub ecx, edi
    call edit_delete
    mov eax, [range_from]
    mov [cursor], eax
    jmp enter_insert_here

current_lines_range:
    mov edi, [cursor]
    call line_start
    mov [range_from], eax
    call effective_count
    mov ecx, eax
    mov edi, [cursor]
.next:
    call next_line_start
    jc .end
    mov edi, eax
    dec ecx
    jnz .next
    mov [range_to], eax
    jmp .done
.end:
    mov eax, [text_size]
    mov [range_to], eax
.done:
    mov byte [range_linewise], 1
    ret

; iw aw i( a( i" a" and friends. eax = object key, object_around = 'i' or 'a'.
; quotes only look on the current line, like vim
text_object:
    push rbx
    mov byte [range_linewise], 0
    mov edi, [cursor]
    cmp edi, [text_size]
    jae .missing
    cmp eax, 'w'
    je .word
    cmp eax, 'W'
    je .word
    cmp eax, '"'
    je .quote
    cmp eax, 39
    je .quote
    cmp eax, '`'
    je .quote
    lea rsi, [object_pairs]
.pair:
    mov cl, [rsi]
    test cl, cl
    jz .missing
    cmp al, cl
    je .pair_found
    cmp al, [rsi + 1]
    je .pair_found
    cmp al, [rsi + 2]
    je .pair_found
    add rsi, 3
    jmp .pair

.pair_found:
    mov dl, [rsi + 1]
    xor r8d, r8d
    cmp [text + rdi], cl
    je .open_found
.scan_back:
    test edi, edi
    jz .missing
    dec edi
    mov bl, [text + rdi]
    cmp bl, dl
    jne .check_open
    inc r8d
    jmp .scan_back
.check_open:
    cmp bl, cl
    jne .scan_back
    test r8d, r8d
    jz .open_found
    dec r8d
    jmp .scan_back
.open_found:
    mov ebx, edi
    mov eax, [cursor]
    push rax
    mov [cursor], edi
    call find_bracket_match
    pop rax
    mov [cursor], eax
    mov eax, [bracket_match]
    cmp eax, -1
    je .missing
    jmp .delimited

.quote:
    mov cl, al
    call line_start
    mov r8d, eax
    mov edi, [cursor]
.quote_back:
    cmp [text + rdi], cl
    je .quote_open
    cmp edi, r8d
    je .quote_ahead
    dec edi
    jmp .quote_back
.quote_ahead:
    mov edi, [cursor]
.quote_forward:
    cmp edi, [text_size]
    jae .missing
    mov al, [text + rdi]
    cmp al, 10
    je .missing
    cmp al, cl
    je .quote_open
    inc edi
    jmp .quote_forward
.quote_open:
    mov ebx, edi
.quote_close:
    inc edi
    cmp edi, [text_size]
    jae .missing
    mov al, [text + rdi]
    cmp al, 10
    je .missing
    cmp al, cl
    jne .quote_close
    mov eax, edi

.delimited:
    cmp byte [object_around], 'a'
    je .around
    inc ebx
    jmp .store
.around:
    inc eax
.store:
    mov [range_from], ebx
    mov [range_to], eax
    pop rbx
    clc
    ret

.word:
    cmp byte [text + rdi], 10
    je .missing
    call class_at
    mov r8d, eax
    mov ebx, edi
.word_back:
    test ebx, ebx
    jz .word_forward
    lea edi, [rbx - 1]
    cmp byte [text + rdi], 10
    je .word_forward
    call class_at
    cmp eax, r8d
    jne .word_forward
    dec ebx
    jmp .word_back
.word_forward:
    mov edi, [cursor]
.word_next:
    inc edi
    cmp edi, [text_size]
    jae .word_done
    cmp byte [text + rdi], 10
    je .word_done
    call class_at
    cmp eax, r8d
    je .word_next
.word_done:
    cmp byte [object_around], 'a'
    jne .word_store
.trailing:
    cmp edi, [text_size]
    jae .word_store
    mov al, [text + rdi]
    cmp al, ' '
    je .blank
    cmp al, 9
    jne .word_store
.blank:
    inc edi
    jmp .trailing
.word_store:
    mov eax, edi
    jmp .store

.missing:
    pop rbx
    stc
    ret

enter_insert_here:
    mov byte [mode], MODE_INSERT
    ret

; every key lands here first, mode picks the handler
handle_key:
    mov [key], edi
    mov qword [message], 0
    movzx eax, byte [mode]
    jmp qword [mode_handlers + rax * 8]

normal_key:
    push r12
    mov byte [motion_vertical], 0
    mov eax, [key]

    cmp byte [pending], 0
    jne .pending

    cmp eax, KEY_ESCAPE
    je .reset
    cmp eax, '1'
    jb .not_count
    cmp eax, '9'
    jbe .count_digit
    jmp .not_count
.count_digit:
    imul ecx, [count], 10
    sub eax, '0'
    add eax, ecx
    mov [count], eax
    jmp .keep
.not_count:
    cmp eax, '0'
    jne .not_zero
    cmp dword [count], 0
    je .not_zero
    imul ecx, [count], 10
    mov [count], ecx
    jmp .keep
.not_zero:

    cmp byte [operator], 0
    je .no_operator
    movzx ecx, byte [operator]
    cmp eax, ecx
    je .doubled
    cmp eax, 'g'
    je .prefix
    cmp eax, 'f'
    je .prefix
    cmp eax, 'F'
    je .prefix
    cmp eax, 't'
    je .prefix
    cmp eax, 'T'
    je .prefix
    cmp eax, 'i'
    je .prefix
    cmp eax, 'a'
    je .prefix
    mov byte [motion_vertical], 0
    call motion
    jc .reset
    call operator_range
    call apply_operator
    jmp .reset

.doubled:
    call current_lines_range
    call apply_operator
    jmp .reset

.no_operator:
    mov byte [motion_vertical], 0
    push rax
    call goto_motion
    pop rax
    jnc .reset

    cmp eax, 'd'
    je .set_operator
    cmp eax, 'c'
    je .set_operator
    cmp eax, 'y'
    je .set_operator
    cmp eax, '>'
    je .set_operator
    cmp eax, '<'
    je .set_operator
    cmp eax, 'g'
    je .prefix
    cmp eax, 'f'
    je .prefix
    cmp eax, 'F'
    je .prefix
    cmp eax, 't'
    je .prefix
    cmp eax, 'T'
    je .prefix
    cmp eax, 'r'
    je .prefix
    cmp eax, 'Z'
    je .prefix

    lea rcx, [normal_commands]
.command:
    cmp dword [rcx], 0
    je .reset
    cmp [rcx], eax
    je .run_command
    add rcx, 12
    jmp .command
.run_command:
    call qword [rcx + 4]
    jmp .reset

.set_operator:
    mov [operator], al
    jmp .keep

.prefix:
    mov [pending], al
    jmp .keep

.pending:
    movzx ecx, byte [pending]
    mov byte [pending], 0
    cmp ecx, 'g'
    je .g_prefix
    cmp ecx, 'r'
    je .replace
    cmp ecx, 'Z'
    je .z_prefix
    cmp ecx, 'i'
    je .object
    cmp ecx, 'a'
    je .object
    mov [find_kind], cl
    mov [find_char], al
    call find_in_line
    jc .reset
    mov [motion_target], eax
    mov byte [motion_linewise], 0
    mov byte [motion_inclusive], 1
    cmp byte [operator], 0
    je .find_move
    call operator_range
    call apply_operator
    jmp .reset
.find_move:
    mov eax, [motion_target]
    mov [cursor], eax
    jmp .reset

.g_prefix:
    cmp eax, 't'
    je .next_tab
    cmp eax, 'T'
    je .previous_tab
    cmp eax, 'g'
    jne .reset
    mov edi, [count]
    test edi, edi
    jnz .g_line
    inc edi
.g_line:
    dec edi
    call line_offset
    mov edi, eax
    call leading_blank_end
    mov [motion_target], eax
    mov byte [motion_linewise], 1
    mov byte [motion_inclusive], 0
    cmp byte [operator], 0
    je .g_move
    call operator_range
    call apply_operator
    jmp .reset
.g_move:
    mov eax, [motion_target]
    mov [cursor], eax
    jmp .reset

.object:
    mov [object_around], cl
    call text_object
    jc .reset
    call apply_operator
    jmp .reset

.next_tab:
    call next_buffer
    jmp .reset
.previous_tab:
    call previous_buffer
    jmp .reset

.replace:
    cmp eax, ' '
    jb .reset
    cmp eax, 126
    ja .reset
    mov [replace_char], al
    call replace_characters
    jmp .reset

.z_prefix:
    cmp eax, 'Z'
    je .write_quit
    cmp eax, 'Q'
    jne .reset
    mov al, 1
    call close_buffer
    jmp .reset
.write_quit:
    call save_file
    jc .reset
    xor eax, eax
    call close_buffer

.reset:
    mov dword [count], 0
    mov byte [operator], 0
    mov byte [pending], 0
    cmp byte [mode], MODE_NORMAL
    jne .keep
    call clamp_normal_cursor
    cmp byte [motion_vertical], 0
    jne .keep
    mov edi, [cursor]
    call column_of
    mov [want_column], eax
.keep:
    pop r12
    ret

replace_characters:
    call begin_change
    call effective_count
    mov ecx, eax
    mov edi, [cursor]
    call line_end
    sub eax, [cursor]
    cmp ecx, eax
    ja .done
    cmp ecx, 256
    ja .done
    push rcx
    mov edi, [cursor]
    call edit_delete
    pop rcx
    push rcx
    mov al, [replace_char]
    lea rdi, [scratch]
    rep stosb
    pop rcx
    mov edi, [cursor]
    lea rsi, [scratch]
    call edit_insert
.done:
    ret

command_insert:
    call begin_change
    jmp enter_insert_here

command_insert_line_start:
    mov edi, [cursor]
    call line_start
    mov edi, eax
    call leading_blank_end
    mov [cursor], eax
    jmp command_insert

command_append:
    mov edi, [cursor]
    call line_end
    cmp eax, [cursor]
    je command_insert
    inc dword [cursor]
    jmp command_insert

command_append_line_end:
    mov edi, [cursor]
    call line_end
    mov [cursor], eax
    jmp command_insert

command_open_below:
    call begin_change
    mov edi, [cursor]
    call line_end
    mov [cursor], eax
    jmp insert_newline_indented

command_open_above:
    call begin_change
    mov edi, [cursor]
    call line_start
    mov [cursor], eax
    mov edi, eax
    call leading_blank_end
    mov ecx, eax
    sub ecx, [cursor]
    push rcx
    mov esi, [cursor]
    lea rsi, [text + rsi]
    lea rdi, [scratch]
    rep movsb
    mov byte [rdi], 10
    pop rcx
    push rcx
    inc ecx
    mov edi, [cursor]
    lea rsi, [scratch]
    call edit_insert
    pop rcx
    add [cursor], ecx
    jmp enter_insert_here

; newline plus the leading blanks of this line (only up to the cursor)
insert_newline_indented:
    mov edi, [cursor]
    call line_start
    mov edi, eax
    push rdi
    call leading_blank_end
    pop rdi
    mov ecx, eax
    sub ecx, edi
    cmp eax, [cursor]
    jbe .copy
    mov ecx, [cursor]
    sub ecx, edi
.copy:
    mov byte [scratch], 10
    lea rsi, [text + rdi]
    lea rdi, [scratch + 1]
    push rcx
    rep movsb
    pop rcx
    inc ecx
    mov edi, [cursor]
    push rcx
    lea rsi, [scratch]
    call edit_insert
    pop rcx
    add [cursor], ecx
    jmp enter_insert_here

command_delete_char:
    call begin_change
    call effective_count
    mov ecx, eax
    mov edi, [cursor]
    call line_end
    sub eax, [cursor]
    cmp ecx, eax
    jbe .range
    mov ecx, eax
.range:
    test ecx, ecx
    jz .done
    mov eax, [cursor]
    mov [range_from], eax
    add eax, ecx
    mov [range_to], eax
    mov byte [range_linewise], 0
    call yank_range
    mov edi, [range_from]
    mov ecx, [range_to]
    sub ecx, edi
    jmp edit_delete
.done:
    ret

command_delete_before:
    call begin_change
    mov edi, [cursor]
    call line_start
    mov ecx, [cursor]
    sub ecx, eax
    jz .done
    call effective_count
    cmp eax, ecx
    jbe .count
    mov eax, ecx
.count:
    sub [cursor], eax
    mov ecx, eax
    mov edi, [cursor]
    call edit_delete
.done:
    ret

command_substitute:
    call command_delete_char
    jmp enter_insert_here

command_change_line:
    call current_lines_range
    mov byte [operator], 'c'
    jmp apply_operator

command_change_to_end:
    mov edi, [cursor]
    call line_end
    mov [motion_target], eax
    mov byte [motion_linewise], 0
    mov byte [motion_inclusive], 0
    call operator_range
    mov byte [operator], 'c'
    jmp apply_operator

command_delete_to_end:
    mov edi, [cursor]
    call line_end
    mov [motion_target], eax
    mov byte [motion_linewise], 0
    mov byte [motion_inclusive], 0
    call operator_range
    mov byte [operator], 'd'
    jmp apply_operator

command_yank_line:
    call current_lines_range
    mov byte [operator], 'y'
    jmp apply_operator

command_paste_after:
    mov byte [paste_before], 0
    jmp paste
command_paste_before:
    mov byte [paste_before], 1

paste:
    mov ecx, [yank_length]
    test ecx, ecx
    jz .done
    call begin_change
    cmp byte [yank_linewise], 0
    jne .lines
    mov edi, [cursor]
    cmp byte [paste_before], 0
    jne .insert_chars
    call line_end
    cmp eax, [cursor]
    je .insert_chars
    inc edi
.insert_chars:
    push rdi
    lea rsi, [yank_buffer]
    mov ecx, [yank_length]
    call edit_insert
    pop rax
    add eax, [yank_length]
    dec eax
    mov [cursor], eax
    ret
.lines:
    mov edi, [cursor]
    cmp byte [paste_before], 0
    jne .before
    call next_line_start
    jnc .at
    ; last line has no newline after it, make one first
    mov edi, [text_size]
    push rdi
    lea rsi, [newline_text]
    mov ecx, 1
    call edit_insert
    pop rax
    inc eax
    jmp .at
.before:
    call line_start
.at:
    mov edi, eax
    push rdi
    lea rsi, [yank_buffer]
    mov ecx, [yank_length]
    call edit_insert
    pop rdi
    call leading_blank_end
    mov [cursor], eax
.done:
    ret

command_join:
    push rbx
    push r12
    call begin_change
    call effective_count
    lea r12d, [rax - 1]
    cmp r12d, 1
    jge .join
    mov r12d, 1
.join:
    mov edi, [cursor]
    call line_end
    cmp eax, [text_size]
    jae .done
    mov ebx, eax
    lea edi, [rax + 1]
    call leading_blank_end
    mov ecx, eax
    sub ecx, ebx
    mov edi, ebx
    call edit_delete
    mov [cursor], ebx
    mov edi, ebx
    lea rsi, [space_text]
    mov ecx, 1
    call edit_insert
    dec r12d
    jnz .join
.done:
    pop r12
    pop rbx
    ret

command_toggle_case:
    mov edi, [cursor]
    cmp edi, [text_size]
    jae .done
    mov al, [text + rdi]
    cmp al, 10
    je .done
    mov cl, al
    or cl, 0x20
    cmp cl, 'a'
    jb .advance
    cmp cl, 'z'
    ja .advance
    xor al, 0x20
    mov [scratch], al
    call begin_change
    mov edi, [cursor]
    mov ecx, 1
    call edit_delete
    mov edi, [cursor]
    lea rsi, [scratch]
    mov ecx, 1
    call edit_insert
.advance:
    mov edi, [cursor]
    call line_end
    mov ecx, [cursor]
    inc ecx
    cmp ecx, eax
    jae .done
    mov [cursor], ecx
.done:
    ret

command_undo:
    call undo
    jnc .done
    lea rax, [oldest_change_text]
    mov [message], rax
.done:
    ret

command_redo:
    call redo
    jnc .done
    lea rax, [newest_change_text]
    mov [message], rax
.done:
    ret

command_visual:
    mov byte [mode], MODE_VISUAL
    mov eax, [cursor]
    mov [anchor], eax
    ret

command_visual_line:
    mov byte [mode], MODE_VISUAL_LINE
    mov eax, [cursor]
    mov [anchor], eax
    ret

command_colon:
    mov al, PROMPT_COMMAND
    lea rsi, [colon_label]
    jmp start_prompt

command_search_forward:
    mov byte [search_direction], 0
    lea rsi, [slash_label]
    jmp open_search
command_search_backward:
    mov byte [search_direction], 1
    lea rsi, [question_label]
open_search:
    mov al, PROMPT_SEARCH
    jmp start_prompt

visual_key:
    push r12
    mov eax, [key]
    cmp eax, KEY_ESCAPE
    je .leave
    cmp eax, 'v'
    je .switch_char
    cmp eax, 'V'
    je .switch_line
    cmp eax, 'o'
    je .swap
    cmp eax, 'd'
    je .delete
    cmp eax, 'x'
    je .delete
    cmp eax, 'y'
    je .yank
    cmp eax, 'c'
    je .change
    cmp eax, '>'
    je .indent
    cmp eax, '<'
    je .outdent
    cmp eax, ':'
    je .colon
    mov byte [motion_vertical], 0
    call goto_motion
    jmp .done

.switch_char:
    cmp byte [mode], MODE_VISUAL
    je .leave
    mov byte [mode], MODE_VISUAL
    jmp .done
.switch_line:
    cmp byte [mode], MODE_VISUAL_LINE
    je .leave
    mov byte [mode], MODE_VISUAL_LINE
    jmp .done
.swap:
    mov eax, [cursor]
    xchg eax, [anchor]
    mov [cursor], eax
    jmp .done

.delete:
    mov byte [operator], 'd'
    jmp .apply
.yank:
    mov byte [operator], 'y'
    jmp .apply
.change:
    mov byte [operator], 'c'
    jmp .apply
.indent:
    mov byte [operator], '>'
    jmp .apply
.outdent:
    mov byte [operator], '<'
.apply:
    call visual_range
    mov byte [mode], MODE_NORMAL
    call apply_operator
    mov byte [operator], 0
    cmp byte [mode], MODE_NORMAL
    jne .done
    call clamp_normal_cursor
    jmp .done

.colon:
    call command_colon
    jmp .done

.leave:
    mov byte [mode], MODE_NORMAL
    call clamp_normal_cursor
.done:
    pop r12
    ret

visual_range:
    mov eax, [anchor]
    mov [motion_target], eax
    mov byte [motion_inclusive], 1
    mov byte [motion_linewise], 0
    cmp byte [mode], MODE_VISUAL_LINE
    jne operator_range
    mov byte [motion_linewise], 1
    jmp operator_range

; arrows still move in insert mode, tab inserts spaces to the next indent stop
insert_key:
    mov eax, [key]
    cmp eax, KEY_ESCAPE
    je .leave
    cmp eax, KEY_CTRL_C
    je .leave
    cmp eax, KEY_CTRL_S
    je save_file
    cmp eax, KEY_NEXT_TAB
    je next_buffer
    cmp eax, KEY_NEXT_PAGE_TAB
    je next_buffer
    cmp eax, KEY_PREVIOUS_PAGE_TAB
    je previous_buffer
    cmp eax, 8
    je .backspace
    cmp eax, KEY_DELETE
    je .delete
    cmp eax, KEY_LEFT
    je .move
    cmp eax, KEY_RIGHT
    je .move
    cmp eax, KEY_UP
    je .move
    cmp eax, KEY_DOWN
    je .move
    cmp eax, KEY_HOME
    je .move
    cmp eax, KEY_END
    je .move
    cmp eax, 9
    je .normal_tab
    cmp eax, 10
    je insert_newline_indented
    cmp eax, ' '
    jb .done
    cmp eax, 126
    ja .done
    mov [scratch], al
    mov edi, [cursor]
    lea rsi, [scratch]
    mov ecx, 1
    call edit_insert
    inc dword [cursor]
    jmp .column

.backspace:
    mov edi, [cursor]
    test edi, edi
    jz .done
    dec edi
    mov [cursor], edi
    mov ecx, 1
    call edit_delete
    jmp .column

.delete:
    mov edi, [cursor]
    cmp edi, [text_size]
    jae .done
    mov ecx, 1
    call edit_delete
    ret

.normal_tab:
    mov edi, [cursor]
    call column_of
    and eax, INDENT - 1
    mov ecx, INDENT
    sub ecx, eax
    mov edi, [cursor]
    lea rsi, [indent_spaces]
    push rcx
    call edit_insert
    pop rcx
    add [cursor], ecx
    jmp .column

.move:
    push r12
    mov dword [count], 0
    mov byte [motion_vertical], 0
    call goto_motion
    pop r12
    ret

.leave:
    mov byte [mode], MODE_NORMAL
    mov edi, [cursor]
    call line_start
    cmp eax, [cursor]
    je .done
    dec dword [cursor]
    ret

.column:
    mov edi, [cursor]
    call column_of
    mov [want_column], eax
.done:
    ret

mode_handlers:
    dq normal_key, insert_key, visual_key, visual_key, prompt_key, prompt_key, files_key, finder_key

normal_commands:
    dd 'i'
    dq command_insert
    dd 'I'
    dq command_insert_line_start
    dd 'a'
    dq command_append
    dd 'A'
    dq command_append_line_end
    dd 'o'
    dq command_open_below
    dd 'O'
    dq command_open_above
    dd 'x'
    dq command_delete_char
    dd KEY_DELETE
    dq command_delete_char
    dd 'X'
    dq command_delete_before
    dd 's'
    dq command_substitute
    dd 'S'
    dq command_change_line
    dd 'C'
    dq command_change_to_end
    dd 'D'
    dq command_delete_to_end
    dd 'Y'
    dq command_yank_line
    dd 'p'
    dq command_paste_after
    dd 'P'
    dq command_paste_before
    dd 'J'
    dq command_join
    dd '~'
    dq command_toggle_case
    dd 'u'
    dq command_undo
    dd KEY_CTRL_R
    dq command_redo
    dd KEY_CTRL_B
    dq panel_toggle
    dd KEY_CTRL_C
    dq panel_toggle
    dd KEY_CTRL_S
    dq save_file
    dd KEY_CTRL_Q
    dq ex_quit_all
    dd KEY_CTRL_T
    dq new_buffer
    dd KEY_CTRL_W
    dq close_tab
    dd KEY_NEXT_TAB
    dq next_buffer
    dd KEY_NEXT_PAGE_TAB
    dq next_buffer
    dd KEY_PREVIOUS_PAGE_TAB
    dq previous_buffer
    dd KEY_CTRL_P
    dq finder_open
    dd 'v'
    dq command_visual
    dd 'V'
    dq command_visual_line
    dd ':'
    dq command_colon
    dd '/'
    dq command_search_forward
    dd '?'
    dq command_search_backward
    dd 0

mode_names dq normal_name, insert_name, visual_name, visual_line_name, command_name, command_name, files_name, finder_name
mode_colors db 0x1F, 0x2F, 0x5F, 0x5F, 0x6F, 0x6F, 0x3F, 0x6F
normal_name db 'NORMAL', 0
insert_name db 'INSERT', 0
visual_name db 'VISUAL', 0
visual_line_name db 'V-LINE', 0
command_name db 'COMMAND', 0
files_name db 'FILES', 0
finder_name db 'FIND', 0
colon_label db ':', 0
slash_label db '/', 0
question_label db '?', 0
object_pairs db '()b', '[][', '{}B', 0
indent_spaces db '    '
space_text db ' '
newline_text db 10
oldest_change_text db 'Already at oldest change', 0
newest_change_text db 'Already at newest change', 0
