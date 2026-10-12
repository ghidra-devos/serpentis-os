include '../runtime/program.inc'

; dd with if= of= bs= count= skip=, no conversions. reports records like the real one
main:
    push rbx
    push r12
    push r13
    call collect_args
    mov qword [block_size], 512
    mov qword [block_count], -1
    xor ebx, ebx

.operand:
    cmp ebx, [arg_count]
    jae .copy
    mov r12, [arg_list + rbx * 8]
    inc ebx
    lea rsi, [operand_names]
    xor r13d, r13d
.match:
    mov rdi, r12
    movzx ecx, byte [rsi]
    test ecx, ecx
    jz .usage
    push rsi
    inc rsi
    repe cmpsb
    pop rsi
    jne .next_name
    cmp byte [rdi], '='
    je .value
.next_name:
    movzx ecx, byte [rsi]
    lea rsi, [rsi + rcx + 1]
    inc r13d
    jmp .match
.value:
    inc rdi
    cmp r13d, 2
    jb .path
    call parse_number
    jc .usage
    mov [block_size + r13 * 8 - 16], rax
    jmp .operand
.path:
    mov [input_path + r13 * 8], rdi
    jmp .operand

.copy:
    cmp qword [block_size], 0
    je .usage
    mov rdi, [input_path]
    test rdi, rdi
    jnz .load
    lea rdi, [stdin_name]
.load:
    call load_input
    jc .failed
    mov rax, [block_size]
    imul rax, [skip_blocks]
    mov r12d, [file_size]
    cmp rax, r12
    jbe .skipped
    mov rax, r12
.skipped:
    mov r13, rax
    sub r12, rax
    mov rax, [block_size]
    imul rax, [block_count]
    cmp qword [block_count], -1
    je .sized
    cmp r12, rax
    jbe .sized
    mov r12, rax
.sized:
    lea rsi, [file_data + r13]
    mov rdx, r12
    mov edi, FD_STDOUT
    mov rbx, [output_path]
    test rbx, rbx
    jz .write
    push rsi
    push rdx
    mov rdi, rbx
    call sys_unlink
    mov rdi, rbx
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    pop rdx
    pop rsi
    test eax, eax
    js .open_failed
    mov edi, eax
.write:
    push rdi
    call sys_write
    pop rdi
    cmp rdi, FD_STDOUT
    je .report
    call sys_close
.report:
    call report_records
    xor eax, eax
    jmp .out

.open_failed:
    lea rdi, [program_name]
    mov rsi, rbx
    mov edx, eax
    call report_error
.failed:
    mov eax, 1
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

report_records:
    push rbx
    mov rbx, r12
    mov rax, r12
    xor edx, edx
    div qword [block_size]
    mov [full_blocks], rax
    xor eax, eax
    test rdx, rdx
    setnz al
    mov [partial_blocks], rax
    lea rdi, [records_in]
    call report_line
    lea rdi, [records_out]
    call report_line
    mov rdi, rbx
    call u64_to_dec
    mov rdi, rax
    call error_text
    lea rdi, [bytes_copied]
    call error_text
    pop rbx
    ret

report_line:
    push rdi
    mov rdi, [full_blocks]
    call u64_to_dec
    mov rdi, rax
    call error_text
    lea rdi, [plus]
    call error_text
    mov rdi, [partial_blocks]
    call u64_to_dec
    mov rdi, rax
    call error_text
    pop rdi

error_text:
    push rdi
    call str_len
    pop rsi
    mov rdx, rax
    mov edi, FD_STDERR
    jmp sys_write

program_name db 'dd', 0
usage_text db 'usage: dd [if=file] [of=file] [bs=n] [count=n] [skip=n]', 0
operand_names db 2, 'if', 2, 'of', 2, 'bs', 5, 'count', 4, 'skip', 0
plus db '+', 0
records_in db ' records in', 10, 0
records_out db ' records out', 10, 0
bytes_copied db ' bytes copied', 10, 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
input_path rq 1
output_path rq 1
block_size rq 1
block_count rq 1
skip_blocks rq 1
full_blocks rq 1
partial_blocks rq 1
