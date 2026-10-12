; file syscalls. paths from user space are copied in, resolved against the process's own cwd
; (the kernel's fs_current_path is only for the kernel commands) and handed to filesystem.asm

; rdi = user string, rsi -> kernel buffer of FS_PATH_MAX. CF on a bad pointer or no terminator
; checks the pointer for every single byte, one call per char. slow, but correct
syscall_copy_user_string:
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push rdi
    push rsi

    mov r12, rdi
    mov r13, rsi
    xor r10d, r10d

.loop:
    cmp r10d, FS_PATH_MAX - 1
    jae .failed

    mov rdi, r12
    add rdi, r10
    mov rdx, 1
    call syscall_user_range_valid
    jc .failed

    mov al, [r12 + r10]
    mov [r13 + r10], al

    test al, al
    jz .success

    inc r10d
    jmp .loop

.success:
    pop rsi
    pop rdi
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    clc
    ret

.failed:
    pop rsi
    pop rdi
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    stc
    ret

; rdi = kernel dst, rsi = user src, rdx = bytes. CF if the user range is not ours
syscall_copy_from_user:
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov rdi, r13
    mov rdx, r14
    call syscall_user_range_valid
    jc .failed

    mov rdi, r12
    mov rsi, r13
    mov rcx, r14
    cld
    rep movsb

    pop r14
    pop r13
    pop r12
    clc
    ret

.failed:
    pop r14
    pop r13
    pop r12
    stc
    ret

syscall_copy_to_user:
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov rdi, r12
    mov rdx, r14
    call syscall_user_range_valid
    jc .failed

    mov rdi, r12
    mov rsi, r13
    mov rcx, r14
    cld
    rep movsb

    pop r14
    pop r13
    pop r12
    clc
    ret

.failed:
    pop r14
    pop r13
    pop r12
    stc
    ret

HANDLE_USED = PROCESS_HANDLE_USED
HANDLE_READ = PROCESS_HANDLE_READ
HANDLE_WRITE = PROCESS_HANDLE_WRITE
HANDLE_CONSOLE = PROCESS_HANDLE_CONSOLE
HANDLE_PIPE = PROCESS_HANDLE_PIPE
HANDLE_FLAGS = PROCESS_HANDLE_FLAGS
HANDLE_OWNER_PID = PROCESS_HANDLE_OWNER
HANDLE_ENTRY_INDEX = PROCESS_HANDLE_ENTRY
HANDLE_OFFSET = PROCESS_HANDLE_OFFSET

syscall_handle_get:
    mov eax, edi
    call process_handle_address
    jc .failed

    cmp byte [rax + HANDLE_FLAGS], 0
    je .failed

    clc
    ret

.failed:
    xor eax, eax
    stc
    ret

syscall_handle_find_free:
    call process_handle_find_free
    ret

; edi = fd 0-2. CF if it was dup2'ed onto a file or pipe, clear if it still means the console
standard_stream_redirected:
    push rdx
    push rdi
    call process_handle_address
    jc .console
    mov al, [rax + HANDLE_FLAGS]
    test al, HANDLE_USED
    jz .console
    test al, HANDLE_CONSOLE
    jnz .console
    pop rdi
    pop rdx
    stc
    ret
.console:
    pop rdi
    pop rdx
    clc
    ret

syscall_time:
    push rbx
    sub rsp, 16
    mov rbx, rdi
    mov rdx, TIME_BYTES
    call syscall_user_range_valid
    jc .bad_address

    call rtc_read_datetime

    mov al, [rtc_second]
    mov [rsp + TIME_SECOND], al
    mov al, [rtc_minute]
    mov [rsp + TIME_MINUTE], al
    mov al, [rtc_hour]
    mov [rsp + TIME_HOUR], al
    mov al, [rtc_day]
    mov [rsp + TIME_DAY], al
    mov al, [rtc_month]
    mov [rsp + TIME_MONTH], al
    mov byte [rsp + TIME_MONTH + 1], 0
    mov ax, [rtc_year]
    mov [rsp + TIME_YEAR], ax

    mov rdi, rbx
    mov rsi, rsp
    mov edx, TIME_BYTES
    call syscall_copy_to_user
    jc .bad_address

    xor eax, eax
    add rsp, 16
    pop rbx
    ret

.bad_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    add rsp, 16
    pop rbx
    ret

syscall_dup2:
    push rbx
    mov ebx, esi
    call syscall_handle_get
    jc .invalid
    mov rsi, rax
    mov edi, ebx
    call process_handle_address
    jc .invalid
    mov rdi, rax
    call pipe_handle_release
    mov ecx, PROCESS_HANDLE_SIZE
    cld
    rep movsb
    mov edi, ebx
    call process_handle_address
    mov rdi, rax
    call pipe_handle_retain
    mov eax, ebx
    pop rbx
    ret
.invalid:
    mov eax, SYS_ERR_INVALID
    pop rbx
    ret

; rdi = path, esi = OPEN_* flags. out: eax = fd or SYS_ERR_*. directories open read only
; so readdir can walk them
syscall_open:
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi
    mov r13d, esi

    mov rdi, r12
    mov rsi, syscall_path_buffer
    call syscall_copy_user_string
    jc .invalid_address

    mov rdi, syscall_path_buffer
    mov rsi, syscall_resolved_path
    call syscall_resolve_path
    jc .invalid_path

    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .existing

    test r13d, OPEN_CREATE
    jz .not_found

    mov dl, FS_FLAG_USED
    call fs_create_entry
    jc .create_failed

    mov r14d, eax
    jmp .allocate_handle

.existing:
    mov r14d, eax

    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .check_access
    test r13d, OPEN_WRITE
    jnz .is_directory

.check_access:
    xor ecx, ecx
    test r13d, OPEN_READ
    jz .want_write
    or cl, ACCESS_READ
.want_write:
    test r13d, OPEN_WRITE
    jz .want_ready
    or cl, ACCESS_WRITE
.want_ready:
    mov eax, r14d
    call fs_access
    jc .access

.allocate_handle:
    call syscall_handle_find_free
    jc .too_many_handles

    mov r15d, eax

    mov edi, r15d
    call process_handle_address
    jc .too_many_handles

    mov rbx, rax

    mov byte [rbx + HANDLE_FLAGS], HANDLE_USED

    test r13d, OPEN_READ
    jz .no_read
    or byte [rbx + HANDLE_FLAGS], HANDLE_READ

.no_read:
    test r13d, OPEN_WRITE
    jz .no_write
    or byte [rbx + HANDLE_FLAGS], HANDLE_WRITE

.no_write:
    mov [rbx + HANDLE_ENTRY_INDEX], r14d
    mov dword [rbx + HANDLE_OFFSET], 0

    mov eax, r15d

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .return_error

.invalid_path:
    mov eax, SYS_ERR_INVALID
    jmp .return_error

.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .return_error

.create_failed:
    mov eax, SYS_ERR_IO
    cmp byte [fs_denied], 0
    je .return_error
.access:
    mov eax, SYS_ERR_ACCESS
    jmp .return_error

.is_directory:
    mov eax, SYS_ERR_IS_DIRECTORY
    jmp .return_error

.too_many_handles:
    mov eax, SYS_ERR_FULL

.return_error:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

syscall_close:
    push rbx

    call syscall_handle_get
    jc .invalid

    mov rbx, rax

    mov rdi, rbx
    call pipe_handle_release

    mov byte [rbx + HANDLE_FLAGS], 0
    mov word [rbx + HANDLE_OWNER_PID], 0

    mov eax, SYS_OK

    pop rbx
    ret

.invalid:
    mov eax, SYS_ERR_INVALID
    pop rbx
    ret

syscall_read:
    push rbx
    push rcx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    mov r13, rsi
    mov r14d, edx
    mov r15, rcx

    test r14d, r14d
    jz .zero

    mov edi, r12d
    call syscall_handle_get
    jc .invalid

    mov rbx, rax

    test byte [rbx + HANDLE_FLAGS], HANDLE_READ
    jz .access

    test byte [rbx + HANDLE_FLAGS], HANDLE_PIPE
    jz .regular
    mov edi, [rbx + HANDLE_ENTRY_INDEX]
    mov rsi, r13
    mov edx, r14d
    mov rcx, r15
    call pipe_read
    jmp .return

.regular:
    movzx eax, word [rbx + HANDLE_ENTRY_INDEX]
    mov r15d, eax

    call fs_load_file
    jc .io

    mov eax, r15d
    shl eax, FS_ENTRY_SHIFT
    lea rdx, [fs_entries + eax]

    mov ecx, [rbx + HANDLE_OFFSET]
    cmp ecx, [rdx + FS_ENTRY_FILE_SIZE]
    jae .zero

    mov eax, [rdx + FS_ENTRY_FILE_SIZE]
    sub eax, ecx

    cmp r14d, eax
    jbe .count_ready
    mov r14d, eax

.count_ready:
    mov eax, [rbx + HANDLE_OFFSET]
    lea rsi, [fs_file_buffer + rax]
    mov rdi, r13
    mov rdx, r14
    call syscall_copy_to_user
    jc .invalid_address

    add [rbx + HANDLE_OFFSET], r14d

    mov eax, r14d

    pop r15
    pop r14
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

.zero:
    xor eax, eax
    jmp .return

.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .return

.access:
    mov eax, SYS_ERR_ACCESS
    jmp .return

.io:
    mov eax, SYS_ERR_IO
    jmp .return

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS

.return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

; every write() puts the whole file back on disk. slow as hell for big files but i trust
; it more than a write cache i'd have to remember to flush
syscall_write_file:
    push rbx
    push rcx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    mov r13, rsi
    mov r14d, edx
    mov r15, rcx

    test r14d, r14d
    jz .zero

    mov edi, r12d
    call syscall_handle_get
    jc .invalid

    mov rbx, rax

    test byte [rbx + HANDLE_FLAGS], HANDLE_WRITE
    jz .access

    test byte [rbx + HANDLE_FLAGS], HANDLE_PIPE
    jz .regular
    mov edi, [rbx + HANDLE_ENTRY_INDEX]
    mov rsi, r13
    mov edx, r14d
    mov rcx, r15
    call pipe_write
    jmp .return

.regular:
    movzx eax, word [rbx + HANDLE_ENTRY_INDEX]
    mov r15d, eax

    call fs_load_file
    jc .io

    mov eax, [rbx + HANDLE_OFFSET]
    add eax, r14d
    jc .too_large

    cmp eax, FS_FILE_BYTES
    ja .too_large

    mov eax, [rbx + HANDLE_OFFSET]
    lea rdi, [fs_file_buffer + rax]
    mov rsi, r13
    mov rdx, r14
    call syscall_copy_from_user
    jc .copy_failed

    mov eax, [rbx + HANDLE_OFFSET]
    add eax, r14d
    cmp eax, [fs_file_size]
    jbe .size_ok
    mov [fs_file_size], eax

.size_ok:
    mov eax, r15d
    call fs_store_file
    jc .io

    add [rbx + HANDLE_OFFSET], r14d

    mov eax, r14d

    pop r15
    pop r14
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

.zero:
    xor eax, eax
    jmp .return

.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .return

.access:
    mov eax, SYS_ERR_ACCESS
    jmp .return

.too_large:
    mov eax, SYS_ERR_FULL
    jmp .return

.io:
    mov eax, SYS_ERR_IO
    jmp .return

.copy_failed:
    mov dword [fs_cached_file], -1
.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS

.return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

syscall_stat:
    push rbx
    push r12
    push r13

    mov r12, rdi
    mov r13, rsi

    mov rdi, r12
    mov rsi, syscall_path_buffer
    call syscall_copy_user_string
    jc .invalid_address

    mov rdi, syscall_path_buffer
    mov rsi, syscall_resolved_path
    call syscall_resolve_path
    jc .invalid_path

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found

    mov ebx, eax
    shl ebx, FS_ENTRY_SHIFT

    mov eax, [fs_entries + rbx + FS_ENTRY_FILE_SIZE]
    mov [syscall_stat_buffer + USER_STAT_FILE_SIZE], rax

    movzx eax, byte [fs_entries + rbx + FS_ENTRY_FLAGS]
    mov [syscall_stat_buffer + USER_STAT_FLAGS], eax

    mov eax, [fs_entries + rbx + FS_ENTRY_MTIME]
    mov [syscall_stat_buffer + USER_STAT_MTIME], eax

    movzx eax, word [fs_entries + rbx + FS_ENTRY_OWNER]
    mov [syscall_stat_buffer + USER_STAT_OWNER], eax
    movzx eax, word [fs_entries + rbx + FS_ENTRY_MODE]
    mov [syscall_stat_buffer + USER_STAT_MODE], eax

    mov rdi, r13
    mov rsi, syscall_stat_buffer
    mov edx, USER_STAT_SIZE
    call syscall_copy_to_user
    jc .invalid_address

    mov eax, SYS_OK

    pop r13
    pop r12
    pop rbx
    ret

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .return

.invalid_path:
    mov eax, SYS_ERR_INVALID
    jmp .return

.not_found:
    mov eax, SYS_ERR_NOT_FOUND

.return:
    pop r13
    pop r12
    pop rbx
    ret

syscall_seek:
    push rbx
    push rcx
    push rdx
    push r12
    push r13
    push r14

    mov r12d, edi
    mov r13, rsi
    mov r14d, edx

    mov edi, r12d
    call syscall_handle_get
    jc .invalid

    mov rbx, rax
    test byte [rbx + HANDLE_FLAGS], HANDLE_CONSOLE
    jnz .invalid

    movzx eax, word [rbx + HANDLE_ENTRY_INDEX]
    shl eax, FS_ENTRY_SHIFT
    lea rdx, [fs_entries + rax]

    cmp r14d, SEEK_SET
    je .from_start

    cmp r14d, SEEK_CUR
    je .from_current

    cmp r14d, SEEK_END
    je .from_end

    mov eax, SYS_ERR_INVALID
    jmp .return

.from_start:
    xor rax, rax
    jmp .apply_offset

.from_current:
    mov eax, [rbx + HANDLE_OFFSET]
    jmp .apply_offset

.from_end:
    mov eax, [rdx + FS_ENTRY_FILE_SIZE]

.apply_offset:
    add rax, r13

    test rax, rax
    js .invalid

    cmp rax, FS_FILE_BYTES
    ja .too_large

    mov [rbx + HANDLE_OFFSET], eax

    jmp .return

.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .return

.too_large:
    mov eax, SYS_ERR_FULL

.return:
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    ret

syscall_resolve_path:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13, rsi

    cmp byte [r12], '/'
    je .absolute

    call process_get_cwd
    mov r14, rsi

    call str_length
    mov rbx, rax

    mov rsi, r12
    call str_length
    mov rdx, rax

    mov rax, rbx
    cmp rbx, 1
    je .root_cwd
    inc rax

.root_cwd:
    add rax, rdx
    inc rax

    cmp rax, FS_PATH_MAX
    jae .failed

    mov rdi, r13
    mov rsi, r14
    mov rcx, rbx
    cld
    rep movsb

    cmp rbx, 1
    je .append_relative
    mov al, '/'
    stosb

.append_relative:
    mov rsi, r12
    mov rcx, rdx
    rep movsb

    mov byte [rdi], 0

    mov rsi, r13
    call fs_make_path
    jc .failed
    jmp .success

.absolute:
    mov rsi, r12
    call fs_make_path
    jc .failed

.success:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    clc
    ret

.failed:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    stc
    ret

syscall_resolved_path:
    times FS_PATH_MAX db 0

syscall_getcwd:
    push rbx
    push r12

    mov r12, rdi
    mov rbx, rsi

    test rbx, rbx
    jz .invalid

    cmp rbx, FS_PATH_MAX
    jb .invalid

    call process_get_cwd

    mov rdi, r12
    mov rdx, FS_PATH_MAX
    call syscall_copy_to_user
    jc .invalid_address

    mov eax, SYS_OK

    pop r12
    pop rbx
    ret

.invalid:
    mov eax, SYS_ERR_INVALID
    pop r12
    pop rbx
    ret

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    pop r12
    pop rbx
    ret

syscall_chdir:
    push rbx
    push r12

    mov r12, rdi

    mov rsi, syscall_path_buffer
    call syscall_copy_user_string
    jc .invalid_address

    mov rdi, syscall_path_buffer
    mov rsi, syscall_resolved_path
    call syscall_resolve_path
    jc .invalid_path

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found

    mov ebx, eax
    shl ebx, FS_ENTRY_SHIFT
    test byte [fs_entries + ebx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .not_directory

    mov cl, ACCESS_EXEC
    call fs_access
    jc .access

    mov rsi, fs_path_buffer
    call process_set_cwd

    mov eax, SYS_OK
    jmp .return

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    jmp .return

.invalid_path:
    mov eax, SYS_ERR_INVALID
    jmp .return

.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .return

.not_directory:
    mov eax, SYS_ERR_NOT_DIRECTORY
    jmp .return

.access:
    mov eax, SYS_ERR_ACCESS

.return:
    pop r12
    pop rbx
    ret

syscall_clear_user_record:
    push r12
    push r13
    push r14

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov rdi, r12
    mov rdx, r14
    call syscall_user_range_valid
    jc .failed

    mov rdi, r13
    xor eax, eax
    mov rcx, r14
    cld
    rep stosb

    pop r14
    pop r13
    pop r12
    clc
    ret

.failed:
    pop r14
    pop r13
    pop r12
    stc
    ret

fs_print_basename_to_buffer:
    push rax
    push rbx
    push rsi
    push rdi
    push r12
    push r13

    mov r12, rsi
    mov r13, rdi

    mov rbx, rsi

.scan:
    mov al, [rsi]
    test al, al
    jz .copy

    cmp al, '/'
    jne .next
    lea rbx, [rsi + 1]

.next:
    inc rsi
    jmp .scan

.copy:
    mov rsi, rbx
    mov rdi, r13
    mov ecx, FS_PATH_MAX - 1
    cld

.loop:
    test ecx, ecx
    jz .terminate

    lodsb
    stosb

    test al, al
    jz .done

    dec ecx
    jmp .loop

.terminate:
    mov al, 0
    stosb

.done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rax
    ret

; edi = fd of an open directory, rsi -> SYS_DIRENT record. returns the next child after the
; handle offset (which is an entry index here, not a byte offset).
; out: eax = SYS_DIRENT_BYTES for a record, 0 at the end
syscall_readdir:
    push rbx
    push rcx
    push r12
    push r13
    push r14

    mov r12d, edi
    mov r13, rsi
    mov r14d, edx

    cmp r14d, SYS_DIRENT_BYTES
    jb .invalid

    mov edi, r12d
    call syscall_handle_get
    jc .invalid

    mov rbx, rax

    movzx eax, word [rbx + HANDLE_ENTRY_INDEX]
    shl eax, FS_ENTRY_SHIFT

    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .not_directory

    mov r12d, [rbx + HANDLE_OFFSET]

.find_entry:
    cmp r12d, FS_MAX_FILES
    jae .end

    mov eax, r12d
    shl eax, FS_ENTRY_SHIFT
    lea r14, [fs_entries + rax]

    test byte [r14 + FS_ENTRY_FLAGS], FS_FLAG_USED
    jz .next

    movzx eax, word [r14 + FS_ENTRY_PARENT]
    mov edx, [rbx + HANDLE_ENTRY_INDEX]
    cmp eax, edx
    jne .next
    cmp r12d, edx
    je .next

    mov rdi, r13
    mov rsi, syscall_dirent_buffer
    mov edx, SYS_DIRENT_BYTES
    call syscall_clear_user_record
    jc .invalid_address

    lea rsi, [r14 + FS_ENTRY_PATH]
    mov rdi, syscall_dirent_buffer + SYS_DIRENT_NAME
    call fs_print_basename_to_buffer

    movzx eax, byte [r14 + FS_ENTRY_FLAGS]
    mov [syscall_dirent_buffer + SYS_DIRENT_FLAGS], eax

    mov eax, [r14 + FS_ENTRY_FILE_SIZE]
    mov [syscall_dirent_buffer + SYS_DIRENT_SIZE], eax

    mov eax, r12d
    mov [syscall_dirent_buffer + SYS_DIRENT_ENTRY], eax
    mov ax, [r14 + FS_ENTRY_OWNER]
    mov [syscall_dirent_buffer + SYS_DIRENT_OWNER], ax
    mov ax, [r14 + FS_ENTRY_MODE]
    mov [syscall_dirent_buffer + SYS_DIRENT_MODE], ax

    mov rdi, r13
    mov rsi, syscall_dirent_buffer
    mov edx, SYS_DIRENT_BYTES
    call syscall_copy_to_user
    jc .invalid_address

    inc r12d
    mov [rbx + HANDLE_OFFSET], r12d

    mov eax, SYS_DIRENT_BYTES
    jmp .return

.next:
    inc r12d
    jmp .find_entry

.end:
    xor eax, eax
    jmp .return

.invalid:
    mov eax, SYS_ERR_INVALID
    jmp .return

.not_directory:
    mov eax, SYS_ERR_NOT_DIRECTORY
    jmp .return

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS

.return:
    pop r14
    pop r13
    pop r12
    pop rcx
    pop rbx
    ret

align 8
syscall_path_buffer:
    times FS_PATH_MAX db 0

syscall_stat_buffer:
    times USER_STAT_SIZE db 0

syscall_dirent_buffer:
    times SYS_DIRENT_BYTES db 0

syscall_user_path_resolve:
    push rsi
    push rdi
    mov rsi, syscall_path_buffer
    call syscall_copy_user_string
    jc .invalid_address
    mov rdi, syscall_path_buffer
    mov rsi, syscall_resolved_path
    call syscall_resolve_path
    jc .invalid_path
    pop rdi
    pop rsi
    clc
    ret

.invalid_address:
    mov eax, SYS_ERR_INVALID_ADDRESS
    pop rdi
    pop rsi
    stc
    ret

.invalid_path:
    mov eax, SYS_ERR_NOT_FOUND
    pop rdi
    pop rsi
    stc
    ret

syscall_mkdir:
    call syscall_user_path_resolve
    jc .done

    mov rsi, fs_path_buffer
    call fs_find_path
    jnc .exists

    mov dl, FS_FLAG_USED or FS_FLAG_DIRECTORY
    call fs_create_entry
    jc .no_parent

    xor eax, eax
    ret

.exists:
    mov eax, SYS_ERR_EXISTS
    ret

.no_parent:
    mov eax, SYS_ERR_NOT_FOUND
    cmp byte [fs_denied], 0
    je .done
    mov eax, SYS_ERR_ACCESS

.done:
    ret

; files and empty directories only, never the root
syscall_unlink:
    push rbx

    call syscall_user_path_resolve
    jc .done

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found

    test eax, eax
    jz .access
    mov ebx, eax

    shl eax, FS_ENTRY_SHIFT
    test byte [fs_entries + eax + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jz .delete

    mov eax, ebx
    call fs_directory_has_children
    jc .busy

.delete:
    mov eax, ebx
    call fs_delete_entry
    jc .io
    xor eax, eax
    jmp .done

.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .done

.access:
    mov eax, SYS_ERR_ACCESS
    jmp .done

.busy:
    mov eax, SYS_ERR_BUSY
    jmp .done

.io:
    mov eax, SYS_ERR_IO
    cmp byte [fs_denied], 0
    je .done
    mov eax, SYS_ERR_ACCESS

.done:
    pop rbx
    ret

syscall_rename:
    push rbx
    push r12

    mov r12, rsi

    call syscall_user_path_resolve
    jc .done

    mov rsi, fs_path_buffer
    call fs_find_path
    jc .not_found
    test eax, eax
    jz .access
    mov ebx, eax

    mov rdi, r12
    call syscall_user_path_resolve
    jc .done

    mov eax, ebx
    mov rsi, fs_path_buffer
    call fsx_move_tree
    jc .failed

    xor eax, eax
    jmp .done

.not_found:
    mov eax, SYS_ERR_NOT_FOUND
    jmp .done

.access:
    mov eax, SYS_ERR_ACCESS
    jmp .done

.failed:
    mov eax, SYS_ERR_EXISTS
    cmp byte [fs_denied], 0
    je .done
    mov eax, SYS_ERR_ACCESS

.done:
    pop r12
    pop rbx
    ret
