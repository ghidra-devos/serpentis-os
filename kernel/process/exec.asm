EXEC_MAX_CODE_BYTES = 0x200000
EXEC_MAX_CODE_PAGES = 512

EXEC_DEFAULT_STACK_SIZE = 0x1000  ; used when the header says 0

EXEC_ERROR_NOT_FOUND = 1
EXEC_ERROR_NOT_EXECUTABLE = 2
EXEC_ERROR_BAD_HEADER = 3
EXEC_ERROR_TOO_LARGE = 4
EXEC_ERROR_NO_PROCESS = 5
EXEC_ERROR_ADDRESS_SPACE = 6
EXEC_ERROR_IO = 7

EXEC_ARGS_MAX = 512

; rsi -> path, rdx -> argument string or 0. loads the file, builds the address space and
; jumps to ring 3. comes back (through exec_return_to_kernel) only when no user process is
; left, or with CF if the program could not be started
exec_run_path_args:
    push rsi
    mov rsi, rdx
    mov rdi, exec_args_buffer
    mov ecx, EXEC_ARGS_MAX - 1
    test rsi, rsi
    jz .terminate
.copy:
    lodsb
    stosb
    test al, al
    jz .done
    dec ecx
    jnz .copy
.terminate:
    mov byte [rdi], 0
.done:
    pop rsi

    mov byte [exec_active], 0
    mov byte [exec_registered], 0

    call fs_make_path
    jc .failed

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .failed

    mov r12d, eax
    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .failed

    mov eax, r12d
    call fs_load_file
    jc .failed

    call serp_parse_header
    jc .failed

    mov rsi, fs_path_buffer
    mov rdi, exec_name
    call fs_copy_string

    mov rax, cr3
    mov [exec_saved_cr3], rax

    mov edi, [serp_code_pages]
    mov esi, [serp_bss_pages]
    mov edx, [serp_stack_pages]
    call address_space_create
    jc .failed

    mov [exec_cr3], rax

    call serp_copy_image

    mov rdi, [as_stack_page]
    add rdi, AS_PAGE_SIZE - EXEC_ARGS_MAX
    mov rsi, exec_args_buffer
    mov ecx, EXEC_ARGS_MAX
    cld
    rep movsb

    mov rdi, USER_CODE_ADDRESS
    add rdi, [serp_entry_offset]
    mov rsi, USER_STACK_TOP - EXEC_ARGS_MAX - 16
    mov rdx, [exec_cr3]
    mov rcx, exec_name
    call process_register_user
    jc .no_process

    mov rsi, fs_current_path
    call process_set_cwd

    call process_current_record
    mov dword [rax + PROCESS_UID], 0

    call process_get_current_pid
    mov [exec_root_pid], eax

    push rbx
    call process_current_record
    mov rbx, rax
    mov rax, [serp_code_size]
    mov [rbx + PROCESS_EXEC_CODE_SIZE], rax
    mov eax, [serp_image_pages]
    mov [rbx + PROCESS_EXEC_CODE_PAGES], eax
    mov rax, [serp_stack_size]
    mov [rbx + PROCESS_EXEC_STACK_SIZE], rax
    pop rbx

    mov byte [exec_registered], 1
    mov byte [exec_active], 1

    mov [exec_return_rsp], rsp

    mov rax, [exec_cr3]
    mov cr3, rax

    call exec_enter_user
    ud2

.no_process:
    mov rdi, [exec_cr3]
    call address_space_destroy
.failed:
    stc
    ret

; first entry into ring 3: clean registers, iretq frame ss, rsp, rflags (if set), cs, rip
exec_enter_user:
    cli

    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    xor eax, eax
    mov ax, USER_DATA_SELECTOR
    push rax

    mov rax, USER_STACK_TOP - EXEC_ARGS_MAX - 16
    push rax

    pushfq
    pop rax
    or eax, 0x200
    push rax

    xor eax, eax
    mov ax, USER_CODE_SELECTOR
    push rax

    mov rax, USER_CODE_ADDRESS
    add rax, [serp_entry_offset]
    push rax

    mov rdi, USER_STACK_TOP - EXEC_ARGS_MAX
    iretq

exec_return_to_kernel:
    mov byte [exec_active], 0
    mov byte [exec_registered], 0
    mov rax, [exec_saved_cr3]
    mov cr3, rax
    mov rsp, [exec_return_rsp]
    clc
    sti
    ret

exec_collect_root:
    push rbx
    push rcx

    mov ecx, 1

