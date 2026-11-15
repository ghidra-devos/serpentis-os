; sets X Y TILE_W TILE_H of every tiled window from zorder, then starts the moves and tells
; the apps their new sizes. zorder[0] is the master
relayout_windows:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov ebx, [z_count]
    test ebx, ebx
    jz .done
    cmp ebx, 1
    je .one
    cmp ebx, GRID_FROM
    jae .grid
    mov eax, [current_workspace]
    movzx eax, byte [layout_modes + rax]
    cmp eax, LAYOUT_COLUMNS
    je .columns
    cmp eax, LAYOUT_ROWS
    je .rows
    cmp ebx, 2
    je .two

    ; master gets 3/5 of the width, the others split the height. the remainder of the division
    ; goes one pixel each to the first ones so the column ends exactly at the bottom
    mov edi, [zorder]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X
    mov dword [r12 + WIN_Y], WORK_Y
    mov dword [r12 + WIN_TILE_W], ((WORK_W - GAP) * 3) / 5
    mov dword [r12 + WIN_TILE_H], WORK_H

    mov r13d, ebx
    dec r13d
    mov eax, WORK_H
    mov ecx, r13d
    dec ecx
    imul ecx, GAP
    sub eax, ecx
    xor edx, edx
    div r13d
    mov r14d, eax
    mov r15d, edx
    mov r11d, WORK_Y
    mov r10d, 1

.stack:
    cmp r10d, ebx
    jae .done
    mov edi, [zorder + r10 * 4]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X + ((WORK_W - GAP) * 3) / 5 + GAP
    mov [r12 + WIN_Y], r11d
    mov dword [r12 + WIN_TILE_W], WORK_W - ((WORK_W - GAP) * 3) / 5 - GAP
    mov eax, r14d
    cmp r10d, r15d
    ja .height_ready
    inc eax
.height_ready:
    mov [r12 + WIN_TILE_H], eax
    add r11d, eax
    add r11d, GAP
    inc r10d
    jmp .stack

.columns:
    lea ecx, [rbx - 1]
    imul ecx, GAP
    mov eax, WORK_W
    sub eax, ecx
    xor edx, edx
    div ebx
    mov r14d, eax
    mov r15d, edx
    mov r11d, WORK_X
    xor r10d, r10d
.column:
    cmp r10d, ebx
    jae .done
    mov edi, [zorder + r10 * 4]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov [r12 + WIN_X], r11d
    mov dword [r12 + WIN_Y], WORK_Y
    mov eax, r14d
    cmp r10d, r15d
    jae .column_ready
    inc eax
.column_ready:
    mov [r12 + WIN_TILE_W], eax
    mov dword [r12 + WIN_TILE_H], WORK_H
    add r11d, eax
    add r11d, GAP
    inc r10d
    jmp .column

.rows:
    lea ecx, [rbx - 1]
    imul ecx, GAP
    mov eax, WORK_H
    sub eax, ecx
    xor edx, edx
    div ebx
    mov r14d, eax
    mov r15d, edx
    mov r11d, WORK_Y
    xor r10d, r10d
.row:
    cmp r10d, ebx
    jae .done
    mov edi, [zorder + r10 * 4]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X
    mov [r12 + WIN_Y], r11d
    mov eax, r14d
    cmp r10d, r15d
    jae .row_ready
    inc eax
.row_ready:
    mov dword [r12 + WIN_TILE_W], WORK_W
    mov [r12 + WIN_TILE_H], eax
    add r11d, eax
    add r11d, GAP
    inc r10d
    jmp .row

.grid:
    mov r13d, 1
.columns_count:
    mov eax, r13d
    imul eax, eax
    cmp eax, ebx
    jae .rows_count
    inc r13d
    jmp .columns_count
.rows_count:
    lea eax, [rbx + r13 - 1]
    xor edx, edx
    div r13d
    mov r14d, eax
    lea ecx, [rax - 1]
    imul ecx, GAP
    mov eax, WORK_H
    sub eax, ecx
    xor edx, edx
    div r14d
    mov r15d, eax
    xor r10d, r10d
    mov r11d, WORK_Y
.grid_row:
    cmp r10d, ebx
    jae .done
    mov ecx, ebx
    sub ecx, r10d
    cmp ecx, r13d
    jbe .row_width
    mov ecx, r13d
.row_width:
    push rcx
    lea eax, [rcx - 1]
    imul eax, GAP
    mov edx, WORK_W
    sub edx, eax
    mov eax, edx
    xor edx, edx
    div ecx
    mov r9d, eax
    pop rcx
    mov r14d, WORK_X
.grid_cell:
    test ecx, ecx
    jz .grid_next_row
    mov edi, [zorder + r10 * 4]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov [r12 + WIN_X], r14d
    mov [r12 + WIN_Y], r11d
    mov [r12 + WIN_TILE_W], r9d
    mov [r12 + WIN_TILE_H], r15d
    add r14d, r9d
    add r14d, GAP
    inc r10d
    dec ecx
    jmp .grid_cell
.grid_next_row:
    add r11d, r15d
    add r11d, GAP
    jmp .grid_row

.one:
    mov edi, [zorder]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X
    mov dword [r12 + WIN_Y], WORK_Y
    mov dword [r12 + WIN_TILE_W], WORK_W
    mov dword [r12 + WIN_TILE_H], WORK_H
    jmp .done

.two:
    mov edi, [zorder]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X
    mov dword [r12 + WIN_Y], WORK_Y
    mov dword [r12 + WIN_TILE_W], (WORK_W - GAP) / 2
    mov dword [r12 + WIN_TILE_H], WORK_H
    mov edi, [zorder + 4]
    imul eax, edi, WIN_SIZE
    lea r12, [windows + rax]
    mov dword [r12 + WIN_X], WORK_X + (WORK_W - GAP) / 2 + GAP
    mov dword [r12 + WIN_Y], WORK_Y
    mov dword [r12 + WIN_TILE_W], WORK_W - (WORK_W - GAP) / 2 - GAP
    mov dword [r12 + WIN_TILE_H], WORK_H

.done:
    call animate_retarget
    call announce_sizes
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

announce_sizes:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, WINDOW_MAX
    jae .done
    imul eax, ebx, WIN_SIZE
    lea r8, [windows + rax]
    cmp dword [r8 + WIN_USED], 0
    je .skip
    mov edx, [r8 + WIN_TILE_W]
    sub edx, 2
    mov ecx, [r8 + WIN_TILE_H]
    sub ecx, TITLE_H + 1
    jle .skip
    cmp edx, [r8 + WIN_SENT_W]
    jne .announce
    cmp ecx, [r8 + WIN_SENT_H]
    je .skip
.announce:
    mov [r8 + WIN_SENT_W], edx
    mov [r8 + WIN_SENT_H], ecx
    mov edi, ebx
    mov esi, GUI_RESIZE
    xor r8d, r8d
    call send_to_window
.skip:
    inc ebx
    jmp .next
.done:
    pop rbx
    ret
