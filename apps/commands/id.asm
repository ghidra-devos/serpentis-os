; id [user]. uid=1000(guest), no groups to show

include '../runtime/program.inc'

main:
    push rbx
    call arg_next
    test rax, rax
    jz .self
    mov rdi, rax
    call user_uid_of
    jc .unknown
    mov ebx, eax
    jmp .print
.self:
    call sys_getuid
    mov ebx, eax
.print:
    lea rdi, [uid_text]
    call print
    mov edi, ebx
    call u64_to_dec
    mov rdi, rax
    call print
    mov edi, '('
    call print_char
    mov edi, ebx
    call user_name
    mov rdi, rax
    call print
    mov edi, ')'
    call print_char
    mov edi, 10
    call print_char
    xor eax, eax
    pop rbx
    ret
.unknown:
    lea rdi, [unknown_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

uid_text db 'uid=', 0
unknown_text db 'id: no such user', 0

include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/users_bss.inc'