.scan:
    cmp ecx, PROCESS_MAX
    jae .reap

    mov eax, ecx
    imul eax, PROCESS_SIZE
    lea rbx, [process_table + rax]

    cmp byte [rbx + PROCESS_STATE], PROCESS_STATE_ZOMBIE
    jne .next
    movzx eax, word [rbx + PROCESS_PID]
    cmp eax, [exec_root_pid]
    jne .next

    mov eax, [rbx + PROCESS_EXIT_CODE]
    mov [exec_exit_code], eax
    jmp .reap

.next:
    inc ecx
    jmp .scan

.reap:
    call process_reap_all

    pop rcx
    pop rbx
    ret

exec_root_pid dd 0

SPAWN_ARGS_MAX = 256

spawn_copy_args:
    push rax
    push rcx
    push rdx
    push rdi
    push rsi
    push r8

    mov r8, rsi
    xor ecx, ecx
    test r8, r8
    jz .terminate

.next:
    cmp ecx, SPAWN_ARGS_MAX - 1
    jae .terminate

    push rcx
    lea rdi, [r8 + rcx]
    mov edx, 1
    call syscall_user_range_valid
    pop rcx
    jc .terminate

    mov al, [r8 + rcx]
    mov [spawn_args_buffer + rcx], al
    test al, al
    jz .done
    inc ecx
    jmp .next

.terminate:
    mov byte [spawn_args_buffer + rcx], 0

.done:
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

spawn_args_buffer:
    times SPAWN_ARGS_MAX db 0

exec_active db 0
exec_registered db 0

exec_saved_cr3 dq 0
exec_cr3 dq 0
exec_return_rsp dq 0
exec_exit_code dd 0

exec_args_buffer:
    times EXEC_ARGS_MAX db 0

exec_name:
    times PROCESS_NAME_SIZE db 0

process_spawn:
    push rbx
    push rcx
    push rdx
    push r12
    push r13
    push r14
    push r15

    mov r15, rdi

    call spawn_copy_args

    call process_get_current_pid
    mov r14d, eax

    mov rsi, r15
    mov rdi, spawn_path_buffer
    mov ecx, FS_PATH_MAX - 1
    cld

.copy_path:
    test ecx, ecx
    jz .path_too_long

    lodsb
    stosb

    test al, al
    jz .path_copied

    dec ecx
    jmp .copy_path

.path_too_long:
    xor al, al
    stosb

.path_copied:
    mov rsi, spawn_path_buffer
    call fs_make_path
    jc .invalid_path

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found

    mov r12d, eax

    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .not_executable

    mov eax, r12d
    mov cl, ACCESS_READ or ACCESS_EXEC
    call fs_access
    jc .denied
    call fs_load_file
    jc .io_error

    call serp_parse_header
    jnc .header_ready
    cmp al, EXEC_ERROR_NOT_EXECUTABLE
    je .not_executable
    cmp al, EXEC_ERROR_TOO_LARGE
    je .too_large
    jmp .bad_header

.header_ready:

    mov rsi, fs_path_buffer
    mov rdi, spawn_name
    call fs_copy_string

    mov edi, [serp_code_pages]
    mov esi, [serp_bss_pages]
    mov edx, [serp_stack_pages]
    call address_space_create
    jc .address_space_error

    mov [spawn_cr3], rax

    call serp_copy_image

    mov rdi, [as_stack_page]
    add rdi, AS_PAGE_SIZE - SPAWN_ARGS_MAX
    mov rsi, spawn_args_buffer
    mov ecx, SPAWN_ARGS_MAX
    cld
    rep movsb

    call process_find_free_slot
    jc .no_process

    mov r13d, eax

    imul eax, PROCESS_SIZE
    lea rbx, [process_table + eax]

    mov byte [rbx + PROCESS_STATE], PROCESS_STATE_READY
    mov byte [rbx + PROCESS_FLAGS], PROCESS_FLAG_USER

    movzx eax, word [process_next_pid]
    mov word [rbx + PROCESS_PID], ax
    inc word [process_next_pid]
    mov r12w, ax

    mov word [rbx + PROCESS_PARENT_PID], r14w

    mov dword [rbx + PROCESS_EXIT_CODE], 0

    push rax
    call process_current_record
    mov al, [rax + PROCESS_CHILD_TTY]
    mov [rbx + PROCESS_TTY], al
    mov [rbx + PROCESS_CHILD_TTY], al
    call process_current_record
    mov eax, [rax + PROCESS_UID]
    cmp dword [spawn_uid], -1
    je .same_user
    mov eax, [spawn_uid]
