include '../runtime/program.inc'

LOGO_COLUMNS = 24
LOGO_ROWS = 22
; neofetch: the serpent on the left, facts on the right. the picture is 24x22 pixels of art
; drawn as 11 text rows, every char cell is two pixels stacked (upper half block, fg = top,
; bg = bottom). colors go out as ansi escapes, lds turns them into attributes

LOGO_LINES = LOGO_ROWS / 2
LOGO_GAP = 3
MIB = 1024 * 1024

main:
    push rbx
    push r12
    lea rdi, [info]
    call sys_sysinfo
    call read_cpu_brand
    call count_programs

    call sys_getuid
    mov edi, eax
    call user_name
    lea rdi, [user_host]
    mov rsi, rax
    call str_copy
    lea rdi, [user_host]
    call str_len
    lea rdi, [user_host + rax]
    lea rsi, [host_text]
    call str_copy
    lea rdi, [user_host]
    lea rsi, [empty_text]
    call add_line
    lea rdi, [dashes]
    lea rsi, [empty_text]
    call add_line
    lea rdi, [os_label]
    lea rsi, [os_value]
    call add_line
    lea rdi, [kernel_label]
    lea rsi, [kernel_value]
    call add_line
    call add_uptime
    call add_programs
    lea rdi, [shell_label]
    lea rsi, [shell_value]
    call add_line
    lea rdi, [resolution_label]
    lea rsi, [resolution_value]
    call add_line
    lea rdi, [wm_label]
    lea rsi, [wm_value]
    call add_line
    lea rdi, [assembler_label]
    lea rsi, [assembler_value]
    call add_line
    lea rdi, [cpu_label]
    lea rsi, [cpu_brand]
    call add_line
    call add_memory

    mov edi, 10
    call out_char
    xor ebx, ebx
.row:
    mov eax, ebx
    cmp eax, [line_count]
    jb .print
    cmp ebx, LOGO_LINES
    jae .done
.print:
    cmp ebx, LOGO_LINES
    jae .no_logo
    call logo_row
    mov r12d, LOGO_COLUMNS
    jmp .pad
.no_logo:
    xor r12d, r12d
.pad:
    cmp r12d, LOGO_COLUMNS + LOGO_GAP
    jae .text
    mov edi, ' '
    call out_char
    inc r12d
    jmp .pad
.text:
    cmp ebx, [line_count]
    jae .end_row
    imul edi, ebx, LINE_BYTES
    lea rdi, [lines + rdi]
    call out_text
.end_row:
    mov edi, 10
    call out_char
    inc ebx
    jmp .row
.done:
    mov edi, 10
    call out_char
    call out_flush
    xor eax, eax
    pop r12
    pop rbx
    ret

; ebx = text row. builds the escape sequences for one row of the logo
logo_row:
    push r12
    push r13
    push r14
    push r15
    imul r12d, ebx, LOGO_COLUMNS * 2
    lea r12, [snake_art + r12]
    mov dword [shown_foreground], -1
    mov dword [shown_background], -1
    xor r13d, r13d
.column:
    cmp r13d, LOGO_COLUMNS
    jae .reset
    movzx edi, byte [r12 + r13]
    call art_color
    mov r14d, eax
    movzx edi, byte [r12 + r13 + LOGO_COLUMNS]
    call art_color
    mov r15d, eax
    test r14d, r14d
    jnz .upper
    test r15d, r15d
    jz .empty
    mov edi, r15d
    mov esi, ANSI_BLACK_BACKGROUND
    mov edx, LOWER_HALF
    jmp .cell
.upper:
    mov edi, r14d
    mov esi, ANSI_BLACK_BACKGROUND
    test r15d, r15d
    jz .upper_only
    lea esi, [r15 + 10]
.upper_only:
    mov edx, UPPER_HALF
    jmp .cell
.empty:
    mov edi, [shown_foreground]
    test edi, edi
    jns .kept
    mov edi, ANSI_DEFAULT_FOREGROUND
