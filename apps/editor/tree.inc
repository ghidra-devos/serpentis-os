; the file tree on the left. it is a flat list in screen order, each node has a depth.
; expanding a directory inserts its children right after it, collapsing cuts them out again,
; so drawing is just a loop and the path of a node is found by walking back up the depths

NODE_MAX = 256
NODE_BYTES = 64
NODE_DEPTH = 0
NODE_FLAGS = 1
NODE_NAME = 2
NODE_NAME_MAX = NODE_BYTES - NODE_NAME - 1
CHILD_MAX = 96
DEPTH_MAX = 16
PANEL_COLUMNS = 26
BUTTON_COLUMNS = 3
PATH_BYTES = 256

NODE_DIRECTORY = 1
NODE_EXPANDED = 2

ATTR_PANEL = 0x07
ATTR_PANEL_DIRECTORY = 0x09
ATTR_PANEL_ICON = 0x03
ATTR_PANEL_CURRENT = 0x0E
ATTR_PANEL_SELECTED = 0x1F
ATTR_PANEL_CURSOR = 0x8F
ATTR_PANEL_EDGE = 0x08
ATTR_BUTTON = 0x87
ATTR_BUTTON_ON = 0x3F

node_address:
    imul eax, eax, NODE_BYTES
    lea rax, [nodes + rax]
    ret

panel_layout:
    xor eax, eax
    cmp byte [panel_open], 0
    je .store
    mov eax, [screen_columns]
    xor edx, edx
    mov ecx, 3
    div ecx
    cmp eax, PANEL_COLUMNS
    jbe .store
    mov eax, PANEL_COLUMNS
.store:
    mov [panel_width], eax
    ret

; ctrl+b. the first time it reads the cwd, after that the tree stays as it was
panel_toggle:
    cmp byte [panel_open], 0
    jne .close
    mov byte [panel_open], 1
    cmp dword [node_count], 0
    jne .focus
    call tree_load
.focus:
    call panel_reveal
    mov byte [mode], MODE_FILES
    ret
.close:
    mov byte [panel_open], 0
    cmp byte [mode], MODE_FILES
    jne .done
    mov byte [mode], MODE_NORMAL
.done:
    ret

tree_load:
    lea rdi, [tree_root]
    mov esi, PATH_BYTES
    call sys_getcwd
    mov dword [node_count], 0
    mov dword [panel_selected], 0
    mov dword [panel_top], 0
    mov edi, -1
    jmp tree_expand

node_path:
    push rbx
    push r12
    push r13
    mov ebx, edi
    lea rdi, [path_buffer]
    lea rsi, [tree_root]
    call str_copy
    cmp ebx, -1
    je .done

    xor r12d, r12d
    mov eax, ebx
    call node_address
    movzx r13d, byte [rax + NODE_DEPTH]
    mov [path_chain], ebx
    inc r12d
    mov ecx, ebx
.ancestor:
    test r13d, r13d
    jz .join
    dec ecx
    js .join
    mov eax, ecx
    call node_address
    movzx edx, byte [rax + NODE_DEPTH]
    lea eax, [r13 - 1]
    cmp edx, eax
    jne .ancestor
    mov [path_chain + r12 * 4], ecx
    inc r12d
    dec r13d
    jmp .ancestor

.join:
    dec r12d
    js .done
    lea rdi, [path_buffer]
    call str_len
    lea rdi, [path_buffer + rax]
    cmp byte [rdi - 1], '/'
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    mov eax, [path_chain + r12 * 4]
    call node_address
    lea rsi, [rax + NODE_NAME]
    call str_copy
    jmp .join
.done:
    pop r13
    pop r12
    pop rbx
    ret

tree_expand:
    push rbx
    push r12
    push r13
    push r14
    mov ebx, edi
    call node_path
    xor r13d, r13d
    cmp ebx, -1
    je .open
    mov eax, ebx
    call node_address
    or byte [rax + NODE_FLAGS], NODE_EXPANDED
    movzx r13d, byte [rax + NODE_DEPTH]
    inc r13d
    cmp r13d, DEPTH_MAX
    jae .out
.open:
    lea rdi, [path_buffer]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .out
    mov r12d, eax
    xor r14d, r14d
