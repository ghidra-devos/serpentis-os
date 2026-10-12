include '../runtime/program.inc'


POLYNOMIAL = 0x04C11DB7

main:
    push rbx
    push r12
    call collect_args
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 0
    je .usage
    call build_table
    xor ebx, ebx
    xor r12d, r12d

.file:
    cmp ebx, [arg_count]
    jae .done
    mov rdi, [arg_list + rbx * 8]
    inc ebx
    call load_input
    jc .failed
    call checksum
    mov edi, eax
    call out_number
    mov edi, ' '
    call out_char
    mov edi, [file_size]
    call out_number
    mov rdi, [arg_list + rbx * 8 - 8]
    cmp word [rdi], '-'
    je .end
    push rdi
    mov edi, ' '
    call out_char
    pop rdi
    call out_text
.end:
    mov edi, 10
    call out_char
    jmp .file
.failed:
    mov r12d, 1
    jmp .file

.done:
    call out_flush
    mov eax, r12d
    pop r12
    pop rbx
    ret
.usage:
    pop r12
    pop rbx
    lea rdi, [usage_text]
    jmp usage

build_table:
    xor ecx, ecx
.entry:
    mov eax, ecx
    shl eax, 24
    mov edx, 8
.bit:
    shl eax, 1
    jnc .next_bit
    xor eax, POLYNOMIAL
.next_bit:
    dec edx
    jnz .bit
    mov [crc_table + rcx * 4], eax
    inc ecx
    cmp ecx, 256
    jb .entry
    ret

crc_step:
    mov edx, eax
    shr edx, 24
    xor dl, cl
    movzx edx, dl
    shl eax, 8
    xor eax, [crc_table + rdx * 4]
    ret

checksum:
    xor eax, eax
    xor r8d, r8d
.byte:
    cmp r8d, [file_size]
    jae .length
    mov cl, [file_data + r8]
    call crc_step
    inc r8d
    jmp .byte
.length:
    mov r8d, [file_size]
.length_byte:
    test r8d, r8d
    jz .done
    mov cl, r8b
    call crc_step
    shr r8d, 8
    jmp .length_byte
.done:
    not eax
    ret

program_name db 'cksum', 0
usage_text db 'usage: cksum file...', 0

include 'common.inc'
include '../runtime/library.inc'
include 'common_bss.inc'

crc_table rd 256
