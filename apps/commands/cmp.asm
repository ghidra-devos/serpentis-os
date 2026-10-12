include '../runtime/program.inc'

SECOND_MAX = 0x80000

main:
    push rbx
    push r12
    push r13
    call collect_args
    cmp dword [arg_count], 2
    jne .usage
    mov rdi, [arg_list]
    call load_input
    jc .failed
    mov rdi, [arg_list + 8]
    lea rsi, [second_data]
    mov edx, SECOND_MAX
    call read_whole
    test eax, eax
    js .second_failed
    mov r13d, eax

    xor ebx, ebx
    mov r12d, 1
.compare:
    cmp ebx, [file_size]
    jae .end
    cmp ebx, r13d
    jae .end
    mov al, [file_data + rbx]
    cmp al, [second_data + rbx]
    jne .differ
    cmp al, 10
    jne .next
    inc r12d
.next:
    inc ebx
    jmp .compare

.differ:
    mov rdi, [arg_list]
    call out_text
    mov edi, ' '
    call out_char
    mov rdi, [arg_list + 8]
    call out_text
    lea rdi, [differ_text]
    call out_text
    lea edi, [rbx + 1]
    call out_number
    lea rdi, [line_text]
    call out_text
    mov edi, r12d
    call out_number
    mov edi, 10
    call out_char
    jmp .different

.end:
    cmp r13d, [file_size]
    je .same
    lea rdi, [eof_text]
    call out_text
    mov rdi, [arg_list]
    cmp r13d, [file_size]
    ja .shorter
    mov rdi, [arg_list + 8]
.shorter:
    call out_text
    lea rdi, [after_text]
    call out_text
    mov edi, ebx
    call out_number
    mov edi, 10
    call out_char
.different:
    call out_flush
    mov eax, 1
    jmp .out
.same:
    xor eax, eax
    jmp .out

.second_failed:
    lea rdi, [program_name]
    mov rsi, [arg_list + 8]
    mov edx, eax
    call report_error
.failed:
    mov eax, 2
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

program_name db 'cmp', 0
usage_text db 'usage: cmp file1 file2', 0
differ_text db ' differ: byte ', 0
line_text db ', line ', 0
eof_text db 'cmp: EOF on ', 0
after_text db ' after byte ', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

second_data rb SECOND_MAX
