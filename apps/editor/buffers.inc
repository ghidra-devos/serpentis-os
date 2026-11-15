; tabs. only the active buffer lives in text, the others are parked one after another in
; pool with their cursor and scroll state in a BUFFER_BYTES record. undo history is not
; kept for parked buffers

BUFFER_MAX = 16
BUFFER_BYTES = 288
BUFFER_PATH = 0
BUFFER_CURSOR = 256
BUFFER_TOP = 260
BUFFER_LEFT = 264
BUFFER_WANT = 268
BUFFER_OFFSET = 272             ; where its text starts in pool
BUFFER_SIZE = 276
BUFFER_DIRTY = 280
BUFFER_USED = 281
POOL_MAX = 0x400000
TAB_HIT_BYTES = 12

buffer_address:
    imul eax, eax, BUFFER_BYTES
    lea rax, [buffers + rax]
    ret

buffers_init:
    mov dword [current_buffer], 0
    mov byte [buffers + BUFFER_USED], 1
    ret

stash_active:
    push rbx
    mov eax, [current_buffer]
    call buffer_address
    mov rbx, rax
    mov eax, [pool_used]
    add eax, [text_size]
    cmp eax, POOL_MAX
    ja .full
    lea rdi, [rbx + BUFFER_PATH]
    lea rsi, [file_path]
    call str_copy
    mov eax, [cursor]
    mov [rbx + BUFFER_CURSOR], eax
    mov eax, [top_line]
    mov [rbx + BUFFER_TOP], eax
    mov eax, [left_column]
    mov [rbx + BUFFER_LEFT], eax
    mov eax, [want_column]
    mov [rbx + BUFFER_WANT], eax
    mov al, [dirty]
    mov [rbx + BUFFER_DIRTY], al
    mov eax, [pool_used]
    mov [rbx + BUFFER_OFFSET], eax
    mov ecx, [text_size]
    mov [rbx + BUFFER_SIZE], ecx
    lea rdi, [pool + rax]
    lea rsi, [text]
    call text_move
    mov eax, [text_size]
    add [pool_used], eax
    pop rbx
    clc
    ret
.full:
    lea rdi, [buffer_memory_text]
    call show_error
    pop rbx
    stc
    ret

restore_buffer:
    push rbx
    push r12
    push r13
    mov [current_buffer], edi
    mov eax, edi
    call buffer_address
    mov rbx, rax
    mov r12d, [rbx + BUFFER_OFFSET]
    mov r13d, [rbx + BUFFER_SIZE]

    lea rdi, [text]
    lea rsi, [pool + r12]
    mov ecx, r13d
    call text_move
    mov [text_size], r13d

    lea rdi, [pool + r12]
    lea rsi, [pool + r12 + r13]
    mov ecx, [pool_used]
    sub ecx, r12d
    sub ecx, r13d
    call text_move
    sub [pool_used], r13d

    xor ecx, ecx
.shift:
    cmp ecx, BUFFER_MAX
    jae .state
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .next
    cmp [rax + BUFFER_OFFSET], r12d
    jbe .next
    sub [rax + BUFFER_OFFSET], r13d
.next:
    inc ecx
    jmp .shift

.state:
    lea rdi, [file_path]
    lea rsi, [rbx + BUFFER_PATH]
    call str_copy
    mov eax, [rbx + BUFFER_CURSOR]
    mov [cursor], eax
    mov eax, [rbx + BUFFER_TOP]
    mov [top_line], eax
    mov eax, [rbx + BUFFER_LEFT]
    mov [left_column], eax
    mov eax, [rbx + BUFFER_WANT]
    mov [want_column], eax
    mov al, [rbx + BUFFER_DIRTY]
    mov [dirty], al
    mov dword [undo_used], 0
    mov dword [redo_used], 0
    call detect_file_kind
    pop r13
    pop r12
    pop rbx
    ret

switch_buffer:
    cmp edi, [current_buffer]
    je .done
    push rdi
    call stash_active
    pop rdi
    jc .done
    call restore_buffer
.done:
    ret

next_buffer:
    mov edx, 1
    jmp step_buffer
; edx = +1 or BUFFER_MAX-1, wraps around to the next used slot
previous_buffer:
    mov edx, BUFFER_MAX - 1
step_buffer:
    mov ecx, [current_buffer]
    mov r8d, BUFFER_MAX - 1
.scan:
    add ecx, edx
    cmp ecx, BUFFER_MAX
    jb .index
    sub ecx, BUFFER_MAX