.entry:
    cmp r14d, CHILD_MAX
    jae .closed
    mov edi, r12d
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .closed
    imul edi, r14d, NODE_BYTES
    lea rdi, [children + rdi]
    mov [rdi + NODE_DEPTH], r13b
    mov byte [rdi + NODE_FLAGS], 0
    test dword [dirent + SYS_DIRENT_FLAGS], FS_FLAG_DIRECTORY
    jz .named
    mov byte [rdi + NODE_FLAGS], NODE_DIRECTORY
.named:
    add rdi, NODE_NAME
    lea rsi, [dirent + SYS_DIRENT_NAME]
    mov ecx, NODE_NAME_MAX
.copy:
    mov al, [rsi]
    test al, al
    jz .terminated
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .copy
.terminated:
    mov byte [rdi], 0
    inc r14d
    jmp .entry

.closed:
    mov edi, r12d
    call sys_close
    mov edi, r14d
    call sort_children

    mov eax, NODE_MAX
    sub eax, [node_count]
    cmp r14d, eax
    jbe .fits
    mov r14d, eax
.fits:
    lea r12d, [rbx + 1]
    mov eax, r12d
    call node_address
    mov rsi, rax
    imul edi, r14d, NODE_BYTES
    add rdi, rax
    mov ecx, [node_count]
    sub ecx, r12d
    imul ecx, ecx, NODE_BYTES
    call text_move
    mov eax, r12d
    call node_address
    mov rdi, rax
    lea rsi, [children]
    imul ecx, r14d, NODE_BYTES
    call text_move
    add [node_count], r14d
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

tree_collapse:
    push rbx
    mov ebx, edi
    mov eax, ebx
    call node_address
    and byte [rax + NODE_FLAGS], not NODE_EXPANDED
    movzx edx, byte [rax + NODE_DEPTH]
    lea ecx, [rbx + 1]
.scan:
    cmp ecx, [node_count]
    jae .remove
    mov eax, ecx
    call node_address
    cmp [rax + NODE_DEPTH], dl
    jbe .remove
    inc ecx
    jmp .scan
.remove:
    push rcx
    mov eax, ecx
    call node_address
    mov rsi, rax
    lea eax, [rbx + 1]
    call node_address
    mov rdi, rax
    pop rdx
    mov ecx, [node_count]
    sub ecx, edx
    imul ecx, ecx, NODE_BYTES
    call text_move
    sub edx, ebx
    dec edx
    sub [node_count], edx
    mov eax, [node_count]
    cmp [panel_selected], eax
    jb .done
    dec eax
    mov [panel_selected], eax
.done:
    pop rbx
    ret

sort_children:
    push rbx
    push r12
    mov r12d, edi
    mov ebx, 1
.item:
    cmp ebx, r12d
    jae .done
    imul esi, ebx, NODE_BYTES
    lea rsi, [children + rsi]
    lea rdi, [node_swap]
    mov ecx, NODE_BYTES
    rep movsb
    mov edx, ebx
.shift:
    test edx, edx
    jz .place
    imul esi, edx, NODE_BYTES
    lea rsi, [children + rsi - NODE_BYTES]
    lea rdi, [node_swap]
    call node_before
    jnc .place
    imul esi, edx, NODE_BYTES
    lea rsi, [children + rsi - NODE_BYTES]
    lea rdi, [rsi + NODE_BYTES]
    mov ecx, NODE_BYTES
    rep movsb
    dec edx
    jmp .shift
.place:
    imul edi, edx, NODE_BYTES
    lea rdi, [children + rdi]
    lea rsi, [node_swap]
    mov ecx, NODE_BYTES
    rep movsb
    inc ebx
    jmp .item
.done:
    pop r12
    pop rbx
    ret

node_before:
    xor ecx, ecx
    mov al, [rdi + NODE_FLAGS]
    xor al, [rsi + NODE_FLAGS]
    test al, NODE_DIRECTORY
    jz .names
    test byte [rdi + NODE_FLAGS], NODE_DIRECTORY
    jnz .yes
    jmp .no
.names:
    mov al, [rdi + NODE_NAME + rcx]
    mov ah, [rsi + NODE_NAME + rcx]
    or al, 0x20
    or ah, 0x20
    cmp al, ah
    jb .yes
    ja .no
    cmp byte [rdi + NODE_NAME + rcx], 0
    je .no
    inc ecx
    jmp .names
.yes:
    stc
    ret
.no:
    clc
    ret

