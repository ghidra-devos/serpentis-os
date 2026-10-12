; graphcalc. four formulas in x, plotted together. trace, newton's method step by step,
; roots and intersections, the derivative curve, zoom and pan

SERP_STACK = 0x8000

include '../runtime/program.inc'

WINDOW_W = 632
WINDOW_H = 420
FUNCTION_MAX = 4                ; power of two, tab wraps with an and
FORMULA_MAX = 64
PANEL_COLUMNS = 23
NEWTON_MAX = 12
NEWTON_DELAY = 40
MARK_MAX = 12
BISECT_STEPS = 60

KEY_BACKSPACE = 8
KEY_TAB = 9
KEY_ENTER = 10
KEY_RETURN = 13
KEY_ESCAPE = 27

MARKS_NONE = 0
MARKS_ROOTS = 1
MARKS_CROSSINGS = 2

main:
    push rbx
    lea rdi, [window_title]
    mov esi, WINDOW_W
    mov edx, WINDOW_H
    call screen_open
    call reset_view
    xor ebx, ebx
.defaults:
    mov rsi, [default_formulas + rbx * 8]
    mov edi, ebx
    call set_formula
    inc ebx
    cmp ebx, FUNCTION_MAX
    jb .defaults
    mov byte [running], 1

.loop:
    call redraw
; ticks only while the newton animation still has steps to show
.wait:
    cmp byte [running], 0
    je .exit
    xor esi, esi
    cmp dword [newton_shown], 0
    je .idle
    mov eax, [newton_shown]
    cmp eax, [newton_count]
    jae .idle
    mov esi, EVENT_WANT_TICK
.idle:
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_CLICK
    je .click
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .exit
    call animate
    test eax, eax
    jnz .loop
    jmp .wait

.key:
    mov eax, [screen_key]
    call handle_key
    jmp .loop

.click:
    call handle_click
    jmp .loop

.resize:
    call redraw
    call screen_acknowledge_resize
    jmp .wait

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret

reset_view:
    xorpd xmm0, xmm0
    movsd [view_x], xmm0
    movsd [view_y], xmm0
    movsd xmm0, [default_scale]
    movsd [view_scale], xmm0
    ret

set_formula:
    push rbx
    push r12
    mov ebx, edi
    mov r12, rsi
    imul eax, ebx, FORMULA_MAX
    lea rdi, [formula_text + rax]
    call str_copy
    mov byte [function_ok + rbx], 0
    mov qword [function_error + rbx * 8], 0
    imul eax, ebx, FORMULA_MAX
    cmp byte [formula_text + rax], 0
    je .done
    lea rsi, [formula_text + rax]
    imul eax, ebx, PROGRAM_MAX
    lea rdi, [program_op_area + rax]
    imul eax, ebx, PROGRAM_MAX * 8
    lea rdx, [program_value_area + rax]
    call compile_function
    mov [function_error + rbx * 8], rax
    test rax, rax
    jnz .done
    mov byte [function_ok + rbx], 1
.done:
    pop r12
    pop rbx
    ret

animate:
    xor eax, eax
    mov ecx, [newton_shown]
    test ecx, ecx
    jz .done
    cmp ecx, [newton_count]
    jae .done
    call sys_getticks
    mov rcx, rax
    sub rcx, [newton_clock]
    cmp rcx, NEWTON_DELAY
    jb .still
    mov [newton_clock], rax
    inc dword [newton_shown]
    mov eax, 1
    ret
.still:
    xor eax, eax
.done:
    ret

handle_key:
    cmp byte [editing], 0
    jne edit_key
    cmp eax, 'q'
    je .quit
    cmp eax, KEY_ESCAPE
    je .escape
    cmp eax, KEY_ENTER
    je begin_edit
    cmp eax, KEY_RETURN
    je begin_edit
    cmp eax, KEY_TAB
    je .next_slot
    cmp eax, '1'
    jb .other
    cmp eax, '0' + FUNCTION_MAX
    ja .other
    sub eax, '1'
    mov [selected], eax
    jmp clear_newton
