; lds, the terminal. a window with a text document in it, the shell types into the same
; document. pid 1, the kernel starts it again if it ever exits

SERP_STACK = 0x8000

include '../runtime/program.inc'

WINDOW_W = DOCUMENT_COLUMNS * 8 + SCREEN_PAD_X * 2
WINDOW_H = 24 * 16 + SCREEN_PAD_Y * 2

main:
    push rbx

    lea rdi, [window_title]
    mov esi, WINDOW_W
    mov edx, WINDOW_H
    call screen_open

    call sys_tty_open
    test eax, eax
    js .failed
    mov [shell_tty], eax

    mov byte [terminal_foreground], COLOR_WHITE
    mov byte [terminal_background], COLOR_BLACK
    call document_reset
    call fit_view
    call shell_start
    mov byte [running], 1

.loop:
    call log_flush
    call render
    cmp byte [running], 0
    je .exit

    ; no child: block until something happens. child running: also wake on the timer so its
    ; output keeps flowing even when it never fills the tty
    xor esi, esi
    cmp dword [child_pid], 0
    je .wait
    mov esi, EVENT_WANT_TICK
.wait:
    call screen_wait
    call handle_event
.drain:
    mov esi, SYS_GETKEY_NOWAIT
    call screen_wait
    test eax, eax
    jz .loop
    call handle_event
    jmp .drain

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

