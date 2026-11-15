include '../runtime/program.inc'

SYSINFO_RELEASE = 32

main:
    push rbx
    xor ebx, ebx
    call arg_next
    test rax, rax
    jz .info
    mov rdi, rax
    lea rsi, [release_flag]
    call str_equal
    mov ebx, eax
.info:
    lea rdi, [info]
    call sys_sysinfo
    test ebx, ebx
    jnz .release
    lea rdi, [name_text]
    call print
.release:
    lea rdi, [info + SYSINFO_RELEASE]
    call print_line
    pop rbx
    xor eax, eax
    ret

name_text db 'serpentis ', 0
release_flag db '-r', 0

include '../runtime/library.inc'

info rq 6
