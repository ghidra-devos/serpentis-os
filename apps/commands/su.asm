; su [user [program args...]]. no program means a new terminal for that user.
; root gets in anywhere without a password, everybody else types it

include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    mov r12, rdi
    lea rax, [root_name]
    mov [target], rax
    mov rdi, r12
    call arg_next
    test rax, rax
    jz .program
    mov [target], rax
    mov r12, rdx

.program:
    lea rax, [shell_path]
    mov [program], rax
    xor r13d, r13d
    mov rdi, r12
    call arg_next
    test rax, rax
    jz .ask
    mov r13, rdx
    mov rdi, rax
    call program_path
    mov [program], rax

.ask:
    xor ebx, ebx
    call sys_getuid
    test eax, eax
    jz .start
    lea rdi, [password_prompt]
    call print
    lea rdi, [password]
    call read_secret
    lea rbx, [password]

.start:
    mov rdi, [program]
    mov rsi, r13
    mov rdx, [target]
    mov rcx, rbx
    call sys_spawnas
    mov r12d, eax
    lea rdi, [password]
    xor esi, esi
    mov edx, SECRET_MAX + 1
    call mem_set
    test r12d, r12d
    js .failed
    mov edi, r12d
    xor esi, esi
    call sys_waitpid
    jmp .out

.failed:
    lea rdi, [program_name]
    mov rsi, [target]
    mov edx, r12d
    call report_error
    mov eax, 1
.out:
    pop r13
    pop r12
    pop rbx
    ret

; rdi -> name -> rax -> path. no slash means /bin/<name>.serp, same rule lds uses
program_path:
    mov rax, rdi
    mov rsi, rdi
.scan:
    mov cl, [rsi]
    test cl, cl
    jz .bin
    cmp cl, '/'
    je .done
    inc rsi
    jmp .scan
.bin:
    mov rsi, rdi
    lea rdi, [path_buffer]
    push rsi
    lea rsi, [bin_prefix]
    call str_copy
    pop rsi
    lea rdi, [path_buffer + 5]
    call str_copy
    lea rdi, [path_buffer]
    call str_len
    lea rdi, [path_buffer + rax]
    lea rsi, [serp_suffix]
    call str_copy
    lea rax, [path_buffer]
.done:
    ret

program_name db 'su', 0
root_name db 'root', 0
shell_path db '/bin/lds.serp', 0
bin_prefix db '/bin/', 0
serp_suffix db '.serp', 0
password_prompt db 'password: ', 0

include 'secret.inc'
include '../runtime/library.inc'

align 8
target rq 1
program rq 1
password rb SECRET_MAX + 1
path_buffer rb 256
