SERP_STACK = 0x4000

include '../runtime/program.inc'

PICKER_COLUMNS = 56
PICKER_ROWS = 15
PICKER_WIDTH = PICKER_COLUMNS * 8 + SCREEN_PAD_X * 2
PICKER_HEIGHT = PICKER_ROWS * 16 + SCREEN_PAD_Y * 2
LIST_TOP = 2
PREVIEW_COLUMN = 21
PREVIEW_WIDTH = 33
PREVIEW_ROWS = 9
THEMES_SHOWN = 6
THEMES_MAX = 16
KEY_ESCAPE = 27

ATTR_TEXT = 0x07
ATTR_TITLE = 0x0F
ATTR_PICKED = 0x1F
ATTR_HINT = 0x08


main:
    push rbx
    call collect_args
    call load_names
    test eax, eax
    jz .failed

    cmp dword [arg_count], 0
    je .picker
    mov rdi, [arg_list]
    call find_theme
    jc .unknown
    mov esi, eax
    mov edi, THEME_SET
    call sys_theme
    mov eax, [chosen]
    call save_theme
    lea rdi, [applied_text]
    call print
    mov eax, [chosen]
    imul eax, THEME_NAME_MAX
    lea rdi, [names + rax]
    call print_line
    xor eax, eax
    pop rbx
    ret

.unknown:
    lea rdi, [unknown_text]
    call print
    mov rdi, [arg_list]
    call print_line
    call list_names
    mov eax, 1
    pop rbx
    ret

.picker:
    mov edi, THEME_CURRENT
    call sys_theme
    mov [original], eax
    mov [selected], eax

    lea rdi, [window_title]
    mov esi, PICKER_WIDTH
    mov edx, PICKER_HEIGHT
    call screen_open
    mov byte [running], 1

.loop:
    call render
    cmp byte [running], 0
    je .exit
    xor esi, esi
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .cancel
    jmp .loop
.key:
    mov edi, [screen_key]
    call handle_key
    jmp .loop
.resize:
    call render
    call screen_acknowledge_resize
    jmp .loop
.cancel:
    call revert

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

.failed:
    mov eax, 1
    pop rbx
    ret

load_names:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, THEMES_MAX
    jae .done
    mov edi, THEME_NAME
    mov esi, ebx
    imul edx, ebx, THEME_NAME_MAX
    lea rdx, [names + rdx]
    call sys_theme
    test eax, eax
    jnz .done
    inc ebx
    jmp .next
.done:
    mov [theme_count], ebx
    mov eax, ebx
    pop rbx
    ret

; rdi -> name. out: eax = index, CF if no theme has that name
find_theme:
    push rbx
    push r12
    mov r12, rdi
    xor ebx, ebx
.next:
    cmp ebx, [theme_count]
    jae .missing
    imul eax, ebx, THEME_NAME_MAX
    lea rdi, [names + rax]
    mov rsi, r12
    call str_equal
    test eax, eax
    jnz .found
    inc ebx
    jmp .next
.found:
    mov [chosen], ebx
    mov eax, ebx
    pop r12
    pop rbx
    clc
    ret
.missing:
    pop r12
    pop rbx
    stc
    ret

list_names:
    push rbx
    lea rdi, [available_text]
    call print_line
    xor ebx, ebx
.next:
    cmp ebx, [theme_count]
    jae .done
    lea rdi, [indent_text]
    call print
    imul eax, ebx, THEME_NAME_MAX
    lea rdi, [names + rax]
    call print_line
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

handle_key:
    cmp edi, KEY_UP
    je .up
    cmp edi, 'k'
    je .up
    cmp edi, KEY_DOWN
    je .down
    cmp edi, 'j'
    je .down
    cmp edi, 10
    je .keep
    cmp edi, 13
    je .keep
    cmp edi, KEY_ESCAPE
    je .cancel
    cmp edi, 'q'
    je .cancel
    ret
.up:
    mov eax, [selected]
    test eax, eax
    jnz .previous
    mov eax, [theme_count]
.previous:
    dec eax
    jmp preview
.down:
    mov eax, [selected]
    inc eax
    cmp eax, [theme_count]
    jb preview
    xor eax, eax
    jmp preview
.keep:
    mov eax, [selected]
    call save_theme
    mov byte [running], 0
    ret
.cancel:
    call revert
    mov byte [running], 0
    ret

preview:
    mov [selected], eax
    mov edi, THEME_SET
    mov esi, eax
    jmp sys_theme

