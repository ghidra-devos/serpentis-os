include '../runtime/program.inc'

main:
    push rbx
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    mov ebx, eax
    mov ecx, [arg_count]
    sub ecx, ebx
    jz .usage
    cmp byte [delete], 0
    jne .one_set
    cmp ecx, 2
    jne .usage
    jmp .sets
.one_set:
    cmp ecx, 1
    jne .usage

.sets:
    xor eax, eax
.identity:
    mov [table + rax], al
    mov byte [removed + rax], 0
    inc eax
    cmp eax, 256
    jb .identity

    mov rdi, [arg_list + rbx * 8]
    lea rsi, [first_set]
    call expand_set
    mov [first_length], eax
    cmp byte [delete], 0
    jne .deletions
    mov rdi, [arg_list + rbx * 8 + 8]
    lea rsi, [second_set]
    call expand_set
    test eax, eax
    jz .usage
    mov [second_length], eax

    xor ecx, ecx
.map:
    cmp ecx, [first_length]
    jae .run
    mov edx, ecx
    cmp edx, [second_length]
    jb .pair
    mov edx, [second_length]
    dec edx
.pair:
    movzx eax, byte [first_set + rcx]
    mov r8b, [second_set + rdx]
    mov [table + rax], r8b
    inc ecx
    jmp .map

.deletions:
    xor ecx, ecx
.mark:
    cmp ecx, [first_length]
    jae .run
    movzx eax, byte [first_set + rcx]
    mov byte [removed + rax], 1
    inc ecx
    jmp .mark

.run:
    lea rdi, [stdin_name]
    call load_input
    jc .failed
    xor ebx, ebx
.byte:
    cmp ebx, [file_size]
    jae .done
    movzx eax, byte [file_data + rbx]
    inc ebx
    cmp byte [removed + rax], 0
    jne .byte
    movzx edi, byte [table + rax]
    call out_char
    jmp .byte
.done:
    call out_flush
    xor eax, eax
    pop rbx
    ret
.failed:
    mov eax, 1
    pop rbx
    ret
.usage:
    pop rbx
    lea rdi, [usage_text]
    jmp usage

expand_set:
    xor eax, eax
.next:
    movzx ecx, byte [rdi]
    test ecx, ecx
    jz .done
    cmp ecx, '\'
    jne .range
    inc rdi
    movzx ecx, byte [rdi]
    cmp ecx, 'n'
    jne .range
    mov ecx, 10
.range:
    cmp byte [rdi + 1], '-'
    jne .single
    movzx edx, byte [rdi + 2]
    test edx, edx
    jz .single
    cmp edx, ecx
    jb .single
.span:
    cmp eax, 256
    jae .done
    mov [rsi + rax], cl
    inc eax
    inc ecx
    cmp ecx, edx
    jbe .span
    add rdi, 3
    jmp .next
.single:
    cmp eax, 256
    jae .done
    mov [rsi + rax], cl
    inc eax
    inc rdi
    jmp .next
.done:
    ret

program_name db 'tr', 0
usage_text db 'usage: tr set1 set2 | tr -d set', 0
flag_letters db 'd', 0
flags:
delete db 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

table rb 256
removed rb 256
first_set rb 256
second_set rb 256
first_length rd 1
second_length rd 1
