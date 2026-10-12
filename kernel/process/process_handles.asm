PROCESS_HANDLE_USED = 1
PROCESS_HANDLE_READ = 2
PROCESS_HANDLE_WRITE = 4
PROCESS_HANDLE_CONSOLE = 8
PROCESS_HANDLE_PIPE = 16

PROCESS_HANDLE_FLAGS = 0

PROCESS_HANDLE_OWNER = 2        ; fs entry index, or pipe number for pipe handles
PROCESS_HANDLE_ENTRY = 4
PROCESS_HANDLE_OFFSET = 8

process_current_record:
    movzx eax, word [process_current_index]
    imul eax, PROCESS_SIZE
    lea rax, [process_table + rax]
    ret

process_handle_address:
    cmp edi, PROCESS_HANDLE_MAX
    jae .failed

    movzx eax, word [process_current_index]
    imul eax, PROCESS_SIZE
    add eax, PROCESS_HANDLES

    mov edx, edi
    imul edx, PROCESS_HANDLE_SIZE
    add eax, edx

    lea rax, [process_table + rax]
    clc
    ret

.failed:
    xor eax, eax
    stc
    ret

process_handle_find_free:
    mov ecx, FD_STDERR + 1

.scan:
    cmp ecx, PROCESS_HANDLE_MAX
    jae .failed

    mov edi, ecx
    call process_handle_address
    jc .failed

    cmp byte [rax + PROCESS_HANDLE_FLAGS], 0
    je .found

    inc ecx
    jmp .scan

.found:
    mov eax, ecx
    clc
    ret

.failed:
    xor eax, eax
    stc
    ret

process_handles_clear_current:
    push rax
    push rcx
    push rdi

    call process_current_record
    add rax, PROCESS_HANDLES
    mov rdi, rax

    xor eax, eax
    mov ecx, PROCESS_HANDLE_TABLE_SIZE
    cld
    rep stosb

    pop rdi
    pop rcx
    pop rax
    ret

process_handles_init_current:
    push rax
    push rbx
    push rcx
    push rdi

    call process_handles_clear_current

    mov edi, FD_STDIN
    call process_handle_address
    jc .done
    mov byte [rax + PROCESS_HANDLE_FLAGS], PROCESS_HANDLE_USED or PROCESS_HANDLE_READ or PROCESS_HANDLE_CONSOLE

    mov edi, FD_STDOUT
    call process_handle_address
    jc .done
    mov byte [rax + PROCESS_HANDLE_FLAGS], PROCESS_HANDLE_USED or PROCESS_HANDLE_WRITE or PROCESS_HANDLE_CONSOLE

    mov edi, FD_STDERR
    call process_handle_address
    jc .done
    mov byte [rax + PROCESS_HANDLE_FLAGS], PROCESS_HANDLE_USED or PROCESS_HANDLE_WRITE or PROCESS_HANDLE_CONSOLE

.done:
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret

process_cwd_init_current:
    push rax
    push rcx
    push rdi

    call process_current_record
    add rax, PROCESS_CWD
    mov rdi, rax

    xor eax, eax
    mov ecx, PROCESS_CWD_SIZE
    cld
    rep stosb

    call process_current_record
    add rax, PROCESS_CWD
    mov byte [rax], '/'
    mov byte [rax + 1], 0

    pop rdi
    pop rcx
    pop rax
    ret

process_get_cwd:
    call process_current_record
    add rax, PROCESS_CWD
    mov rsi, rax
    ret

process_set_cwd:
    push rax
    push rcx
    push rdi
    push rsi

    call process_current_record
    add rax, PROCESS_CWD
    mov rdi, rax

    mov rcx, PROCESS_CWD_SIZE - 1
    cld

.copy:
    test rcx, rcx
    jz .terminate

    lodsb
    stosb

    test al, al
    jz .done

    dec rcx
    jmp .copy

.terminate:
    mov al, 0
    stosb

.done:
    pop rsi
    pop rdi
    pop rcx
    pop rax
    ret
