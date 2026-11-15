include '../runtime/program.inc'

main:
    push rbx
    push r12
    push r13
    call collect_args
    lea rdi, [flag_letters]
    lea rsi, [flags]
    call parse_flags
    jc .usage
    mov ebx, eax
    cmp ebx, [arg_count]
    jae .usage
    mov rsi, [arg_list + rbx * 8]
    mov [pattern], rsi
    mov rdi, rsi
    call str_len
    mov [pattern_length], eax
    inc ebx
    mov ecx, ebx
    call use_stdin
    mov eax, [arg_count]
    sub eax, ebx
    jz .usage
    cmp eax, 1                  ; file names in front only with more than one file
    seta [show_names]
    mov r13d, 1

.file:
    cmp ebx, [arg_count]
    jae .done
    mov r12, [arg_list + rbx * 8]
    inc ebx
    mov rdi, r12
    call load_input
    jc .failed
    mov rdi, r12
    call search_file
    test eax, eax
    jz .file
    cmp r13d, 2
    je .file
    xor r13d, r13d
    jmp .file

.failed:
    mov r13d, 2
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

; rdi -> name. prints the matching lines (or just the count with -c), eax = matches
search_file:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r15, rdi
    lea rbx, [file_data]
    mov r12d, [file_size]
    add r12, rbx
    xor r13d, r13d
    xor r14d, r14d

.line:
    cmp rbx, r12
    jae .finished
    inc r13d
    mov rdi, rbx
    mov rsi, r12
    call line_end_at
    push rax
    mov rdi, rbx
    mov rsi, rax
    call line_matches
    xor al, [invert]
    jz .next
    inc r14d
    cmp byte [count_only], 0
    jne .next
    call print_name
    cmp byte [numbers], 0
    je .text
    mov edi, r13d
    call out_number
    mov edi, ':'
    call out_char
.text:
    mov rdi, rbx
    mov rsi, [rsp]
    sub rsi, rbx
    call out_bytes
    mov edi, 10
    call out_char
.next:
    pop rbx
    inc rbx
    jmp .line

.finished:
    cmp byte [count_only], 0
    je .result
    call print_name
    mov edi, r14d
    call out_number
    mov edi, 10
    call out_char
.result:
    mov eax, r14d
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

print_name:
    cmp byte [show_names], 0
    je .done
    mov rdi, r15
    call out_text
    mov edi, ':'
    jmp out_char
.done:
    ret

line_matches:
    mov ecx, [pattern_length]
    mov rdx, rsi
    sub rdx, rcx
.start:
    cmp rdi, rdx
    ja .no
    xor r8d, r8d
.compare:
    cmp r8d, ecx
    jae .yes
    mov al, [rdi + r8]
    mov r9, [pattern]
    mov r9b, [r9 + r8]
    cmp byte [ignore_case], 0
    je .exact
    call fold_pair
.exact:
    cmp al, r9b
    jne .advance
    inc r8d
    jmp .compare
.advance:
    inc rdi
    jmp .start
.yes:
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

fold_pair:
    cmp al, 'A'
    jb .second
    cmp al, 'Z'
    ja .second
    or al, 0x20
.second:
    cmp r9b, 'A'
    jb .done
    cmp r9b, 'Z'
    ja .done
    or r9b, 0x20
.done:
    ret

program_name db 'grep', 0
usage_text db 'usage: grep [-icnv] pattern file...', 0
flag_letters db 'icnv', 0

flags:
ignore_case db 0
count_only db 0
numbers db 0
invert db 0
show_names db 0
align 8
pattern dq 0
pattern_length dd 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
