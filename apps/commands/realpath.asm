include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r12d, r12d

.path:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call normalize
    lea rdi, [resolved]
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .missing
    lea rdi, [resolved]
    call print_line
    jmp .path
.missing:
    lea rdi, [program_name]
    mov rsi, [arg_list + rbx * 8 - 8]
    mov edx, eax
    call report_error
    mov r12d, 1
    jmp .path

.done:
    mov eax, r12d
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

normalize:
    push rbx
    push r12
    mov r12, rdi
    mov word [resolved], '/'
    cmp byte [r12], '/'
    je .components
    lea rdi, [joined]
    mov esi, 240
    call sys_getcwd
    lea rdi, [joined]
    call walk_components
.components:
    mov rdi, r12
    call walk_components
    pop r12
    pop rbx
    ret

walk_components:
    push rbx
    mov rbx, rdi
.next:
    cmp byte [rbx], '/'
    jne .start
    inc rbx
    jmp .next
.start:
    cmp byte [rbx], 0
    je .done
    lea rdi, [component]
    xor ecx, ecx
.copy:
    mov al, [rbx]
    test al, al
    jz .copied
    cmp al, '/'
    je .copied
    cmp ecx, 63                  ; longer names are cut
    jae .skip
    mov [rdi + rcx], al
    inc ecx
.skip:
    inc rbx
    jmp .copy
.copied:
    mov byte [rdi + rcx], 0
    cmp word [component], '.'
    je .next
    cmp word [component], '..'
    jne .append
    cmp byte [component + 2], 0
    jne .append
    call drop_last
    jmp .next
.append:
    lea rdi, [resolved]
    call str_len
    lea rdi, [resolved + rax]
    cmp eax, 1
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    lea rsi, [component]
    call str_copy
    jmp .next
.done:
    pop rbx
    ret

drop_last:
    lea rdi, [resolved]
    call str_len
    lea rcx, [resolved + rax]
.back:
    dec rcx
    cmp rcx, resolved
    jbe .root
    cmp byte [rcx], '/'
    jne .back
    mov byte [rcx], 0
    ret
.root:
    mov word [resolved], '/'
    ret

program_name db 'realpath', 0
usage_text db 'usage: realpath path...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
status rb USER_STAT_SIZE
resolved rb 256
joined rb 256
component rb 64