panel_activate:
    push rbx
    mov ebx, edi
    mov [panel_selected], ebx
    mov eax, ebx
    call node_address
    test byte [rax + NODE_FLAGS], NODE_DIRECTORY
    jz .file
    test byte [rax + NODE_FLAGS], NODE_EXPANDED
    jnz .collapse
    mov edi, ebx
    call tree_expand
    jmp .done
.collapse:
    mov edi, ebx
    call tree_collapse
    jmp .done
.file:
    mov edi, ebx
    call node_path
    lea rsi, [path_buffer]
    call open_path
    mov byte [mode], MODE_NORMAL
.done:
    pop rbx
    ret

; h: an open directory closes, anything else jumps to its parent like nerdtree
panel_parent:
    mov eax, [panel_selected]
    call node_address
    movzx edx, byte [rax + NODE_DEPTH]
    test byte [rax + NODE_FLAGS], NODE_EXPANDED
    jz .search
    mov edi, [panel_selected]
    jmp tree_collapse
.search:
    test edx, edx
    jz .done
    dec edx
    mov ecx, [panel_selected]
.back:
    dec ecx
    js .done
    mov eax, ecx
    call node_address
    cmp [rax + NODE_DEPTH], dl
    jne .back
    mov [panel_selected], ecx
.done:
    ret

files_key:
    mov eax, [key]
    cmp eax, KEY_ESCAPE
    je .leave
    cmp eax, KEY_CTRL_B
    je panel_toggle
    cmp eax, KEY_CTRL_C
    je panel_toggle
    cmp eax, KEY_CTRL_S
    je save_file
    cmp eax, KEY_CTRL_Q
    je ex_quit_all
    cmp eax, KEY_CTRL_T
    je new_buffer
    cmp eax, KEY_NEXT_TAB
    je next_buffer
    cmp eax, KEY_NEXT_PAGE_TAB
    je next_buffer
    cmp eax, KEY_PREVIOUS_PAGE_TAB
    je previous_buffer
    cmp eax, 'q'
    je panel_toggle
    cmp eax, ':'
    je command_colon
    cmp eax, KEY_CTRL_P
    je finder_open
    cmp eax, 'a'
    je panel_ask_new_file
    cmp eax, 'A'
    je panel_ask_new_directory
    cmp dword [node_count], 0
    je .done
    cmp eax, 'r'
    je panel_ask_rename
    cmp eax, 'd'
    je panel_ask_delete
    cmp eax, 'j'
    je .down
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 'k'
    je .up
    cmp eax, KEY_UP
    je .up
    cmp eax, 'g'
    je .first
    cmp eax, 'G'
    je .last
    cmp eax, 'h'
    je panel_parent
    cmp eax, KEY_LEFT
    je panel_parent
    cmp eax, 'R'
    je tree_load
    cmp eax, 10
    je .open
    cmp eax, 'l'
    je .open
    cmp eax, 'o'
    je .open
    cmp eax, KEY_RIGHT
    je .open
.done:
    ret
.leave:
    mov byte [mode], MODE_NORMAL
    ret
.down:
    mov eax, [panel_selected]
    inc eax
    cmp eax, [node_count]
    jae .done
    mov [panel_selected], eax
    ret
.up:
    cmp dword [panel_selected], 0
    je .done
    dec dword [panel_selected]
    ret
.first:
    mov dword [panel_selected], 0
    ret
.last:
    mov eax, [node_count]
    dec eax
    mov [panel_selected], eax
    ret
.open:
    mov edi, [panel_selected]
    jmp panel_activate

panel_reveal:
    push rbx
    xor ebx, ebx
.ancestor:
    cmp ebx, [node_count]
    jae .search
    mov eax, ebx
    call node_address
    test byte [rax + NODE_FLAGS], NODE_DIRECTORY
    jz .next_ancestor
    test byte [rax + NODE_FLAGS], NODE_EXPANDED
    jnz .next_ancestor
    mov edi, ebx
    call node_path
    lea rsi, [path_buffer]
    lea rdi, [file_path]
.prefix:
    mov al, [rsi]
    test al, al
    jz .boundary
    cmp al, [rdi]
    jne .next_ancestor
    inc rsi
    inc rdi
    jmp .prefix
; file_path must start with the dir path and continue with a /, else /a would match /ab
.boundary:
    cmp byte [rdi], '/'
    jne .next_ancestor
    mov edi, ebx
    call tree_expand
.next_ancestor:
    inc ebx
    jmp .ancestor

.search:
    xor ebx, ebx
