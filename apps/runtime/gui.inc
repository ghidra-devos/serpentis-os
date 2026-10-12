GUI_CREATE = 1
GUI_DAMAGE = 2
GUI_DESTROY = 3
GUI_RESIZED = 4
GUI_TITLE = 5
GUI_CREATED = 101
GUI_KEY = 102
GUI_MOUSE = 103
GUI_CLOSE = 104
GUI_RESIZE = 106
GUI_REFUSED = 105
GUI_WHEEL = 107
GUI_NOTIFY_EXIT = 200
CREATE_ATTEMPTS = 200

GUI_SERVICE_QUERY = 0
GUI_SERVICE_REGISTER = 1
GUI_SERVICE_FONT = 2

EVENT_NONE = 0
EVENT_KEY = 1
EVENT_MOUSE = 2
EVENT_MESSAGE = 3
EVENT_TTY = 4
EVENT_TICK = 5
EVENT_WANT_TICK = 2
EVENT_TYPE = 0
EVENT_CODE = 4
EVENT_X = 8
EVENT_Y = 12
EVENT_BUTTONS = 16
EVENT_WHEEL = 20
EVENT_PAYLOAD = 16
EVENT_BYTES = 80

MSG_OP = 0
MSG_WINDOW = 4
MSG_A = 8
MSG_B = 12
MSG_C = 16
MSG_D = 20
MSG_TITLE = 24
MSG_TITLE_MAX = 40
MSG_BYTES = 64

SHM_BASE = SHM_ADDRESS
SURFACE_BYTES = 0x54000
WINDOW_MAX = 24
SHM_FONT_OFFSET = WINDOW_MAX * SURFACE_BYTES

gui_connect:
    xor edi, edi
    call sys_service
    test eax, eax
    js .done
    mov [gui_server], eax
    call sys_shm_attach
    mov qword [draw_font], SHM_BASE + SHM_FONT_OFFSET
    xor eax, eax
.done:
    ret

gui_open_window:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13d, esi
    mov rbx, rdx

    lea rdi, [gui_message]
    xor eax, eax
    mov ecx, MSG_BYTES
    rep stosb
    mov dword [gui_message + MSG_OP], GUI_CREATE
    mov [gui_message + MSG_A], r12d
    mov [gui_message + MSG_B], r13d
    lea rdi, [gui_message + MSG_TITLE]
    mov rsi, rbx
    mov ecx, MSG_TITLE_MAX - 1
.title:
    mov al, [rsi]
    test al, al
    jz .titled
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .title
.titled:
    mov ebx, CREATE_ATTEMPTS
.send:
    mov edi, [gui_server]
    lea rsi, [gui_message]
    call sys_send
    test eax, eax
    jns .wait
    cmp eax, SYS_ERR_NO_PROCESS
    je .done
    dec ebx
    jz .done
    call sys_yield
    jmp .send

.wait:
    lea rdi, [gui_event]
    xor esi, esi
    call sys_event
    cmp eax, EVENT_MESSAGE
    jne .wait
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_REFUSED
    je .refused
    cmp eax, GUI_CREATED
    jne .wait
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_WINDOW]
    mov edx, [gui_event + EVENT_PAYLOAD + MSG_A]
    add rdx, SHM_BASE
    jmp .done
.refused:
    mov eax, -1
.done:
    pop r13
    pop r12
    pop rbx
    ret

gui_request:
    mov [gui_message + MSG_OP], esi
    mov [gui_message + MSG_WINDOW], edi
    mov edi, [gui_server]
    lea rsi, [gui_message]
    jmp sys_send

gui_damage:
    mov esi, GUI_DAMAGE
    jmp gui_request

; edi = window, rsi -> new title
gui_set_title:
    push rdi
    lea rdi, [gui_message + MSG_TITLE]
    mov ecx, MSG_TITLE_MAX - 1
.copy:
    mov al, [rsi]
    mov [rdi], al
    test al, al
    jz .copied
    inc rsi
    inc rdi
    dec ecx
    jnz .copy
    mov byte [rdi], 0
.copied:
    pop rdi
    mov esi, GUI_TITLE
    jmp gui_request

gui_close_window:
    mov esi, GUI_DESTROY
    jmp gui_request

gui_ack_resize:
    mov [gui_message + MSG_A], esi
    mov [gui_message + MSG_B], edx
    mov esi, GUI_RESIZED
    jmp gui_request

align 8
gui_server dd 0
gui_event:
    times EVENT_BYTES db 0
gui_message:
    times MSG_BYTES db 0
