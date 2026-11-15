include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    mov ebx, eax
    lea rdi, [stdin_name]
    call load_input
    jc .failed
    lea rdi, [file_data]
    mov esi, [file_size]
    call out_bytes
    call out_flush
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call write_copy
    test eax, eax
    jns .file
    lea rdi, [program_name]
    mov rsi, [arg_list + rbx * 8 - 8]
    mov edx, eax
    call report_error
    mov r12d, 1
    jmp .file

.done:
    mov eax, r12d
    pop r12
    pop rbx
    ret
.failed:
    mov eax, 1
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

write_copy:
    push rbx
    push r12
    mov r12, rdi
    cmp byte [append], 0
    jne .open
    call sys_unlink
.open:
    mov rdi, r12
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .out
    mov ebx, eax
    cmp byte [append], 0
    je .write
    mov edi, ebx
    xor esi, esi
    mov edx, SEEK_END
    call sys_seek
.write:
    mov edi, ebx
    lea rsi, [file_data]
    mov edx, [file_size]
    call sys_write
    push rax
    mov edi, ebx
    call sys_close
    pop rax
.out:
    pop r12
    pop rbx
    ret

program_name db 'tee', 0
usage_text db 'usage: tee [-a] file...', 0
flag_letters db 'a', 0
flags:
append db 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