.kept:
    mov esi, ANSI_BLACK_BACKGROUND
    mov edx, ' '
.cell:
    call logo_cell
    inc r13d
    jmp .column
.reset:
    lea rdi, [ansi_reset]
    call out_text
    pop r15
    pop r14
    pop r13
    pop r12
    ret

logo_cell:
    push rdx
    cmp edi, [shown_foreground]
    jne .change
    cmp esi, [shown_background]
    je .glyph
.change:
    mov [shown_foreground], edi
    mov [shown_background], esi
    mov edi, 27
    call out_char
    mov edi, '['
    call out_char
    mov edi, [shown_foreground]
    call out_number
    mov edi, ';'
    call out_char
    mov edi, [shown_background]
    call out_number
    mov edi, 'm'
    call out_char
.glyph:
    pop rdi
    jmp out_char

art_color:
    lea rsi, [art_palette]
.next:
    mov al, [rsi]
    test al, al
    jz .none
    cmp al, dil
    je .found
    add rsi, 2
    jmp .next
.found:
    movzx eax, byte [rsi + 1]
    ret
.none:
    xor eax, eax
    ret

add_line:
    push rsi
    push rdi
    mov eax, [line_count]
    imul eax, eax, LINE_BYTES
    lea rdi, [lines + rax]
    pop rsi
    call str_copy
    mov rdi, rax
    call str_len
    add rdi, rax
    pop rsi
    call str_copy
    inc dword [line_count]
    ret

current_line:
    mov eax, [line_count]
    imul eax, eax, LINE_BYTES
    lea rdi, [lines + rax]
    ret

append:
    push rsi
    call current_line
    push rdi
    call str_len
    pop rdi
    add rdi, rax
    pop rsi
    jmp str_copy

append_number:
    call u64_to_dec
    mov rsi, rax
    jmp append

add_uptime:
    call current_line
    lea rsi, [uptime_label]
    call str_copy
    mov rax, [info + 16]
    xor edx, edx
    mov ecx, PIT_FREQUENCY
    div rcx
    mov rbx, rax
    xor edx, edx
    mov ecx, 3600
    div rcx
    test rax, rax
    jz .minutes
    mov rdi, rax
    call append_number
    lea rsi, [hours_text]
    call append
.minutes:
    mov rax, rbx
    xor edx, edx
    mov ecx, 3600
    div rcx
    mov rax, rdx
    xor edx, edx
    mov ecx, 60
    div rcx
    push rdx
    mov rdi, rax
    call append_number
    lea rsi, [minutes_text]
    call append
    pop rdi
    call append_number
    lea rsi, [seconds_text]
    call append
    inc dword [line_count]
    ret

add_programs:
    call current_line
    lea rsi, [programs_label]
    call str_copy
    mov edi, [program_count]
    call append_number
    lea rsi, [programs_suffix]
    call append
    inc dword [line_count]
    ret

add_memory:
    call current_line
    lea rsi, [memory_label]
    call str_copy
    mov rax, [info + 8]
    xor edx, edx
    mov ecx, MIB
    div rcx
    mov rdi, rax
    call append_number
    lea rsi, [mib_separator]
    call append
    mov rax, [info]
    xor edx, edx
    mov ecx, MIB
    div rcx
    mov rdi, rax
    call append_number
    lea rsi, [mib_text]
    call append
    inc dword [line_count]
    ret

read_cpu_brand:
    push rbx
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000004
    jb .generic
    lea rdi, [brand_raw]
    mov esi, 0x80000002
.leaf:
    mov eax, esi
    push rdi
    push rsi
    cpuid
    pop rsi
    pop rdi
    mov [rdi], eax
    mov [rdi + 4], ebx
    mov [rdi + 8], ecx
    mov [rdi + 12], edx
    add rdi, 16
    inc esi
    cmp esi, 0x80000004
    jbe .leaf
    mov byte [rdi], 0
    lea rsi, [brand_raw]
