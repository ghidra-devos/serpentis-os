include '../runtime/program.inc'

; line diff through the longest common subsequence table. O(n*m) words of memory, which is why
; it stops at LINES_MAX lines per file. output is plain unified-ish hunks, not exactly gnu diff

LINES_MAX = 600                 ; 601 * 601 * 2 bytes of table, fine
SECOND_MAX = 0x40000

main:
    push rbx
    push r12
    call collect_args
    cmp dword [arg_count], 2
    jne .usage

    mov rdi, [arg_list]
    lea rsi, [file_data]
    mov edx, FILE_MAX - 1
    lea rcx, [old_lines]
    call read_lines
    jc .failed
    mov [old_count], eax

    mov rdi, [arg_list + 8]
    lea rsi, [second_data]
    mov edx, SECOND_MAX - 1
    lea rcx, [new_lines]
    call read_lines
    jc .failed
    mov [new_count], eax

    call build_table
    call walk_table
    call out_flush
    mov eax, [changed]
    jmp .out

.failed:
    mov eax, 2
.out:
    pop r12
    pop rbx
    ret

.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

read_lines:
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13, rcx
    call read_whole
    test eax, eax
    js .error
    jz .indexed
    cmp byte [r12 + rax - 1], 10
    je .indexed
    mov byte [r12 + rax], 10
    inc eax
.indexed:
    lea rsi, [r12 + rax]
    mov rdi, r12
    xor ecx, ecx
.line:
    cmp rdi, rsi
    jae .done
    cmp ecx, LINES_MAX
    jae .too_long
    mov [r13 + rcx * 8], rdi
    inc ecx
    push rcx
    call line_end_at
    pop rcx
    lea rdi, [rax + 1]
    jmp .line
.done:
    mov eax, ecx
    clc
    jmp .out
.too_long:
    lea rdi, [program_name]
    call print
    lea rdi, [too_long_text]
    call print_line
    stc
    jmp .out
.error:
    lea rdi, [program_name]
    mov rsi, rbx
    mov edx, eax
    call report_error
    stc
.out:
    pop r13
    pop r12
    pop rbx
    ret

lines_equal:
    mov al, [rdi]
    cmp al, [rsi]
    jne .done
    cmp al, 10
    je .done
    inc rdi
    inc rsi
    jmp lines_equal
.done:
    ret

cell:
    mov eax, edi
    imul eax, [table_width]
    add eax, esi
    lea rax, [table + rax * 2]
    ret

build_table:
    push rbx
    push r12
    push r13
    mov eax, [new_count]
    inc eax
    mov [table_width], eax
    mov r12d, [old_count]
.row:
    mov r13d, [new_count]
.column:
    xor ebx, ebx
    cmp r12d, [old_count]
    je .store
    cmp r13d, [new_count]
    je .store
    mov rdi, [old_lines + r12 * 8]
    mov rsi, [new_lines + r13 * 8]
    call lines_equal
    jne .best
    lea edi, [r12 + 1]
    lea esi, [r13 + 1]
    call cell
    movzx ebx, word [rax]
    inc ebx
    jmp .store
.best:
    lea edi, [r12 + 1]
    mov esi, r13d
    call cell
    movzx ebx, word [rax]
    mov edi, r12d
    lea esi, [r13 + 1]
    call cell
    movzx eax, word [rax]
    cmp eax, ebx
    jbe .store
    mov ebx, eax
.store:
    mov edi, r12d
    mov esi, r13d
    call cell
    mov [rax], bx
    dec r13d
    jns .column
    dec r12d
    jns .row
    pop r13
    pop r12
    pop rbx
    ret

walk_table:
    push rbx
    push r12
    push r13
    push r14
    push r15
    xor r12d, r12d
    xor r13d, r13d
    mov r14d, -1
.step:
    mov eax, r12d
    cmp eax, [old_count]
    jb .compare
    cmp r13d, [new_count]
    jae .finish
    jmp .insert
.compare:
    cmp r13d, [new_count]
    jae .delete
    mov rdi, [old_lines + r12 * 8]
    mov rsi, [new_lines + r13 * 8]
    call lines_equal
    jne .choose
    call flush_hunk
    inc r12d
    inc r13d
    jmp .step
.choose:
    lea edi, [r12 + 1]
    mov esi, r13d
    call cell
    movzx ebx, word [rax]
    mov edi, r12d
    lea esi, [r13 + 1]
    call cell
    movzx eax, word [rax]
    cmp eax, ebx
    jb .delete
.insert:
    call open_hunk
    inc r13d
    jmp .step
.delete:
    call open_hunk
    inc r12d
    jmp .step
.finish:
    call flush_hunk
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

open_hunk:
    cmp r14d, -1
    jne .done
    mov r14d, r12d
    mov r15d, r13d
.done:
    ret

flush_hunk:
    cmp r14d, -1
    je .done
    mov dword [changed], 1
    cmp r12d, r14d
    je .added
    cmp r13d, r15d
    je .deleted
    lea edi, [r14 + 1]
    mov esi, r12d
    call out_range
    mov edi, 'c'
    call out_char
    lea edi, [r15 + 1]
    mov esi, r13d
    call out_range
    jmp .body
.added:
    mov edi, r14d
    call out_number
    mov edi, 'a'
    call out_char
    lea edi, [r15 + 1]
    mov esi, r13d
    call out_range
    jmp .body
.deleted:
    lea edi, [r14 + 1]
    mov esi, r12d
    call out_range
    mov edi, 'd'
    call out_char
    mov edi, r15d
    call out_number
.body:
    mov edi, 10
    call out_char
    lea rdi, [old_lines]
    mov esi, r14d
    mov edx, r12d
    lea rcx, [removed_prefix]
    call out_hunk_lines
    cmp r12d, r14d
    je .new_side
    cmp r13d, r15d
    je .new_side
    lea rdi, [separator_text]
    call out_line
.new_side:
    lea rdi, [new_lines]
    mov esi, r15d
    mov edx, r13d
    lea rcx, [added_prefix]
    call out_hunk_lines
    mov r14d, -1
.done:
    ret

out_range:
    push rsi
    push rdi
    call out_number
    pop rdi
    pop rsi
    cmp esi, edi
    jbe .done
    push rsi
    mov edi, ','
    call out_char
    pop rdi
    jmp out_number
.done:
    ret

out_hunk_lines:
    push rbx
    push r12
    push r13
    push r14
    mov rbx, rdi
    mov r12d, esi
    mov r13d, edx
    mov r14, rcx
.line:
    cmp r12d, r13d
    jae .done
    mov rdi, r14
    call out_text
    mov rdi, [rbx + r12 * 8]
    mov rsi, -1
    mov edx, 1
    call out_lines
    inc r12d
    jmp .line
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

program_name db 'diff', 0
usage_text db 'usage: diff file1 file2', 0
too_long_text db ': file has too many lines', 0
removed_prefix db '< ', 0
added_prefix db '> ', 0
separator_text db '---', 0
align 4
changed dd 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
old_lines rq LINES_MAX
new_lines rq LINES_MAX
old_count rd 1
new_count rd 1
table_width rd 1
second_data rb SECOND_MAX
table rw (LINES_MAX + 1) * (LINES_MAX + 1)