.same_user:
    mov [rbx + PROCESS_UID], eax
    pop rax

    mov rax, [spawn_cr3]
    mov [rbx + PROCESS_CR3], rax

    mov rax, USER_CODE_ADDRESS
    add rax, [serp_entry_offset]
    mov [rbx + PROCESS_ENTRY], rax

    mov rax, USER_STACK_TOP - SPAWN_ARGS_MAX - 16
    mov [rbx + PROCESS_USER_RSP], rax

    mov rax, TSS_STACK_TOP
    mov [rbx + PROCESS_KERNEL_RSP], rax

    push rdi
    push rcx
    lea rdi, [rbx + PROCESS_SAVED_FRAME]
    xor eax, eax
    mov ecx, PROCESS_SAVED_FRAME_SIZE / 8
    cld
    rep stosq
    pop rcx
    pop rdi

    mov rax, [rbx + PROCESS_ENTRY]
    mov [rbx + PROCESS_SAVED_FRAME + TRAP_RIP], rax
    mov qword [rbx + PROCESS_SAVED_FRAME + TRAP_RDI], USER_STACK_TOP - SPAWN_ARGS_MAX

    mov qword [rbx + PROCESS_SAVED_FRAME + TRAP_CS], USER_CODE_SELECTOR
    mov qword [rbx + PROCESS_SAVED_FRAME + TRAP_RFLAGS], 0x202
    mov qword [rbx + PROCESS_SAVED_FRAME + TRAP_USER_SS], USER_DATA_SELECTOR

    mov rax, [rbx + PROCESS_USER_RSP]
    mov [rbx + PROCESS_SAVED_FRAME + TRAP_USER_RSP], rax

    mov rsi, spawn_name
    lea rdi, [rbx + PROCESS_NAME]
    mov ecx, PROCESS_NAME_SIZE - 1
    cld

.copy_name:
    cmp ecx, 0
    je .terminate_name

    lodsb
    stosb

    test al, al
    jz .name_copied

    dec ecx
    jmp .copy_name

.terminate_name:
    mov al, 0
    stosb

.name_copied:
    call process_get_cwd
    lea rdi, [rbx + PROCESS_CWD]
    mov ecx, PROCESS_CWD_SIZE
    cld
    rep movsb
    cmp byte [spawn_home], 0
    je .cwd_ready
    mov rsi, spawn_home
    lea rdi, [rbx + PROCESS_CWD]
    call fs_copy_string
.cwd_ready:

    mov qword [rbx + PROCESS_HEAP_TOP], 0
    call process_current_record
    lea rsi, [rax + PROCESS_HANDLES]
    lea rdi, [rbx + PROCESS_HANDLES]
    mov ecx, PROCESS_HANDLE_SIZE * (FD_STDERR + 1)
    rep movsb
    xor eax, eax
    mov ecx, PROCESS_HANDLE_TABLE_SIZE - PROCESS_HANDLE_SIZE * (FD_STDERR + 1)
    rep stosb

    lea rdi, [rbx + PROCESS_HANDLES]
    mov ecx, FD_STDERR + 1
.inherit:
    push rcx
    push rdi
    call pipe_handle_retain
    pop rdi
    pop rcx
    add rdi, PROCESS_HANDLE_SIZE
    dec ecx
    jnz .inherit

    mov rax, [serp_code_size]
    mov [rbx + PROCESS_EXEC_CODE_SIZE], rax

    mov eax, [serp_image_pages]
    mov [rbx + PROCESS_EXEC_CODE_PAGES], eax

    mov rax, [serp_stack_size]
    mov [rbx + PROCESS_EXEC_STACK_SIZE], rax

    mov edi, r13d
    call scheduler_ready_enqueue
    jc .enqueue_failed

    movzx eax, r12w
    clc

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    ret

.invalid_path:
    mov eax, SYS_ERR_INVALID
    jmp .error_exit

.denied:
    mov eax, SYS_ERR_ACCESS
    jmp .error_exit

.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .error_exit

.not_executable:
    mov eax, SYS_ERR_BAD_FORMAT
    jmp .error_exit

.bad_header:
    mov eax, SYS_ERR_BAD_FORMAT
    jmp .error_exit

.too_large:
    mov eax, SYS_ERR_NO_MEMORY
    jmp .error_exit

.io_error:
    mov eax, SYS_ERR_IO
    jmp .error_exit

.address_space_error:
    mov eax, SYS_ERR_NO_MEMORY
    jmp .error_exit

.no_process:
    mov rdi, [spawn_cr3]
    call address_space_destroy

    mov eax, SYS_ERR_NO_PROCESS
    jmp .error_exit

.enqueue_failed:
    mov byte [rbx + PROCESS_STATE], PROCESS_STATE_FREE

    mov rdi, [spawn_cr3]
    call address_space_destroy

    mov eax, SYS_ERR_NO_MEMORY

.error_exit:
    stc
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    ret

spawn_cr3 dq 0

spawn_path_buffer:
    times FS_PATH_MAX db 0

spawn_name:
    times PROCESS_NAME_SIZE db 0