.skip:
    cmp byte [rsi], ' '
    jne .copy
    inc rsi
    jmp .skip
.copy:
    lea rdi, [cpu_brand]
    call str_copy
    pop rbx
    ret
.generic:
    lea rdi, [cpu_brand]
    lea rsi, [generic_cpu]
    call str_copy
    pop rbx
    ret

count_programs:
    push rbx
    lea rdi, [bin_path]
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    js .done
    mov ebx, eax
.entry:
    mov edi, ebx
    lea rsi, [dirent]
    mov edx, SYS_DIRENT_BYTES
    call sys_readdir
    test eax, eax
    jle .close
    inc dword [program_count]
    jmp .entry
.close:
    mov edi, ebx
    call sys_close
.done:
    pop rbx
    ret

program_name db 'neofetch', 0

UPPER_HALF = 0xDF
LOWER_HALF = 0xDC
ANSI_BLACK_BACKGROUND = 40
ANSI_DEFAULT_FOREGROUND = 37

snake_art:
    db '......DDDDDDD...........'
    db '....DDGGGGGGGDD.........'
    db '...DGGGLLGGGGGGD........'
    db '..DGGWWGGGGGGGGGD.......'
    db '..DGGWKGGGGGGGGGD.......'
    db '.DGGGGGGGGGGGLGGD.......'
    db '.DDDDDDDDGGGGGGGD.......'
    db 'R.DYYYYYYDGGDGGD........'
    db '.RRDDDDDDDGDDDGD........'
    db 'R.........DGDGGD........'
    db '..........DGGGLGD.......'
    db '...........DGDGGD.......'
    db '............DDDGGD......'
    db '....DDDDDD...DGDGGD.....'
    db '..DDGLLGGGDD..DGGLD.....'
    db '.DGGGDGGGDGGD.DGDGD.....'
    db 'DGGYYYYYYYGGGDDDDGD.....'
    db 'DGYYYYYYYYYYGGGDGD......'
    db 'DGGYYYYYYYYYYGGGGD......'
    db '.DGGGYYYYYYYYYGGD.......'
    db '..DDGGGDGGGDGGDD........'
    db '....DDDDDDDDDD..........'
assert $ - snake_art = LOGO_COLUMNS * LOGO_ROWS

art_palette db 'D', 32, 'G', 92, 'L', 92, 'Y', 93, 'W', 97, 'K', 30, 'R', 91, 0
ansi_reset db 27, '[0m', 0

host_text db '@serpentis', 0
dashes db '---------------', 0
empty_text db 0
os_label db 'OS: ', 0
os_value db 'serpentis-os x86_64', 0
kernel_label db 'Kernel: ', 0
kernel_value db 'serpentis', 0
uptime_label db 'Uptime: ', 0
hours_text db ' hours, ', 0
minutes_text db ' mins, ', 0
seconds_text db ' secs', 0
programs_label db 'Programs: ', 0
programs_suffix db ' (/bin)', 0
shell_label db 'Shell: ', 0
shell_value db 'lds', 0
resolution_label db 'Resolution: ', 0
resolution_value db '640x480', 0
wm_label db 'WM: ', 0
wm_value db 'gui', 0
assembler_label db 'Assembler: ', 0
assembler_value db 'Apple', 0
cpu_label db 'CPU: ', 0
memory_label db 'Memory: ', 0
mib_separator db 'MiB / ', 0
mib_text db 'MiB', 0
generic_cpu db 'x86-64 processor', 0
bin_path db '/bin', 0

include 'common.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
include '../runtime/users_bss.inc'

LINE_BYTES = 96
align 8
info rq 4
line_count rd 1
shown_foreground rd 1
shown_background rd 1
program_count rd 1
brand_raw rb 52
cpu_brand rb 52
dirent rb SYS_DIRENT_BYTES
lines rb LINE_BYTES * 16
user_host rb 64
