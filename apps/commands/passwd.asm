; passwd [user]. your own password needs the old one first, root just sets anybody's

include '../runtime/program.inc'

main:
    push rbx
    push r12
    mov r12, rdi
    call sys_getuid
    mov ebx, eax
    mov rdi, r12
    call arg_next
    test rax, rax
    jnz .named
    mov edi, ebx
    call user_name
    lea rdi, [user]
    mov rsi, rax
    call str_copy
    jmp .ready
.named:
    lea rdi, [user]
    mov rsi, rax
    call str_copy

.ready:
    test ebx, ebx
    jz .new
    lea rdi, [old_prompt]
    call print
    lea rdi, [old]
    call read_secret
.new:
    lea rdi, [new_prompt]
    call print
    lea rdi, [new]
    call read_secret
    lea rdi, [again_prompt]
    call print
    lea rdi, [again]
    call read_secret
    lea rdi, [new]
    lea rsi, [again]
    call str_equal
    test eax, eax
    jz .mismatch

    lea rdi, [user]
    lea rsi, [old]
    lea rdx, [new]
    call sys_passwd
    mov ebx, eax
    call forget
    test ebx, ebx
    js .failed
    lea rdi, [done_text]
    call print_line
    xor eax, eax
    jmp .out

.mismatch:
    call forget
    lea rdi, [mismatch_text]
    call print_line
    mov eax, 1
    jmp .out
.failed:
    lea rdi, [program_name]
    lea rsi, [user]
    mov edx, ebx
    call report_error
    mov eax, 1
.out:
    pop r12
    pop rbx
    ret

forget:
    lea rdi, [old]
    xor esi, esi
    mov edx, (SECRET_MAX + 1) * 3
    jmp mem_set

program_name db 'passwd', 0
old_prompt db 'current password: ', 0
new_prompt db 'new password: ', 0
again_prompt db 'again: ', 0
mismatch_text db 'passwd: they dont match, nothing changed', 0
done_text db 'password changed', 0

include 'secret.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/users_bss.inc'

old rb SECRET_MAX + 1
new rb SECRET_MAX + 1
again rb SECRET_MAX + 1
user rb 64
