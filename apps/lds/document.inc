; the terminal text. 512 lines of char/attr pairs like vga text memory, the window shows
; view_rows of them starting at document_view_top. when it fills up the first line drops off

DOCUMENT_COLUMNS = 78
DOCUMENT_LINES = 512
DOCUMENT_LINE_BYTES = DOCUMENT_COLUMNS * 2

document_reset:
    push rax
    push rcx
    push rdx
    push rdi

    call document_attribute
    mov ah, dl
    mov al, ' '
    lea rdi, [document_buffer]
    mov ecx, DOCUMENT_LINES * DOCUMENT_COLUMNS
    cld
    rep stosw

    lea rdi, [document_line_lengths]
    xor eax, eax
    mov ecx, DOCUMENT_LINES
    rep stosb
    lea rdi, [document_protected_lengths]
    mov ecx, DOCUMENT_LINES
    rep stosb
    lea rdi, [document_continued]
    mov ecx, DOCUMENT_LINES
    rep stosb

    mov word [document_line_count], 1
    mov word [document_cursor_line], 0
    mov word [document_cursor_column], 0
    mov word [document_view_top], 0
    mov byte [document_dirty], 1

    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

document_attribute:
    mov dl, [terminal_background]
    shl dl, 4
    or dl, [terminal_foreground]
    ret

document_line_address:
    imul eax, DOCUMENT_LINE_BYTES
    lea rax, [document_buffer + rax]
    ret

document_clear_line:
    push rax
    push rcx
    push rdx
    push rdi
    push r10

    mov r10d, eax
    call document_line_address
    mov rdi, rax
    call document_attribute
    mov al, ' '
    mov ah, dl
    mov ecx, DOCUMENT_COLUMNS
    cld
    rep stosw

    mov byte [document_line_lengths + r10], 0
    mov byte [document_protected_lengths + r10], 0
    mov byte [document_continued + r10], 0

    pop r10
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

; makes sure lines up to eax exist, new ones come out blank
document_ensure_line:
    push rax
    push rbx
    push r12

    mov r12d, eax
    cmp r12d, DOCUMENT_LINES
    jae .done
    movzx ebx, word [document_line_count]
.next:
    cmp ebx, r12d
    ja .done
    mov eax, ebx
    call document_clear_line
    inc ebx
    mov [document_line_count], bx
    jmp .next
.done:
    pop r12
    pop rbx
    pop rax
    ret

document_drop_first_line:
    push rax
    push rcx
    push rsi
    push rdi

    lea rsi, [document_buffer + DOCUMENT_LINE_BYTES]
    lea rdi, [document_buffer]
    mov ecx, (DOCUMENT_LINES - 1) * DOCUMENT_LINE_BYTES
    cld
    rep movsb
    lea rsi, [document_line_lengths + 1]
    lea rdi, [document_line_lengths]
    mov ecx, DOCUMENT_LINES - 1
    rep movsb
    lea rsi, [document_protected_lengths + 1]
    lea rdi, [document_protected_lengths]
    mov ecx, DOCUMENT_LINES - 1
    rep movsb
    mov byte [document_protected_lengths + DOCUMENT_LINES - 1], 0  ; clear_line below does it again, harmless
    lea rsi, [document_continued + 1]
    lea rdi, [document_continued]
    mov ecx, DOCUMENT_LINES - 1
    rep movsb

    mov eax, DOCUMENT_LINES - 1
    call document_clear_line

    cmp word [document_cursor_line], 0
    je .view
    dec word [document_cursor_line]
.view:
    cmp word [document_view_top], 0
    je .done
    dec word [document_view_top]
.done:
    mov byte [document_dirty], 1
    pop rdi
    pop rsi
    pop rcx
    pop rax
    ret

; scroll just enough to keep the cursor line on screen
terminal_ensure_cursor_visible:
    push rax
    push rbx
    push rcx

    movzx eax, word [document_cursor_line]
    movzx ebx, word [document_view_top]
    mov ecx, ebx
    add ecx, [view_rows]
    cmp eax, ebx
    jb .move_up
    cmp eax, ecx
    jae .move_down
    jmp .done
.move_up:
    mov [document_view_top], ax
    jmp .done
.move_down:
    sub eax, [view_rows]
    inc eax
    mov [document_view_top], ax
.done:
    mov byte [document_dirty], 1
    pop rcx
    pop rbx
    pop rax
    ret

