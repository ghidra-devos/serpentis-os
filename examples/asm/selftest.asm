include '/lib/start.inc'

COUNT = 10
BUFFER_SIZE = 64 * 1024
SERP_STACK = 0x2000

main:
    push rbx
    push r12

    mov ecx, 1
    call check_sum
    jne .fail

    mov ecx, 2
    call check_factorial
    jne .fail

    mov ecx, 3
    call check_division
    jne .fail

    mov ecx, 4
    call check_strings
    jne .fail

    mov ecx, 5
    call check_buffer
    jne .fail

    mov ecx, 6
    call check_conditions
    jne .fail

    mov ecx, 7
    call check_table
    jne .fail

    mov ecx, 8
    call check_recursion
    jne .fail

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    pop r12
    pop rbx
    ret

.fail:
    mov ebx, ecx
    lea rdi, [fail_text]
    call print
    mov edi, ebx
    call print_u64
    mov edi, 10
    call print_char
    mov eax, 1
    pop r12
    pop rbx
    ret

check_sum:
    xor eax, eax
    mov edx, 1
.loop:
    add eax, edx
    inc edx
    cmp edx, COUNT
    jbe .loop
    cmp eax, COUNT * (COUNT + 1) / 2
    ret

check_factorial:
    mov eax, 1
    mov r8d, 1
.next:
    imul rax, r8
    inc r8d
    cmp r8d, 12
    jbe .next
    mov rdx, 479001600
    cmp rax, rdx
    ret

check_division:
    mov rax, 1000003
    xor edx, edx
    mov r8, 97
    div r8
    cmp rdx, 1000003 mod 97
    jne .done
    cmp rax, 1000003 / 97
.done:
    ret

check_strings:
    lea rdi, [copy_area]
    lea rsi, [phrase]
    call str_copy
    lea rdi, [copy_area]
    call str_len
    cmp rax, phrase_end - phrase - 1
    jne .done
    lea rsi, [copy_area]
    lea rdi, [phrase]
    mov rcx, rax
    repe cmpsb
.done:
    ret

check_buffer:
    lea rdi, [big_buffer]
    mov ecx, BUFFER_SIZE / 8
    mov rax, 0x0102030405060708
    rep stosq
    xor eax, eax
    lea rsi, [big_buffer]
    mov ecx, BUFFER_SIZE
.sum:
    movzx edx, byte [rsi]
    add eax, edx
    inc rsi
    dec ecx
    jnz .sum
    cmp eax, (BUFFER_SIZE / 8) * 36
    ret

check_conditions:
    mov eax, 5
    cmp eax, 7
    setl dl
    movzx edx, dl
    mov ecx, 100
    mov r8d, 200
    cmovl ecx, r8d
    cmp ecx, 200
    jne .done
    mov rax, -8
    sar rax, 2
    cmp rax, -2
    jne .done
    movsx rax, byte [negative_byte]
    cmp rax, -3
    jne .done
    bt edx, 0
    jnc .flip
    xor edx, edx
.flip:
    cmp edx, 0
.done:
    ret

check_table:
    xor eax, eax
    xor ecx, ecx
.next:
    call qword [handlers + rcx * 8]
    inc ecx
    cmp ecx, 3
    jb .next
    cmp eax, 111
    ret

handler_one:
    add eax, 1
    ret
handler_ten:
    add eax, 10
    ret
handler_hundred:
    add eax, 100
    ret

check_recursion:
    mov edi, 20
    call fibonacci
    cmp eax, 6765
    ret

fibonacci:
    cmp edi, 2
    jae .split
    mov eax, edi
    ret
.split:
    push rbx
    push rdi
    dec edi
    call fibonacci
    mov ebx, eax
    pop rdi
    push rdi
    sub edi, 2
    call fibonacci
    add eax, ebx
    pop rdi
    pop rbx
    ret

ok_text db 'SELFTEST OK', 0
fail_text db 'SELFTEST FAIL ', 0
phrase db 'assembled inside serpentis', 0
phrase_end:
negative_byte db -3
    align 8
handlers dq handler_one, handler_ten, handler_hundred
    times 3 db 0

include '/lib/library.inc'

copy_area rb 64
big_buffer rb BUFFER_SIZE
