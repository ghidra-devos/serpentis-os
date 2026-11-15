include '../runtime/program.inc'

RANGES_MAX = 32

main:
    push rbx
    push r12
    push r13
    call collect_args
    xor ebx, ebx
    xor r12d, r12d

.argument:
    cmp ebx, [arg_count]
    jae .check
    mov rsi, [arg_list + rbx * 8]
    inc ebx
    cmp byte [rsi], '-'
    jne .keep
    mov al, [rsi + 1]
    test al, al
    jz .usage
    lea rdi, [rsi + 2]
    cmp byte [rdi], 0
    jne .value
    cmp ebx, [arg_count]
    jae .usage
    mov rdi, [arg_list + rbx * 8]
    inc ebx
.value:
    cmp al, 'd'
    je .delimiter
    cmp al, 'f'
    je .fields
    cmp al, 'c'
    jne .usage
    mov byte [by_chars], 1
.fields:
    call parse_ranges
    jc .usage
    jmp .argument
.delimiter:
    mov al, [rdi]
    mov [delimiter], al
    jmp .argument
.keep:
    mov [arg_list + r12 * 8], rsi
    inc r12d
    jmp .argument

.check:
    mov [arg_count], r12d
    xor ecx, ecx
    call use_stdin
    cmp dword [range_count], 0
    je .usage
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r13d, r13d
.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call cut_file
    jmp .file
.failed:
    mov r13d, 1
    jmp .file
.done:
    call out_flush
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

parse_ranges:
    mov ecx, [range_count]
.range:
    cmp ecx, RANGES_MAX
    jae .bad
    mov dword [range_low + rcx * 4], 1
    mov dword [range_high + rcx * 4], -1
    cmp byte [rdi], '-'
    je .high
    call read_value
    jc .bad
    mov [range_low + rcx * 4], eax
    mov [range_high + rcx * 4], eax
    cmp byte [rdi], '-'
    jne .stored
    mov dword [range_high + rcx * 4], -1
.high:
    inc rdi
    cmp byte [rdi], ','
    je .stored
    cmp byte [rdi], 0
    je .stored
    call read_value
    jc .bad
    mov [range_high + rcx * 4], eax
.stored:
    inc ecx
    mov [range_count], ecx
    cmp byte [rdi], ','
    jne .end
    inc rdi
    jmp .range
.end:
    cmp byte [rdi], 0
    jne .bad
    clc
    ret
.bad:
    stc
    ret

; decimal at rdi, 0 is not a valid column
read_value:
    xor eax, eax
    movzx edx, byte [rdi]
    sub edx, '0'
    cmp edx, 9
    ja .bad
.digit:
    movzx edx, byte [rdi]
    sub edx, '0'
    cmp edx, 9
    ja .done
    imul eax, 10
    add eax, edx
    inc rdi
    jmp .digit
.done:
    test eax, eax
    jz .bad
    clc
    ret
.bad:
    stc
    ret

selected:
    xor edx, edx
.range:
    cmp edx, [range_count]
    jae .no
    cmp eax, [range_low + rdx * 4]
    jb .next
    cmp eax, [range_high + rdx * 4]
    jbe .yes
.next:
    inc edx
    jmp .range
.yes:
    stc
    ret
.no:
    clc
    ret

cut_file:
    push rbx
    push r12
    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
.line:
    cmp rbx, r12
    jae .done
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    push rax
    mov rdi, rbx
    mov rsi, rax
    cmp byte [by_chars], 0
    je .fields
    call cut_chars
    jmp .next
.fields:
    call cut_fields
.next:
    mov edi, 10
    call out_char
    pop rbx
    inc rbx
    jmp .line
.done:
    pop r12
    pop rbx
    ret

cut_chars:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13d, 1
.next:
    cmp rbx, r12
    jae .done
    mov eax, r13d
    call selected
    jnc .skip
    movzx edi, byte [rbx]
    call out_char
.skip:
    inc rbx
    inc r13d
    jmp .next
.done:
    pop r13
    pop r12
    pop rbx
    ret

cut_fields:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rbx, rdi
    mov r12, rsi
    mov al, [delimiter]
    mov rcx, rsi
    sub rcx, rdi
    repne scasb
    jne .whole
    mov r13, rbx
    mov r14d, 1
    xor r15d, r15d
.scan:
    cmp rbx, r12
    jae .field
    mov al, [rbx]
    cmp al, [delimiter]
    je .field
    inc rbx
    jmp .scan
.field:
    mov eax, r14d
    call selected
    jnc .after
    test r15d, r15d
    jz .first
    movzx edi, byte [delimiter]
    call out_char
.first:
    mov r15d, 1
    mov rdi, r13
    mov rsi, rbx
    sub rsi, r13
    call out_bytes
.after:
    cmp rbx, r12
    jae .done
    inc rbx
    mov r13, rbx
    inc r14d
    jmp .scan
.whole:
    mov rdi, rbx
    mov rsi, r12
    sub rsi, rbx
    call out_bytes
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

program_name db 'cut', 0
usage_text db 'usage: cut -f list [-d delim] file... | cut -c list file...', 0
delimiter db 9
by_chars db 0
align 4
range_count dd 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

range_low rd RANGES_MAX
range_high rd RANGES_MAX
