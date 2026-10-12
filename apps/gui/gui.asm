; gui, the window manager. a normal program that registers itself as the gui service. apps
; draw into their own slice of a shared memory block and send messages, we compose all of it
; into one 640x480 frame and blit it. tiles windows like dwm, floats and pins on top

SERP_STACK = 0x4000

include '../runtime/program.inc'

SCREEN_W = 640
SCREEN_H = 480
SCREEN_BYTES = SCREEN_W * SCREEN_H
BAR_H = 20
GAP = 6
WORK_X = GAP
WORK_Y = BAR_H + GAP
WORK_W = SCREEN_W - GAP * 2
WORK_H = SCREEN_H - BAR_H - GAP * 2
TITLE_H = 18
CURSOR_W = 8
CURSOR_H = 12

COLOR_BLACK = 0
COLOR_BLUE = 1
COLOR_RED = 4
COLOR_LIGHT_GRAY = 7
COLOR_DARK_GRAY = 8
COLOR_LIGHT_CYAN = 11
COLOR_YELLOW = 14
COLOR_WHITE = 15

; one record per window, WIN_SIZE bytes each. X Y TILE_W TILE_H is where the window should
; be, DRAW_* is where it is drawn right now (animated towards the first), W H is the size of
; the surface the app draws into
WIN_USED = 0
WIN_OWNER = 4
WIN_X = 8
WIN_Y = 12
WIN_W = 16
WIN_H = 20
WIN_TITLE = 24
WIN_TILE_W = 64
WIN_TILE_H = 68
WIN_SENT_W = 72                 ; last size we told the app, so GUI_RESIZE only goes out on a change
WIN_SENT_H = 76
WIN_DRAW_X = 80
WIN_DRAW_Y = 84
WIN_DRAW_W = 88
WIN_DRAW_H = 92
WIN_FRESH = 96
WIN_CLOSING = 100
WIN_WORKSPACE = 104
WIN_PINNED = 108
WIN_FLOAT = 112
WIN_FROM_X = 116
WIN_FROM_Y = 120
WIN_FROM_W = 124
WIN_FROM_H = 128
WIN_GOAL_X = 132
WIN_GOAL_Y = 136
WIN_GOAL_W = 140
WIN_GOAL_H = 144
WIN_PHASE = 148
WIN_FULL = 152
WIN_SAVE_X = 156
WIN_SAVE_Y = 160
WIN_SAVE_W = 164
WIN_SAVE_H = 168
WIN_SAVE_FLOAT = 172
WIN_SIZE = 176

WORKSPACE_MAX = 4
LAYOUT_MASTER = 0
LAYOUT_COLUMNS = 1
LAYOUT_ROWS = 2
LAYOUT_COUNT = 3
PIN_W = 260
PIN_H = 190
PIN_X = SCREEN_W - GAP - PIN_W
PIN_Y = SCREEN_H - GAP - PIN_H
PIN_CASCADE = 26
PIN_STACK = 6
FLOAT_W = 360
FLOAT_H = 260
FLOAT_MAX_W = SCREEN_W * 3 / 4
FLOAT_MAX_H = SCREEN_H * 3 / 4
MIN_W = 120
MIN_H = 72
RESIZE_EDGE = 12
GRID_FROM = 6
KEY_PIN = 15
KEY_LAYOUT = 12
KEY_FULLSCREEN = 6
CLOCK_INTERVAL = 20
TOAST_TICKS = 250
CLOCK_CHARS = 8
CLOCK_X = SCREEN_W - 7 - CLOCK_CHARS * 8
KEY_ESCAPE = 27
APP_COUNT = 9
LAUNCHER_W = 200
LAUNCHER_ROW = 18
SEARCH_MAX = 16
SEARCH_ROW = 22
LAUNCHER_H = TITLE_H + SEARCH_ROW + APP_COUNT * LAUNCHER_ROW + 8
LAUNCHER_X = (SCREEN_W - LAUNCHER_W) / 2
LAUNCHER_Y = (SCREEN_H - LAUNCHER_H) / 2
ANIMATION_CATCH_UP = 6
WALLPAPER_H = SCREEN_H - BAR_H
WALLPAPER_BYTES = SCREEN_W * WALLPAPER_H
WALLPAPER_HEADER = 8
WALLPAPER_COLORS = 256
WALLPAPER_MAGIC = 0x4C415057
SLIDE_KEY = 255
SLIDE_FRAMES = 20
MOVE_FRAMES = 18
COMPOSE_FULL = 0
COMPOSE_LAYER = 1               ; only the windows, on SLIDE_KEY
COMPOSE_BACKDROP = 2
SHADE_KEEP = 5
SHADOW = 3

KEY_EXIT = KEY_F1 + 9
CLIENT_EXIT_TICKS = 100

