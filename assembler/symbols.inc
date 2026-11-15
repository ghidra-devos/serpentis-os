SYM_NAME = 0
SYM_NAME_MAX = 64
SYM_VALUE = 64

SYM_PASS = 72
SYM_FLAGS = 76
SYM_SIZE = 80
SYM_SLOTS = 8192
LAST_PASS = 3

SYM_FLAG_USED = 1
SYM_FLAG_CONSTANT = 2
SYM_FLAG_LABEL = 4

symbol_build_name:
    push rbx
    lea rdi, [symbol_name]
    xor ebx, ebx

    cmp byte [rsi], '.'
    jne .copy
    cmp byte [global_label], 0
    je .copy
    push rsi
    lea rsi, [global_label]
.prefix:
    mov al, [rsi]
    test al, al
    jz .prefix_done
    mov [rdi + rbx], al
    inc rsi
    inc ebx
    jmp .prefix
.prefix_done:
    pop rsi

.copy:
    test ecx, ecx
    jz .done
    cmp ebx, SYM_NAME_MAX - 1
    jae .too_long
    mov al, [rsi]
    mov [rdi + rbx], al
    inc rsi
    inc ebx
    dec ecx
    jmp .copy

.done:
    mov byte [rdi + rbx], 0
    mov eax, ebx
    pop rbx
    ret

.too_long:
    pop rbx
    lea rdi, [error_name_too_long]
    jmp asm_error

symbol_lookup:
    call symbol_build_name

    mov eax, 0x811C9DC5         ; fnv-1a
    lea rsi, [symbol_name]
.hash:
    movzx edx, byte [rsi]
    test edx, edx
    jz .hashed
    xor eax, edx
    imul eax, eax, 0x01000193
    inc rsi
    jmp .hash
.hashed:
    and eax, SYM_SLOTS - 1
    mov r8d, SYM_SLOTS

.probe:
    imul rdx, rax, SYM_SIZE
    lea rdx, [symbol_table + rdx]
    test dword [rdx + SYM_FLAGS], SYM_FLAG_USED
    jz .create

    lea rsi, [symbol_name]
    lea rdi, [rdx + SYM_NAME]
    push rax
    push rdx
    call str_equal
    mov ecx, eax
    pop rdx
    pop rax
    test ecx, ecx
    jnz .found

    inc eax
    and eax, SYM_SLOTS - 1
    dec r8d
    jnz .probe

    lea rdi, [error_symbols_full]
    jmp asm_error

.create:
    mov dword [rdx + SYM_FLAGS], SYM_FLAG_USED
    mov dword [rdx + SYM_PASS], 0
    mov qword [rdx + SYM_VALUE], 0
    lea rdi, [rdx + SYM_NAME]
    lea rsi, [symbol_name]
    push rdx
    call str_copy
    pop rdx

.found:
    mov rax, rdx
    ret

symbol_define:
    push rbx
    push r12
    mov r12, rdx
    push rdx
    call symbol_lookup
    pop rdx
    mov rbx, rax

    test dword [rbx + SYM_FLAGS], SYM_FLAG_LABEL
    jz .set
    mov eax, [current_pass]
    cmp [rbx + SYM_PASS], eax
    je .duplicate
    cmp [rbx + SYM_VALUE], r12
    je .set
    cmp byte [final_pass], 0
    je .moved
    cmp dword [error_count], 0
    jne .set
    jmp .phase
.moved:
    mov byte [layout_changed], 1

.set:
    mov [rbx + SYM_VALUE], r12
    mov eax, [current_pass]
    mov [rbx + SYM_PASS], eax
    mov eax, [symbol_define_flags]
    or [rbx + SYM_FLAGS], eax
    mov rax, rbx
    pop r12
    pop rbx
    ret

.duplicate:
    lea rdi, [error_duplicate_label]
    jmp asm_error

.phase:
    lea rdi, [error_phase]
    jmp asm_error