terminal_putc:
    cmp al, 10
    je terminal_newline
    cmp al, 13
    je terminal_carriage_return
    cmp al, 9
    je terminal_tab
    cmp al, 8
    je document_delete_backward
    cmp al, 12
    je document_reset
    cmp al, ' '
    jb .ignored

    call log_char

    push rax
    push rdx
    push r12
    push r13
    push r14

    mov r14b, al
    movzx r12d, word [document_cursor_line]
    mov eax, r12d
    call document_ensure_line
    movzx r13d, word [document_cursor_column]
    cmp r13d, DOCUMENT_COLUMNS
    jb .column_ready
    call terminal_newline
    movzx r12d, word [document_cursor_line]
    xor r13d, r13d
.column_ready:
    mov eax, r12d
    call document_line_address
    mov [rax + r13 * 2], r14b
    call document_attribute
    mov [rax + r13 * 2 + 1], dl

    lea eax, [r13 + 1]
    cmp al, [document_line_lengths + r12]
    jbe .advance
    mov [document_line_lengths + r12], al
.advance:
    mov byte [document_dirty], 1
    cmp r13d, DOCUMENT_COLUMNS - 1
    je .wrap
    inc r13d
    mov [document_cursor_column], r13w
    jmp .restore
.wrap:
    call terminal_newline
.restore:
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rax
.ignored:
    ret

terminal_carriage_return:
    mov word [document_cursor_column], 0
    mov byte [document_dirty], 1
    ret

terminal_newline:
    push rax
    push r12

    mov al, 10
    call log_char

    mov word [document_cursor_column], 0
    movzx r12d, word [document_cursor_line]
    inc r12d
    cmp r12d, DOCUMENT_LINES
    jb .normal
    call document_drop_first_line
    mov r12d, DOCUMENT_LINES - 1
    mov [document_cursor_line], r12w
    jmp .visible
.normal:
    mov [document_cursor_line], r12w
    mov eax, r12d
    call document_ensure_line
.visible:
    call terminal_ensure_cursor_visible

    pop r12
    pop rax
    ret

terminal_tab:
    push rax
    push rcx
    push rdx
    movzx ecx, word [document_cursor_column]
    and ecx, 3
    mov edx, 4
    sub edx, ecx
.next:
    mov al, ' '
    call terminal_putc
    dec edx
    jnz .next
    pop rdx
    pop rcx
    pop rax
    ret

terminal_print:
    push rsi
.next:
    lodsb
    test al, al
    jz .done
    call terminal_putc
    jmp .next
.done:
    pop rsi
    ret

terminal_print_line:
    call terminal_print
    jmp terminal_newline

terminal_set_foreground:
    and al, 0x0F
    mov [terminal_foreground], al
    ret

; typing path. inserts at the cursor and pushes the rest of the line right, what falls off
; the end is lost
document_insert_char:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r15b, al
    movzx r12d, word [document_cursor_line]
    mov eax, r12d
    call document_ensure_line
    call document_clamp_cursor

    movzx r13d, word [document_cursor_column]
    cmp r13d, DOCUMENT_COLUMNS
    jae .done

    movzx r14d, byte [document_line_lengths + r12]
    mov eax, r12d
    call document_line_address
    mov rbx, rax
    call document_attribute

    cmp r13d, r14d
    jbe .shift_check
    mov ecx, r13d
    sub ecx, r14d
    lea rdi, [rbx + r14 * 2]
    mov al, ' '
    mov ah, dl
    cld
    rep stosw

.shift_check:
    cmp r14d, DOCUMENT_COLUMNS
    jae .write
    cmp r13d, r14d
    jae .extend
    mov ecx, r14d
    sub ecx, r13d
    lea rsi, [rbx + r14 * 2 - 2]
    lea rdi, [rsi + 2]
    std                         ; overlapping move to the right has to go backwards
    rep movsw
    cld
    inc r14d
    jmp .write
.extend:
    lea r14d, [r13 + 1]

.write:
    mov [rbx + r13 * 2], r15b
    mov [rbx + r13 * 2 + 1], dl
    cmp r14d, DOCUMENT_COLUMNS
    jbe .length_ready
    mov r14d, DOCUMENT_COLUMNS
.length_ready:
    mov [document_line_lengths + r12], r14b
    mov byte [document_dirty], 1

    cmp r13d, DOCUMENT_COLUMNS - 1
    je .wrap
    inc r13d
    mov [document_cursor_column], r13w
    call terminal_ensure_cursor_visible
    jmp .done
.wrap:
    call terminal_newline
    movzx eax, word [document_cursor_line]
    mov byte [document_continued + rax], 1
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

document_delete_backward:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    movzx r12d, word [document_cursor_line]
    movzx r13d, word [document_cursor_column]
    movzx edx, byte [document_protected_lengths + r12]
    cmp r13d, edx
    jb .done
    ja .same_line
    test edx, edx
    jnz .done
    test r13d, r13d
    jnz .same_line
    test r12d, r12d
    jz .done

    dec r12d
    mov [document_cursor_line], r12w
    movzx r13d, byte [document_line_lengths + r12]
    cmp r13d, DOCUMENT_COLUMNS
    jb .previous_column_ready
    mov r13d, DOCUMENT_COLUMNS - 1
