speaker_init:
    mov byte [speaker_active], 0
    mov qword [speaker_deadline], 0
    call speaker_stop
    ret

speaker_beep:
    push rcx
    push rdx
    push r8
    push r9

    cmp eax, 20
    jb .failed

    cmp eax, 20000
    ja .failed

    cmp ebx, 1
    jb .failed

    cmp ebx, 5000
    ja .failed

    mov ecx, eax
    mov r9d, ebx

    mov eax, PIT_BASE_FREQUENCY
    xor edx, edx
    div ecx

    cmp eax, 1
    jb .failed

    cmp eax, 65535
    ja .failed

    mov r8w, ax

    mov dx, PIT_COMMAND
    mov al, 0xB6                ; channel 2, low then high byte, mode 3
    out dx, al

    mov dx, PIT_CHANNEL2
    mov ax, r8w
    out dx, al

    mov al, ah
    out dx, al

    mov dx, 0x61
    in al, dx
    or al, 3                    ; gate channel 2 + speaker on
    out dx, al

    mov eax, r9d
    add eax, 9
    xor edx, edx
    mov ecx, 10
    div ecx

    test eax, eax
    jnz .deadline

    mov eax, 1

.deadline:
    mov rdx, [pit_ticks]
    add rdx, rax
    mov [speaker_deadline], rdx

    mov byte [speaker_active], 1

    pop r9
    pop r8
    pop rdx
    pop rcx
    clc
    ret

.failed:
    pop r9
    pop r8
    pop rdx
    pop rcx
    stc
    ret

speaker_stop:
    push rax
    push rdx

    mov dx, 0x61
    in al, dx
    and al, 0xFC
    out dx, al

    mov byte [speaker_active], 0

    pop rdx
    pop rax
    ret

speaker_service:
    cmp byte [speaker_active], 0
    je .done

    mov rax, [pit_ticks]
    cmp rax, [speaker_deadline]
    jb .done

    call speaker_stop

.done:
    ret

speaker_active db 0
speaker_deadline dq 0
