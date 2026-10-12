; edit, a vim-ish editor. file tree on the left, tabs on top, status and command line at the
; bottom. the text is one flat gap-less buffer, every edit moves the tail

SERP_STACK = 0x8000

include '../runtime/program.inc'

WINDOW_WIDTH = 78 * 8 + SCREEN_PAD_X * 2
WINDOW_HEIGHT = 26 * 16 + SCREEN_PAD_Y * 2

main:
    push rbx
    push rdi
    mov edi, HEAP_BYTES
    call sys_alloc
    cmp rax, text               ; text and the undo stacks sit at fixed addresses from HEAP_BASE on
    jne .no_memory
    call buffers_init
    pop rdi

    call arg_next
    test rax, rax
    jz .browse
    mov rsi, rax
    call open_path
    jmp .open_screen

.browse:
    call panel_toggle

.open_screen:
    call file_name
    mov rdi, rax
    mov esi, WINDOW_WIDTH
    mov edx, WINDOW_HEIGHT
    call screen_open
    call invalidate_screen
    mov byte [running], 1

.loop:
    call render
    cmp byte [running], 0
    je .exit
    xor esi, esi
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_CLICK
    je .click
    cmp eax, SCREEN_WHEEL
    je .wheel
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .exit
    jmp .loop

.key:
    mov edi, [screen_key]
    call handle_key
    jmp .loop

.click:
    call place_cursor_at_click
    jmp .loop

.wheel:
    imul edi, [screen_wheel], SCROLL_STEP
    call scroll_lines
    jmp .loop

.resize:
    call invalidate_screen
    call render
    call screen_acknowledge_resize
    jmp .loop

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

.no_memory:
    pop rdi
    lea rdi, [no_memory_text]
    call print_line
    mov eax, 1
    pop rbx
    ret

place_cursor_at_click:
    call scrollbar_click
    jc .done
    mov eax, [screen_click_row]
    call text_rows
    cmp [screen_click_row], eax
    ja .status
    cmp dword [screen_click_row], 0
    je tab_click
    mov eax, [screen_click_column]
    cmp eax, [panel_width]
    jb panel_click
    cmp byte [mode], MODE_FILES
    jne .editor
    mov byte [mode], MODE_NORMAL
.editor:
    cmp byte [mode], MODE_NORMAL
    jne .done
    mov ecx, [screen_click_row]
    dec ecx
    add ecx, [top_line]
    push rcx
    call line_count
    pop rdi
    cmp edi, eax
    jae .done
    call line_offset
    mov edi, eax
    mov esi, [screen_click_column]
    sub esi, [gutter]
    jns .column
    xor esi, esi
.column:
    add esi, [left_column]
    call place_on_line
    mov [cursor], eax
    call clamp_normal_cursor
    mov edi, [cursor]
    call column_of
    mov [want_column], eax
.done:
    ret
.status:
    inc eax
    cmp [screen_click_row], eax
    jne .done
    cmp dword [screen_click_column], BUTTON_COLUMNS
    jb panel_toggle
    ret

align 8
undo_stack_descriptor dq undo_buffer, 0, 0
redo_stack_descriptor dq redo_buffer, 0, 0
undo_used = undo_stack_descriptor + 16
text = HEAP_BASE
undo_buffer = text + TEXT_MAX
redo_buffer = undo_buffer + UNDO_MAX
pool = redo_buffer + UNDO_MAX
HEAP_BYTES = TEXT_MAX + UNDO_MAX * 2 + POOL_MAX
redo_used = redo_stack_descriptor + 16
file_kind_name dq plain_kind
show_numbers db 1
no_memory_text db 'edit: not enough memory', 0

include 'text.inc'
include 'view.inc'
include 'syntax.inc'
include 'c_syntax.inc'
include 'vim.inc'
include 'command.inc'
include 'tree.inc'
include 'buffers.inc'
include 'finder.inc'
include 'scroll.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'

align 8
message rq 1
prompt_label rq 1
text_size rd 1
cursor rd 1
want_column rd 1
top_line rd 1
left_column rd 1
cursor_line rd 1
scroll_rows rd 1
cursor_column rd 1
gutter rd 1
panel_target rd 1
finder_count rd 1
finder_length rd 1
finder_selected rd 1
finder_top rd 1
finder_match_count rd 1
walk_relative rd 1
current_buffer rd 1
tab_first rd 1
pool_used rd 1
tab_hit_count rd 1
panel_width rd 1
panel_selected rd 1
panel_top rd 1
node_count rd 1
path_chain rd DEPTH_MAX + 1
count rd 1
anchor rd 1
motion_target rd 1
range_from rd 1
range_to rd 1
key rd 1
command_length rd 1
search_length rd 1
replacement_length rd 1
selection_from rd 1
selection_to rd 1
bracket_match rd 1
token_end rd 1
c_block_start rd 1
token_class rd 1
syntax_state rd 1
line_first_word rd 1
vertical_direction rd 1
undo_group rd 1
yank_length rd 1
mode rb 1
operator rb 1
pending rb 1
find_kind rb 1
find_char rb 1
object_around rb 1
replace_char rb 1
motion_linewise rb 1
motion_inclusive rb 1
motion_vertical rb 1
range_linewise rb 1
yank_linewise rb 1
paste_before rb 1
indent_direction rb 1
dirty rb 1
running rb 1
finder_return rb 1
line_leading rb 1
tab_dirty rb 1
tab_attribute rb 1
panel_open rb 1
search_direction rb 1
search_highlight rb 1
message_error rb 1
prompt_kind rb 1
prompt_selected rb 1
prompt_return rb 1
relative_numbers rb 1
syntax_enabled rb 1
string_quote rb 1
c_block_open rb 1
c_include_line rb 1
c_escaped rb 1
frame_changed rb 1
substitute_global rb 1
substitute_all_lines rb 1
align 8
scratch rb 256
command_line rb COMMAND_MAX + 2
search_pattern rb PATTERN_MAX
replacement rb PATTERN_MAX
file_path rb 256
file_info rb 320
status_right rb 64
cells:
    rb CELL_BYTES
shadow:
    rb CELL_BYTES
yank_buffer rb YANK_MAX
tree_root rb PATH_BYTES
path_buffer rb PATH_BYTES
dirent rb SYS_DIRENT_BYTES
node_swap rb NODE_BYTES
nodes rb NODE_MAX * NODE_BYTES
children rb CHILD_MAX * NODE_BYTES
buffers rb BUFFER_MAX * BUFFER_BYTES
tab_hits:
    rd BUFFER_MAX * TAB_HIT_BYTES / 4
prompt_text rb 128
rename_source rb PATH_BYTES
walk_path rb PATH_BYTES
finder_query rb FINDER_QUERY_MAX + 2
align 4
finder_matches rd FINDER_MAX
finder_paths rb FINDER_MAX * FINDER_PATH
absolute_path rb PATH_BYTES
title_shown rb PATH_BYTES
