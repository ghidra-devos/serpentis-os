include '../runtime/program.inc'

ARGUMENT_LIMIT = 200
CAPTURE_BYTES = 0x8000


main:
    push rbx
    push r12
    push r13
    call collect_args
    lea rax, [default_command]
    cmp dword [arg_count], 0
    je .named
    mov rax, [arg_list]
.named:
    mov [command], rax

    mov byte [fixed_arguments], 0
    mov ebx, 1
.fixed:
    cmp ebx, [arg_count]
    jae .input
    mov rsi, [arg_list + rbx * 8]
    lea rdi, [fixed_arguments]
    call append_word
    inc ebx
    jmp .fixed

.input:
    mov dword [arg_count], 0
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 0
    je .usage
    lea rdi, [stdin_name]
    call load_input
    jc .failed

    call start_batch
    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
.word:
    cmp rbx, r12
    jae .last
    movzx eax, byte [rbx]
    cmp al, ' '
    ja .begin
    inc rbx
    jmp .word
.begin:
    mov r13, rbx
.scan:
    cmp rbx, r12
    jae .ended
    cmp byte [rbx], ' '
    jbe .ended
    inc rbx
    jmp .scan
.ended:
    mov al, [rbx]
    mov byte [rbx], 0
    push rax
    lea rdi, [batch]
    call str_len
    mov rcx, rbx
    sub rcx, r13
    add rax, rcx
    cmp rax, ARGUMENT_LIMIT
    jb .fits
    call run_batch
    call start_batch
.fits:
    lea rdi, [batch]
    mov rsi, r13
    call append_word
    mov byte [pending], 1
    pop rax
    mov [rbx], al
    jmp .word

.last:
    cmp byte [pending], 0
    je .done
    call run_batch
.done:
    xor eax, eax
    jmp .out
.failed:
    mov eax, 1
.out:
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

append_word:
    push rsi
    call str_len
    lea rdi, [rdi + rax]
    test eax, eax
    jz .copy
    mov byte [rdi], ' '
    inc rdi
.copy:
    pop rsi
    jmp str_copy

start_batch:
    lea rdi, [batch]
    lea rsi, [fixed_arguments]
    call str_copy
    mov byte [pending], 0
    ret

; /bin/<command>.serp first. if there is no such program it may be a kernel command (ls, cat,
; ...), those run through SYS_KCOMMAND and come back as char/attr pairs
run_batch:
    lea rdi, [spawn_path]
    lea rsi, [bin_prefix]
    call str_copy
    lea rdi, [spawn_path]
    call str_len
    lea rdi, [spawn_path + rax]
    mov rsi, [command]
    call str_copy
    lea rdi, [spawn_path]
    call str_len
    lea rdi, [spawn_path + rax]
    lea rsi, [serp_suffix]
    call str_copy
    lea rdi, [spawn_path]
    lea rsi, [batch]
    call sys_spawn
    test eax, eax
    js .kernel
    mov edi, eax
    xor esi, esi
    jmp sys_waitpid

.kernel:
    lea rdi, [kernel_line]
    mov rsi, [command]
    call str_copy
    lea rdi, [kernel_line]
    lea rsi, [batch]
    call append_word
    lea rdi, [kernel_line]
    lea rsi, [capture]
    mov edx, CAPTURE_BYTES
    call sys_kcommand
    test eax, eax
    jle .done
    xor ecx, ecx
    xor edx, edx
.pack:
    cmp ecx, eax
    jae .write
    mov r8b, [capture + rcx]
    mov [capture + rdx], r8b
    inc edx
    add ecx, 2
    jmp .pack
.write:
    mov edi, FD_STDOUT
    lea rsi, [capture]
    jmp sys_write
.done:
    ret

program_name db 'xargs', 0
usage_text db 'usage: command | xargs [command [arguments]]', 0
default_command db 'echo', 0
bin_prefix db '/bin/', 0
serp_suffix db '.serp', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
command rq 1
pending rb 1
fixed_arguments rb 256
batch rb 256
spawn_path rb 256
kernel_line rb 512
capture rb CAPTURE_BYTES
