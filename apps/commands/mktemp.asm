include '../runtime/program.inc'

ATTEMPTS = 64

main:
    push rbx
    push r12
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    cmp eax, [arg_count]
    jne .usage

    lea rdi, [directory]
    call sys_mkdir              ; /tmp may not exist yet, an error here is fine
    call sys_getticks
    mov rbx, rax
    mov r12d, ATTEMPTS

.attempt:
    mov rax, rbx
    lea rdi, [name_suffix + 5]
    mov ecx, 6
.digit:
    xor edx, edx
    mov r8d, 36
    div r8
    mov dl, [alphabet + rdx]
    mov [rdi], dl
    dec rdi
    dec ecx
    jnz .digit

    lea rdi, [name_template]
    lea rsi, [status]
    call sys_stat
    test eax, eax
    jns .taken
    lea rdi, [name_template]
    cmp byte [make_directory], 0
    je .file
    call sys_mkdir
    test eax, eax
    js .failed
    jmp .made
.file:
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    mov edi, eax
    call sys_close
.made:
    lea rdi, [name_template]
    call print_line
    xor eax, eax
    jmp .out
.taken:
    imul rbx, rbx, 7919
    add rbx, 104729
    dec r12d
    jnz .attempt
.failed:
    lea rdi, [failed_text]
    call print_line
    mov eax, 1
.out:
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'mktemp', 0
usage_text db 'usage: mktemp [-d]', 0
failed_text db 'mktemp: cannot create a temporary name', 0
flag_letters db 'd', 0
flags:
make_directory db 0
alphabet db '0123456789abcdefghijklmnopqrstuvwxyz'
directory db '/tmp', 0
name_template db '/tmp/tmp.'
name_suffix db 'XXXXXX', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
status rb USER_STAT_SIZE