.other:
    cmp eax, KEY_DELETE
    je .delete
    cmp eax, KEY_LEFT
    je .left
    cmp eax, KEY_RIGHT
    je .right
    cmp eax, KEY_UP
    je .up
    cmp eax, KEY_DOWN
    je .down
    cmp eax, '+'
    je zoom_in
    cmp eax, '='
    je zoom_in
    cmp eax, KEY_PAGE_UP
    je zoom_in
    cmp eax, '-'
    je zoom_out
    cmp eax, KEY_PAGE_DOWN
    je zoom_out
    cmp eax, '0'
    je reset_view
    cmp eax, KEY_HOME
    je reset_view
    cmp eax, 't'
    je toggle_trace
    cmp eax, 'n'
    je start_newton
    cmp eax, 'r'
    je .roots
    cmp eax, 'i'
    je .crossings
    cmp eax, 'd'
    je .derivative
    cmp eax, 'c'
    je center_trace
    ret
.quit:
    mov byte [running], 0
    ret
.escape:
    cmp dword [newton_count], 0
    jne clear_newton
    cmp byte [marks_mode], MARKS_NONE
    je .quit
    mov byte [marks_mode], MARKS_NONE
    ret
.next_slot:
    mov eax, [selected]
    inc eax
    and eax, FUNCTION_MAX - 1
    mov [selected], eax
    jmp clear_newton
.delete:
    mov edi, [selected]
    lea rsi, [empty_text]
    call set_formula
    jmp clear_newton
; arrows pan by an eighth of the plot, with trace on left/right move the trace instead
.left:
    mov eax, -1
    jmp .horizontal
.right:
    mov eax, 1
.horizontal:
    cmp byte [trace_on], 0
    jne .move_trace
    imul eax, [plot_width]
    sar eax, 3
    cvtsi2sd xmm0, eax
    mulsd xmm0, [view_scale]
    addsd xmm0, [view_x]
    movsd [view_x], xmm0
    ret
.move_trace:
    shl eax, 2
    cvtsi2sd xmm0, eax
    mulsd xmm0, [view_scale]
    addsd xmm0, [trace_x]
    movsd [trace_x], xmm0
    call x_column
    cmp eax, 0
    jl .follow
    cmp eax, [plot_width]
    jl .traced
.follow:
    movsd xmm0, [trace_x]
    movsd [view_x], xmm0
.traced:
    ret
.up:
    mov eax, 1
    jmp .vertical
.down:
    mov eax, -1
.vertical:
    imul eax, [plot_height]
    sar eax, 3
    cvtsi2sd xmm0, eax
    mulsd xmm0, [view_scale]
    addsd xmm0, [view_y]
    movsd [view_y], xmm0
    ret
.roots:
    mov al, MARKS_ROOTS
    jmp .marks
.crossings:
    mov al, MARKS_CROSSINGS
.marks:
    cmp [marks_mode], al
    jne .set_marks
    xor eax, eax
.set_marks:
    mov [marks_mode], al
    ret
.derivative:
    xor byte [derivative_on], 1
    ret

; limits so the scale stays a sane double
zoom_in:
    movsd xmm0, [view_scale]
    mulsd xmm0, [zoom_factor]
    ucomisd xmm0, [scale_min]
    jb .done
    movsd [view_scale], xmm0
.done:
    ret

zoom_out:
    movsd xmm0, [view_scale]
    divsd xmm0, [zoom_factor]
    ucomisd xmm0, [scale_max]
    ja .done
    movsd [view_scale], xmm0
.done:
    ret

toggle_trace:
    xor byte [trace_on], 1
    jz clear_newton
    movsd xmm0, [view_x]
    movsd [trace_x], xmm0
    ret

