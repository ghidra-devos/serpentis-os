SCROLL_STEP = 3
ATTR_SCROLL_TRACK = 0x08
ATTR_SCROLL_THUMB = 0x07
SCROLL_TRACK_CHAR = 0xDB
SCROLL_THUMB_CHAR = 0xDB

scroll_limit:
    call line_count
    mov ecx, eax
    call text_rows
    sub ecx, eax
    jns .done
    xor ecx, ecx
.done:
    mov eax, ecx
    ret

scroll_lines:
    push rbx
    mov ebx, edi
    call scroll_limit
    mov ecx, [top_line]
    cmp eax, ecx
    jge .limit
    mov eax, ecx
.limit:
    add ecx, ebx
    jns .low
    xor ecx, ecx
.low:
    cmp ecx, eax
    jle .high
    mov ecx, eax
.high:
    mov [top_line], ecx
    call keep_cursor_visible
    pop rbx
    ret

keep_cursor_visible:
    push rbx
    push r12
    push r13
    call text_rows
    mov r12d, eax
    mov ebx, SCROLL_MARGIN
    shr eax, 1
    cmp ebx, eax
    jbe .margin
    mov ebx, eax
.margin:
    mov edi, [cursor]
    call line_of
    mov r13d, eax
    mov eax, [top_line]
    test eax, eax
    jz .high
    add eax, ebx
    cmp r13d, eax
    jae .high
    mov r13d, eax
    jmp .move
.high:
    call scroll_limit
    cmp [top_line], eax
    jae .done
    mov eax, [top_line]
    add eax, r12d
    dec eax
    sub eax, ebx
    cmp r13d, eax
    jbe .done
    mov r13d, eax
.move:
    call line_count
    cmp r13d, eax
    jb .inside
    lea r13d, [rax - 1]
.inside:
    mov edi, r13d
    call line_offset
    mov edi, eax
    mov esi, [want_column]
    call place_on_line
    mov [cursor], eax
    cmp byte [mode], MODE_INSERT
    je .done
    call clamp_normal_cursor
.done:
    pop r13
    pop r12
    pop rbx
    ret

scrollbar_column:
    mov eax, [screen_columns]
    cmp eax, MAX_COLUMNS
    jbe .fits
    mov eax, MAX_COLUMNS
.fits:
    dec eax
    ret

; scrolled past the end counts as more lines, so the thumb doesn't jump when you get there
scroll_total:
    call line_count
    mov ecx, [top_line]
    add ecx, [scroll_rows]
    cmp eax, ecx
    jae .done
    mov eax, ecx
.done:
    ret

scrollbar_shown:
    call text_rows
    mov [scroll_rows], eax
    call line_count
    cmp eax, [scroll_rows]
    ja .shown
    cmp dword [top_line], 0
    jne .shown
    clc
    ret
.shown:
    stc
    ret

render_scrollbar:
    push rbx
    push r12
    push r13
    call scrollbar_shown
    jnc .done
    call scroll_total
    mov ecx, eax
    mov eax, [scroll_rows]
    imul eax, eax
    xor edx, edx
    div ecx
    cmp eax, 2
    jae .sized
    mov eax, 2
.sized:
    mov r12d, eax
    mov eax, [top_line]
    imul eax, [scroll_rows]
    xor edx, edx
    div ecx
    mov r13d, eax
    add eax, r12d
    sub eax, [scroll_rows]
    jle .placed
    sub r13d, eax
.placed:
    add r12d, r13d
    xor ebx, ebx
.row:
    cmp ebx, [scroll_rows]
    jae .done
    call scrollbar_column
    mov edi, eax
    lea esi, [rbx + 1]
    mov dl, SCROLL_TRACK_CHAR
    mov cl, ATTR_SCROLL_TRACK
    cmp ebx, r13d
    jb .put
    cmp ebx, r12d
    jae .put
    mov dl, SCROLL_THUMB_CHAR
    mov cl, ATTR_SCROLL_THUMB
.put:
    call cell_put
    inc ebx
    jmp .row
.done:
    pop r13
    pop r12
    pop rbx
    ret

scrollbar_click:
    call scrollbar_shown
    jnc .miss
    call scrollbar_column
    cmp [screen_click_column], eax
    jne .miss
    mov eax, [screen_click_row]
    test eax, eax
    jz .miss
    cmp eax, [scroll_rows]
    ja .miss
    call scroll_total
    mov ecx, [screen_click_y]
    sub ecx, FONT_HEIGHT_PIXELS
    jns .track
    xor ecx, ecx
.track:
    imul rax, rcx
    mov ecx, [scroll_rows]
    shl ecx, 4
    xor edx, edx
    div rcx
    mov ecx, [scroll_rows]
    shr ecx, 1
    sub eax, ecx
    sub eax, [top_line]
    mov edi, eax
    call scroll_lines
    stc
    ret
.miss:
    clc
    ret
