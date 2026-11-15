; arrows or hjkl, enter/l opens, backspace/h goes up, r or f5 reloads, d/del asks first
handle_key:
    mov qword [message], 0
    cmp byte [confirming], 0
    jne .confirm
    mov eax, edi
    cmp eax, 'q'
    je .quit
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 'j'
    je .down
    cmp eax, KEY_UP
    je .up
    cmp eax, 'k'
    je .up
    cmp eax, KEY_HOME
    je .first
    cmp eax, 'g'
    je .first
    cmp eax, KEY_END
    je .last
    cmp eax, 'G'
    je .last
    cmp eax, 10
    je open_selected
    cmp eax, 'l'
    je open_selected
    cmp eax, KEY_RIGHT
    je open_selected
    cmp eax, 8
    je go_parent
    cmp eax, 'h'
    je go_parent
    cmp eax, KEY_LEFT
    je go_parent
    cmp eax, 'r'
    je load_directory
    cmp eax, KEY_F1 + 4
    je load_directory
    cmp eax, KEY_DELETE
    je .ask_delete
    cmp eax, 'd'
    je .ask_delete
    ret
.quit:
    mov byte [running], 0
    ret
.down:
    mov eax, [selected]
    inc eax
    cmp eax, [entry_count]
    jae .done
    mov [selected], eax
    ret
.up:
    cmp dword [selected], 0
    je .done
    dec dword [selected]
    ret
.first:
    mov dword [selected], 0
    ret
.last:
    mov eax, [entry_count]
    dec eax
    js .done
    mov [selected], eax
    ret
.ask_delete:
    cmp dword [entry_count], 0
    je .done
    mov byte [confirming], 1
    lea rsi, [confirm_text]
    jmp set_message
.confirm:
    mov byte [confirming], 0
    or edi, 0x20
    cmp edi, 'y'
    je delete_selected
.done:
    ret

handle_click:
    push rbx
    push r12
    mov ebx, edi
    mov r12d, esi
    cmp r12d, TOOLBAR_H
    jae .body
    cmp ebx, 26
    jae .done
    call go_parent
    jmp .done
.body:
    mov eax, [height]
    sub eax, STATUS_H
    cmp r12d, eax
    jae .done
    cmp ebx, SIDEBAR_W
    jae .list
    mov eax, r12d
    sub eax, TOOLBAR_H + 6
    js .done
    xor edx, edx
    mov ecx, ROW_H
    div ecx
    cmp eax, PLACE_COUNT
    jae .done
    mov rsi, [place_paths + rax * 8]
    call enter_path
    jmp .done
.list:
    mov eax, r12d
    sub eax, TOOLBAR_H + 2
    js .done
    xor edx, edx
    mov ecx, ROW_H
    div ecx
    add eax, [top]
    cmp eax, [entry_count]
    jae .done
    mov ecx, eax
    call sys_getticks
    cmp ecx, [selected]
    jne .select
    mov rdx, rax
    sub rdx, [click_tick]
    cmp rdx, DOUBLE_CLICK_TICKS
    ja .select
    mov [click_tick], rax
    call open_selected
    jmp .done
.select:
    mov [selected], ecx
    mov [click_tick], rax
.done:
    pop r12
    pop rbx
    ret

keep_visible:
    call visible_rows
    mov ecx, [selected]
    cmp ecx, [top]
    jae .below
    mov [top], ecx
    ret
.below:
    mov edx, [top]
    add edx, eax
    cmp ecx, edx
    jb .done
    sub ecx, eax
    inc ecx
    mov [top], ecx
.done:
    ret