center_trace:
    cmp byte [trace_on], 0
    je .done
    movsd xmm0, [trace_x]
    movsd [view_x], xmm0
    movsd xmm0, [trace_y]
    call is_finite
    jnc .done
    movsd [view_y], xmm0
.done:
    ret

clear_newton:
    mov dword [newton_count], 0
    mov dword [newton_shown], 0
    ret

begin_edit:
    mov eax, [selected]
    imul eax, FORMULA_MAX
    lea rsi, [formula_text + rax]
    lea rdi, [edit_buffer]
    call str_copy
    lea rdi, [edit_buffer]
    call str_len
    mov [edit_length], eax
    mov byte [editing], 1
    ret

edit_key:
    cmp eax, KEY_ESCAPE
    je .cancel
    cmp eax, KEY_ENTER
    je .commit
    cmp eax, KEY_RETURN
    je .commit
    cmp eax, KEY_BACKSPACE
    je .erase
    cmp eax, 127
    je .erase
    cmp eax, ' '
    jb .done
    cmp eax, '~'
    ja .done
    mov ecx, [edit_length]
    cmp ecx, FORMULA_MAX - 1
    jae .done
    mov [edit_buffer + rcx], al
    inc ecx
    mov byte [edit_buffer + rcx], 0
    mov [edit_length], ecx
.done:
    ret
.erase:
    mov ecx, [edit_length]
    test ecx, ecx
    jz .done
    dec ecx
    mov byte [edit_buffer + rcx], 0
    mov [edit_length], ecx
    ret
.cancel:
    mov byte [editing], 0
    ret
.commit:
    mov byte [editing], 0
    mov edi, [selected]
    lea rsi, [edit_buffer]
    call set_formula
    jmp clear_newton

handle_click:
    mov eax, [screen_click_column]
    shl eax, 3
    add eax, [screen_origin_x]
    add eax, 4                  ; middle of the character cell, screen.inc only gives us cells
    mov edx, [screen_click_row]
    shl edx, 4
    add edx, [screen_origin_y]
    add edx, 8
    sub eax, [plot_left]
    jl .panel
    cmp edx, [plot_height]
    jae .done
    call column_x
    movsd [trace_x], xmm0
    mov byte [trace_on], 1
    jmp clear_newton
.panel:
    sub edx, PANEL_TOP
    jl .done
    shr edx, 4
    mov eax, edx
    xor edx, edx
    mov ecx, 2
    div ecx
    cmp eax, FUNCTION_MAX
    jae .done
    mov [selected], eax
    call clear_newton
    cmp byte [editing], 0
    jne .done
    jmp begin_edit
.done:
    ret

start_newton:
    push rbx
    mov eax, [selected]
    cmp byte [function_ok + rax], 0
    je .none
    cmp byte [trace_on], 0
    jne .seeded
    mov byte [trace_on], 1
    movsd xmm0, [view_x]
    movsd [trace_x], xmm0
.seeded:
    mov byte [newton_converged], 0
    movsd xmm0, [trace_x]
    xor ebx, ebx
.step:
    movsd [newton_x + rbx * 8], xmm0
    mov edi, [selected]
    call evaluate_slot
    movsd [newton_y + rbx * 8], xmm0
    inc ebx
    call is_finite
    jnc .finish
    andpd xmm0, [abs_mask]
    ucomisd xmm0, [newton_tolerance]
    jb .converged
    cmp ebx, NEWTON_MAX
    jae .finish
    movsd xmm0, [newton_x + rbx * 8 - 8]
    call slope_at
    call is_finite
    jnc .finish
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    je .finish
    movsd xmm1, [newton_y + rbx * 8 - 8]
    divsd xmm1, xmm0
    movsd xmm0, [newton_x + rbx * 8 - 8]
    subsd xmm0, xmm1
    call is_finite
    jnc .finish
    jmp .step
.converged:
    mov byte [newton_converged], 1
