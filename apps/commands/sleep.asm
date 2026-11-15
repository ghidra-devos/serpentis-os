include '../runtime/program.inc'

main:
    push rbx
    call collect_args
    cmp dword [arg_count], 1
    jne .usage
    mov rdi, [arg_list]
    call parse_number
    jc .usage
    imul rbx, rax, PIT_FREQUENCY
    call sys_getticks
    add rbx, rax
.wait:
    call sys_getticks
    cmp rax, rbx
    jae .done
    call sys_yield
    jmp .wait
.done:
    xor eax, eax
    pop rbx
    ret

.usage:
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'sleep', 0
usage_text db 'usage: sleep seconds', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