.node:
    cmp ebx, [node_count]
    jae .done
    mov edi, ebx
    call node_path
    lea rdi, [path_buffer]
    lea rsi, [file_path]
    call str_equal
    test eax, eax
    jnz .found
    inc ebx
    jmp .node
.found:
    mov [panel_selected], ebx
.done:
    pop rbx
    ret

panel_parent_of:
    mov eax, edi
    call node_address
    movzx edx, byte [rax + NODE_DEPTH]
    mov eax, -1
    test edx, edx
    jz .done
    dec edx
    mov ecx, edi
.back:
    dec ecx
    js .root
    mov eax, ecx
    call node_address
    cmp [rax + NODE_DEPTH], dl
    mov eax, ecx
    jne .back
.done:
    ret
.root:
    mov eax, -1
    ret

panel_choose_target:
    mov eax, -1
    cmp dword [node_count], 0
    je .store
    mov edi, [panel_selected]
    mov eax, edi
    call node_address
    test byte [rax + NODE_FLAGS], NODE_DIRECTORY
    mov eax, edi
    jnz .store
    call panel_parent_of
.store:
    mov [panel_target], eax
    ret

panel_target_path:
    mov edi, [panel_target]
    call node_path
    lea rdi, [scratch]
    lea rsi, [path_buffer]
    call str_copy
    lea rdi, [scratch]
    call str_len
    lea rdi, [scratch + rax]
    cmp byte [rdi - 1], '/'
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    lea rsi, [command_line]
    jmp str_copy

; re-read the target dir so the change shows
panel_refresh:
    mov edi, [panel_target]
    cmp edi, -1
    je tree_load
    push rdi
    mov eax, edi
    call node_address
    test byte [rax + NODE_FLAGS], NODE_EXPANDED
    jz .expand
    mov edi, [rsp]
    call tree_collapse
.expand:
    pop rdi
    jmp tree_expand

panel_ask_new_file:
    call panel_choose_target
    mov al, PROMPT_NEW_FILE
    lea rsi, [new_file_label]
    jmp start_prompt

panel_ask_new_directory:
    call panel_choose_target
    mov al, PROMPT_NEW_DIRECTORY
    lea rsi, [new_directory_label]
    jmp start_prompt

panel_ask_rename:
    mov edi, [panel_selected]
    call panel_parent_of
    mov [panel_target], eax
    mov al, PROMPT_RENAME
    lea rsi, [rename_label]
    call start_prompt
    mov eax, [panel_selected]
    call node_address
    lea rsi, [rax + NODE_NAME]
    lea rdi, [command_line]
    call str_copy
    lea rdi, [command_line]
    call str_len
    mov [command_length], eax
    mov byte [prompt_selected], 1
    ret

panel_ask_delete:
    mov edi, [panel_selected]
    call panel_parent_of
    mov [panel_target], eax
    lea rdi, [prompt_text]
    lea rsi, [delete_label]
    call str_copy
    mov eax, [panel_selected]
    call node_address
    lea rsi, [rax + NODE_NAME]
    lea rdi, [prompt_text]
    push rsi
    call str_len
    pop rsi
    lea rdi, [prompt_text + rax]
    call str_copy
    lea rdi, [prompt_text]
    call str_len
    lea rdi, [prompt_text + rax]
    lea rsi, [confirm_label]
    call str_copy
    mov al, PROMPT_DELETE
    lea rsi, [prompt_text]
    jmp start_prompt

panel_new_file:
    cmp dword [command_length], 0
    je .done
    call panel_target_path
    lea rdi, [scratch]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js panel_failed
    mov edi, eax
    call sys_close
    call panel_refresh
    lea rsi, [scratch]
    call open_path
    mov byte [mode], MODE_NORMAL
.done:
    ret

panel_new_directory:
    cmp dword [command_length], 0
    je .done
    call panel_target_path
    lea rdi, [scratch]
    call sys_mkdir
    test eax, eax
    js panel_failed
    jmp panel_refresh
.done:
    ret

panel_rename:
    cmp dword [command_length], 0
    je .done
    mov edi, [panel_selected]
    call node_path
    lea rdi, [rename_source]
    lea rsi, [path_buffer]
    call str_copy
    call panel_target_path
    lea rdi, [rename_source]
    lea rsi, [scratch]
    call sys_rename
    test eax, eax
    js panel_failed
    lea rdi, [file_path]
    lea rsi, [rename_source]
    call str_equal
    test eax, eax
    jz .refresh
    lea rdi, [file_path]
    lea rsi, [scratch]
    call str_copy