.finish:
    mov [newton_count], ebx
    mov dword [newton_shown], 1
    call sys_getticks
    mov [newton_clock], rax
    pop rbx
    ret
.none:
    lea rax, [no_function_text]
    mov [status_text], rax
    pop rbx
    ret

; central difference, step scales with |x| so it works at 1e6 too
slope_at:
    movsd [slope_x], xmm0
    andpd xmm0, [abs_mask]
    maxsd xmm0, [one]
    mulsd xmm0, [slope_step]
    movsd [slope_h], xmm0
    movsd xmm0, [slope_x]
    addsd xmm0, [slope_h]
    mov edi, [selected]
    call evaluate_slot
    movsd [slope_high], xmm0
    movsd xmm0, [slope_x]
    subsd xmm0, [slope_h]
    mov edi, [selected]
    call evaluate_slot
    movsd xmm1, [slope_high]
    subsd xmm1, xmm0
    movsd xmm0, [slope_h]
    addsd xmm0, xmm0
    divsd xmm1, xmm0
    movsd xmm0, xmm1
    ret

difference_at:
    movsd [difference_x], xmm0
    mov edi, [mark_first]
    call evaluate_slot
    cmp dword [mark_second], 0
    jl .done
    movsd [difference_value], xmm0
    movsd xmm0, [difference_x]
    mov edi, [mark_second]
    call evaluate_slot
    movsd xmm1, [difference_value]
    subsd xmm1, xmm0
    movsd xmm0, xmm1
.done:
    ret

find_marks:
    mov dword [mark_count], 0
    cmp byte [marks_mode], MARKS_NONE
    je .done
    mov eax, [selected]
    cmp byte [function_ok + rax], 0
    je .done
    mov [mark_first], eax
    mov dword [mark_second], -1
    cmp byte [marks_mode], MARKS_ROOTS
    je scan_crossings
    push rbx
    xor ebx, ebx
.pair:
    cmp ebx, [selected]
    je .next_pair
    cmp byte [function_ok + rbx], 0
    je .next_pair
    mov [mark_second], ebx
    call scan_crossings
.next_pair:
    inc ebx
    cmp ebx, FUNCTION_MAX
    jb .pair
    pop rbx
.done:
    ret

; sign change between two columns -> bisect. an exact 0 counts too, then the next column
; is not compared so the same root isn't added twice. a root that only touches 0 (x^2)
; has no sign change and is missed
scan_crossings:
    push rbx
    mov byte [scan_valid], 0
    xor ebx, ebx
.column:
    cmp ebx, [plot_width]
    ja .done
    cmp dword [mark_count], MARK_MAX
    jae .done
    mov eax, ebx
    call column_x
    movsd [scan_x], xmm0
    call difference_at
    movsd [scan_value], xmm0
    call is_finite
    jnc .invalid
    cmp byte [scan_valid], 0
    je .remember
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    je .exact
    movq rax, xmm0
    xor rax, [scan_previous]
    jns .remember
    movsd xmm0, [scan_previous_x]
    movsd xmm1, [scan_x]
    call bisect
    jmp .remember
.exact:
    movsd xmm0, [scan_x]
    call add_mark
.remember:
    movsd xmm0, [scan_value]
    movsd [scan_previous], xmm0
    movsd xmm0, [scan_x]
    movsd [scan_previous_x], xmm0
    mov byte [scan_valid], 1
    xorpd xmm1, xmm1
    ucomisd xmm1, [scan_value]
    jne .next
    mov byte [scan_valid], 0
    jmp .next
.invalid:
    mov byte [scan_valid], 0
.next:
    inc ebx
    jmp .column
.done:
    pop rbx
    ret

bisect:
    push rbx
    movsd [bisect_low], xmm0
    movsd [bisect_high], xmm1
    call difference_at
    movsd [bisect_low_value], xmm0
    mov ebx, BISECT_STEPS
