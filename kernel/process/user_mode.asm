; old ring 3 test mode. nothing switches it on anymore, it just sits here
user_test_abort:
    cli

    call process_finish_current

    mov byte [user_test_active], 0

    mov rax, [user_return_cr3]
    mov cr3, rax

    mov rsp, [user_return_rsp]
    sti
    ret

user_test_exit:
    xor edi, edi
    call user_test_abort

user_test_active db 0
user_return_rsp dq 0
user_return_cr3 dq 0

