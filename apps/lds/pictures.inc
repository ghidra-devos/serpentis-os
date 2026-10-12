; pictures a program draws right in the document (termgfx.inc on its side). it asks with
; ESC _ G o pid;width;height ESC \ and gets a shm slot back in a message, after that each
; ESC _ G f n ESC \ says frame n is done. the picture sits on blank lines and scrolls with
; them, when the program is gone the last frame just stays.
; ESC _ G s x;z ESC \ instead says frame 0 holds a mesh, and from then on lds turns it
; itself with gfx3d, a full turn around x every x ticks and around z every z. spin3d cube
; uses it. it keeps turning after the program is gone, as long as it is on screen, and only
; what is on screen gets drawn

PICTURES_MAX = 8
PICTURE_LINE = 0
PICTURE_WIDTH = 4
PICTURE_HEIGHT = 6
PICTURE_SLOT = 8
PICTURE_FLAGS = 9
PICTURE_FRONT = 10
PICTURE_SPINS = 11
PICTURE_X_TURN = 12
PICTURE_Z_TURN = 14
PICTURE_START = 16
PICTURE_BYTES = 24
PICTURE_SHOWN = 1
PICTURE_LIVE = 2
PICTURE_NO_FRAME = 0xFF
PICTURE_ARGUMENTS = 4
PICTURE_WIDTH_MAX = DOCUMENT_COLUMNS * 8
PICTURE_HEIGHT_MAX = 32 * 16
PICTURE_SPIN_TICKS = 4

TERMGFX_AREA = SHM_ADDRESS + TERMGFX_OFFSET

picture_init:
    call sys_shm_attach
    call sys_getpid
    mov [picture_owner], eax
    ret

picture_slot_pixels:
    imul eax, eax, TERMGFX_SLOT_BYTES
    add rax, TERMGFX_AREA + TERMGFX_TABLE_BYTES
    ret

; rbx -> record, al = flag to drop. once a picture is neither shown nor live its slot is free
; for any terminal again, so never call this twice for the same flag
picture_unflag:
    test [rbx + PICTURE_FLAGS], al
    jz .done
    not al
    and [rbx + PICTURE_FLAGS], al
    jnz .done
    movzx eax, byte [rbx + PICTURE_SLOT]
    mov dword [TERMGFX_AREA + rax * 8], 0
.done:
    ret

picture_clear_all:
    push rax
    push rbx
    push rcx
    lea rbx, [pictures]
    mov ecx, PICTURES_MAX
.next:
    mov al, PICTURE_SHOWN
    call picture_unflag
    add rbx, PICTURE_BYTES
    dec ecx
    jnz .next
    pop rcx
    pop rbx
    pop rax
    ret

picture_scroll:
    push rax
    push rbx
    push rcx
    lea rbx, [pictures]
    mov ecx, PICTURES_MAX
.next:
    test byte [rbx + PICTURE_FLAGS], PICTURE_SHOWN
    jz .skip
    dec dword [rbx + PICTURE_LINE]
    movzx eax, word [rbx + PICTURE_HEIGHT]
    add eax, 15
    shr eax, 4
    add eax, [rbx + PICTURE_LINE]
    jg .skip
    mov al, PICTURE_SHOWN
    call picture_unflag
.skip:
    add rbx, PICTURE_BYTES
    dec ecx
    jnz .next
    pop rcx
    pop rbx
    pop rax
    ret

picture_free_record:
    lea rbx, [pictures]
    mov ecx, PICTURES_MAX
.next:
    cmp byte [rbx + PICTURE_FLAGS], 0
    je .found
    add rbx, PICTURE_BYTES
    dec ecx
    jnz .next
    stc
    ret
.found:
    clc
    ret

picture_evict:
    push rbx
    push rcx
    push rdx
    xor edx, edx
    lea rbx, [pictures]
    mov ecx, PICTURES_MAX
.next:
    cmp byte [rbx + PICTURE_FLAGS], PICTURE_SHOWN
    jne .skip
    test rdx, rdx
    jz .take
    mov eax, [rbx + PICTURE_LINE]
    cmp eax, [rdx + PICTURE_LINE]
    jge .skip
.take:
    mov rdx, rbx
.skip:
    add rbx, PICTURE_BYTES
    dec ecx
    jnz .next
    test rdx, rdx
    jz .none
    mov rbx, rdx
    mov al, PICTURE_SHOWN
    call picture_unflag
    clc
    jmp .out
.none:
    stc
.out:
    pop rdx
    pop rcx
    pop rbx
    ret

; out: eax = slot now owned by this terminal, CF when all of them are taken. other lds
; windows grab slots from the same table, hence the lock
picture_claim_slot:
    push rcx
    push rdx
    mov edx, [picture_owner]
    xor ecx, ecx
.next:
    xor eax, eax
    lock cmpxchg [TERMGFX_AREA + rcx * 8], edx
    je .claimed
    inc ecx
    cmp ecx, TERMGFX_SLOTS
    jb .next
    stc
    jmp .out
.claimed:
    mov dword [TERMGFX_AREA + rcx * 8 + 4], 0
    mov eax, ecx
    clc
