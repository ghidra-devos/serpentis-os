wait_for_clients:
    push rbx
    call sys_getticks
    mov rbx, rax
.next:
    cmp dword [z_count], 0
    je .done
    call sys_getticks
    sub rax, rbx
    cmp rax, CLIENT_EXIT_TICKS
    jae .done
    lea rdi, [event]
    mov esi, SYS_GETKEY_NOWAIT
    call sys_event
    test eax, eax
    jz .next
    call handle_event
    jmp .next
.done:
    pop rbx
    ret

close_all_windows:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, WINDOW_MAX
    jae .done
    imul eax, ebx, WIN_SIZE
    cmp dword [windows + rax + WIN_USED], 0
    je .skip
    mov edi, ebx
    mov esi, GUI_CLOSE
    xor edx, edx
    xor ecx, ecx
    xor r8d, r8d
    call send_to_window
.skip:
    inc ebx
    jmp .next
.done:
    pop rbx
    ret
