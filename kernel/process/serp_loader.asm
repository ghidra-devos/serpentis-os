SERP_MAX_STACK = 0x10000

serp_parse_header:
    push rdx

    cmp dword [fs_file_size], SERP_HEADER_SIZE
    jb .bad_header

    cmp dword [fs_file_buffer + SERP_HEADER_MAGIC], SERP_MAGIC
    jne .bad_header

    cmp word [fs_file_buffer + SERP_HEADER_VERSION], SERP_VERSION
    jne .bad_header

    test word [fs_file_buffer + SERP_HEADER_FLAGS], SERP_FLAG_USER
    jz .not_executable

    cmp qword [fs_file_buffer + SERP_HEADER_HEADER_SIZE], SERP_HEADER_SIZE
    jne .bad_header

    cmp qword [fs_file_buffer + SERP_HEADER_DATA_SIZE], 0  ; separate data segment never got used, reject it
    jne .bad_header

    mov rax, [fs_file_buffer + SERP_HEADER_ENTRY]
    mov [serp_entry_offset], rax

    mov rax, [fs_file_buffer + SERP_HEADER_CODE_OFFSET]
    mov [serp_code_offset], rax

    mov rax, [fs_file_buffer + SERP_HEADER_CODE_SIZE]
    mov [serp_code_size], rax
    test rax, rax
    jz .bad_header
    cmp rax, EXEC_MAX_CODE_BYTES
    ja .too_large

    add rax, 0xFFF
    shr rax, 12
    mov [serp_code_pages], eax

    mov rax, [serp_entry_offset]
    cmp rax, [serp_code_size]
    jae .bad_header

    mov rax, [serp_code_offset]
    add rax, [serp_code_size]
    jc .bad_header
    mov edx, [fs_file_size]
    cmp rax, rdx
    ja .bad_header

    mov rax, [fs_file_buffer + SERP_HEADER_REQUIRED_PAGES]
    test rax, rax
    jz .bss
    cmp eax, [serp_code_pages]
    jne .bad_header

.bss:
    mov rax, [fs_file_buffer + SERP_HEADER_BSS_SIZE]
    cmp rax, EXEC_MAX_CODE_BYTES
    ja .too_large
    add rax, 0xFFF
    shr rax, 12
    mov [serp_bss_pages], eax
    add eax, [serp_code_pages]
    cmp eax, EXEC_MAX_CODE_PAGES
    ja .too_large
    mov [serp_image_pages], eax

    mov rax, [fs_file_buffer + SERP_HEADER_STACK_SIZE]
    test rax, rax
    jnz .stack_given
    mov eax, EXEC_DEFAULT_STACK_SIZE

.stack_given:
    cmp rax, SERP_MAX_STACK
    ja .bad_header
    add rax, 0xFFF
    shr rax, 12
    mov [serp_stack_pages], eax
    shl rax, 12
    mov [serp_stack_size], rax

    pop rdx
    clc
    ret

.bad_header:
    mov al, EXEC_ERROR_BAD_HEADER
    jmp .failed

.not_executable:
    mov al, EXEC_ERROR_NOT_EXECUTABLE
    jmp .failed

.too_large:
    mov al, EXEC_ERROR_TOO_LARGE

.failed:
    pop rdx
    stc
    ret

serp_copy_image:
    push rcx
    push rsi
    push rdi
    push r12
    push r13
    push r14

    mov rsi, fs_file_buffer
    add rsi, [serp_code_offset]
    mov r13, [serp_code_size]
    xor r12d, r12d

.next_page:
    test r13, r13
    jz .done
    mov rdi, [as_code_pages + r12 * 8]
    mov r14, AS_PAGE_SIZE
    cmp r13, r14
    jae .copy
    mov r14, r13
.copy:
    mov rcx, r14
    cld
    rep movsb
    sub r13, r14
    inc r12d
    jmp .next_page

.done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rcx
    ret

align 8
serp_entry_offset dq 0
serp_code_offset dq 0
serp_code_size dq 0
serp_stack_size dq 0
serp_code_pages dd 0
serp_bss_pages dd 0
serp_image_pages dd 0
serp_stack_pages dd 0
