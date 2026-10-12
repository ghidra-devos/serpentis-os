; ctrl+p, a fuzzy file finder like in vscode. walks the whole tree under the cwd once when
; it opens, then filters the list on every key

FINDER_MAX = 256
FINDER_PATH = 96
FINDER_QUERY_MAX = 48
FINDER_WIDTH = 60
FINDER_ROWS = 10
KEY_CTRL_N = 14
KEY_CTRL_P = 16

ATTR_FINDER = 0x07
ATTR_FINDER_BORDER = 0x08
ATTR_FINDER_NAME = 0x0F
ATTR_FINDER_DIRECTORY = 0x08
ATTR_FINDER_SELECTED = 0x1F
ATTR_FINDER_PROMPT = 0x0B

finder_open:
    lea rdi, [tree_root]
    mov esi, PATH_BYTES
    call sys_getcwd
    lea rdi, [walk_path]
    lea rsi, [tree_root]
    call str_copy
    lea rdi, [walk_path]
    call str_len
    cmp byte [walk_path + rax - 1], '/'
    je .relative
    inc eax
.relative:
    mov [walk_relative], eax
    mov dword [finder_count], 0
    call finder_walk
    mov dword [finder_length], 0
    mov byte [finder_query], 0
    mov dword [finder_selected], 0
    mov dword [finder_top], 0
    call finder_filter
    mov al, [mode]
    mov [finder_return], al
    mov byte [mode], MODE_FINDER
    ret