.previous_column_ready:
    mov [document_cursor_column], r13w
    call terminal_ensure_cursor_visible
    jmp .done

.same_line:
    dec r13d
    movzx r14d, byte [document_line_lengths + r12]
    cmp r13d, r14d
    jb .delete_character
    mov [document_cursor_column], r13w
    jmp .done
.delete_character:
    call document_remove_char_in_line
    mov [document_cursor_column], r13w
.done:
    mov byte [document_dirty], 1
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

document_remove_char_in_line:
    mov eax, r12d
    call document_line_address
    mov rbx, rax
    mov ecx, r14d
    sub ecx, r13d
    dec ecx
    jz .clear_tail
    lea rdi, [rbx + r13 * 2]
    lea rsi, [rdi + 2]
    cld
    rep movsw
.clear_tail:
    dec r14d
    mov byte [rbx + r14 * 2], ' '
    call document_attribute
    mov [rbx + r14 * 2 + 1], dl
    mov [document_line_lengths + r12], r14b
    mov byte [document_dirty], 1
    ret

document_delete_at_cursor:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    movzx r12d, word [document_cursor_line]
    movzx r13d, word [document_cursor_column]
    movzx r14d, byte [document_line_lengths + r12]
    movzx edx, byte [document_protected_lengths + r12]
    cmp r13d, edx
    jb .done
    cmp r13d, r14d
    jae .done
    call document_remove_char_in_line
.done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

document_move_left:
    movzx eax, word [document_cursor_line]
    movzx edx, byte [document_protected_lengths + rax]
    cmp word [document_cursor_column], dx
    ja .same_line
    test edx, edx
    jnz .protected_boundary
    cmp word [document_cursor_column], 0
    jne .protected_boundary
    cmp word [document_cursor_line], 0
    je .finish
    dec word [document_cursor_line]
    movzx eax, word [document_cursor_line]
    movzx eax, byte [document_line_lengths + rax]
    cmp eax, DOCUMENT_COLUMNS
    jb .store_previous
    mov eax, DOCUMENT_COLUMNS - 1
.store_previous:
    mov [document_cursor_column], ax
    call document_clamp_cursor
    jmp terminal_ensure_cursor_visible
.same_line:
    dec word [document_cursor_column]
    jmp terminal_ensure_cursor_visible
.protected_boundary:
    mov [document_cursor_column], dx
    jmp terminal_ensure_cursor_visible
.finish:
    ret

document_move_right:
    call document_clamp_cursor
    cmp word [document_cursor_column], DOCUMENT_COLUMNS - 1
    jb .same_line
    cmp word [document_cursor_line], DOCUMENT_LINES - 1
    jae .finish
    inc word [document_cursor_line]
    mov word [document_cursor_column], 0
    movzx eax, word [document_cursor_line]
    call document_ensure_line
    call document_clamp_cursor
    jmp terminal_ensure_cursor_visible
.same_line:
    inc word [document_cursor_column]
    jmp terminal_ensure_cursor_visible
.finish:
    ret

document_move_up:
    cmp word [document_cursor_line], 0
    je .finish
    dec word [document_cursor_line]
    call document_clamp_cursor
    jmp terminal_ensure_cursor_visible
.finish:
    ret

document_move_down:
    movzx eax, word [document_cursor_line]
    cmp eax, DOCUMENT_LINES - 1
    jae .finish
    inc eax
    mov [document_cursor_line], ax
    call document_ensure_line
    call document_clamp_cursor
    jmp terminal_ensure_cursor_visible
.finish:
    ret

document_move_home:
    movzx eax, word [document_cursor_line]
    movzx eax, byte [document_protected_lengths + rax]
    mov [document_cursor_column], ax
    mov byte [document_dirty], 1
    ret

document_move_end:
    movzx eax, word [document_cursor_line]
    movzx ecx, byte [document_line_lengths + rax]
    movzx edx, byte [document_protected_lengths + rax]
    cmp ecx, edx
    jae .length_ready
    mov ecx, edx
.length_ready:
    cmp ecx, DOCUMENT_COLUMNS - 1
    jbe .store
    mov ecx, DOCUMENT_COLUMNS - 1
.store:
    mov [document_cursor_column], cx
    mov byte [document_dirty], 1
    ret

document_page_up:
    movzx eax, word [document_cursor_line]
    cmp eax, [view_rows]
    jb .first
    sub eax, [view_rows]
    jmp .store
