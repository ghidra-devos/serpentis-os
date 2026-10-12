include '../runtime/program.inc'

SAMPLE_BYTES = 512

main:
    push rbx
    push r12
    push r13
    call collect_args
    cmp dword [arg_count], 0
    je .usage
    xor ebx, ebx
    xor r13d, r13d

.path:
    cmp ebx, [arg_count]
    jae .done
    mov r12, [arg_list + rbx * 8]
    inc ebx
    mov rdi, r12
    lea rsi, [status]
    call sys_stat
    test eax, eax
    js .failed
    mov rdi, r12
    call print
    lea rdi, [separator]
    call print
    call describe
    mov rdi, rax
    call print_line
    jmp .path

.failed:
    lea rdi, [program_name]
    mov rsi, r12
    mov edx, eax
    call report_error
    mov r13d, 1
    jmp .path

.done:
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

describe:
    lea rax, [directory_text]
    test dword [status + USER_STAT_FLAGS], FS_FLAG_DIRECTORY
    jnz .done
    lea rax, [empty_text]
    cmp qword [status + USER_STAT_FILE_SIZE], 0
    je .done
    mov rdi, r12
    lea rsi, [file_data]
    mov edx, SAMPLE_BYTES
    call read_whole
    mov ecx, eax
    lea rax, [data_text]
    test ecx, ecx
    jle .done
    cmp ecx, 4
    jb .text_check
    cmp dword [file_data], SERP_MAGIC
    jne .text_check
    lea rax, [executable_text]
    ret
.text_check:
    xor edx, edx
.byte:
    cmp edx, ecx
    jae .text
    mov r8b, [file_data + rdx]
    inc edx
    cmp r8b, 9
    je .byte
    cmp r8b, 10
    je .byte
    cmp r8b, 13
    je .byte
    cmp r8b, ' '
    jb .done
    cmp r8b, 126
    ja .done
    jmp .byte
.text:
    mov rdi, r12
    call str_len
    cmp eax, 4
    jb .plain
    mov eax, [r12 + rax - 4]
    cmp eax, '.asm'
    je .source
    cmp eax, '.inc'
    je .source
.plain:
    lea rax, [text_text]
    ret
.source:
    lea rax, [source_text]
.done:
    ret

program_name db 'file', 0
usage_text db 'usage: file path...', 0
separator db ': ', 0
directory_text db 'directory', 0
empty_text db 'empty', 0
data_text db 'data', 0
executable_text db 'SERPENTIS executable', 0
text_text db 'ASCII text', 0
source_text db 'assembly source, ASCII text', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

align 8
status rb USER_STAT_SIZE