.step:
    movsd xmm0, [bisect_low]
    addsd xmm0, [bisect_high]
    mulsd xmm0, [half]
    movsd [bisect_middle], xmm0
    call difference_at
    call is_finite
    jnc .fail
    movq rax, xmm0
    xor rax, [bisect_low_value]
    js .lower
    movsd [bisect_low_value], xmm0
    movsd xmm0, [bisect_middle]
    movsd [bisect_low], xmm0
    jmp .next
.lower:
    movsd xmm0, [bisect_middle]
    movsd [bisect_high], xmm0
.next:
    dec ebx
    jnz .step
    movsd xmm0, [bisect_middle]
    call difference_at
    andpd xmm0, [abs_mask]
    ucomisd xmm0, [mark_tolerance]
    ja .fail
    movsd xmm0, [bisect_middle]
    call add_mark
.fail:
    pop rbx
    ret

add_mark:
    mov ecx, [mark_count]
    cmp ecx, MARK_MAX
    jae .done
    movsd [mark_new_x], xmm0
    mov edi, [mark_first]
    call evaluate_slot
    movsd xmm1, [mark_new_x]
    mov ecx, [mark_count]
.shift:
    test ecx, ecx
    jz .place
    ucomisd xmm1, [marks_x + rcx * 8 - 8]
    jae .place
    movsd xmm2, [marks_x + rcx * 8 - 8]
    movsd [marks_x + rcx * 8], xmm2
    movsd xmm2, [marks_y + rcx * 8 - 8]
    movsd [marks_y + rcx * 8], xmm2
    dec ecx
    jmp .shift
.place:
    movsd [marks_x + rcx * 8], xmm1
    movsd [marks_y + rcx * 8], xmm0
    inc dword [mark_count]
.done:
    ret

draw_marks:
    push rbx
    xor ebx, ebx
.mark:
    cmp ebx, [mark_count]
    jae .done
    movsd xmm0, [marks_x + rbx * 8]
    call x_column
    push rax
    movsd xmm0, [marks_y + rbx * 8]
    call y_row
    mov esi, eax
    pop rdi
    mov edx, COLOR_ROOT
    call plot_marker
    inc ebx
    jmp .mark
.done:
    pop rbx
    ret

draw_derivative:
    cmp byte [derivative_on], 0
    je .done
    mov eax, [selected]
    cmp byte [function_ok + rax], 0
    je .done
    push rbx
    xor ebx, ebx
.column:
    cmp ebx, [plot_width]
    jae .finish
    mov eax, ebx
    call column_x
    call slope_at
    call is_finite
    jnc .next
    call y_row
    mov esi, eax
    mov edi, ebx
    mov edx, COLOR_DIM
    call plot_pixel
.next:
    add ebx, 2
    jmp .column
.finish:
    pop rbx
.done:
    ret

redraw:
    call layout
    xor edi, edi
    xor esi, esi
    mov edx, [screen_width]
    mov ecx, [screen_height]
    mov r8d, COLOR_BACK
    call draw_fill
    call draw_grid
    call find_marks
    call draw_derivative
    call draw_curves
    call draw_newton
    call draw_marks
    call draw_trace
    call draw_panel
    call draw_status
    jmp screen_present

panel_line:
    imul esi, esi, 16
    add esi, PANEL_TOP
    mov edi, 8
    jmp draw_text

draw_panel:
    cmp byte [panel_shown], 0
    je .done
    push rbx
    push r12
    xor edi, edi
    xor esi, esi
    mov edx, PANEL_W - 2
    mov ecx, [plot_height]
    mov r8d, COLOR_BACK
    call draw_fill
    mov edi, PANEL_W - 2
    xor esi, esi
    mov edx, 2
    mov ecx, [plot_height]
    mov r8d, COLOR_PANEL
    call draw_fill
    mov edi, 8
    mov esi, 4
    lea rdx, [panel_title]
    mov cl, COLOR_TEXT
    mov r8d, -1
    call draw_text

    xor ebx, ebx
