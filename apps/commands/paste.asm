include '../runtime/program.inc'

COLUMNS_MAX = 16

main:
    push rbx
    push r12
    call collect_args
    xor ecx, ecx
    call use_stdin
    mov eax, [arg_count]
    test eax, eax
    jz .usage
    cmp eax, COLUMNS_MAX
    ja .usage

    xor ebx, ebx
    mov r12d, -1
.load:
    cmp ebx, [arg_count]
    jae .rows
    mov [column_source + rbx * 4], ebx
    mov rdi, [arg_list + rbx * 8]
    cmp word [rdi], '-'
    jne .read
    test r12d, r12d
    js .first_stdin
    mov [column_source + rbx * 4], r12d
    inc ebx
    jmp .load
.first_stdin:
    mov r12d, ebx
.read:
    call load_more
    jc .failed
    mov [column_next + rbx * 8], rax
    add rax, rdx
    mov [column_end + rbx * 8], rax
    inc ebx
    jmp .load

.rows:
    xor ebx, ebx
.any:
    cmp ebx, [arg_count]
    jae .done
    mov ecx, [column_source + rbx * 4]
    mov rax, [column_next + rcx * 8]
    cmp rax, [column_end + rcx * 8]
    jb .row
    inc ebx
    jmp .any
.row:
    xor ebx, ebx
.column:
    cmp ebx, [arg_count]
    jae .end_row
    test ebx, ebx
    jz .text
    mov edi, 9
    call out_char
.text:
    mov ecx, [column_source + rbx * 4]
    mov rdi, [column_next + rcx * 8]
    mov rsi, [column_end + rcx * 8]
    cmp rdi, rsi
    jae .next_column
    call line_end_at
    mov r12, rax
    mov ecx, [column_source + rbx * 4]
    mov rdi, [column_next + rcx * 8]
    mov rsi, r12
    sub rsi, rdi
    call out_bytes
    lea rax, [r12 + 1]
    mov ecx, [column_source + rbx * 4]
    mov [column_next + rcx * 8], rax
.next_column:
    inc ebx
    jmp .column
.end_row:
    mov edi, 10
    call out_char
    jmp .rows

.done:
    call out_flush
    xor eax, eax
    jmp .out
.failed:
    mov eax, 1
.out:
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

program_name db 'paste', 0
usage_text db 'usage: paste file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

column_next rq COLUMNS_MAX
column_end rq COLUMNS_MAX
column_source rd COLUMNS_MAX
