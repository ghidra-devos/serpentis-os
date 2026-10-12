include '../runtime/program.inc'

main:
    push rbx
    push r12
    call collect_args
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r12d, r12d

.name:
    cmp ebx, [arg_count]
    jae .done
    mov rsi, [arg_list + rbx * 8]
    inc ebx
    lea rdi, [candidate]
    call str_copy
    mov rdi, [arg_list + rbx * 8 - 8]
    call has_slash
    je .check
    lea rdi, [candidate]
    lea rsi, [bin_prefix]
    call str_copy
    lea rdi, [candidate + bin_prefix_length]
    mov rsi, [arg_list + rbx * 8 - 8]
    call str_copy
    lea rdi, [candidate]
    call str_len
    lea rdi, [candidate + rax]
    lea rsi, [serp_suffix]
    call str_copy
.check:
    lea rdi, [candidate]
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .missing
    test dword [status + USER_STAT_FLAGS], FS_FLAG_DIRECTORY
    jnz .missing
    lea rdi, [candidate]
    call print_line
    jmp .name
.missing:
    lea rdi, [missing_text]
    call print
    mov rdi, [arg_list + rbx * 8 - 8]
    call print
    lea rdi, [missing_tail]
    call print_line
    mov r12d, 1
    jmp .name

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

has_slash:
    mov al, [rdi]
    test al, al
    jz .none
    cmp al, '/'
    je .found
    inc rdi
    jmp has_slash
.none:
    cmp al, 1
.found:
    ret

program_name db 'which', 0
usage_text db 'usage: which name...', 0
bin_prefix db '/bin/', 0
bin_prefix_length = $ - bin_prefix - 1
serp_suffix db '.serp', 0
missing_text db 'which: no ', 0
missing_tail db ' in /bin', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
status rb USER_STAT_SIZE
candidate rb 256