.slot:
    cmp ebx, FUNCTION_MAX
    jae .info
    call draw_slot
    inc ebx
    jmp .slot

.info:
    mov r12d, FUNCTION_MAX * 2 + 1
    cmp dword [newton_count], 0
    jne .newton
    cmp byte [trace_on], 0
    je .marks
    lea rsi, [trace_x_label]
    movsd xmm0, [trace_x]
    mov ecx, COLOR_TEXT
    mov edx, r12d
    call value_at
    inc r12d
    lea rsi, [trace_y_label]
    movsd xmm0, [trace_y]
    mov ecx, COLOR_TEXT
    mov edx, r12d
    call value_at
    inc r12d
    movsd xmm0, [trace_x]
    call slope_at
    lea rsi, [trace_slope_label]
    mov ecx, COLOR_DIM
    mov edx, r12d
    call value_at
    inc r12d
    inc r12d

.marks:
    cmp dword [mark_count], 0
    je .range
    lea rdx, [roots_title]
    cmp byte [marks_mode], MARKS_ROOTS
    je .marks_title
    lea rdx, [crossings_title]
.marks_title:
    mov esi, r12d
    mov cl, COLOR_ROOT
    mov r8d, -1
    call panel_line
    inc r12d
    xor ebx, ebx
.mark:
    cmp ebx, [mark_count]
    jae .range
    call panel_room
    jc .range
    lea rsi, [mark_label]
    movsd xmm0, [marks_x + rbx * 8]
    cmp byte [marks_mode], MARKS_ROOTS
    je .mark_value
    call format_point
    jmp .mark_next
.mark_value:
    mov ecx, COLOR_TEXT
    mov edx, r12d
    call value_at
.mark_next:
    inc r12d
    inc ebx
    jmp .mark

.newton:
    lea rdx, [newton_title]
    mov esi, r12d
    mov cl, COLOR_TANGENT
    mov r8d, -1
    call panel_line
    inc r12d
    xor ebx, ebx
.newton_step:
    cmp ebx, [newton_shown]
    jae .newton_end
    call panel_room
    jc .range
    lea rsi, [step_label]
    mov eax, ebx
    add al, '0'
    cmp ebx, 10
    jb .digit
    add al, 'a' - '0' - 10
.digit:
    mov [step_label + 1], al
    movsd xmm0, [newton_x + rbx * 8]
    mov ecx, COLOR_TEXT
    mov edx, r12d
    call value_at
    inc r12d
    inc ebx
    jmp .newton_step
.newton_end:
    mov eax, [newton_shown]
    cmp eax, [newton_count]
    jb .range
    call panel_room
    jc .range
    lea rdx, [newton_failed_text]
    mov cl, COLOR_ERROR
    cmp byte [newton_converged], 0
    je .verdict
    lea rdx, [newton_done_text]
    mov cl, COLOR_ROOT
.verdict:
    mov esi, r12d
    mov r8d, -1
    call panel_line

.range:
    mov eax, [plot_height]
    sub eax, PANEL_TOP + 16
    shr eax, 4
    mov r12d, eax
    xor eax, eax
    call column_x
    lea rsi, [range_left_label]
    mov ecx, COLOR_DIM
    mov edx, r12d
    dec edx
    call value_at
    mov eax, [plot_width]
    call column_x
    lea rsi, [range_right_label]
    mov ecx, COLOR_DIM
    mov edx, r12d
    call value_at
    pop r12
    pop rbx
.done:
    ret

panel_room:
    mov eax, [plot_height]
    sub eax, PANEL_TOP + 48
    shr eax, 4
    cmp r12d, eax
    jae .full
    clc
    ret
.full:
    stc
    ret

