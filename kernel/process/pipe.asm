PIPE_MAX = 32
PIPE_BYTES = 4096

PIPE_USED = 0
; kernel pipes. 32 rings of 4 KB at PIPE_AREA_ADDRESS. a pipe dies when its last reader and
; last writer are both gone. one waiting reader and one waiting writer per pipe, that is all
; the shell pipelines ever need

PIPE_READERS = 1
PIPE_WRITERS = 2
PIPE_HEAD = 4
PIPE_TAIL = 8
PIPE_COUNT = 12
PIPE_READ_WAITER = 16           ; slot blocked in read, 0 = none
PIPE_WRITE_WAITER = 18
PIPE_DATA = 24
PIPE_SIZE = PIPE_DATA + PIPE_BYTES

pipe_init:
    push rax
    push rcx
    push rdi
    lea rdi, [pipe_table]
    xor eax, eax
    mov ecx, PIPE_MAX * PIPE_SIZE
    cld
    rep stosb
    pop rdi
    pop rcx
    pop rax
    ret

pipe_address:
    cmp edi, PIPE_MAX
    jae .failed
    imul eax, edi, PIPE_SIZE
    lea rax, [pipe_table + rax]
    cmp byte [rax + PIPE_USED], 0
    je .failed
    clc
    ret
.failed:
    xor eax, eax
    stc
    ret

pipe_allocate:
    push rbx
    push rcx
    xor ecx, ecx
.scan:
    cmp ecx, PIPE_MAX
    jae .full
    imul eax, ecx, PIPE_SIZE
    lea rbx, [pipe_table + rax]
    cmp byte [rbx + PIPE_USED], 0
    je .found
    inc ecx
    jmp .scan
.found:
    mov byte [rbx + PIPE_USED], 1
    mov byte [rbx + PIPE_READERS], 1
    mov byte [rbx + PIPE_WRITERS], 1
    mov dword [rbx + PIPE_HEAD], 0
    mov dword [rbx + PIPE_TAIL], 0
    mov dword [rbx + PIPE_COUNT], 0
    mov word [rbx + PIPE_READ_WAITER], 0
    mov word [rbx + PIPE_WRITE_WAITER], 0
    mov eax, ecx
    pop rcx
    pop rbx
    clc
    ret
.full:
    pop rcx
    pop rbx
    stc
    ret

pipe_wake:
    push rax
    push rdi
    movzx eax, word [rdi + rsi]
    test eax, eax
    jz .done
    mov word [rdi + rsi], 0
    mov edi, eax
    call process_wake_slot
.done:
    pop rdi
    pop rax
    ret

pipe_handle_retain:
    push rax
    push rcx
    push rdi
    movzx ecx, byte [rdi + PROCESS_HANDLE_FLAGS]
    test cl, PROCESS_HANDLE_PIPE
    jz .done
    mov edi, [rdi + PROCESS_HANDLE_ENTRY]
    call pipe_address
    jc .done
    test cl, PROCESS_HANDLE_READ
    jz .writer
    inc byte [rax + PIPE_READERS]
.writer:
    test cl, PROCESS_HANDLE_WRITE
    jz .done
    inc byte [rax + PIPE_WRITERS]
.done:
    pop rdi
    pop rcx
    pop rax
    ret

pipe_handle_release:
    push rbx
    push rcx
    push rdi
    movzx ecx, byte [rdi + PROCESS_HANDLE_FLAGS]
    test cl, PROCESS_HANDLE_PIPE
    jz .done
    mov edi, [rdi + PROCESS_HANDLE_ENTRY]
    call pipe_address
    jc .done
    mov rbx, rax

    test cl, PROCESS_HANDLE_READ
    jz .writer
    cmp byte [rbx + PIPE_READERS], 0
    je .writer
    dec byte [rbx + PIPE_READERS]
    jnz .writer
    mov rdi, rbx
    mov esi, PIPE_WRITE_WAITER
    call pipe_wake

