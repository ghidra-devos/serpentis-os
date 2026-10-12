exception_fatal:
    cli
    mov rsp, PANIC_STACK_TOP

    mov [last_exception], dil

    mov al, COLOR_RED
    call vga_clear

    movzx ebx, byte [last_exception]

    ; vector as two hex digits, numbers.asm is not safe to call from here
    mov eax, ebx
    and eax, 0x0F
    mov dl, [numbers_hex_chars + rax]
    mov [exception_vector_text + 1], dl

    mov eax, ebx
    shr eax, 4
    and eax, 0x0F
    mov dl, [numbers_hex_chars + rax]
    mov [exception_vector_text], dl

    mov rsi, exception_title
    mov ebx, 112
    mov ecx, 128
    mov r8b, COLOR_WHITE
    mov r9b, COLOR_RED
    call text_draw_string

    mov rsi, exception_message
    mov ebx, 112
    mov ecx, 176
    mov r8b, COLOR_YELLOW
    mov r9b, COLOR_RED
    call text_draw_string

    mov rsi, exception_vector_text
    mov ebx, 320
    mov ecx, 176
    mov r8b, COLOR_WHITE
    mov r9b, COLOR_RED
    call text_draw_string

    mov rsi, exception_halted_text
    mov ebx, 112
    mov ecx, 224
    mov r8b, COLOR_WHITE
    mov r9b, COLOR_RED
    call text_draw_string

.halt:
    hlt
    jmp .halt

exception_common:
    cld
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call exception_frame_dispatch

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq

exception_frame_dispatch:
    push rbx
    mov rbx, rdi

    mov eax, [rbx + TRAP_VECTOR]
    mov [exception_last_vector], eax

    mov eax, [rbx + TRAP_ERROR]
    mov [exception_last_error], eax

    mov eax, [rbx + TRAP_CS]
    mov [exception_last_cs], eax

    test al, 3
    jz .kernel_exception

.user_exception:
    mov al, COLOR_LIGHT_RED
    call terminal_set_foreground

    mov rsi, user_exception_text
    call terminal_print

    mov eax, [exception_last_vector]
    call terminal_print_hex64
    call terminal_newline

    mov al, COLOR_WHITE
    call terminal_set_foreground

    cmp byte [user_test_active], 0
    jne .test_exception

    movzx eax, word [process_current_index]
    test eax, eax
    jz .kernel_exception

    mov esi, 0x80000000
    or esi, [exception_last_vector]
    mov rdi, rbx
    call process_exit_switch
    jc exec_return_to_kernel    ; no user process left to run, unwind to the exec caller

    pop rbx
    ret

.test_exception:
    mov edi, 0x80000000
    or edi, [exception_last_vector]
    call user_test_abort
    jmp $

.kernel_exception:
    mov edi, [exception_last_vector]
    jmp exception_fatal

macro exception_no_error label_name, vector_number
{
label_name:
    push 0
    push vector_number
    jmp exception_common
}

macro exception_with_error label_name, vector_number
{
label_name:
    push vector_number
    jmp exception_common
}

exception_no_error exception_0, 0
exception_no_error exception_1, 1
exception_no_error exception_2, 2
exception_no_error exception_3, 3
exception_no_error exception_4, 4
exception_no_error exception_5, 5
exception_no_error exception_6, 6
exception_no_error exception_7, 7
exception_with_error exception_8, 8
exception_no_error exception_9, 9
exception_with_error exception_10, 10
exception_with_error exception_11, 11
exception_with_error exception_12, 12
exception_with_error exception_13, 13
exception_with_error exception_14, 14
exception_no_error exception_15, 15
exception_no_error exception_16, 16
exception_with_error exception_17, 17
exception_no_error exception_18, 18
exception_no_error exception_19, 19
exception_no_error exception_20, 20
exception_no_error exception_21, 21
exception_no_error exception_22, 22
exception_no_error exception_23, 23
exception_no_error exception_24, 24
exception_no_error exception_25, 25
exception_no_error exception_26, 26
exception_no_error exception_27, 27
exception_no_error exception_28, 28
exception_no_error exception_29, 29
exception_no_error exception_30, 30
exception_no_error exception_31, 31

exception_handler_table:
    dq exception_0
    dq exception_1
    dq exception_2
    dq exception_3
    dq exception_4
    dq exception_5
    dq exception_6
    dq exception_7
    dq exception_8
    dq exception_9
    dq exception_10
    dq exception_11
    dq exception_12
    dq exception_13
    dq exception_14
    dq exception_15
    dq exception_16
    dq exception_17
    dq exception_18
    dq exception_19
    dq exception_20
    dq exception_21
    dq exception_22
    dq exception_23
    dq exception_24
    dq exception_25
    dq exception_26
    dq exception_27
    dq exception_28
    dq exception_29
    dq exception_30
    dq exception_31

last_exception db 0
exception_last_vector dd 0
exception_last_error dd 0
exception_last_cs dd 0

exception_title db 'SERPENTIS KERNEL PANIC', 0
exception_message db 'CPU exception vector: 0x', 0
exception_vector_text db '00', 0
exception_halted_text db 'The processor has been halted.', 0
user_exception_text db 'Ring 3 process terminated, vector: ', 0