; eax = index, written to /etc/theme so the kernel picks it up at boot
save_theme:
    push rbx
    imul ebx, eax, THEME_NAME_MAX
    lea rdi, [theme_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .done
    push rax
    lea rdi, [names + rbx]
    call str_len
    mov edx, eax
    pop rdi
    push rdi
    lea rsi, [names + rbx]
    call sys_write
    mov rdi, [rsp]
    lea rsi, [newline_byte]
    mov edx, 1
    call sys_write
    pop rdi
    call sys_close
.done:
    pop rbx
    ret

revert:
    mov edi, THEME_SET
    mov esi, [original]
    jmp sys_theme

render:
    push rbx
    call layout_picker
    call screen_clear

    mov edi, 2
    xor esi, esi
    lea rdx, [heading_text]
    mov cl, ATTR_TITLE
    call put_text

    xor ebx, ebx
.row:
    cmp ebx, [theme_count]
    jae .swatch
    cmp ebx, [selected]
    jne .plain
    mov edi, 1
    lea esi, [rbx + LIST_TOP]
    mov edx, PREVIEW_COLUMN - 3
    mov cl, ATTR_PICKED
    call put_bar
    mov edi, 2
    lea esi, [rbx + LIST_TOP]
    lea rdx, [marker_text]
    mov cl, ATTR_PICKED
    call put_text
    mov cl, ATTR_PICKED
    jmp .name
.plain:
    mov cl, ATTR_TEXT
.name:
    mov edi, 4
    lea esi, [rbx + LIST_TOP]
    imul eax, ebx, THEME_NAME_MAX
    lea rdx, [names + rax]
    call put_text
    inc ebx
    jmp .row

.swatch:
    xor ebx, ebx
.color:
    cmp ebx, 16
    jae .hints
    lea edi, [rbx * 2 + 1]
    mov esi, [swatch_row]
    mov dl, 0xDB
    mov cl, bl
    call screen_put
    lea edi, [rbx * 2 + 2]
    mov esi, [swatch_row]
    mov dl, 0xDB
    mov cl, bl
    call screen_put
    inc ebx
    jmp .color

.hints:
    call draw_preview
    mov edi, 2
    mov esi, [hint_row]
    lea rdx, [hint_text]
    mov cl, ATTR_HINT
    call put_text

    call screen_present
    pop rbx
    ret

layout_picker:
    cmp dword [screen_columns], PICKER_COLUMNS
    jb .narrow
    mov dword [preview_column], PREVIEW_COLUMN
    mov dword [preview_row], LIST_TOP
    jmp .rows
.narrow:
    mov dword [preview_column], 1
    mov eax, [theme_count]
    add eax, LIST_TOP + 1
    mov [preview_row], eax
.rows:
    mov eax, [preview_row]
    add eax, PREVIEW_ROWS + 1
    cmp eax, LIST_TOP + THEMES_SHOWN + 1
    jae .swatch
    mov eax, LIST_TOP + THEMES_SHOWN + 1
.swatch:
    mov [swatch_row], eax
    add eax, 2
    mov [hint_row], eax
    ret

draw_preview:
    push rbx
    mov edi, [preview_column]
    mov esi, [preview_row]
    mov edx, PREVIEW_WIDTH
    mov cl, ATTR_PICKED
    call put_bar
    lea rbx, [preview_items]
.next:
    movzx esi, byte [rbx]
    cmp esi, 0xFF
    je .done
    add esi, [preview_row]
    movzx edi, byte [rbx + 1]
    add edi, [preview_column]
    mov cl, [rbx + 2]
    lea rdx, [rbx + 3]
    call put_text
    add rbx, 3
.skip:
    cmp byte [rbx], 0
    je .ended
    inc rbx
    jmp .skip
.ended:
    inc rbx
    jmp .next
.done:
    pop rbx
    ret

put_text:
    push rbx
    push r12
    push r13
    mov rbx, rdx
    mov r12d, edi
    mov r13d, esi
.next:
    mov dl, [rbx]
    test dl, dl
    jz .done
    mov edi, r12d
    mov esi, r13d
    push rcx
    call screen_put
    pop rcx
    inc r12d
    inc rbx
    jmp .next
.done:
    pop r13
    pop r12
    pop rbx
    ret

put_bar:
    push rbx
    push r12
    mov r12d, edi
    mov ebx, edx
.next:
    test ebx, ebx
    jz .done
    mov edi, r12d
    mov dl, ' '
    push rcx
    push rsi
    call screen_put
    pop rsi
    pop rcx
    inc r12d
    dec ebx
    jmp .next
.done:
    pop r12
    pop rbx
    ret

program_name db 'theme', 0
window_title db 'Theme', 0
newline_byte db 10
theme_path db '/etc/theme', 0

preview_items:
    db 0, 1, ATTR_PICKED, 'Terminal', 0
    db 2, 0, 0x0B, 'guest@serpentis', 0
    db 2, 15, 0x07, ':', 0
    db 2, 16, 0x0E, '/src', 0
    db 2, 20, 0x0F, '$ ls', 0
    db 3, 0, 0x09, 'bin  lib  src', 0
    db 3, 15, 0x0F, 'notes.txt', 0
    db 5, 0, 0x09, 'main:', 0
    db 6, 4, 0x0D, 'mov', 0
    db 6, 8, 0x0B, 'eax', 0
    db 6, 11, 0x07, ', ', 0
    db 6, 13, 0x06, '42', 0
    db 7, 4, 0x0D, 'jmp', 0
    db 7, 8, 0x09, '.loop', 0
    db 7, 15, 0x08, '; again', 0
    db 8, 0, 0x0F, 'text', 0
    db 8, 5, 0x0E, 'db', 0
    db 8, 8, 0x0A, "'hello'", 0
    db 0xFF
heading_text db 'Choose a theme', 0
marker_text db '>', 0
hint_text db 'Up Down choose    Enter keep    Esc cancel', 0
applied_text db 'Theme: ', 0
unknown_text db 'theme: no theme named ', 0
available_text db 'Available themes:', 0
indent_text db '  ', 0

include 'common.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
include '../runtime/screen_bss.inc'

align 8
theme_count rd 1
selected rd 1
original rd 1
chosen rd 1
preview_column rd 1
preview_row rd 1
swatch_row rd 1
hint_row rd 1
running rb 1
names rb THEMES_MAX * THEME_NAME_MAX
