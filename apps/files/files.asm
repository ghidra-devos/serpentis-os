SERP_STACK = 0x4000

include '../runtime/program.inc'

START_W = 440
START_H = 300
TOOLBAR_H = 22
STATUS_H = 18
SIDEBAR_W = 104
ROW_H = 18
ENTRY_MAX = 256
ENTRY_BYTES = 72
ENTRY_FLAGS = 0
ENTRY_SIZE = 4
ENTRY_NAME = 8
ENTRY_NAME_MAX = ENTRY_BYTES - ENTRY_NAME - 1
PATH_BYTES = 256
DOUBLE_CLICK_TICKS = 40

COLOR_BLACK = 0
COLOR_BLUE = 1
COLOR_LIGHT_GRAY = 7
COLOR_DARK_GRAY = 8
COLOR_LIGHT_GREEN = 10
COLOR_LIGHT_CYAN = 11
COLOR_YELLOW = 14
COLOR_WHITE = 15

main:
    push rbx
    call gui_connect
    test eax, eax
    js .no_gui
    mov edi, START_W
    mov esi, START_H
    lea rdx, [title]
    call gui_open_window
    test eax, eax
    js .no_gui
    mov [window], eax
    mov [surface], rdx
    mov edi, START_W
    mov esi, START_H
    call set_size

    call sys_getuid
    mov edi, eax
    call user_home
    lea rdi, [home_path]
    mov rsi, rax
    call str_copy
    lea rax, [home_path]
    mov [place_paths + 8], rax

    lea rdi, [current]
    mov esi, PATH_BYTES
    call sys_getcwd
    call load_directory
    call render

.wait:
    lea rdi, [gui_event]
    xor esi, esi
    call sys_event
    cmp eax, EVENT_MESSAGE
    jne .wait
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_OP]
    cmp eax, GUI_CLOSE
    je .close
    cmp eax, GUI_KEY
    je .key
    cmp eax, GUI_MOUSE
    je .mouse
    cmp eax, GUI_RESIZE
    jne .wait
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    call set_size
    call render
    mov edi, [window]
    mov esi, [width]
    mov edx, [height]
    call gui_ack_resize
    jmp .wait

.key:
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    call handle_key
    cmp byte [running], 0
    je .close
    call render
    jmp .wait

; button 1 going down only, holding it sends more messages
.mouse:
    mov eax, [gui_event + EVENT_PAYLOAD + MSG_C]
    mov ecx, [last_buttons]
    mov [last_buttons], eax
    test eax, 1
    jz .wait
    test ecx, 1
    jnz .wait
    mov edi, [gui_event + EVENT_PAYLOAD + MSG_A]
    mov esi, [gui_event + EVENT_PAYLOAD + MSG_B]
    call handle_click
    call render
    jmp .wait

.close:
    mov edi, [window]
    call gui_close_window
    xor eax, eax
    pop rbx
    ret

.no_gui:
    lea rdi, [no_gui_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

set_size:
    mov [width], edi
    mov [height], esi
    mov rdi, [surface]
    mov esi, [width]
    mov edx, esi
    mov ecx, [height]
    jmp draw_set_target

visible_rows:
    mov eax, [height]
    sub eax, TOOLBAR_H + STATUS_H + 4
    xor edx, edx
    mov ecx, ROW_H
    div ecx
    test eax, eax
    jnz .done
    inc eax
.done:
    ret

entry_address:
    imul eax, eax, ENTRY_BYTES
    lea rax, [entries + rax]
    ret

include 'browse.inc'
include 'input.inc'
include 'view.inc'

title db 'Files', 0
no_gui_text db 'files: start the GUI first', 0
editor_path db '/bin/edit.serp', 0
items_text db ' items', 0
bytes_suffix db ' B', 0
kilobytes_suffix db ' KB', 0
opening_text db 'Opening in the editor', 0
running_text db 'Program started', 0
deleted_text db 'Deleted', 0
delete_failed_text db 'Could not delete (folders must be empty)', 0
confirm_text db 'Delete the selected item? (y/n)', 0

place_root_name db 'Computer', 0
place_home_name db 'Home', 0
place_bin_name db 'Programs', 0
place_source_name db 'Source', 0
place_lib_name db 'Library', 0
place_tmp_name db 'Temp', 0
place_root db '/', 0
place_bin db '/bin', 0
place_source db '/usr/src/serpentis', 0
place_lib db '/lib', 0
place_tmp db '/tmp', 0
PLACE_COUNT = 6

align 8
place_names dq place_root_name, place_home_name, place_bin_name, place_source_name, place_lib_name, place_tmp_name
place_paths dq place_root, 0, place_bin, place_source, place_lib, place_tmp
running db 1

include '../runtime/gui.inc'
include '../runtime/draw.inc'
include '../runtime/users.inc'
include '../runtime/library.inc'
include '../runtime/users_bss.inc'

align 8
surface rq 1
message rq 1
click_tick rq 1
window rd 1
width rd 1
height rd 1
last_buttons rd 1
entry_count rd 1
selected rd 1
top rd 1
confirming rb 1
current:
    rb PATH_BYTES
home_path:
    rb PATH_BYTES
target:
    rb PATH_BYTES
status_text rb 64
size_text rb 32
dirent:
    rb SYS_DIRENT_BYTES
swap:
    rb ENTRY_BYTES
entries:
    rb ENTRY_MAX * ENTRY_BYTES
