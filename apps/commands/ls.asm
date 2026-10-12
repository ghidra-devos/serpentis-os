include '../runtime/program.inc'

main:
    push rbx
    push r12

    call arg_next
    test rax, rax
    jnz .open
    lea rax, [current_dir]

.open:
    mov r12, rax
    mov rdi, rax
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax

.next:
    mov edi, ebx
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .done

    lea rdi, [dirent + SYS_DIRENT_NAME]
    call print
    test dword [dirent + SYS_DIRENT_FLAGS], FS_FLAG_DIRECTORY
    jz .file

    mov edi, '/'
    call print_char
    mov edi, 10
    call print_char
    jmp .next

.file:
    lea rdi, [size_gap]
    call print
    mov edi, [dirent + SYS_DIRENT_SIZE]
    call print_u64
    mov edi, 10
    call print_char
    jmp .next

.done:
    mov edi, ebx
    call sys_close
    xor eax, eax
    pop r12
    pop rbx
    ret

.failed:
    lea rdi, [program_name]
    mov rsi, r12
    mov edx, eax
    call report_error
    mov eax, 1
    pop r12
    pop rbx
    ret

program_name db 'ls', 0
current_dir db '.', 0
size_gap db '  ', 0

align 8
dirent:
    times SYS_DIRENT_BYTES db 0

include '../runtime/library.inc'
