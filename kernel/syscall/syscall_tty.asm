; pseudo terminals. a terminal window (lds in the gui) opens one and starts programs on it:
; their stdin reads keys the owner pushes in, their stdout lands in a 4 KB ring the owner reads
; and draws. numbered from 1, 0 in a process record means "the real console"

TTY_ADDRESS = 0xA70000
TTY_MAX = 32
TTY_STRIDE = 0x1800
TTY_OWNER_PID = 0
TTY_OWNER_SLOT = 4
TTY_IN_HEAD = 8
TTY_IN_COUNT = 12
TTY_OUT_HEAD = 16
TTY_OUT_COUNT = 20
TTY_HANGUP = 24
TTY_INPUT = 64
TTY_INPUT_SLOTS = 256
TTY_OUTPUT = TTY_INPUT + TTY_INPUT_SLOTS * 4
TTY_OUTPUT_BYTES = 4096
TTY_DRAIN = -1

EVENT_TTY = 4

tty_init:
    push rax
    push rcx
    push rdi
    mov rdi, TTY_ADDRESS
    xor eax, eax
    mov ecx, TTY_STRIDE * TTY_MAX / 8
    rep stosq
    pop rdi
    pop rcx
    pop rax
    ret

tty_address:
    dec eax
    imul eax, eax, TTY_STRIDE
    add eax, TTY_ADDRESS
    ret

tty_current:
    call process_current_record
    movzx eax, byte [rax + PROCESS_TTY]
    ret

; only finds the first tty the process owns. nothing owns two yet so whatever
tty_owned_by_current:
    push rcx
    call process_get_current_pid
    mov ecx, 1
.scan:
    cmp ecx, TTY_MAX
    ja .none
    push rax
    mov eax, ecx
    call tty_address
    mov rdx, rax
    pop rax
    cmp [rdx + TTY_OWNER_PID], eax
    je .found
    inc ecx
    jmp .scan
.found:
    mov eax, ecx
    pop rcx
    clc
    ret
.none:
    pop rcx
    stc
    ret

syscall_tty_open:
    push rbx
    mov ebx, 1
.scan:
    cmp ebx, TTY_MAX
    ja .full
    mov eax, ebx
    call tty_address
    cmp dword [rax + TTY_OWNER_PID], 0
    je .claim
    inc ebx
    jmp .scan
.claim:
    mov rdi, rax
    push rdi
    xor eax, eax
    mov ecx, TTY_OUTPUT / 8
    rep stosq
    pop rdi
    call process_get_current_pid
    mov [rdi + TTY_OWNER_PID], eax
    movzx eax, word [process_current_index]
    mov [rdi + TTY_OWNER_SLOT], eax
    call process_current_record
    mov [rax + PROCESS_CHILD_TTY], bl
    mov eax, ebx
    pop rbx
    ret
.full:
    mov eax, SYS_ERR_FULL
    pop rbx
    ret

syscall_tty_read:
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13d, edx

    call tty_owned_by_current
    jc .invalid
    cmp eax, edi
    jne .invalid
    call tty_address
    mov rbx, rax

    mov rdi, r12
    mov edx, r13d
    call syscall_user_range_valid
    jc .invalid

    mov dword [rbx + TTY_HANGUP], 0
    xor ecx, ecx
.copy:
    cmp ecx, r13d
    jae .copied
    cmp dword [rbx + TTY_OUT_COUNT], 0
    je .copied
    mov eax, [rbx + TTY_OUT_HEAD]
    mov dl, [rbx + TTY_OUTPUT + rax]
    mov [r12 + rcx], dl
    inc eax
    and eax, TTY_OUTPUT_BYTES - 1
    mov [rbx + TTY_OUT_HEAD], eax
    dec dword [rbx + TTY_OUT_COUNT]
    inc ecx
    jmp .copy
.copied:
    push rcx
    mov eax, edi
    call tty_wake_members
    pop rax
    jmp .out

.invalid:
    mov eax, SYS_ERR_INVALID
.out:
    pop r13
    pop r12
    pop rbx
    ret

syscall_tty_input:
    push rbx
    push rcx
    mov ecx, edi
    call tty_owned_by_current
    jc .invalid
    cmp eax, ecx
    jne .invalid
    call tty_address
    mov rbx, rax
    cmp esi, TTY_DRAIN
    je .drain
    cmp dword [rbx + TTY_IN_COUNT], TTY_INPUT_SLOTS
    jae .full
    mov eax, [rbx + TTY_IN_HEAD]
    add eax, [rbx + TTY_IN_COUNT]
    and eax, TTY_INPUT_SLOTS - 1
    mov [rbx + TTY_INPUT + rax * 4], esi
    inc dword [rbx + TTY_IN_COUNT]
    mov eax, ecx
    call tty_wake_members
    xor eax, eax
    jmp .out