value_at:
    push rdx
    push rcx
    lea rdi, [line_text]
    call str_copy
    dec rdi
    call format_value
    mov byte [line_text + PANEL_COLUMNS], 0
    pop rcx
    pop rsi
    lea rdx, [line_text]
    mov r8d, -1
    jmp panel_line

format_point:
    push rdx
    movsd xmm0, [marks_y + rbx * 8]
    movsd [point_y_value], xmm0
    movsd xmm0, [marks_x + rbx * 8]
    lea rdi, [line_text]
    mov byte [rdi], '('
    inc rdi
    call format_value
    lea rdi, [line_text]
    call str_len
    lea rdi, [line_text + rax]
    mov word [rdi], ', '
    add rdi, 2
    movsd xmm0, [point_y_value]
    call format_value
    lea rdi, [line_text]
    call str_len
    mov word [line_text + rax], ')'
    mov byte [line_text + PANEL_COLUMNS], 0
    pop rdx
    lea rdx, [line_text]
    mov esi, r12d
    mov cl, COLOR_TEXT
    mov r8d, -1
    jmp panel_line

; two lines per slot: number and formula, or the parse error under it
draw_slot:
    lea esi, [rbx * 2]
    imul esi, esi, 16
    add esi, PANEL_TOP - 2
    cmp ebx, [selected]
    jne .plain
    mov edi, 2
    mov edx, PANEL_W - 6
    mov ecx, 36
    mov r8d, COLOR_EDIT
    push rsi
    call draw_fill
    pop rsi
.plain:
    mov edi, 4
    add esi, 6
    mov edx, 3
    mov ecx, 6
    movzx r8d, byte [slot_colors + rbx]
    cmp byte [function_ok + rbx], 0
    jne .swatch
    mov r8d, COLOR_GRID
.swatch:
    call draw_fill

    lea rdi, [line_text]
    mov byte [rdi], 'y'
    lea eax, [rbx + '1']
    mov [rdi + 1], al
    mov byte [rdi + 2], '='
    add rdi, 3
    imul eax, ebx, FORMULA_MAX
    lea rsi, [formula_text + rax]
    cmp byte [editing], 0
    je .copy
    cmp ebx, [selected]
    jne .copy
    lea rsi, [edit_buffer]
    mov eax, [edit_length]
    sub eax, PANEL_COLUMNS - 5
    jle .copy
    add rsi, rax
.copy:
    call str_copy
    cmp byte [editing], 0
    je .clip
    cmp ebx, [selected]
    jne .clip
    mov word [rdi - 1], '_'
.clip:
    mov byte [line_text + PANEL_COLUMNS - 1], 0
    lea esi, [rbx * 2]
    lea rdx, [line_text]
    mov cl, COLOR_TEXT
    cmp byte [function_ok + rbx], 0
    jne .color
    mov cl, COLOR_DIM
.color:
    mov r8d, -1
    mov edi, 12
    imul esi, esi, 16
    add esi, PANEL_TOP
    call draw_text

    mov rdx, [function_error + rbx * 8]
    test rdx, rdx
    jz .done
    cmp byte [editing], 0
    je .error
    cmp ebx, [selected]
    je .done
.error:
    lea esi, [rbx * 2 + 1]
    mov edi, 12
    imul esi, esi, 16
    add esi, PANEL_TOP
    mov cl, COLOR_ERROR
    mov r8d, -1
    call draw_text
.done:
    ret

draw_status:
    mov edi, 0
    mov esi, [plot_height]
    mov edx, [screen_width]
    mov ecx, STATUS_H
    mov r8d, COLOR_PANEL
    call draw_fill
    lea rdx, [help_text]
    cmp byte [editing], 0
    je .message
    lea rdx, [edit_help_text]
.message:
    mov rax, [status_text]
    test rax, rax
    jz .show
    mov rdx, rax
    mov qword [status_text], 0
.show:
    mov edi, 6
    mov esi, [plot_height]
    add esi, 2
    mov cl, COLOR_TEXT
    mov r8d, -1
    jmp draw_text