main:
    push rbx
    push r12
    push r13

    mov edi, GUI_SERVICE_QUERY
    call sys_service
    test eax, eax
    jns .already_running

    mov edi, GUI_SERVICE_REGISTER
    call sys_service
    call sys_shm_attach
    mov edi, GUI_SERVICE_FONT
    mov rsi, SHM_BASE + SHM_FONT_OFFSET
    call sys_service
    mov qword [draw_font], SHM_BASE + SHM_FONT_OFFSET

    mov dword [focus], -1
    mov dword [dragging], -1
    mov dword [resizing], -1
    mov dword [mouse_capture], -1
    mov dword [cursor_x], SCREEN_W / 2
    mov dword [cursor_y], SCREEN_H / 2
    mov byte [scene_dirty], 1
    call load_wallpaper
    call build_shade

.loop:
    call render
    cmp byte [quitting], 0
    jne .exit

    lea rdi, [event]
    mov esi, EVENT_WANT_TICK
    call sys_event
    call handle_event

.drain:
    lea rdi, [event]
    mov esi, SYS_GETKEY_NOWAIT
    call sys_event
    test eax, eax
    jz .loop
    call handle_event
    jmp .drain

.exit:
    call close_all_windows
    call wait_for_clients
    xor eax, eax
    jmp .out

.already_running:
    lea rdi, [already_text]
    call print_line
    mov eax, 1

.out:
    pop r13
    pop r12
    pop rbx
    ret

handle_event:
    cmp eax, EVENT_TICK
    je animate_tick
    cmp eax, EVENT_MESSAGE
    je handle_message
    cmp eax, EVENT_KEY
    je handle_key
    cmp eax, EVENT_MOUSE
    je handle_mouse
    ret

include 'windows.inc'
include 'layout.inc'
include 'animate.inc'
include 'wallpaper.inc'
include 'session.inc'
include 'input.inc'
include 'render.inc'
include 'compose.inc'

wallpaper_path db '/share/wallpaper.raw', 0
already_text db 'gui: already running', 0
calculator_path db '/bin/calc.serp', 0
terminal_path db '/bin/lds.serp', 0
demo_path db '/bin/demo.serp', 0
paint_path db '/bin/paint.serp', 0
files_path db '/bin/files.serp', 0
editor_path db '/bin/edit.serp', 0
clock_path db '/bin/clock.serp', 0
graphcalc_path db '/bin/graphcalc.serp', 0
theme_path db '/bin/theme.serp', 0

align 8
app_programs dq demo_path, paint_path, terminal_path, calculator_path, files_path, editor_path, clock_path, graphcalc_path, theme_path
app_names dq demo_name, paint_name, terminal_name, calculator_name, files_name, editor_name, clock_name, graphcalc_name, themes_name

demo_name db 'Demo', 0
paint_name db 'Paint', 0
terminal_name db 'Terminal', 0
calculator_name db 'Calculator', 0
files_name db 'Files', 0
editor_name db 'Editor', 0
clock_name db 'Clock', 0
graphcalc_name db 'GraphCalc', 0
themes_name db 'Themes', 0
launcher_title db 'Launch', 0
no_match_text db 'No match', 0
layout_symbols db '[]=', 0, '|||', 0, '===', 0


include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/library.inc'

align 8
windows:
    rb WIN_SIZE * WINDOW_MAX
zorder:
    rd WINDOW_MAX
z_count rd 1
current_workspace rd 1
focus rd 1
dragging rd 1
grab_x rd 1
grab_y rd 1
anchor_x rd 1
anchor_y rd 1
resize_left rd 1
resize_top rd 1
resizing rd 1
float_count rd 1
float_stack rd WINDOW_MAX
slide_from rd 1
slide_frame rd 1
slide_pos rd 1
slide_direction rd 1
best_distance rd 1
compose_mode rb 1
slide_active rb 1
slide_dirty rb 1
cursor_x rd 1
cursor_y rd 1
drawn_x rd 1
drawn_y rd 1
previous_buttons rd 1
mouse_capture rd 1
scene_dirty rb 1
last_tick rd 1
toast_until rd 1
damaged_windows rd 1
quitting rb 1
workspace_digit rb 2
layout_modes rb WORKSPACE_MAX
launcher_open rb 1
search_text rb SEARCH_MAX
search_line rb SEARCH_MAX + 4
match_list rb APP_COUNT
align 4
launcher_index rd 1
search_length rd 1
match_count rd 1
clock_text rb CLOCK_CHARS + 1
clock_shown rb CLOCK_CHARS + 1
clock_dirty rb 1
clock_time rb TIME_BYTES
align 4
clock_ticks rd 1
align 8
event:
    rb EVENT_BYTES
outgoing:
    rb MSG_BYTES
scene:
    rb SCREEN_BYTES
frame:
    rb SCREEN_BYTES
animating:
    rb 1
wallpaper_ready:
    rb 1
align 8
wallpaper:
    rb WALLPAPER_BYTES
wallpaper_header:
    rb WALLPAPER_HEADER
wallpaper_palette:
    rb WALLPAPER_COLORS * 3
shade:
    rb 256
pinned_row:
    rb SCREEN_W
align 4
column_map:
    rd SCREEN_W
slide_old:
    rb SCREEN_BYTES
slide_new:
    rb SCREEN_BYTES