.index:
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    jne .found
    dec r8d
    jnz .scan
    ret
.found:
    mov edi, ecx
    jmp switch_buffer

make_absolute:
    lea rdi, [absolute_path]
    cmp byte [rsi], '/'
    je str_copy
    push rsi
    mov esi, PATH_BYTES
    call sys_getcwd
    lea rdi, [absolute_path]
    call str_len
    lea rdi, [absolute_path + rax]
    cmp byte [rdi - 1], '/'
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    pop rsi
    jmp str_copy

open_path:
    push rbx
    call make_absolute
    lea rbx, [absolute_path]
    xor ecx, ecx
.existing:
    cmp ecx, BUFFER_MAX
    jae .reuse
    cmp ecx, [current_buffer]
    je .skip
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .skip
    lea rdi, [rax + BUFFER_PATH]
    mov rsi, rbx
    push rcx
    call str_equal
    pop rcx
    test eax, eax
    jz .skip
    mov edi, ecx
    call switch_buffer
    jmp .done
.skip:
    inc ecx
    jmp .existing

.reuse:
    lea rdi, [file_path]
    mov rsi, rbx
    call str_equal
    test eax, eax
    jnz .done
    cmp byte [dirty], 0
    jne .new_slot
    cmp byte [file_path], 0
    jne .new_slot
    cmp dword [text_size], 0
    je .load

.new_slot:
    xor ecx, ecx
.free:
    cmp ecx, BUFFER_MAX
    jae .too_many
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .claim
    inc ecx
    jmp .free
.claim:
    push rcx
    call stash_active
    pop rcx
    jc .done
    mov eax, ecx
    call buffer_address
    mov byte [rax + BUFFER_USED], 1
    mov [current_buffer], ecx
.load:
    lea rdi, [file_path]
    mov rsi, rbx
    call str_copy
    call load_file
    jmp .done
.too_many:
    lea rdi, [too_many_buffers_text]
    call show_error
.done:
    pop rbx
    ret

close_buffer:
    test al, al
    jnz .forced
    cmp byte [dirty], 0
    jne unsaved_error
.forced:
    mov eax, [current_buffer]
    call buffer_address
    mov byte [rax + BUFFER_USED], 0
    xor ecx, ecx
.other:
    cmp ecx, BUFFER_MAX
    jae .last
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    jne .restore
    inc ecx
    jmp .other
.restore:
    mov edi, ecx
    jmp restore_buffer
.last:
    mov byte [running], 0
    ret

any_buffer_dirty:
    mov al, [dirty]
    xor ecx, ecx
.scan:
    cmp ecx, BUFFER_MAX
    jae .done
    cmp ecx, [current_buffer]
    je .next
    push rax
    mov eax, ecx
    call buffer_address
    mov rdx, rax
    pop rax
    cmp byte [rdx + BUFFER_USED], 0
    je .next
    or al, [rdx + BUFFER_DIRTY]
.next:
    inc ecx
    jmp .scan
.done:
    test al, al
    ret

new_buffer:
    xor ecx, ecx
.free:
    cmp ecx, BUFFER_MAX
    jae .too_many
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .claim
    inc ecx
    jmp .free
.claim:
    push rcx
    call stash_active
    pop rcx
    jc .done
    mov eax, ecx
    call buffer_address
    mov byte [rax + BUFFER_USED], 1
    mov [current_buffer], ecx
    mov byte [file_path], 0
    xor eax, eax
    mov [text_size], eax
    mov [cursor], eax
    mov [top_line], eax
    mov [left_column], eax
    mov [want_column], eax
    mov [undo_used], eax
    mov [redo_used], eax
    mov byte [dirty], 0
    mov byte [mode], MODE_NORMAL
    jmp detect_file_kind
.too_many:
    lea rdi, [too_many_buffers_text]
    jmp show_error
.done:
    ret

; ctrl+w. the last tab doesn't close, it turns into an empty one
close_tab:
    cmp byte [dirty], 0
    jne unsaved_error
    xor ecx, ecx
    xor edx, edx
.count:
    cmp ecx, BUFFER_MAX
    jae .counted
    mov eax, ecx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .next
    inc edx
.next:
    inc ecx
    jmp .count
.counted:
    cmp edx, 1
    jbe .empty
    xor eax, eax
    jmp close_buffer
.empty:
    mov byte [file_path], 0
    xor eax, eax
    mov [text_size], eax
    mov [cursor], eax
    mov [top_line], eax
    mov [left_column], eax
    mov [undo_used], eax
    mov [redo_used], eax
    jmp detect_file_kind