.failed:
    call screen_close
    lea rdi, [failed_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

fit_view:
    mov eax, [screen_columns]
    cmp eax, DOCUMENT_COLUMNS
    jbe .columns
    mov eax, DOCUMENT_COLUMNS
.columns:
    mov [view_columns], eax
    mov eax, [screen_rows]
    test eax, eax
    jnz .rows
    inc eax
.rows:
    mov [view_rows], eax
    call terminal_ensure_cursor_visible
    mov byte [full_redraw], 1
    ret

handle_event:
    test eax, eax
    jz .tick
    cmp eax, SCREEN_TTY
    je shell_child_output
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_CLICK
    je handle_click
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .close
    ret
.key:
    mov edi, [screen_key]
    jmp handle_key
.tick:
    cmp dword [child_pid], 0
    je .idle
    jmp shell_child_output
.idle:
    ret
.resize:
    call fit_view
    call render
    jmp screen_acknowledge_resize
.close:
    mov byte [running], 0
    ret

handle_click:
    cmp dword [child_pid], 0
    jne .done
    mov eax, [screen_click_column]
    cmp eax, [view_columns]
    jae .done
    mov ebx, [screen_click_row]
    cmp ebx, [view_rows]
    jae .done
    jmp document_set_cursor_screen
.done:
    ret

handle_key:
    cmp dword [child_pid], 0
    je .editing
    push rdi
    call shell_child_output
    pop rdi
    cmp dword [child_pid], 0
    je .editing
    cmp edi, KEY_INTERRUPT
    jne .forward
    mov edi, [child_pid]
    call sys_kill
    lea rsi, [interrupt_text]
    jmp terminal_print
.forward:
    mov eax, [typeahead_count]
    cmp eax, TYPEAHEAD_MAX
    jae .send
    mov [typeahead_keys + rax * 4], edi
    inc dword [typeahead_count]
.send:
    mov esi, edi
    mov edi, [shell_tty]
    jmp sys_tty_input

.editing:
    mov eax, edi
    cmp eax, 10
    je shell_input_enter
    cmp eax, KEY_SHIFT_ENTER
    je terminal_newline
    cmp eax, 8
    je .backspace
    cmp eax, 9
    je .tab
    cmp eax, 27
    je .cancel
    cmp eax, KEY_F1
    je .help
    cmp eax, KEY_UP or KEY_CTRL
    je shell_history_previous
    cmp eax, KEY_DOWN or KEY_CTRL
    je shell_history_next
    cmp eax, KEY_LEFT
    je document_move_left
    cmp eax, KEY_RIGHT
    je document_move_right
    cmp eax, KEY_UP
    je document_move_up
    cmp eax, KEY_DOWN
    je document_move_down
    cmp eax, KEY_HOME
    je document_move_home
    cmp eax, KEY_END
    je document_move_end
    cmp eax, KEY_PAGE_UP
    je document_page_up
    cmp eax, KEY_PAGE_DOWN
    je document_page_down
    cmp eax, KEY_DELETE
    je document_delete_at_cursor
    cmp eax, ' '
    jb .done
    cmp eax, 126
    ja .done
    call document_insert_char
    jmp .reset_history

.backspace:
    call document_delete_backward
    jmp .reset_history

.tab:
    mov ebx, 4                  ; no tab stops in the document, just spaces
.tab_next:
    mov al, ' '
    call document_insert_char
    dec ebx
    jnz .tab_next
    ret

.cancel:
    call document_clear_command
    jmp .reset_history

.help:
    call terminal_move_to_document_end
    call terminal_newline
    lea rdi, [help_command]
    lea rsi, [capture_buffer]
    mov edx, CAPTURE_MAX
    call sys_kcommand
    test eax, eax
    js .help_prompt
    mov edi, eax
    call shell_insert_capture
.help_prompt:
    jmp shell_prompt

.reset_history:
    mov al, [history_count]
    mov [history_nav], al
.done:
    ret

render:
    cmp byte [full_redraw], 0
    jne .draw
    cmp byte [document_dirty], 0
    je .done
.draw:
    push rbx
    push r12
    push r13
    push r14

    cmp byte [full_redraw], 0
    je .rows
    call screen_clear

.rows:
    xor r12d, r12d
.row:
    cmp r12d, [view_rows]
    jae .cursor
    movzx r13d, word [document_view_top]
    add r13d, r12d
    xor r14d, r14d
.cell:
    cmp r14d, [view_columns]
    jae .next_row
    mov dl, ' '
    mov cl, COLOR_WHITE
    movzx eax, word [document_line_count]
    cmp r13d, eax
    jae .draw_cell
    mov eax, r13d
    call document_line_address
    mov dl, [rax + r14 * 2]
    mov cl, [rax + r14 * 2 + 1]
.draw_cell:
    mov edi, r14d
    mov esi, r12d
    call screen_put
    inc r14d
    jmp .cell
.next_row:
    inc r12d
    jmp .row

.cursor:
    cmp dword [child_pid], 0
    jne .present
    movzx r12d, word [document_cursor_line]
    movzx eax, word [document_view_top]
    sub r12d, eax
    js .present
    cmp r12d, [view_rows]
    jae .present
    movzx r14d, word [document_cursor_column]
    cmp r14d, [view_columns]
    jae .present
    mov dl, ' '
    mov cl, COLOR_WHITE
    movzx r13d, word [document_cursor_line]
    movzx eax, word [document_line_count]
    cmp r13d, eax
    jae .swap
    mov eax, r13d
    call document_line_address
    mov dl, [rax + r14 * 2]
    mov cl, [rax + r14 * 2 + 1]
; cursor is the cell with fg and bg swapped. if they come out the same (a blank on black)
; it would vanish, so that case gets a fixed color
.swap:
    rol cl, 4
    mov al, cl
    shr al, 4
    mov ah, cl
    and ah, 0x0F
    cmp al, ah
    jne .cursor_draw
    mov cl, (COLOR_BLUE shl 4) or COLOR_YELLOW
.cursor_draw:
    mov edi, r14d
    mov esi, r12d
    call screen_put

.present:
    mov byte [document_dirty], 0
    mov byte [full_redraw], 0
    call screen_present

    pop r14
    pop r13
    pop r12
    pop rbx
.done:
    ret

window_title db 'Terminal', 0
failed_text db 'lds: cannot start', 0
help_command db 'help', 0
interrupt_text db '^C', 0

include 'document.inc'
include 'shell.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'
include '../runtime/users_bss.inc'

align 8
child_pid rd 1
shell_uid rd 1
shell_prompt_user rb 64
report_exit rb 1
shell_tty rd 1
view_rows rd 1
view_columns rd 1
document_line_count rw 1
document_cursor_line rw 1
document_cursor_column rw 1
document_view_top rw 1
terminal_foreground rb 1
terminal_background rb 1
document_dirty rb 1
full_redraw rb 1
running rb 1
history_count rb 1
history_nav rb 1
log_used rd 1
stage_count rd 1
stage_next rd 1
stage_list rq STAGES_MAX
stage_pipe rd 2
streams_open rb 1
pending_read rd 1
pipeline_used rd 1
pipeline_pids rd PIPELINE_MAX
output_append rb 1
log_buffer:
    rb LOG_BUFFER_BYTES
command_buffer:
    rb COMMAND_MAX
pipeline_buffer:
    rb COMMAND_MAX
stage_command:
    rb COMMAND_MAX
input_path:
    rb PATH_LIMIT
output_path:
    rb PATH_LIMIT
command_word:
    rb 64
shell_cwd:
    rb PATH_LIMIT
ansi_value rd 1
typeahead_count rd 1
typeahead_keys rd TYPEAHEAD_MAX
replay_keys rd TYPEAHEAD_MAX
ansi_state rb 1
spawn_path:
    rb PATH_LIMIT + 16
history_buffer:
    rb HISTORY_ENTRIES * COMMAND_MAX
document_line_lengths:
    rb DOCUMENT_LINES
document_protected_lengths:     ; per line, how many columns belong to the prompt and can't be edited
    rb DOCUMENT_LINES
document_continued:
    rb DOCUMENT_LINES
align 16
document_buffer:
    rb DOCUMENT_LINES * DOCUMENT_LINE_BYTES
capture_buffer:
    rb CAPTURE_MAX