.writer:
    test cl, PROCESS_HANDLE_WRITE
    jz .collect
    cmp byte [rbx + PIPE_WRITERS], 0
    je .collect
    dec byte [rbx + PIPE_WRITERS]
    jnz .collect
    mov rdi, rbx
    mov esi, PIPE_READ_WAITER
    call pipe_wake

.collect:
    cmp byte [rbx + PIPE_READERS], 0
    jne .done
    cmp byte [rbx + PIPE_WRITERS], 0
    jne .done
    mov byte [rbx + PIPE_USED], 0

.done:
    pop rdi
    pop rcx
    pop rbx
    ret

pipe_release_current:
    push rbx
    push rcx
    push rdi
    xor ebx, ebx
.next:
    cmp ebx, PROCESS_HANDLE_MAX
    jae .done
    mov edi, ebx
    call process_handle_address
    jc .skip
    mov rdi, rax
    call pipe_handle_release
    mov byte [rdi + PROCESS_HANDLE_FLAGS], 0
.skip:
    inc ebx
    jmp .next
.done:
    pop rdi
    pop rcx
    pop rbx
    ret

; edi = pipe, rsi = user buffer, edx = max bytes, rcx -> trap frame.
; empty with writers left: block and rerun the syscall later. empty and no writers: 0 = eof
; both ends copy byte by byte through pipe_stage. lazy, two rep movsb around the wrap
; would do it
pipe_read:
    push rbx
    push r12
    push r13
    push r14

    mov r12, rsi
    mov r13d, edx
    mov r14, rcx

    call pipe_address
    jc .invalid
    mov rbx, rax

    mov eax, [rbx + PIPE_COUNT]
    test eax, eax
    jz .empty

    cmp r13d, eax
    jbe .sized
    mov r13d, eax
.sized:
    xor ecx, ecx
    mov eax, [rbx + PIPE_TAIL]
.take:
    cmp ecx, r13d
    jae .taken
    mov dl, [rbx + rax + PIPE_DATA]
    mov [pipe_stage + rcx], dl
    inc eax
    cmp eax, PIPE_BYTES
    jb .wrapped
    xor eax, eax
.wrapped:
    inc ecx
    jmp .take
.taken:
    mov [rbx + PIPE_TAIL], eax
    sub [rbx + PIPE_COUNT], r13d

    mov rdi, r12
    lea rsi, [pipe_stage]
    mov edx, r13d
    call syscall_copy_to_user
    jc .bad_address

    mov rdi, rbx
    mov esi, PIPE_WRITE_WAITER
    call pipe_wake

    mov eax, r13d
    clc
    jmp .out

.empty:
    cmp byte [rbx + PIPE_WRITERS], 0
    je .eof
    movzx eax, word [process_current_index]
    test eax, eax
    jz .eof
    mov [rbx + PIPE_READ_WAITER], ax
    sub qword [r14 + TRAP_RIP], 2
    mov rdi, r14
    call process_block_current
    jnc .switched
    add qword [r14 + TRAP_RIP], 2
    mov word [rbx + PIPE_READ_WAITER], 0

.eof:
    xor eax, eax
    clc
    jmp .out

.invalid:
    mov eax, SYS_ERR_INVALID
    clc
    jmp .out

.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    clc
    jmp .out

.switched:
    stc

.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

pipe_write:
    push rbx
    push r12
    push r13
    push r14

    mov r12, rsi
    mov r13d, edx
    mov r14, rcx

    call pipe_address
    jc .invalid
    mov rbx, rax

    cmp byte [rbx + PIPE_READERS], 0
    je .broken

    mov eax, PIPE_BYTES
    sub eax, [rbx + PIPE_COUNT]
    test eax, eax
    jz .full

    cmp r13d, eax
    jbe .sized
    mov r13d, eax