.first:
    xor eax, eax
.store:
    mov [document_cursor_line], ax
    jmp terminal_ensure_cursor_visible

document_page_down:
    movzx eax, word [document_cursor_line]
    add eax, [view_rows]
    cmp eax, DOCUMENT_LINES
    jb .store
    mov eax, DOCUMENT_LINES - 1
.store:
    mov [document_cursor_line], ax
    call document_ensure_line
    jmp terminal_ensure_cursor_visible

document_set_cursor_screen:
    cmp eax, DOCUMENT_COLUMNS
    jb .column_ready
    mov eax, DOCUMENT_COLUMNS - 1
.column_ready:
    cmp ebx, [view_rows]
    jb .row_ready
    mov ebx, [view_rows]
    dec ebx
.row_ready:
    movzx ecx, word [document_view_top]
    add ecx, ebx
    cmp ecx, DOCUMENT_LINES
    jb .line_ready
    mov ecx, DOCUMENT_LINES - 1
.line_ready:
    mov [document_cursor_line], cx
    mov [document_cursor_column], ax
    mov eax, ecx
    call document_ensure_line
    call document_clamp_cursor
    mov byte [document_dirty], 1
    ret

document_clear_command:
    push rax
    push rcx
    push rdx
    push rdi
    push r12
    push r13
    push r14

    call document_clamp_cursor
    call document_command_start
    mov r13d, eax
    movzx r12d, word [document_cursor_line]
    movzx r14d, byte [document_line_lengths + r12]
    cmp r13d, r14d
    ja .done

    mov eax, r12d
    call document_line_address
    lea rdi, [rax + r13 * 2]
    mov ecx, r14d
    sub ecx, r13d
    call document_attribute
    mov al, ' '
    mov ah, dl
    cld
    rep stosw
    mov [document_line_lengths + r12], r13b
    cmp r13d, DOCUMENT_COLUMNS
    jb .column_ready
    mov r13d, DOCUMENT_COLUMNS - 1
.column_ready:
    mov [document_cursor_column], r13w
    mov byte [document_dirty], 1
.done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

document_replace_command:
    push rax
    push r14
    push r15
    mov r15, rsi
    call document_clear_command
    xor r14d, r14d
.copy:
    cmp r14d, DOCUMENT_COLUMNS
    jae .done
    mov al, [r15 + r14]
    test al, al
    jz .done
    call document_insert_char
    inc r14d
    jmp .copy
.done:
    pop r15
    pop r14
    pop rax
    ret

; cursor at the end of the last line, so output never lands in the middle of old text
terminal_move_to_document_end:
    push rax
    push r12
    movzx r12d, word [document_line_count]
    test r12d, r12d
    jnz .count_ready
    mov r12d, 1
    mov word [document_line_count], 1
.count_ready:
    dec r12d
    mov [document_cursor_line], r12w
    movzx eax, byte [document_line_lengths + r12]
    cmp eax, DOCUMENT_COLUMNS
    jb .column_ready
    mov word [document_cursor_column], DOCUMENT_COLUMNS - 1
    call terminal_newline
    jmp .done
.column_ready:
    mov [document_cursor_column], ax
    call terminal_ensure_cursor_visible
.done:
    pop r12
    pop rax
    ret

document_clamp_cursor:
    movzx eax, word [document_cursor_line]
    cmp eax, DOCUMENT_LINES
    jae .done
    movzx edx, byte [document_protected_lengths + rax]
    cmp word [document_cursor_column], dx
    jae .done
    mov [document_cursor_column], dx
.done:
    ret

document_mark_prompt:
    movzx eax, word [document_cursor_line]
    movzx edx, word [document_cursor_column]
    cmp edx, DOCUMENT_COLUMNS
    jb .store
    mov edx, DOCUMENT_COLUMNS
.store:
    mov [document_protected_lengths + rax], dl
    ret

document_command_start:
    movzx eax, word [document_cursor_line]
    cmp eax, DOCUMENT_LINES
    jb .read
    xor eax, eax
    ret
.read:
    movzx eax, byte [document_protected_lengths + rax]
    ret

LOG_BUFFER_BYTES = 128

log_char:
    push rcx
    mov ecx, [log_used]
    mov [log_buffer + rcx], al
    inc ecx
    mov [log_used], ecx
    cmp al, 10
    je .flush
    cmp ecx, LOG_BUFFER_BYTES
    jb .done
.flush:
    call log_flush
.done:
    pop rcx
    ret

log_flush:
    push rax
    push rcx
    push rdx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    mov esi, [log_used]
    test esi, esi
    jz .done
    lea rdi, [log_buffer]
    call sys_log
    mov dword [log_used], 0
.done:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret
