SERP_STACK = 0x4000

include '../runtime/program.inc'


COLOR_BLUE = 1
COLOR_LIGHT_GRAY = 7
COLOR_DARK_GRAY = 8
COLOR_LIGHT_CYAN = 11

CLOCK_WIDTH = 260
CLOCK_HEIGHT = 150
CLOCK_MARGIN = 10
DATE_CHARS = 10
TIME_UNITS = 31
KEY_ESCAPE = 27

main:
    push rbx
    lea rdi, [window_title]
    mov esi, CLOCK_WIDTH
    mov edx, CLOCK_HEIGHT
    call screen_open
    mov byte [running], 1
    mov byte [last_second], 0xFF  ; no real second, forces the first draw

.loop:
    call refresh
    cmp byte [running], 0
    je .exit
    mov esi, EVENT_WANT_TICK
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .exit
    jmp .loop

.key:
    mov eax, [screen_key]
    cmp eax, 'q'
    je .quit
    cmp eax, KEY_ESCAPE
    je .quit
    jmp .loop
.quit:
    mov byte [running], 0
    jmp .loop

.resize:
    mov byte [last_second], 0xFF
    call refresh
    call screen_acknowledge_resize
    jmp .loop

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

; ticks wake us often, we only draw when the rtc second changed
refresh:
    lea rdi, [now]
    call sys_time
    test eax, eax
    js .done
    mov al, [now + TIME_SECOND]
    cmp al, [last_second]
    je .done
    mov [last_second], al
    call layout
    call render
    call screen_present
.done:
    ret

layout:
    mov eax, [screen_width]
    sub eax, CLOCK_MARGIN * 2
    xor edx, edx
    mov ecx, TIME_UNITS + 2
    div ecx
    mov r8d, eax
    mov eax, [screen_height]
    sub eax, CLOCK_MARGIN * 2 + 28
    jle .smallest
    xor edx, edx
    mov ecx, 9
    div ecx
    cmp eax, r8d
    jae .sized
    mov r8d, eax
.sized:
    cmp r8d, 2
    jae .ready
.smallest:
    mov r8d, 2
.ready:
    mov [unit], r8d
    mov eax, r8d
    shr eax, 2
    mov ecx, r8d
    sub ecx, eax
    cmp ecx, 2
    jae .thickness
    mov ecx, 2
.thickness:
    mov [segment_size], ecx
    lea eax, [r8 * 4]
    mov [digit_width], eax
    imul eax, r8d, 7
    mov [digit_height], eax

    imul eax, r8d, TIME_UNITS
    mov ecx, [screen_width]
    sub ecx, eax
    sar ecx, 1
    mov [left], ecx

    imul eax, r8d, 9
    add eax, 22
    mov ecx, [screen_height]
    sub ecx, eax
    sar ecx, 1
    mov [top], ecx
    ret

render:
    push rbx
    call screen_clear

    mov ecx, 10
    movzx eax, byte [now + TIME_HOUR]
    xor edx, edx
    div ecx
    mov [pair], eax
    mov [pair + 4], edx
    movzx eax, byte [now + TIME_MINUTE]
    xor edx, edx
    div ecx
    mov [pair + 8], eax
    mov [pair + 12], edx
    movzx eax, byte [now + TIME_SECOND]
    xor edx, edx
    div ecx
    mov [pair + 16], eax
    mov [pair + 20], edx

    xor ebx, ebx
.digit:
    cmp ebx, 6
    jae .colon
    movzx eax, byte [digit_columns + rbx]
    imul eax, [unit]
    add eax, [left]
    mov [digit_x], eax
    mov eax, [top]
    mov [digit_y], eax
    mov edi, [pair + rbx * 4]
    call draw_digit
    inc ebx
    jmp .digit

.colon:
    mov edi, 10
    call draw_colon
    mov edi, 21
    call draw_colon

    call draw_seconds
    call draw_date
    pop rbx
    ret

draw_colon:
    push rbx
    imul edi, [unit]
    add edi, [left]
    mov eax, [unit]
    shr eax, 1
    sub edi, eax
    mov ebx, edi
    imul esi, [unit], 2
    add esi, [top]
    mov edx, [unit]
    mov ecx, [unit]
    mov r8d, COLOR_LIGHT_CYAN
    call draw_fill
    mov edi, ebx
    imul esi, [unit], 5
    add esi, [top]
    mov edx, [unit]
    mov ecx, [unit]
    mov r8d, COLOR_LIGHT_CYAN
    call draw_fill
    pop rbx
    ret

draw_seconds:
    push rbx
    imul ebx, [unit], TIME_UNITS
    imul esi, [unit], 8
    add esi, [top]
    mov [bar_y], esi
    mov edi, [left]
    mov edx, ebx
    mov ecx, 4
    mov r8d, COLOR_DARK_GRAY
    call draw_fill

    movzx eax, byte [now + TIME_SECOND]
    inc eax
    imul eax, ebx
    xor edx, edx
    mov ecx, 60
    div ecx
    mov edx, eax
    mov edi, [left]
    mov esi, [bar_y]
    mov ecx, 4
    mov r8d, COLOR_BLUE
    call draw_fill
    pop rbx
    ret

draw_date:
    movzx eax, byte [now + TIME_DAY]
    lea rdi, [date_text]
    call put_pair
    mov byte [date_text + 2], '.'
    movzx eax, byte [now + TIME_MONTH]
    lea rdi, [date_text + 3]
    call put_pair
    mov byte [date_text + 5], '.'
    movzx eax, word [now + TIME_YEAR]
    xor edx, edx
    mov ecx, 100
    div ecx
    push rdx
    lea rdi, [date_text + 6]
    call put_pair
    pop rax
    lea rdi, [date_text + 8]
    call put_pair
    mov byte [date_text + DATE_CHARS], 0

    mov edi, [screen_width]
    sub edi, DATE_CHARS * 8
    sar edi, 1
    mov esi, [bar_y]
    add esi, 10
    lea rdx, [date_text]
    mov cl, COLOR_LIGHT_GRAY
    mov r8d, -1
    jmp draw_text

put_pair:
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    mov [rdi], al
    add dl, '0'
    mov [rdi + 1], dl
    ret

window_title db 'Clock', 0
digit_columns db 0, 5, 11, 16, 22, 27

include 'digits.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'

align 8
pair rd 6
unit rd 1
segment_size rd 1
digit_width rd 1
digit_height rd 1
digit_x rd 1
digit_y rd 1
left rd 1
top rd 1
bar_y rd 1
now rb TIME_BYTES
date_text rb DATE_CHARS + 2
last_second rb 1
running rb 1
