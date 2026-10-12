ARG_MAX = 64
OUT_BYTES = 4096

FILE_MAX = 0x80000              ; tools work on whole files in memory, 512 KB is plenty here

collect_args:
    push rbx
    mov rbx, rdi
    mov dword [arg_count], 0
.next:
    mov rdi, rbx
    call arg_next
    test rax, rax
    jz .done
    mov rbx, rdx
    mov ecx, [arg_count]
    cmp ecx, ARG_MAX
    jae .next
    mov [arg_list + rcx * 8], rax
    inc dword [arg_count]
    jmp .next
.done:
    pop rbx
    ret

parse_flags:
    push rbx
    xor ebx, ebx
.option:
    cmp ebx, [arg_count]
    jae .done
    mov rdx, [arg_list + rbx * 8]
    cmp byte [rdx], '-'
    jne .done
    cmp byte [rdx + 1], 0
    je .done
    inc ebx
.letter:
    inc rdx
    mov al, [rdx]
    test al, al
    jz .option
    xor ecx, ecx
.find:
    cmp byte [rdi + rcx], 0
    je .bad
    cmp [rdi + rcx], al
    je .set
    inc ecx
    jmp .find
.set:
    mov byte [rsi + rcx], 1
    jmp .letter
.done:
    mov eax, ebx
    pop rbx
    clc
    ret
.bad:
    pop rbx
    stc
    ret

take_count_option:
    push rbx
    push r12
    xor ebx, ebx
    xor r12d, r12d
.next:
    cmp ebx, [arg_count]
    jae .done
    mov rax, [arg_list + rbx * 8]
    inc ebx
    cmp byte [rax], '-'
    jne .keep
    cmp byte [rax + 1], 0
    je .keep
    lea rdi, [rax + 1]
    mov cl, [rdi]
    sub cl, '0'
    cmp cl, 9
    jbe .parse
    inc rdi
    cmp byte [rdi], 0
    jne .parse
    cmp ebx, [arg_count]
    jae .bad
    mov rdi, [arg_list + rbx * 8]
    inc ebx
.parse:
    call parse_number
    jc .bad
    mov [count_value], rax
    jmp .next
.keep:
    mov [arg_list + r12 * 8], rax
    inc r12d
    jmp .next
.done:
    mov [arg_count], r12d
    pop r12
    pop rbx
    clc
    ret
.bad:
    pop r12
    pop rbx
    stc
    ret

out_lines:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx
.line:
    test r13, r13
    jz .done
    cmp rbx, r12
    jae .done
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    push rax
    mov rdi, rbx
    mov rsi, rax
    sub rsi, rbx
    call out_bytes
    mov edi, 10
    call out_char
    pop rbx
    inc rbx
    dec r13
    jmp .line
.done:
    pop r13
    pop r12
    pop rbx
    ret

parse_number:
    xor eax, eax
    cmp byte [rdi], 0
    je .bad
.digit:
    movzx ecx, byte [rdi]
    test ecx, ecx
    jz .good
    sub ecx, '0'
    cmp ecx, 9
    ja .bad
    imul rax, 10
    add rax, rcx
    inc rdi
    jmp .digit
.good:
    clc
    ret
.bad:
    stc
    ret

; ecx = argument count meaning "no files given". adds "-" if stdin is a file or a pipe.
; seek fails on the console, that is the whole test
use_stdin:
    cmp dword [arg_count], ecx
    jne .done
    mov edi, FD_STDIN
    xor esi, esi
    mov edx, SEEK_CUR
    call sys_seek
    test eax, eax
    js .done
    mov ecx, [arg_count]
    lea rax, [stdin_name]
    mov [arg_list + rcx * 8], rax
    inc dword [arg_count]
.done:
    ret

read_whole:
    push rbx
    push r12
    push r13
    push r14
    mov r12, rsi
    mov r13d, edx
    xor r14d, r14d
    mov ebx, FD_STDIN
    cmp word [rdi], '-'
    je .read
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .out
    mov ebx, eax
