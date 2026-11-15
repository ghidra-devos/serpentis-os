; xxd: offset, 16 bytes in pairs of hex, then the printable ones (dots for the rest)

include '../runtime/program.inc'

BYTES_PER_LINE = 16

main:
    push rbx
    push r12
    call collect_args
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 1
    jne .usage
    mov rdi, [arg_list]
    call load_input
    jc .failed

    xor ebx, ebx
.line:
    cmp ebx, [file_size]
    jae .done
    mov edi, ebx
    mov esi, 8
    call out_hex
    mov edi, ':'
    call out_char
    xor r12d, r12d
.column:
    cmp r12d, BYTES_PER_LINE
    jae .text
    test r12d, 1                ; a space before every second byte, xxd groups by two
    jnz .digits
    mov edi, ' '
    call out_char
.digits:
    lea eax, [rbx + r12]
    cmp eax, [file_size]
    jae .pad
    movzx edi, byte [file_data + rax]
    mov esi, 2
    call out_hex
    jmp .next_column
.pad:
    mov edi, ' '
    call out_char
    mov edi, ' '
    call out_char
.next_column:
    inc r12d
    jmp .column

.text:
    mov edi, ' '
    call out_char
    mov edi, ' '
    call out_char
    xor r12d, r12d
.character:
    cmp r12d, BYTES_PER_LINE
    jae .end_line
    lea eax, [rbx + r12]
    cmp eax, [file_size]
    jae .end_line
    movzx edi, byte [file_data + rax]
    cmp edi, ' '
    jb .dot
    cmp edi, 126
    jbe .show
.dot:
    mov edi, '.'
.show:
    call out_char
    inc r12d
    jmp .character
.end_line:
    mov edi, 10
    call out_char
    add ebx, BYTES_PER_LINE
    jmp .line

.done:
    call out_flush
    xor eax, eax
    pop r12
    pop rbx
    ret
.failed:
    mov eax, 1
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

out_hex:
    push rbx
    push r12
    mov ebx, edi
    mov r12d, esi
.digit:
    dec r12d
    js .done
    lea ecx, [r12 * 4]
    mov eax, ebx
    shr eax, cl
    and eax, 0x0F
    movzx edi, byte [hex_digits + rax]
    call out_char
    jmp .digit
.done:
    pop r12
    pop rbx
    ret

program_name db 'xxd', 0
usage_text db 'usage: xxd file', 0
hex_digits db '0123456789abcdef'

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
