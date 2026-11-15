include '../runtime/program.inc'

main:
    call collect_args
    cmp dword [arg_count], 1
    jne .usage
    mov rdi, [arg_list]
    call strip_slashes
    xor edx, edx
    mov rcx, rdi
.scan:
    mov al, [rcx]
    test al, al
    jz .cut
    cmp al, '/'
    jne .next
    mov rdx, rcx
.next:
    inc rcx
    jmp .scan
.cut:
    test rdx, rdx
    jz .current
    cmp rdx, rdi
    je .root
    mov byte [rdx], 0
    call strip_slashes
    cmp byte [rdi], 0
    je .root
    jmp .print
.root:
    lea rdi, [root_text]
    jmp .print
.current:
    lea rdi, [current_text]
.print:
    call print_line
    xor eax, eax
    ret

.usage:
    lea rdi, [usage_text]
    jmp usage

program_name db 'dirname', 0
usage_text db 'usage: dirname path', 0
root_text db '/', 0
current_text db '.', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