finder_walk:
    push rbx
    push r12
    push r13
    lea rdi, [walk_path]
    call str_len
    mov r13d, eax
    lea rdi, [walk_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .out
    mov r12d, eax
.entry:
    mov edi, r12d
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .closed
    lea rdi, [walk_path + r13]
    cmp byte [rdi - 1], '/'
    je .append
    mov byte [rdi], '/'
    inc rdi
.append:
    lea rsi, [dirent + SYS_DIRENT_NAME]
    call str_copy
    test dword [dirent + SYS_DIRENT_FLAGS], FS_FLAG_DIRECTORY
    jz .file
    call finder_walk
    jmp .restore
.file:
    mov ebx, [finder_count]
    cmp ebx, FINDER_MAX
    jae .restore
    imul edi, ebx, FINDER_PATH
    lea rdi, [finder_paths + rdi]
    mov esi, [walk_relative]
    lea rsi, [walk_path + rsi]
    mov ecx, FINDER_PATH - 1
.copy:
    mov al, [rsi]
    test al, al
    jz .copied
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .copy
.copied:
    mov byte [rdi], 0
    inc dword [finder_count]
.restore:
    mov byte [walk_path + r13], 0
    jmp .entry
.closed:
    mov edi, r12d
    call sys_close
.out:
    pop r13
    pop r12
    pop rbx
    ret

; two passes: names that start with the query first, then any path that has the query
; letters in order
finder_filter:
    push rbx
    xor r8d, r8d
    mov bl, 1
.pass:
    xor ecx, ecx
.path:
    cmp ecx, [finder_count]
    jae .next_pass
    imul esi, ecx, FINDER_PATH
    lea rsi, [finder_paths + rsi]
    push rcx
    call base_name
    pop rcx
    xor edx, edx
    call finder_prefix
    cmp al, bl
    jne .next
    imul esi, ecx, FINDER_PATH
    lea rsi, [finder_paths + rsi]
    xor edx, edx
.char:
    cmp edx, [finder_length]
    jae .matched
    mov al, [rsi]
    test al, al
    jz .next
    inc rsi
    or al, 0x20
    mov ah, [finder_query + rdx]
    or ah, 0x20
    cmp al, ah
    jne .char
    inc edx
    jmp .char
.matched:
    mov [finder_matches + r8 * 4], ecx
    inc r8d
.next:
    inc ecx
    jmp .path
.next_pass:
    test bl, bl
    jz .done
    xor bl, bl
    jmp .pass
.done:
    mov [finder_match_count], r8d
    mov eax, [finder_selected]
    cmp eax, r8d
    jb .ready
    mov dword [finder_selected], 0
.ready:
    pop rbx
    ret

; rax -> name, edx = 0. al = 1 if the name starts with the query
finder_prefix:
    cmp edx, [finder_length]
    jae .yes
    mov r9b, [rax + rdx]
    or r9b, 0x20
    mov r10b, [finder_query + rdx]
    or r10b, 0x20
    cmp r9b, r10b
    jne .no
    inc edx
    jmp finder_prefix
.yes:
    mov al, 1
    ret
.no:
    xor eax, eax
    ret

finder_key:
    mov eax, [key]
    cmp eax, KEY_ESCAPE
    je .close
    cmp eax, 10
    je .open
    cmp eax, KEY_DOWN
    je .down
    cmp eax, KEY_CTRL_N
    je .down
    cmp eax, KEY_UP
    je .up
    cmp eax, KEY_CTRL_P
    je .up
    cmp eax, 8
    je .erase
    cmp eax, ' '
    jb .done
    cmp eax, 126
    ja .done
    mov ecx, [finder_length]
    cmp ecx, FINDER_QUERY_MAX
    jae .done
    mov [finder_query + rcx], al
    inc ecx
    mov [finder_length], ecx
    mov byte [finder_query + rcx], 0
    mov dword [finder_selected], 0
    jmp finder_filter
.erase:
    mov ecx, [finder_length]
    test ecx, ecx
    jz .done
    dec ecx
    mov [finder_length], ecx
    mov byte [finder_query + rcx], 0
    jmp finder_filter
.down:
    mov eax, [finder_selected]
    inc eax
    cmp eax, [finder_match_count]
    jae .done
    mov [finder_selected], eax
    ret
.up:
    cmp dword [finder_selected], 0
    je .done
    dec dword [finder_selected]
    ret
.close:
    mov al, [finder_return]
    mov [mode], al
    ret
.open:
    cmp dword [finder_match_count], 0
    je .done
    mov byte [mode], MODE_NORMAL
    mov eax, [finder_selected]
    mov eax, [finder_matches + rax * 4]
    imul eax, eax, FINDER_PATH
    push rax
    lea rdi, [path_buffer]
    lea rsi, [walk_path]
    mov ecx, [walk_relative]
    rep movsb
    mov ecx, [walk_relative]
    mov byte [path_buffer + rcx - 1], '/'
    pop rax
    lea rsi, [finder_paths + rax]
    lea rdi, [path_buffer + rcx]
    call str_copy
    lea rsi, [path_buffer]
    jmp open_path
.done:
    ret

; box in the middle of the text area: query line, then the matches, name bright and the
; directory part dim after it
render_finder:
    cmp byte [mode], MODE_FINDER
    jne .none
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12d, [screen_columns]
    sub r12d, 4
    cmp r12d, FINDER_WIDTH
    jbe .width
    mov r12d, FINDER_WIDTH
.width:
    mov r13d, [screen_columns]
    sub r13d, r12d
    shr r13d, 1
    call text_rows
    sub eax, 4
    cmp eax, FINDER_ROWS
    jbe .rows
    mov eax, FINDER_ROWS
.rows:
    mov r14d, eax

    mov ecx, [finder_selected]
    cmp ecx, [finder_top]
    jae .below
    mov [finder_top], ecx
.below:
    mov edx, [finder_top]
    add edx, r14d
    cmp ecx, edx
    jb .scrolled
    sub ecx, r14d
    inc ecx
    mov [finder_top], ecx
.scrolled:

    mov esi, 2
    mov dl, 0xDA
    mov al, 0xC4
    mov ah, 0xBF
    call finder_border
    mov esi, 3
    call finder_blank_row
    lea edi, [r13 + 2]
    mov dl, '>'
    mov cl, ATTR_FINDER_PROMPT
    call cell_put
    add edi, 2
    lea rdx, [finder_query]
    mov cl, ATTR_FINDER_NAME
    call cell_text
    mov dl, ' '
    mov cl, ATTR_CURSOR_NORMAL
    call cell_put
    mov esi, 4
    mov dl, 0xC3
    mov al, 0xC4
    mov ah, 0xB4
    call finder_border

    xor r15d, r15d
.item:
    cmp r15d, r14d
    jae .bottom
    lea esi, [r15 + 5]
    call finder_blank_row
    mov ebx, [finder_top]
    add ebx, r15d
    cmp ebx, [finder_match_count]
    jae .next_item
    mov bl, ATTR_FINDER
    mov eax, [finder_top]
    add eax, r15d
    cmp eax, [finder_selected]
    jne .paint
    mov bl, ATTR_FINDER_SELECTED
    push rsi
    lea edi, [r13 + 1]
.highlight:
    lea eax, [r13 + r12 - 1]
    cmp edi, eax
    jae .highlighted
    mov dl, ' '
    mov cl, bl
    call cell_put
    inc edi
    jmp .highlight
.highlighted:
    pop rsi
.paint:
    mov eax, [finder_top]
    add eax, r15d
    mov eax, [finder_matches + rax * 4]
    imul eax, eax, FINDER_PATH
    lea rsi, [finder_paths + rax]
    push rsi
    call base_name
    mov rdx, rax
    lea edi, [r13 + 2]
    lea esi, [r15 + 5]
    mov cl, ATTR_FINDER_NAME
    push rax
    call finder_text
    pop rcx
    pop rdx
    cmp rcx, rdx
    je .next_item
    mov al, [rcx - 1]
    mov byte [rcx - 1], 0
    push rcx
    push rax
    inc edi
    mov cl, ATTR_FINDER_DIRECTORY
    call finder_text
    pop rax
    pop rcx
    mov [rcx - 1], al
.next_item:
    inc r15d
    jmp .item

.bottom:
    lea esi, [r14 + 5]
    mov dl, 0xC0
    mov al, 0xC4
    mov ah, 0xD9
    call finder_border

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
.none:
    ret

finder_text:
    cmp bl, ATTR_FINDER
    je .limit
    and cl, 0x0F
    or cl, ATTR_FINDER_SELECTED and 0xF0
.limit:
    lea eax, [r13 + r12 - 2]
.next:
    cmp edi, eax
    jae .done
    mov r8b, [rdx]
    test r8b, r8b
    jz .done
    push rdx
    push rax
    mov dl, r8b
    call cell_put
    pop rax
    pop rdx
    inc rdx
    inc edi
    jmp .next
.done:
    ret

finder_border:
    mov edi, r13d
    mov cl, ATTR_FINDER_BORDER
    push rax
    call cell_put
    pop rax
    inc edi
.line:
    lea ecx, [r13 + r12 - 1]
    cmp edi, ecx
    jae .corner
    push rax
    mov dl, al
    mov cl, ATTR_FINDER_BORDER
    call cell_put
    pop rax
    inc edi
    jmp .line
.corner:
    mov dl, ah
    mov cl, ATTR_FINDER_BORDER
    jmp cell_put

finder_blank_row:
    mov dl, 0xB3
    mov al, ' '
    mov ah, 0xB3
    mov edi, r13d
    mov cl, ATTR_FINDER_BORDER
    push rax
    call cell_put
    pop rax
    inc edi
.line:
    lea ecx, [r13 + r12 - 1]
    cmp edi, ecx
    jae .edge
    push rax
    mov dl, ' '
    mov cl, ATTR_FINDER
    call cell_put
    pop rax
    inc edi
    jmp .line
.edge:
    mov dl, ah
    mov cl, ATTR_FINDER_BORDER
    jmp cell_put
