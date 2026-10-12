; chmod 755 file... three octal digits like everywhere, the middle one is stored but there
; are no groups so it does nothing

include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    cmp dword [arg_count], 2
    jb .usage
    mov rsi, [arg_list]
    xor ebx, ebx
.digit:
    movzx eax, byte [rsi]
    test eax, eax
    jz .parsed
    sub eax, '0'
    cmp eax, 7
    ja .usage
    shl ebx, 3
    or ebx, eax
    inc rsi
    jmp .digit
.parsed:
    cmp ebx, 777o
    ja .usage
    mov r12d, 1
    xor r13d, r13d
.file:
    cmp r12d, [arg_count]
    jae .done
    mov rdi, [arg_list + r12 * 8]
    mov esi, ebx
    call sys_chmod
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
.usage:
    pop r13
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'chmod', 0
usage_text db 'usage: chmod 755 file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