.sized:
    lea rdi, [pipe_stage]
    mov rsi, r12
    mov edx, r13d
    call syscall_copy_from_user
    jc .bad_address

    xor ecx, ecx
    mov eax, [rbx + PIPE_HEAD]
.give:
    cmp ecx, r13d
    jae .given
    mov dl, [pipe_stage + rcx]
    mov [rbx + rax + PIPE_DATA], dl
    inc eax
    cmp eax, PIPE_BYTES
    jb .wrapped
    xor eax, eax
.wrapped:
    inc ecx
    jmp .give
.given:
    mov [rbx + PIPE_HEAD], eax
    add [rbx + PIPE_COUNT], r13d

    mov rdi, rbx
    mov esi, PIPE_READ_WAITER
    call pipe_wake

    mov eax, r13d
    clc
    jmp .out

.full:
    movzx eax, word [process_current_index]
    test eax, eax
    jz .retry
    mov [rbx + PIPE_WRITE_WAITER], ax
    sub qword [r14 + TRAP_RIP], 2
    mov rdi, r14
    call process_block_current
    jnc .switched
    add qword [r14 + TRAP_RIP], 2
    mov word [rbx + PIPE_WRITE_WAITER], 0

.retry:
    xor eax, eax
    clc
    jmp .out

.broken:
    call process_current_record
    or byte [rax + PROCESS_FLAGS], PROCESS_FLAG_KILLED
    mov eax, SYS_ERR_IO
    clc
    jmp .out

.invalid:
    mov eax, SYS_ERR_INVALID
    clc
    jmp .out

.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    clc
    jmp .out

.switched:
    stc

.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

syscall_pipe:
    push rbx
    push r12
    push r13

    mov r12, rdi
    mov rdx, 8
    call syscall_user_range_valid
    jc .bad_address

    call pipe_allocate
    jc .full
    mov r13d, eax

    call syscall_handle_find_free
    jc .no_handles
    mov ebx, eax
    mov edi, ebx
    call process_handle_address
    jc .no_handles
    mov byte [rax + PROCESS_HANDLE_FLAGS], PROCESS_HANDLE_USED or PROCESS_HANDLE_READ or PROCESS_HANDLE_PIPE
    mov [rax + PROCESS_HANDLE_ENTRY], r13d
    mov qword [rax + PROCESS_HANDLE_OFFSET], 0
    mov [pipe_fds], ebx

    call syscall_handle_find_free
    jc .undo_read
    mov ebx, eax
    mov edi, ebx
    call process_handle_address
    jc .undo_read
    mov byte [rax + PROCESS_HANDLE_FLAGS], PROCESS_HANDLE_USED or PROCESS_HANDLE_WRITE or PROCESS_HANDLE_PIPE
    mov [rax + PROCESS_HANDLE_ENTRY], r13d
    mov qword [rax + PROCESS_HANDLE_OFFSET], 0
    mov [pipe_fds + 4], ebx

    mov rdi, r12
    lea rsi, [pipe_fds]
    mov edx, 8
    call syscall_copy_to_user
    jc .undo_read

    xor eax, eax
    jmp .out

.undo_read:
    mov edi, [pipe_fds]
    call process_handle_address
    jc .release
    mov byte [rax + PROCESS_HANDLE_FLAGS], 0

.release:
    mov edi, r13d
    call pipe_address
    jc .no_handles
    mov byte [rax + PIPE_USED], 0

.no_handles:
    mov eax, SYS_ERR_FULL
    jmp .out

.full:
    mov eax, SYS_ERR_BUSY
    jmp .out

.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS

.out:
    pop r13
    pop r12
    pop rbx
    ret

pipe_fds = PIPE_AREA_ADDRESS    ; the kernel has no bss, so the scratch goes in front of the table
pipe_stage = PIPE_AREA_ADDRESS + 16
pipe_table = PIPE_AREA_ADDRESS + 16 + PIPE_BYTES
