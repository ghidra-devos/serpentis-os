; drawing. everything goes into cells (char/attr like text mode) first, flush_cells sends
; only the cells that differ from the last frame

MAX_COLUMNS = 80
MAX_ROWS = 30
CELL_BYTES = MAX_COLUMNS * MAX_ROWS * 2
SCROLL_MARGIN = 3               ; like vim scrolloff=3

ATTR_TEXT = 0x07
ATTR_IDENTIFIER = 0x0F
ATTR_COMMENT = 0x08
ATTR_STRING = 0x0A
ATTR_NUMBER = 0x06
ATTR_LABEL = 0x09
ATTR_INSTRUCTION = 0x0D
ATTR_REGISTER = 0x0B
ATTR_DIRECTIVE = 0x0E
ATTR_GUTTER = 0x08
ATTR_GUTTER_CURRENT = 0x0E
ATTR_TABS = 0x87
ATTR_TAB_ACTIVE = 0x0F
ATTR_TAB_DIRTY = 0x0E
ATTR_STATUS = 0x8F
ATTR_MESSAGE = 0x07
ATTR_ERROR = 0x0C
ATTR_SELECTION = 0x1F
ATTR_MATCH = 0x60
ATTR_CURSOR_NORMAL = 0xF0
ATTR_CURSOR_INSERT = 0xB0
ATTR_BRACKET = 0x5F
ATTR_PROJECT = 0x8F
ATTR_TAB_CLOSE = 0x08
ATTR_TAB_EDGE = 0x80
ATTR_INDENT_GUIDE = 0x08

; screen minus the tab bar, status and command line
text_rows:
    mov eax, [screen_rows]
    sub eax, 3
    jg .done
    mov eax, 1
.done:
    ret

gutter_width:
    xor eax, eax
    cmp byte [show_numbers], 0
    je .done
    call line_count
    mov ecx, 1
    mov edx, 10
.digits:
    cmp eax, edx
    jb .counted
    inc ecx
    imul edx, 10
    jmp .digits
.counted:
    lea eax, [rcx + 2]
    cmp eax, 4
    jae .done
    mov eax, 4
.done:
    add eax, [panel_width]
    ret

cell_put:
    cmp edi, [screen_columns]
    jae .skip
    cmp esi, [screen_rows]
    jae .skip
    cmp edi, MAX_COLUMNS
    jae .skip
    cmp esi, MAX_ROWS
    jae .skip
    imul eax, esi, MAX_COLUMNS
    add eax, edi
    mov [cells + rax * 2], dl
    mov [cells + rax * 2 + 1], cl
.skip:
    ret

cell_text:
    push rbx
    mov rbx, rdx
.next:
    mov dl, [rbx]
    test dl, dl
    jz .done
    call cell_put
    inc rbx
    inc edi
    jmp .next
.done:
    pop rbx
    ret

cell_fill_row:
    push rbx
    mov ebx, [screen_columns]
    xor edi, edi
.next:
    cmp edi, ebx
    jae .done
    mov dl, ' '
    call cell_put
    inc edi
    jmp .next
.done:
    pop rbx
    ret

scroll_to_cursor:
    mov edi, [cursor]
    call line_of
    mov [cursor_line], eax
    call text_rows
    mov ecx, eax
    mov edx, SCROLL_MARGIN
    mov r8d, ecx
    shr r8d, 1
    cmp edx, r8d
    jbe .margin_ready
    mov edx, r8d
.margin_ready:
    mov eax, [cursor_line]
    sub eax, edx
    jns .above
    xor eax, eax
.above:
    cmp eax, [top_line]
    jae .below
    mov [top_line], eax
.below:
    mov eax, [cursor_line]
    add eax, edx
    sub eax, ecx
    inc eax
    js .horizontal
    cmp eax, [top_line]
    jbe .horizontal
    mov [top_line], eax

.horizontal:
    mov edi, [cursor]
    call column_of
    mov [cursor_column], eax
    call gutter_width
    mov ecx, [screen_columns]
    sub ecx, eax
    dec ecx
    jg .width_ready
    mov ecx, 1