tab_label:
    mov eax, ebx
    call buffer_address
    lea rsi, [rax + BUFFER_PATH]
    mov dl, [rax + BUFFER_DIRTY]
    mov cl, ATTR_TABS
    cmp ebx, [current_buffer]
    jne .named
    lea rsi, [file_path]
    mov dl, [dirty]
    mov cl, ATTR_TAB_ACTIVE
.named:
    lea rax, [file_title]
    cmp byte [rsi], 0
    je .done
    push rdx
    push rcx
    call base_name
    pop rcx
    pop rdx
.done:
    ret

tab_width:
    push rdx
    push rcx
    call tab_label
    mov rdi, rax
    call str_len
    add eax, 5
    pop rcx
    pop rdx
    ret

tab_scroll:
    push rbx
    push r12
    push r13
    mov r12d, [screen_columns]
    sub r12d, edi
    mov eax, [current_buffer]
    cmp eax, [tab_first]
    jae .fit
    mov [tab_first], eax
.fit:
    xor r13d, r13d
    mov ebx, [tab_first]
.sum:
    cmp ebx, [current_buffer]
    ja .check
    mov eax, ebx
    call buffer_address
    cmp byte [rax + BUFFER_USED], 0
    je .skip
    call tab_width
    add r13d, eax
.skip:
    inc ebx
    jmp .sum
.check:
    cmp r13d, r12d
    jbe .done
    mov eax, [tab_first]
    cmp eax, [current_buffer]
    jae .done
    inc dword [tab_first]
    jmp .fit
.done:
    pop r13
    pop r12
    pop rbx
    ret

render_tab_strip:
    push rbx
    push r12
    push r13
    mov dword [tab_hit_count], 0
    mov r12d, edi
    call tab_scroll
    mov ebx, [tab_first]
.tab:
    cmp ebx, BUFFER_MAX
    jae .done
    mov eax, ebx
    call buffer_address
    mov r13, rax
    cmp byte [r13 + BUFFER_USED], 0
    je .next
    cmp r12d, [screen_columns]
    jae .done

    mov ecx, [tab_hit_count]
    imul ecx, ecx, TAB_HIT_BYTES
    mov [tab_hits + rcx], r12d
    mov [tab_hits + rcx + 8], ebx

    call tab_label
    mov [tab_dirty], dl
    mov [tab_attribute], cl
    mov rdx, rax
    mov edi, r12d
    xor esi, esi
    mov cl, [tab_attribute]
    push rdx
    mov dl, ' '
    call cell_put
    pop rdx
    inc edi
    call cell_text
    mov dl, ' '
    call cell_put
    inc edi
    mov ecx, [tab_hit_count]
    imul ecx, ecx, TAB_HIT_BYTES
    mov [tab_hits + rcx + 4], edi
    mov dl, 'x'
    mov cl, [tab_attribute]
    and cl, 0xF0
    or cl, ATTR_TAB_CLOSE
    cmp byte [tab_dirty], 0
    je .mark
    mov dl, 7                   ; cp437 bullet
    and cl, 0xF0
    or cl, ATTR_TAB_DIRTY and 0x0F
.mark:
    call cell_put
    inc edi
    mov dl, ' '
    mov cl, [tab_attribute]
    call cell_put
    inc edi
    mov dl, 0xB3
    mov cl, ATTR_TAB_EDGE
    call cell_put
    inc edi
    mov r12d, edi
    inc dword [tab_hit_count]
.next:
    inc ebx
    jmp .tab
.done:
    mov eax, r12d
    pop r13
    pop r12
    pop rbx
    ret

; click on the x closes that tab (it has to be switched to first)
tab_click:
    mov edx, [screen_click_column]
    xor ecx, ecx
.hit:
    cmp ecx, [tab_hit_count]
    jae .done
    imul eax, ecx, TAB_HIT_BYTES
    cmp edx, [tab_hits + rax]
    jb .next
    mov r8d, [tab_hits + rax + 4]
    cmp edx, r8d
    ja .next
    mov edi, [tab_hits + rax + 8]
    cmp edx, r8d
    je .close
    jmp switch_buffer
.next:
    inc ecx
    jmp .hit
.close:
    push rdi
    call switch_buffer
    pop rdi
    cmp edi, [current_buffer]
    jne .done
    xor eax, eax
    jmp close_buffer
.done:
    ret

buffer_memory_text db 'E: not enough memory for another buffer', 0
too_many_buffers_text db 'E: too many open buffers', 0