.out:
    pop rdx
    pop rcx
    ret

picture_open:
    push rbx
    push r12
    push r13
    call picture_close
    mov r12d, [picture_arguments + 4]
    mov r13d, [picture_arguments + 8]
    mov eax, SYS_ERR_INVALID
    test r12d, r12d
    jz .refuse
    test r13d, r13d
    jz .refuse
    cmp r12d, PICTURE_WIDTH_MAX
    ja .refuse
    cmp r13d, PICTURE_HEIGHT_MAX
    ja .refuse
    mov ecx, r12d
    imul ecx, r13d
    cmp ecx, TERMGFX_FRAME_BYTES
    ja .refuse

    call picture_free_record
    jnc .record
    call picture_evict
    jc .full
    call picture_free_record
.record:
    call picture_claim_slot
    jnc .claimed
    call picture_evict
    jc .full
    call picture_claim_slot
    jc .full
.claimed:
    mov [rbx + PICTURE_SLOT], al
    mov [rbx + PICTURE_WIDTH], r12w
    mov [rbx + PICTURE_HEIGHT], r13w
    mov byte [rbx + PICTURE_FRONT], PICTURE_NO_FRAME
    mov byte [rbx + PICTURE_SPINS], 0

    cmp word [document_cursor_column], 0
    je .lines
    call terminal_newline
.lines:
    lea r13d, [r13 + 15]
    shr r13d, 4
    mov r12d, r13d
.blank:
    call terminal_newline
    dec r12d
    jnz .blank
    movzx eax, word [document_cursor_line]
    sub eax, r13d
    mov [rbx + PICTURE_LINE], eax
    mov byte [rbx + PICTURE_FLAGS], PICTURE_SHOWN or PICTURE_LIVE
    mov [picture_live], rbx

    movzx eax, byte [rbx + PICTURE_SLOT]
    mov ecx, eax
    imul eax, eax, TERMGFX_SLOT_BYTES
    add eax, TERMGFX_OFFSET + TERMGFX_TABLE_BYTES
    mov [picture_message + MSG_A], eax
    lea eax, [TERMGFX_OFFSET + rcx * 8 + 4]
    mov [picture_message + MSG_B], eax
    mov eax, TERMGFX_OPENED
    jmp .reply
.full:
    mov eax, SYS_ERR_FULL
.refuse:
    mov [picture_message + MSG_A], eax
    mov eax, TERMGFX_REFUSED
.reply:
    mov [picture_message + MSG_OP], eax
    mov edi, [picture_arguments]
    lea rsi, [picture_message]
    call sys_send
    pop r13
    pop r12
    pop rbx
    ret

; frame n went to buffer (n - 1) and 1. the program waits for the shown counter before it
; touches the other buffer, so we never draw from a half painted one
picture_frame:
    mov rax, [picture_live]
    test rax, rax
    jz .done
    mov ecx, [picture_arguments]
    lea edx, [rcx - 1]
    and edx, 1
    mov [rax + PICTURE_FRONT], dl
    mov byte [rax + PICTURE_SPINS], 0
    movzx edx, byte [rax + PICTURE_SLOT]
    mov [TERMGFX_AREA + rdx * 8 + 4], ecx
    mov byte [picture_dirty], 1
.done:
    ret

picture_spin:
    mov rax, [picture_live]
    test rax, rax
    jz .done
    mov ecx, [picture_arguments]
    mov edx, [picture_arguments + 4]
    cmp ecx, 0xFFFF
    ja .done
    cmp edx, 0xFFFF
    ja .done
    mov [rax + PICTURE_X_TURN], cx
    mov [rax + PICTURE_Z_TURN], dx
    push rax
    call sys_getticks
    mov edx, eax
    pop rax
    mov [rax + PICTURE_START], edx
    mov byte [rax + PICTURE_FRONT], 1
    mov byte [rax + PICTURE_SPINS], 1
    mov byte [picture_dirty], 1
.done:
    ret

; on the timer, a new pose every PICTURE_SPIN_TICKS (40 ms)
picture_tick:
    cmp byte [picture_animating], 0
    je .done
    call sys_getticks
    mov rdx, rax
    sub rdx, [picture_turned]
    cmp rdx, PICTURE_SPIN_TICKS
    jb .done
    mov [picture_turned], rax
    mov byte [picture_dirty], 1
.done:
    ret

; rbx -> record. draws its mesh from frame 0 into frame 1 at the pose for now
picture_turn:
    push r12
    movzx eax, byte [rbx + PICTURE_SLOT]
    call picture_slot_pixels
    mov r12, rax
