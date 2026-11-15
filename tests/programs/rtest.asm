include '../../apps/runtime/program.inc'

main:
    push rbx

    lea rdi, [sample]
    call str_len
    cmp rax, sample_end - sample - 1
    jne .fail

    lea rdi, [copy_buffer]
    lea rsi, [sample]
    call str_copy
    lea rdi, [copy_buffer]
    lea rsi, [sample]
    call str_equal
    test eax, eax
    jz .fail

    lea rdi, [copy_buffer]
    mov esi, 'x'
    mov edx, 3
    call mem_set
    cmp dword [copy_buffer], 'xxxp'
    jne .fail

    mov rdi, 18446744073709551615
    call u64_to_dec
    mov rdi, rax
    lea rsi, [max_u64_text]
    call str_equal
    test eax, eax
    jz .fail

    lea rdi, [hello_path]
    call sys_spawn
    test eax, eax
    js .fail
    mov edi, eax
    xor esi, esi
    call sys_waitpid
    cmp eax, 5
    jne .fail

    lea rdi, [pid_text]
    call print
    call sys_getpid
    mov rdi, rax
    call print_u64
    mov edi, 10
    call print_char

    lea rdi, [ok_text]
    call print_line
    xor eax, eax
    pop rbx
    ret

.fail:
    lea rdi, [fail_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

sample db 'serpentis', 0
sample_end:
max_u64_text db '18446744073709551615', 0
hello_path db '/bin/hello.serp', 0
pid_text db 'rtest pid ', 0
ok_text db 'RUNTIME OK', 0
fail_text db 'RUNTIME FAIL', 0
copy_buffer:
    times 16 db 0

include '../../apps/runtime/library.inc'