.width_ready:
    mov eax, [cursor_column]
    cmp eax, [left_column]
    jae .right
    mov [left_column], eax
    ret
.right:
    mov edx, [left_column]
    add edx, ecx
    cmp eax, edx
    jb .done
    sub eax, ecx
    inc eax
    mov [left_column], eax
.done:
    ret

render:
    push rbx
    push r12
    push r13
    push r14
    push r15

    call panel_layout
    call scroll_to_cursor
    call find_bracket_match
    call compute_selection

    mov esi, 0
    mov cl, ATTR_TEXT
.clear_rows:
    cmp esi, [screen_rows]
    jae .cleared
    call cell_fill_row
    inc esi
    jmp .clear_rows
.cleared:

    call render_tabs
    call sync_title
    call gutter_width
    mov [gutter], eax

    mov edi, [top_line]
    call line_offset
    mov r12d, eax
    ; the view starts mid file, so the C highlighter first has to find out if top_line is inside
    ; a /* comment
    cmp byte [syntax_enabled], 2
    jne .scanned
    mov edi, r12d
    call c_block_before
.scanned:
    mov r13d, [top_line]
    xor r14d, r14d
.line:
    call text_rows
    cmp r14d, eax
    jae .chrome
    lea esi, [r14 + 1]
    cmp r12d, [text_size]
    jbe .has_line
    jmp .chrome
.has_line:
    mov edi, r12d
    mov edx, r13d
    call render_line
    mov edi, r12d
    call next_line_start
    jc .chrome
    mov r12d, eax
    inc r13d
    inc r14d
    jmp .line

.chrome:
    call render_scrollbar
    call render_panel
    call render_finder
    call render_status
    call render_command_line
    call flush_cells

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

render_tabs:
    xor esi, esi
    mov cl, ATTR_TABS
    call cell_fill_row
    xor edi, edi
    cmp dword [panel_width], 0
    je .strip
    lea rsi, [tree_root]
    call base_name
    mov rdx, rax
    mov edi, 1
    xor esi, esi
    mov cl, ATTR_PROJECT
    call cell_text
    mov edi, [panel_width]
.strip:
    call render_tab_strip
    mov ebx, eax
    lea rdx, [brand_text]
    mov cl, ATTR_TABS
.brand:
    push rcx
    push rdx
    mov rdi, rdx
    call str_len
    pop rdx
    pop rcx
    mov edi, [screen_columns]
    sub edi, eax
    dec edi
    cmp edi, ebx
    jl .done
    xor esi, esi
    call cell_text
.done:
    ret

; the window title follows the file in the active tab
sync_title:
    call file_name
    mov rsi, rax
    lea rdi, [title_shown]
    call str_equal
    test eax, eax
    jnz .same
    call file_name
    mov rsi, rax
    lea rdi, [title_shown]
    call str_copy
    lea rdi, [title_shown]
    jmp screen_set_title
.same:
    ret

file_name:
    lea rax, [file_title]
    cmp byte [file_path], 0
    je .done
    lea rsi, [file_path]
    jmp base_name
.done:
    ret
render_line:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, edi
    mov r13d, esi
    mov r15d, edx

    cmp byte [show_numbers], 0
    je .text
    mov eax, r15d
    cmp byte [relative_numbers], 0
    je .absolute
    sub eax, [cursor_line]      ; relative numbers, the cursor line keeps its real number
    jz .absolute
    jns .number_ready
    neg eax
    jmp .number_ready
.absolute:
    mov eax, r15d
    inc eax
.number_ready:
    mov edi, eax
    call u64_to_dec
    mov rbx, rax
    mov rdi, rax
    call str_len
    mov edi, [gutter]
    sub edi, eax
    dec edi
    mov esi, r13d
    mov cl, ATTR_GUTTER
    cmp r15d, [cursor_line]
    jne .number_color
    mov cl, ATTR_GUTTER_CURRENT
.number_color:
    mov rdx, rbx
    call cell_text

.text:
    mov dword [token_end], 0
    mov dword [syntax_state], 0
    cmp byte [syntax_enabled], 2
    jne .state_ready
    call c_line_state
.state_ready:
    mov dword [token_class], ATTR_TEXT
    mov dword [line_first_word], 1
    mov byte [line_leading], 1
    mov r14d, r12d
    xor ebx, ebx
.char:
    mov edi, r14d
    cmp edi, [text_size]
    jae .eol
    movzx eax, byte [text + rdi]
    cmp al, 10
    je .eol

    call classify_char
    mov r15d, eax

    call selection_contains
    jnc .not_selected
    mov r15d, ATTR_SELECTION
.not_selected:
    call match_contains
    jnc .not_match
    mov r15d, ATTR_MATCH
.not_match:
    cmp r14d, [bracket_match]
    jne .not_bracket
    mov r15d, ATTR_BRACKET
.not_bracket:
    cmp r14d, [cursor]
    jne .place
    call cursor_attribute
    mov r15d, eax

.place:
    mov ecx, r15d
    mov eax, ebx
    sub eax, [left_column]
    jb .next
    add eax, [gutter]
    mov edi, eax
    mov esi, r13d
    mov dl, [text + r14]
    cmp dl, ' '
    je .blank
    mov byte [line_leading], 0
    cmp dl, 9
    jne .visible
    mov dl, ' '
    jmp .visible
.blank:
    cmp byte [line_leading], 0
    je .visible
    test ebx, INDENT - 1
    jnz .visible
    cmp r15d, ATTR_TEXT
    jne .visible
    mov dl, 0xB3                ; cp437 vertical line
    mov cl, ATTR_INDENT_GUIDE
.visible:
    call cell_put
.next:
    inc r14d
    inc ebx
    jmp .char

.eol:
    cmp r14d, [cursor]
    jne .done
    call cursor_attribute
    mov ecx, eax
    mov eax, ebx
    sub eax, [left_column]
    jb .done
    add eax, [gutter]
    mov edi, eax
    mov esi, r13d
    mov dl, ' '
    call cell_put
.done:
    cmp byte [syntax_enabled], 2
    jne .finished
    call c_line_finished
.finished:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

cursor_attribute:
    mov eax, ATTR_CURSOR_NORMAL
    cmp byte [mode], MODE_INSERT
    jne .done
    mov eax, ATTR_CURSOR_INSERT
.done:
    ret

; tree button, mode, file path, [+], then Ln/Col and the file kind on the right. when it
; gets too long the path turns into just the name, then the kind goes away, then the name
render_status:
    push rbx
    push r12
    push r13
    mov esi, [screen_rows]
    sub esi, 2
    mov cl, ATTR_STATUS
    call cell_fill_row

    mov cl, ATTR_BUTTON
    cmp byte [panel_open], 0
    je .button
    mov cl, ATTR_BUTTON_ON
.button:
    xor edi, edi
    mov dl, ' '
    call cell_put
    inc edi
    mov dl, 0xF0                ; cp437 three bars, the tree button
    call cell_put
    inc edi
    mov dl, ' '
    call cell_put
    inc edi
    inc edi

    movzx eax, byte [mode]
    mov rdx, [mode_names + rax * 8]
    mov cl, [mode_colors + rax]
    push rdx
    mov dl, ' '
    call cell_put
    pop rdx
    inc edi
    call cell_text
    mov dl, ' '
    call cell_put
    inc edi
    inc edi

    mov ebx, edi
    lea rdi, [status_right]
    lea rsi, [line_label]
    call str_copy
    mov edi, [cursor_line]
    inc edi
    call status_append_number
    lea rsi, [column_label]
    call status_append
    mov edi, [cursor_column]
    inc edi
    call status_append_number
    lea rdi, [status_right]
    call str_len
    mov r12d, eax
    mov rdi, [file_kind_name]
    call str_len
    lea r13d, [rax + 3]

    lea rdx, [file_title]
    cmp byte [file_path], 0
    je .full
    lea rdx, [file_path]
.full:
    call status_fits
    jbe .chosen
    call file_name
    mov rdx, rax
    call status_fits
    jbe .chosen
    xor r13d, r13d
    call status_fits
    jbe .chosen
    xor edx, edx

.chosen:
    test r13d, r13d
    jz .draw
    push rdx
    lea rsi, [status_gap]
    call status_append
    mov rsi, [file_kind_name]
    call status_append
    pop rdx
.draw:
    mov esi, [screen_rows]
    sub esi, 2
    test rdx, rdx
    jz .right
    mov edi, ebx
    mov cl, ATTR_STATUS
    call cell_text
    cmp byte [dirty], 0
    je .right
    lea rdx, [modified_text]
    call cell_text
.right:
    lea rdi, [status_right]
    call str_len
    mov edi, [screen_columns]
    sub edi, eax
    dec edi
    js .done
    mov esi, [screen_rows]
    sub esi, 2
    lea rdx, [status_right]
    mov cl, ATTR_STATUS
    call cell_text
.done:
    pop r13
    pop r12
    pop rbx
    ret

status_fits:
    push rdx
    mov rdi, rdx
    call str_len
    pop rdx
    add eax, ebx
    add eax, r12d
    add eax, r13d
    add eax, 2
    cmp byte [dirty], 0
    je .compare
    add eax, modified_text_end - modified_text - 1
.compare:
    cmp eax, [screen_columns]
    ret

status_append_number:
    call u64_to_dec
    mov rsi, rax

status_append:
    push rsi
    lea rdi, [status_right]
    call str_len
    pop rsi
    lea rdi, [status_right + rax]
    jmp str_copy

render_command_line:
    mov esi, [screen_rows]
    dec esi
    mov cl, ATTR_MESSAGE
    call cell_fill_row
    xor edi, edi
    cmp byte [mode], MODE_COMMAND
    je .prompt
    cmp byte [mode], MODE_SEARCH
    je .prompt
    mov rdx, [message]
    test rdx, rdx
    jz .done
    mov cl, ATTR_MESSAGE
    cmp byte [message_error], 0
    je .message
    mov cl, ATTR_ERROR
.message:
    jmp cell_text
.prompt:
    mov rdx, [prompt_label]
    mov cl, ATTR_TEXT
    call cell_text
    cmp byte [prompt_selected], 0
    je .typed
    mov cl, ATTR_SELECTION
.typed:
    lea rdx, [command_line]
    call cell_text
    mov dl, ' '
    mov cl, ATTR_CURSOR_NORMAL
    jmp cell_put
.done:
    ret

flush_cells:
    push rbx
    push r12
    push r13
    xor r12d, r12d
    xor r13d, r13d
.row:
    cmp r13d, [screen_rows]
    jae .present
    cmp r13d, MAX_ROWS
    jae .present
    xor r12d, r12d
.column:
    cmp r12d, [screen_columns]
    jae .next_row
    cmp r12d, MAX_COLUMNS
    jae .next_row
    imul ebx, r13d, MAX_COLUMNS
    add ebx, r12d
    mov ax, [cells + rbx * 2]
    cmp ax, [shadow + rbx * 2]
    je .next_column
    mov [shadow + rbx * 2], ax
    mov byte [frame_changed], 1
    mov edi, r12d
    mov esi, r13d
    mov dl, al
    mov cl, ah
    call screen_put
.next_column:
    inc r12d
    jmp .column
.next_row:
    inc r13d
    jmp .row
.present:
    cmp byte [frame_changed], 0
    je .done
    mov byte [frame_changed], 0
    call screen_present
.done:
    pop r13
    pop r12
    pop rbx
    ret

invalidate_screen:
    lea rdi, [shadow]
    mov esi, 0xFF
    mov edx, CELL_BYTES
    jmp mem_set

brand_text db 'SERPENTIS EDIT', 0
modified_text db ' [+]', 0
modified_text_end:
line_label db 'Ln ', 0
column_label db ', Col ', 0
status_gap db '   ', 0
file_title db '[No Name]', 0