; the program could have left anything there, the whole mesh has to sit inside frame 0
    mov rdi, r12
    mov esi, TERMGFX_FRAME_BYTES
    call g3_mesh_check
    test eax, eax
    jz .done
    lea rdi, [r12 + TERMGFX_FRAME_BYTES]
    movzx edx, word [rbx + PICTURE_WIDTH]
    mov esi, edx
    movzx ecx, word [rbx + PICTURE_HEIGHT]
    lea r8, [picture_zbuffer]
    call g3_begin
    mov edi, TERMGFX_CLEAR
    call g3_clear
    xorps xmm0, xmm0
    xorps xmm1, xmm1
    movss xmm2, [picture_distance]
    call g3_translate
    movzx ecx, word [rbx + PICTURE_Z_TURN]
    call picture_angle
    call g3_rotate_z
    movzx ecx, word [rbx + PICTURE_X_TURN]
    call picture_angle
    call g3_rotate_x
    mov rdi, r12
    call g3_fit
    mov rdi, r12
    mov esi, G3_NO_CULL
    call g3_draw_mesh
.done:
    pop r12
    ret

; ecx = ticks for a full turn, 0 stands still. out: xmm0 = angle now
picture_angle:
    xorps xmm0, xmm0
    test ecx, ecx
    jz .done
    push rcx
    call sys_getticks
    pop rcx
    sub eax, [rbx + PICTURE_START]
    xor edx, edx
    div ecx
    cvtsi2ss xmm0, edx
    mulss xmm0, [picture_full_turn]
    cvtsi2ss xmm1, ecx
    divss xmm0, xmm1
.done:
    ret

picture_close:
    push rax
    push rbx
    mov rbx, [picture_live]
    test rbx, rbx
    jz .done
    mov qword [picture_live], 0
    mov al, PICTURE_LIVE
    call picture_unflag
.done:
    pop rbx
    pop rax
    ret

picture_draw:
    push rbx
    push r12
    push r13
    push r14
    mov byte [picture_animating], 0
    lea rbx, [pictures]
    mov r14d, PICTURES_MAX
.next:
    test byte [rbx + PICTURE_FLAGS], PICTURE_SHOWN
    jz .skip
    movzx eax, byte [rbx + PICTURE_FRONT]
    cmp al, PICTURE_NO_FRAME
    je .skip
    imul eax, eax, TERMGFX_FRAME_BYTES
    mov r12, rax
    movzx eax, byte [rbx + PICTURE_SLOT]
    call picture_slot_pixels
    add r12, rax

    movzx r13d, word [rbx + PICTURE_HEIGHT]
    mov esi, [rbx + PICTURE_LINE]
    movzx eax, word [document_view_top]
    sub esi, eax
    shl esi, 4
    mov eax, [view_rows]
    shl eax, 4
    sub eax, esi
    jle .skip
    cmp r13d, eax
    jle .top
    mov r13d, eax
.top:
    cmp byte [rbx + PICTURE_SPINS], 0
    je .still
    push rsi
    call picture_turn
    pop rsi
    mov byte [picture_animating], 1
.still:
    test esi, esi
    jns .across
    add r13d, esi
    jle .skip
    mov eax, esi
    neg eax
    movzx ecx, word [rbx + PICTURE_WIDTH]
    imul eax, ecx
    add r12, rax
    xor esi, esi
.across:
    movzx ecx, word [rbx + PICTURE_WIDTH]
    mov r9d, ecx
    mov edi, [view_columns]
    shl edi, 3
    cmp ecx, edi
    jbe .centre
    mov ecx, edi
.centre:
    sub edi, ecx
    shr edi, 1
    add edi, [screen_origin_x]
    add esi, [screen_origin_y]
    mov rdx, r12
    mov r8d, r13d
    call draw_blit
.skip:
    add rbx, PICTURE_BYTES
    dec r14d
    jnz .next
    mov byte [picture_dirty], 0
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

align 4
picture_distance dd 0x40400000
picture_full_turn dd 0x40C90FDB

picture_escape_start:
    mov byte [ansi_state], 3
    mov byte [picture_command], 0
    xor eax, eax
    mov [picture_argument], eax
    lea rdi, [picture_arguments]
    mov ecx, PICTURE_ARGUMENTS
    rep stosd
    ret

; ansi states 3 to 5: right after ESC _, inside the string, and the ESC of the closing ST
picture_escape:
    cmp edx, 4
    je .body
    ja .end
    mov byte [ansi_state], 4
    cmp al, 'G'
    je .done
    mov byte [picture_command], '?'
    ret
.body:
    cmp al, 27
    je .terminator
    cmp al, ';'
    je .separator
    cmp al, '0'
    jb .letter
    cmp al, '9'
    ja .letter
    mov ecx, [picture_argument]
    cmp ecx, PICTURE_ARGUMENTS
    jae .done
    movzx eax, al
    sub eax, '0'
    imul edx, [picture_arguments + rcx * 4], 10
    add edx, eax
    mov [picture_arguments + rcx * 4], edx
    ret
.separator:
    inc dword [picture_argument]
    ret
.letter:
    cmp byte [picture_command], 0
    jne .done
    mov [picture_command], al
    ret
.terminator:
    mov byte [ansi_state], 5
    ret
.end:
    mov byte [ansi_state], 0
    mov al, [picture_command]
    cmp al, 'o'
    je picture_open
    cmp al, 'f'
    je picture_frame
    cmp al, 'c'
    je picture_close
    cmp al, 's'
    je picture_spin
.done:
    ret
