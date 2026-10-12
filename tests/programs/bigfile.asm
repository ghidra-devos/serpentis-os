include '../../apps/runtime/program.inc'

CHUNK = 2048
TOTAL = 200 * 1024

main:
    push rbx
    push r12
    push r13

    lea rdi, [path]
    call sys_unlink

    lea rdi, [path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .fail
    mov ebx, eax

    xor r12d, r12d
.write_chunk:
    cmp r12d, TOTAL
    jae .written
    mov esi, r12d
    call fill_chunk
    mov edi, ebx
    lea rsi, [chunk]
    mov edx, CHUNK
    call sys_write
    cmp eax, CHUNK
    jne .fail
    add r12d, CHUNK
    jmp .write_chunk

.written:
    mov edi, ebx
    call sys_close

    lea rdi, [path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .fail
    mov ebx, eax

    xor r12d, r12d
.read_chunk:
    mov edi, ebx
    lea rsi, [readback]
    mov edx, CHUNK
    call sys_read
    test eax, eax
    jz .read_done
    js .fail
    cmp eax, CHUNK
    jne .fail

    mov esi, r12d
    call fill_chunk
    lea rsi, [chunk]
    lea rdi, [readback]
    mov ecx, CHUNK
    repe cmpsb
    jne .fail

    add r12d, CHUNK
    jmp .read_chunk

.read_done:
    cmp r12d, TOTAL
    jne .fail
    mov edi, ebx
    call sys_close

    lea rdi, [path]
    call sys_unlink
    test eax, eax
    jnz .fail

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    jmp .out

.fail:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1

.out:
    pop r13
    pop r12
    pop rbx
    ret

fill_chunk:
    xor ecx, ecx
.next:
    lea eax, [rsi + rcx]
    imul eax, 7
    shr eax, 3
    mov [chunk + rcx], al
    inc ecx
    cmp ecx, CHUNK
    jb .next
    ret

path db '/big.bin', 0
ok_text db 'BIGFILE OK', 0
fail_text db 'BIGFILE FAIL', 0

chunk:
    times CHUNK db 0
readback:
    times CHUNK db 0

include '../../apps/runtime/library.inc'