PANEL_TOP = 26

window_title db 'GraphCalc', 0
panel_title db 'GraphCalc', 0
empty_text db 0
formula_sine db 'sin(x)', 0
formula_parabola db 'x^2/4-2', 0
default_formulas dq formula_sine, formula_parabola, empty_text, empty_text
trace_x_label db 'x  = ', 0
trace_y_label db 'y  = ', 0
trace_slope_label db "y' = ", 0
roots_title db 'roots', 0
crossings_title db 'intersections', 0
mark_label db 'x = ', 0
newton_title db 'newton', 0
step_label db 'x0 = ', 0
newton_done_text db 'converged', 0
newton_failed_text db 'no root found', 0
range_left_label db 'from ', 0
range_right_label db 'to   ', 0
no_function_text db 'select a valid function first', 0
help_text db 'Enter edit Tab slot t trace n newton r roots i cross d slope +/- zoom 0 home', 0
edit_help_text db 'type a formula in x   Enter apply   Esc cancel   e.g. 2sin(3x)+|x|', 0

align 8
default_scale dq 0x3FA999999999999A
zoom_factor dq 0x3FE999999999999A
scale_min dq 0x3D06849B86A12B9B
scale_max dq 0x40C3880000000000
newton_tolerance dq 0x3DDB7CDFD9D7BDBB
mark_tolerance dq 0x3EB0C6F7A0B5ED8D
slope_step dq 0x3EE4F8B588E368F1

include 'math.inc'
include 'expr.inc'
include 'text.inc'
include 'graph.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include '../runtime/screen_bss.inc'

align 8
view_x rq 1
view_y rq 1
view_scale rq 1
trace_x rq 1
trace_y rq 1
grid_unit rq 1
grid_value rq 1
slope_x rq 1
slope_h rq 1
slope_high rq 1
difference_x rq 1
difference_value rq 1
scan_x rq 1
scan_value rq 1
scan_previous rq 1
scan_previous_x rq 1
bisect_low rq 1
bisect_high rq 1
bisect_low_value rq 1
bisect_middle rq 1
point_y_value rq 1
mark_new_x rq 1
tan_angle rq 1
root_target rq 1
tan_cosine rq 1
power_base rq 1
power_exponent rq 1
parse_cursor rq 1
program_ops rq 1
program_values rq 1
parse_error rq 1
eval_x rq 1
call_target rq 1
status_text rq 1
newton_clock rq 1
function_error rq FUNCTION_MAX
newton_x rq NEWTON_MAX
newton_y rq NEWTON_MAX
marks_x rq MARK_MAX
marks_y rq MARK_MAX
eval_stack rq STACK_MAX
program_value_area rq FUNCTION_MAX * PROGRAM_MAX
quadrant rd 1
program_length rd 1
eval_depth rd 1
bar_depth rd 1
selected rd 1
edit_length rd 1
newton_count rd 1
newton_shown rd 1
mark_count rd 1
mark_first rd 1
mark_second rd 1
plot_left rd 1
plot_top rd 1
plot_width rd 1
plot_height rd 1
axis_column rd 1
axis_row rd 1
line_dx rd 1
line_dy rd 1
line_sx rd 1
line_sy rd 1
line_error rd 1
previous_row rd 1
tangent_column rd 1
tangent_row rd 1
program_op_area rb FUNCTION_MAX * PROGRAM_MAX
formula_text rb FUNCTION_MAX * FORMULA_MAX
edit_buffer rb FORMULA_MAX
function_ok rb FUNCTION_MAX
name_text rb NAME_MAX
digit_scratch rb 32
label_text rb 40
line_text rb 160
running rb 1
editing rb 1
trace_on rb 1
derivative_on rb 1
marks_mode rb 1
newton_converged rb 1
panel_shown rb 1
line_thick rb 1
previous_valid rb 1
scan_valid rb 1