.read:
    mov edx, r13d
    sub edx, r14d
    jz .close
    mov edi, ebx
    lea rsi, [r12 + r14]
    call sys_read
    test eax, eax
    jle .close
    add r14d, eax
    jmp .read
.close:
    cmp ebx, FD_STDIN
    je .finished
    mov edi, ebx
    call sys_close
.finished:
    mov eax, r14d
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; rdi -> path. reads it into file_data, complains with the tool name on failure
load_input:
    push rdi
    lea rsi, [file_data]
    mov edx, FILE_MAX
    call read_whole
    pop rsi
    test eax, eax
    js .failed
    mov [file_size], eax
    clc
    ret
.failed:
    lea rdi, [program_name]
    mov edx, eax
    call report_error
    stc
    ret

strip_slashes:
    call str_len
    lea rcx, [rdi + rax - 1]
.trim:
    cmp rcx, rdi
    jbe .done
    cmp byte [rcx], '/'
    jne .done
    mov byte [rcx], 0
    dec rcx
    jmp .trim
.done:
    ret

load_more:
    push rbx
    push r12
    mov r12, rdi
    mov ebx, [file_size]
    lea rsi, [file_data + rbx]
    mov edx, FILE_MAX - 1
    sub edx, ebx
    call read_whole
    test eax, eax
    js .failed
    add eax, ebx
    cmp eax, ebx
    je .sized
    cmp byte [file_data + rax - 1], 10
    je .sized
    mov byte [file_data + rax], 10
    inc eax
.sized:
    mov [file_size], eax
    lea rax, [file_data + rbx]
    mov edx, [file_size]
    sub edx, ebx
    pop r12
    pop rbx
    clc
    ret
.failed:
    lea rdi, [program_name]
    mov rsi, r12
    mov edx, eax
    call report_error
    pop r12
    pop rbx
    stc
    ret

line_compare:
    mov al, [rdi]
    mov cl, [rsi]
    cmp al, 10
    jne .left
    xor al, al
.left:
    cmp cl, 10
    jne .right
    xor cl, cl
.right:
    cmp al, cl
    jb .less
    ja .greater
    test al, al
    jz .equal
    inc rdi
    inc rsi
    jmp line_compare
.less:
    mov eax, -1
    ret
.greater:
    mov eax, 1
    ret
.equal:
    xor eax, eax
    ret

line_end_at:
    mov rax, rdi
.scan:
    cmp rax, rsi
    jae .done
    cmp byte [rax], 10
    je .done
    inc rax
    jmp .scan
.done:
    ret

out_char:
    mov ecx, [out_used]
    mov [out_buffer + rcx], dil
    inc ecx
    mov [out_used], ecx
    cmp ecx, OUT_BYTES
    jae out_flush
    ret

out_bytes:
    push rbx
    push r12
    mov rbx, rdi
    mov r12d, esi
.next:
    test r12d, r12d
    jz .done
    movzx edi, byte [rbx]
    call out_char
    inc rbx
    dec r12d
    jmp .next
.done:
    pop r12
    pop rbx
    ret

out_text:
    push rbx
    mov rbx, rdi
.next:
    movzx edi, byte [rbx]
    test edi, edi
    jz .done
    call out_char
    inc rbx
    jmp .next
.done:
    pop rbx
    ret

out_line:
    call out_text
    mov edi, 10
    jmp out_char

out_number:
    call u64_to_dec
    mov rdi, rax
    jmp out_text

out_padded:
    push rbx
    mov ebx, esi
    call u64_to_dec
    push rax
    mov rdi, rax
    call str_len
    sub ebx, eax
.pad:
    test ebx, ebx
    jle .text
    mov edi, ' '
    call out_char
    dec ebx
    jmp .pad
.text:
    pop rdi
    pop rbx
    jmp out_text

out_flush:
    mov edx, [out_used]
    test edx, edx
    jz .done
    mov edi, FD_STDOUT
    lea rsi, [out_buffer]
    call sys_write
    mov dword [out_used], 0
.done:
    ret

usage:
    call print_line
    mov eax, 1
    ret

stdin_name db '-', 0
out_used dd 0
