; chown user file... root only, the kernel says no to anybody else

include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    cmp dword [arg_count], 2
    jb .usage
    mov rdi, [arg_list]
    call user_uid_of
    jc .unknown
    mov ebx, eax
    mov r12d, 1
    xor r13d, r13d
.file:
    cmp r12d, [arg_count]
    jae .done
    mov rdi, [arg_list + r12 * 8]
    mov esi, ebx
    call sys_chown
    test eax, eax
    jns .next
    lea rdi, [program_name]
    mov rsi, [arg_list + r12 * 8]
    mov edx, eax
    call report_error
    mov r13d, 1
.next:
    inc r12d
    jmp .file
.done:
    mov eax, r13d
    pop r13
    pop r12
    pop rbx
    ret
.unknown:
    lea rdi, [unknown_text]
    call print_line
    mov eax, 1
    pop r13
    pop r12
    pop rbx
    ret
.usage:
    pop r13
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'chown', 0
usage_text db 'usage: chown user file...', 0
unknown_text db 'chown: no such user', 0

include 'common.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
include '../runtime/users_bss.inc'