.refresh:
    jmp panel_refresh
.done:
    ret

; only y deletes
panel_delete:
    mov al, [command_line]
    or al, 0x20
    cmp al, 'y'
    jne .done
    mov edi, [panel_selected]
    call node_path
    lea rdi, [path_buffer]
    call sys_unlink
    test eax, eax
    js panel_failed
    jmp panel_refresh
.done:
    ret

panel_failed:
    lea rdi, [panel_failed_text]
    jmp show_error

panel_click:
    mov eax, [screen_click_row]
    dec eax
    add eax, [panel_top]
    cmp eax, [node_count]
    jae .done
    mov byte [mode], MODE_FILES
    mov edi, eax
    jmp panel_activate
.done:
    ret

panel_scroll:
    call text_rows
    mov ecx, [panel_selected]
    cmp ecx, [panel_top]
    jae .below
    mov [panel_top], ecx
    ret
.below:
    mov edx, [panel_top]
    add edx, eax
    cmp ecx, edx
    jb .done
    sub ecx, eax
    inc ecx
    mov [panel_top], ecx
.done:
    ret

render_panel:
    cmp dword [panel_width], 0
    je .none
    push rbx
    push r12
    push r13
    push r14
    call panel_scroll
    xor r12d, r12d
.row:
    call text_rows
    cmp r12d, eax
    jae .finished
    lea esi, [r12 + 1]
    mov r13d, [panel_top]
    add r13d, r12d
    mov r14d, [panel_width]
    dec r14d

    mov cl, ATTR_PANEL
    cmp r13d, [panel_selected]
    jne .fill
    cmp r13d, [node_count]
    jae .fill
    mov cl, ATTR_PANEL_CURSOR
    cmp byte [mode], MODE_FILES
    jne .fill
    mov cl, ATTR_PANEL_SELECTED
.fill:
    mov bl, cl
    xor edi, edi
.blank:
    cmp edi, r14d
    jae .edge
    mov dl, ' '
    call cell_put
    inc edi
    jmp .blank
.edge:
    mov dl, 0xB3
    mov cl, ATTR_PANEL_EDGE
    call cell_put

    cmp r13d, [node_count]
    jae .next
    mov eax, r13d
    call node_address
    mov r13, rax
    movzx edi, byte [r13 + NODE_DEPTH]
    lea edi, [rdi * 2 + 1]
    mov dl, ' '
    test byte [r13 + NODE_FLAGS], NODE_DIRECTORY
    jz .icon
    mov dl, 0x10
    test byte [r13 + NODE_FLAGS], NODE_EXPANDED
    jz .icon
    mov dl, 0x1F
.icon:
    mov cl, ATTR_PANEL_ICON
    call panel_put
    add edi, 2
    mov cl, ATTR_PANEL_DIRECTORY
    test byte [r13 + NODE_FLAGS], NODE_DIRECTORY
    jnz .color
    mov cl, ATTR_PANEL
    push rdi
    push rsi
    push rcx
    mov eax, [panel_top]
    add eax, r12d
    mov edi, eax
    call node_path
    lea rdi, [path_buffer]
    lea rsi, [file_path]
    call str_equal
    pop rcx
    pop rsi
    pop rdi
    test eax, eax
    jz .color
    mov cl, ATTR_PANEL_CURRENT
.color:
    lea rdx, [r13 + NODE_NAME]
.name:
    mov al, [rdx]
    test al, al
    jz .next
    cmp edi, r14d
    jae .next
    push rdx
    mov dl, al
    call panel_put
    pop rdx
    inc rdx
    inc edi
    jmp .name
.next:
    inc r12d
    jmp .row
.finished:
    pop r14
    pop r13
    pop r12
    pop rbx
.none:
    ret

panel_put:
    cmp bl, ATTR_PANEL
    je .plain
    and cl, 0x0F
    mov al, bl
    and al, 0xF0
    or cl, al
.plain:
    cmp edi, r14d
    jae .done
    jmp cell_put
.done:
    ret

new_file_label db 'New file: ', 0
new_directory_label db 'New folder: ', 0
rename_label db 'Rename to: ', 0
delete_label db 'Delete ', 0
confirm_label db '? (y/n) ', 0
panel_failed_text db 'E: the file system refused that', 0