.drain:
    xor eax, eax
    xchg eax, [rbx + TTY_IN_COUNT]
    jmp .out
.full:
    mov eax, SYS_ERR_FULL
    jmp .out
.invalid:
    mov eax, SYS_ERR_INVALID
.out:
    pop rcx
    pop rbx
    ret

tty_wake_members:
    push rcx
    push rdi
    push rdx
    mov edx, eax
    mov ecx, 1
.scan:
    cmp ecx, PROCESS_MAX
    jae .done
    mov eax, ecx
    imul eax, PROCESS_SIZE
    cmp byte [process_table + rax + PROCESS_STATE], PROCESS_STATE_BLOCKED
    jne .next
    cmp [process_table + rax + PROCESS_TTY], dl
    jne .next
    mov edi, ecx
    call process_wake_slot
.next:
    inc ecx
    jmp .scan
.done:
    pop rdx
    pop rdi
    pop rcx
    ret

tty_wake_owner:
    push rdi
    mov edi, [rax + TTY_OWNER_SLOT]
    call process_wake_slot
    pop rdi
    ret

tty_read_key:
    call tty_address
    cmp dword [rax + TTY_OWNER_PID], 0
    je .hangup
    cmp dword [rax + TTY_IN_COUNT], 0
    je .empty
    mov ecx, [rax + TTY_IN_HEAD]
    mov edx, [rax + TTY_INPUT + rcx * 4]
    inc ecx
    and ecx, TTY_INPUT_SLOTS - 1
    mov [rax + TTY_IN_HEAD], ecx
    dec dword [rax + TTY_IN_COUNT]
    mov eax, edx
    clc
    ret
.hangup:
    mov eax, 4
    clc
    ret
.empty:
    stc
    ret

; eax = tty, rsi = data, rdx = length. CF when the ring has no room for all of it, the
; caller blocks and retries the whole write (output is never split)
tty_write:
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13, rdx

    call tty_address
    mov rbx, rax
    cmp dword [rbx + TTY_OWNER_PID], 0
    je .discard

    mov rdi, r12
    mov rdx, r13
    call syscall_user_range_valid
    jc .invalid

    mov rax, r13
    cmp rax, TTY_OUTPUT_BYTES
    jbe .check_space
    mov eax, TTY_OUTPUT_BYTES
.check_space:
    mov ecx, TTY_OUTPUT_BYTES
    sub ecx, [rbx + TTY_OUT_COUNT]
    cmp eax, ecx
    ja .no_space

    mov r13d, eax
    xor ecx, ecx
.copy:
    cmp ecx, r13d
    jae .copied
    mov eax, [rbx + TTY_OUT_HEAD]
    add eax, [rbx + TTY_OUT_COUNT]
    and eax, TTY_OUTPUT_BYTES - 1
    mov dl, [r12 + rcx]
    mov [rbx + TTY_OUTPUT + rax], dl
    inc dword [rbx + TTY_OUT_COUNT]
    inc ecx
    jmp .copy
.copied:
    mov rax, rbx
    call tty_wake_owner
    mov eax, r13d
    clc
    jmp .out

.no_space:
    mov rax, rbx
    call tty_wake_owner
    stc
    jmp .out

.discard:
    mov rax, r13
    clc
    jmp .out
.invalid:
    mov eax, SYS_ERR_INVALID_ADDRESS
    clc
.out:
    pop r13
    pop r12
    pop rbx
    ret

tty_event:
    call tty_owned_by_current
    jc .none
    push rax
    call tty_address
    mov rdx, rax
    pop rax
    cmp dword [rdx + TTY_OUT_COUNT], 0
    jne .ready
    cmp dword [rdx + TTY_HANGUP], 0
    jne .ready
.none:
    stc
    ret
.ready:
    clc
    ret

tty_release_current:
    push rax
    push rcx
    push rdx
    call tty_current
    test eax, eax
    jz .owner
    call tty_address
    mov dword [rax + TTY_HANGUP], 1
    call tty_wake_owner
.owner:
    call tty_owned_by_current
    jc .done
    mov ecx, eax
    call tty_address
    mov dword [rax + TTY_OWNER_PID], 0
    mov eax, ecx
    call tty_wake_members
.done:
    pop rdx
    pop rcx
    pop rax
    ret
