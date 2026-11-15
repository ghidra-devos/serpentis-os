; the plot: world <-> pixel mapping, grid with labels, the curves, trace and newton
; overlays. view_x/view_y is the world point in the middle, view_scale is units per pixel

PANEL_W = 192
STATUS_H = 20
GRID_SPACING = 64
COORD_LIMIT = 6000              ; off screen pixel coordinates are clamped here so ints never overflow

COLOR_BACK = 0
COLOR_GRID = 8
COLOR_AXIS = 7
COLOR_LABEL = 7
COLOR_PANEL = 8
COLOR_TEXT = 15
COLOR_DIM = 7
COLOR_ERROR = 12
COLOR_TANGENT = 12
COLOR_ROOT = 10
COLOR_EDIT = 1

layout:
    mov eax, [screen_width]
    mov dword [plot_left], PANEL_W
    mov byte [panel_shown], 1
    cmp eax, PANEL_W + 160
    jae .wide
    mov dword [plot_left], 0
    mov byte [panel_shown], 0
.wide:
    sub eax, [plot_left]
    mov [plot_width], eax
    mov dword [plot_top], 0
    mov eax, [screen_height]
    sub eax, STATUS_H
    jg .height
    mov eax, 1
.height:
    mov [plot_height], eax
    ret

column_x:
    mov ecx, [plot_width]
    shr ecx, 1
    sub eax, ecx
    cvtsi2sd xmm0, eax
    mulsd xmm0, [view_scale]
    addsd xmm0, [view_x]
    ret

row_y:
    mov ecx, [plot_height]
    shr ecx, 1
    sub ecx, eax
    cvtsi2sd xmm0, ecx
    mulsd xmm0, [view_scale]
    addsd xmm0, [view_y]
    ret

clamp_coordinate:
    call is_finite
    jnc .huge
    ucomisd xmm0, [coordinate_high]
    ja .huge
    ucomisd xmm0, [coordinate_low]
    jb .tiny
    call math_round
    cvttsd2si rax, xmm0
    ret
.huge:
    mov eax, COORD_LIMIT
    ret
.tiny:
    mov eax, -COORD_LIMIT
    ret

x_column:
    subsd xmm0, [view_x]
    divsd xmm0, [view_scale]
    mov eax, [plot_width]
    shr eax, 1
    cvtsi2sd xmm1, eax
    addsd xmm0, xmm1
    jmp clamp_coordinate

y_row:
    subsd xmm0, [view_y]
    divsd xmm0, [view_scale]
    mov eax, [plot_height]
    shr eax, 1
    cvtsi2sd xmm1, eax
    subsd xmm1, xmm0
    movsd xmm0, xmm1
    jmp clamp_coordinate

; edi/esi inside the plot, dl color. straight into the surface, no clipping beyond the plot
plot_pixel:
    cmp edi, [plot_width]
    jae .done
    cmp esi, [plot_height]
    jae .done
    mov eax, esi
    add eax, [plot_top]
    imul eax, [draw_stride]
    add eax, edi
    add eax, [plot_left]
    mov rcx, [draw_base]
    mov [rcx + rax], dl
.done:
    ret

plot_line:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12d, edi
    mov r13d, esi
    mov r14d, edx
    mov r15d, ecx
    mov ebx, r8d
    mov eax, r14d
    sub eax, r12d
    cdq
    xor eax, edx
    sub eax, edx
    mov [line_dx], eax
    mov eax, r15d
    sub eax, r13d
    cdq
    xor eax, edx
    sub eax, edx
    neg eax
    mov [line_dy], eax
    mov dword [line_sx], 1
    cmp r12d, r14d
    jl .sx
    mov dword [line_sx], -1
.sx:
    mov dword [line_sy], 1
    cmp r13d, r15d
    jl .sy
    mov dword [line_sy], -1
.sy:
    mov eax, [line_dx]
    add eax, [line_dy]
    mov [line_error], eax
.pixel:
    mov edi, r12d
    mov esi, r13d
    mov edx, ebx
    call plot_pixel
    cmp byte [line_thick], 0
    je .thin
    mov edi, r12d
    lea esi, [r13 + 1]
    mov edx, ebx
    call plot_pixel
.thin:
    cmp r12d, r14d
    jne .step
    cmp r13d, r15d
    je .done
.step:
    mov eax, [line_error]
    add eax, eax
    cmp eax, [line_dy]
    jl .vertical
    mov ecx, [line_dy]
    add [line_error], ecx
    add r12d, [line_sx]
.vertical:
    cmp eax, [line_dx]
    jg .pixel
    mov ecx, [line_dx]
    add [line_error], ecx
    add r13d, [line_sy]
    jmp .pixel
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

plot_marker:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13d, esi
    mov ebx, edx
    mov ecx, -2
.row:
    cmp ecx, 2
    jg .done
    mov edx, -2
.column:
    cmp edx, 2
    jg .next_row
    push rcx
    push rdx
    lea edi, [r12 + rdx]
    lea esi, [r13 + rcx]
    mov edx, ebx
    call plot_pixel
    pop rdx
    pop rcx
    inc edx
    jmp .column
.next_row:
    inc ecx
    jmp .row
.done:
    pop r13
    pop r12
    pop rbx
    ret

grid_step:
    mov eax, GRID_SPACING
    cvtsi2sd xmm2, eax
    mulsd xmm2, [view_scale]
    movsd xmm0, [one]
.up:
    movsd xmm1, xmm0
    mulsd xmm1, [ten]
    ucomisd xmm1, xmm2
    ja .down
    movsd xmm0, xmm1
    jmp .up
.down:
    ucomisd xmm0, xmm2
    jbe .choose
    divsd xmm0, [ten]
    jmp .down
.choose:
    ucomisd xmm0, xmm2
    jae .done
    movsd xmm1, xmm0
    addsd xmm1, xmm0
    ucomisd xmm1, xmm2
    jb .five
    movsd xmm0, xmm1
    ret
.five:
    movsd xmm1, xmm0
    mulsd xmm1, [five]
    ucomisd xmm1, xmm2
    jb .ten
    movsd xmm0, xmm1
    ret
.ten:
    mulsd xmm0, [ten]
.done:
    ret

draw_grid:
    push rbx
    push r12
    call grid_step
    movsd [grid_unit], xmm0

    xor eax, eax
    call column_x
    divsd xmm0, [grid_unit]
    call math_ceil
    mulsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    mov ebx, 64
.vertical:
    dec ebx
    js .horizontal
    movsd xmm0, [grid_value]
    call x_column
    cmp eax, [plot_width]
    jge .horizontal
    mov r12d, eax
    mov edi, r12d
    xor esi, esi
    mov edx, r12d
    mov ecx, [plot_height]
    dec ecx
    mov r8d, COLOR_GRID
    mov byte [line_thick], 0
    call plot_line
    movsd xmm0, [grid_value]
    addsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    jmp .vertical

.horizontal:
    mov eax, [plot_height]
    dec eax
    call row_y
    divsd xmm0, [grid_unit]
    call math_ceil
    mulsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    mov ebx, 64
.row:
    dec ebx
    js .axes
    movsd xmm0, [grid_value]
    call y_row
    test eax, eax
    js .axes
    mov r12d, eax
    xor edi, edi
    mov esi, r12d
    mov edx, [plot_width]
    dec edx
    mov ecx, r12d
    mov r8d, COLOR_GRID
    call plot_line
    movsd xmm0, [grid_value]
    addsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    jmp .row

.axes:
    xorpd xmm0, xmm0
    call x_column
    mov [axis_column], eax
    xorpd xmm0, xmm0
    call y_row
    mov [axis_row], eax
    mov edi, [axis_column]
    xor esi, esi
    mov edx, edi
    mov ecx, [plot_height]
    dec ecx
    mov r8d, COLOR_AXIS
    call plot_line
    xor edi, edi
    mov esi, [axis_row]
    mov edx, [plot_width]
    dec edx
    mov ecx, esi
    mov r8d, COLOR_AXIS
    call plot_line
    call draw_labels
    pop r12
    pop rbx
    ret

draw_labels:
    push rbx
    push r12
    xor eax, eax
    call column_x
    divsd xmm0, [grid_unit]
    call math_ceil
    mulsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    mov ebx, 64
.vertical:
    dec ebx
    js .horizontal
    movsd xmm0, [grid_value]
    call x_column
    cmp eax, [plot_width]
    jge .horizontal
    mov r12d, eax
    call grid_is_zero
    jc .next_column
    movsd xmm0, [grid_value]
    lea rdi, [label_text]
    call format_value
    lea edi, [r12 + 3]
    mov esi, [axis_row]
    add esi, 3
    call label_at
.next_column:
    movsd xmm0, [grid_value]
    addsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    jmp .vertical
.horizontal:
    mov eax, [plot_height]
    dec eax
    call row_y
    divsd xmm0, [grid_unit]
    call math_ceil
    mulsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    mov ebx, 64
.row:
    dec ebx
    js .done
    movsd xmm0, [grid_value]
    call y_row
    test eax, eax
    js .done
    mov r12d, eax
    call grid_is_zero
    jc .next_row
    movsd xmm0, [grid_value]
    lea rdi, [label_text]
    call format_value
    mov edi, [axis_column]
    add edi, 4
    lea esi, [r12 - 17]
    call label_at
.next_row:
    movsd xmm0, [grid_value]
    addsd xmm0, [grid_unit]
    movsd [grid_value], xmm0
    jmp .row
.done:
    pop r12
    pop rbx
    ret

grid_is_zero:
    movsd xmm0, [grid_value]
    andpd xmm0, [abs_mask]
    movsd xmm1, [grid_unit]
    mulsd xmm1, [label_epsilon]
    ucomisd xmm0, xmm1
    jb .zero
    clc
    ret
.zero:
    stc
    ret

; label_text at edi/esi, pushed back inside the plot so it never hangs off the edge
label_at:
    push rbx
    push r12
    mov ebx, edi
    mov r12d, esi
    lea rdi, [label_text]
    call str_len
    shl eax, 3
    mov ecx, [plot_width]
    sub ecx, eax
    sub ecx, 2
    cmp ebx, ecx
    jle .x_high
    mov ebx, ecx
.x_high:
    cmp ebx, 2
    jge .x_low
    mov ebx, 2
.x_low:
    mov ecx, [plot_height]
    sub ecx, 18
    cmp r12d, ecx
    jle .y_high
    mov r12d, ecx
.y_high:
    cmp r12d, 2
    jge .y_low
    mov r12d, 2
.y_low:
    lea edi, [rbx + 0]
    add edi, [plot_left]
    mov esi, r12d
    add esi, [plot_top]
    lea rdx, [label_text]
    mov cl, COLOR_LABEL
    mov r8d, COLOR_BACK
    call draw_text
    pop r12
    pop rbx
    ret

draw_curves:
    push rbx
    push r12
    push r13
    xor ebx, ebx
.slot:
    cmp ebx, FUNCTION_MAX
    jae .done
    cmp byte [function_ok + rbx], 0
    je .next_slot
    mov byte [line_thick], 1
    cmp ebx, [selected]
    je .thick
    mov byte [line_thick], 0
.thick:
    mov byte [previous_valid], 0
    xor r12d, r12d
.column:
    cmp r12d, [plot_width]
    jae .next_slot
    mov eax, r12d
    call column_x
    mov edi, ebx
    call evaluate_slot
    call is_finite
    jnc .gap
    call y_row
    mov r13d, eax
    cmp byte [previous_valid], 0
    je .first
    mov eax, r13d
    sub eax, [previous_row]
    cdq
    xor eax, edx
    sub eax, edx
    mov ecx, [plot_height]
    shr ecx, 1
    cmp eax, ecx
    jle .join
    call continuous
    jnc .first
.join:
    lea edi, [r12 - 1]
    mov esi, [previous_row]
    mov edx, r12d
    mov ecx, r13d
    movzx r8d, byte [slot_colors + rbx]
    call plot_line
    jmp .remember
.first:
    mov edi, r12d
    mov esi, r13d
    movzx edx, byte [slot_colors + rbx]
    call plot_pixel
.remember:
    mov [previous_row], r13d
    mov byte [previous_valid], 1
    inc r12d
    jmp .column
.gap:
    mov byte [previous_valid], 0
    inc r12d
    jmp .column
.next_slot:
    inc ebx
    jmp .slot
.done:
    mov byte [line_thick], 0
    pop r13
    pop r12
    pop rbx
    ret

continuous:
    mov eax, r12d
    call column_x
    movsd xmm1, [view_scale]
    mulsd xmm1, [half]
    subsd xmm0, xmm1
    mov edi, ebx
    call evaluate_slot
    call is_finite
    jnc .broken
    call y_row
    cmp eax, COORD_LIMIT
    je .broken
    cmp eax, -COORD_LIMIT
    je .broken
    mov ecx, [previous_row]
    mov edx, r13d
    cmp ecx, edx
    jle .ordered
    xchg ecx, edx
.ordered:
    cmp eax, ecx
    jl .broken
    cmp eax, edx
    jg .broken
    stc
    ret
.broken:
    clc
    ret

evaluate_slot:
    movsxd rdi, edi
    cmp byte [function_ok + rdi], 0
    je nan_value
    imul rax, rdi, PROGRAM_MAX * 8
    lea rsi, [program_value_area + rax]
    imul rax, rdi, PROGRAM_MAX
    lea rdi, [program_op_area + rax]
    jmp evaluate

dotted_vertical:
    push rbx
    push r12
    push r13
    mov ebx, edi
    mov r12d, esi
    mov r13d, edx
    cmp r12d, r13d
    jle .loop
    xchg r12d, r13d
.loop:
    cmp r12d, r13d
    jg .done
    mov edi, ebx
    mov esi, r12d
    mov edx, ecx
    push rcx
    call plot_pixel
    pop rcx
    add r12d, 3
    jmp .loop
.done:
    pop r13
    pop r12
    pop rbx
    ret

draw_trace:
    cmp byte [trace_on], 0
    je .done
    push rbx
    movsd xmm0, [trace_x]
    call x_column
    mov ebx, eax
    mov edi, ebx
    xor esi, esi
    mov edx, [plot_height]
    mov ecx, COLOR_AXIS
    call dotted_vertical
    movsd xmm0, [trace_x]
    mov edi, [selected]
    call evaluate_slot
    movsd [trace_y], xmm0
    call is_finite
    jnc .out
    call y_row
    mov esi, eax
    mov edi, ebx
    mov edx, COLOR_TEXT
    call plot_marker
.out:
    pop rbx
.done:
    ret

; each step: the point on the curve, the tangent down to the x axis, then dotted up to the
; next point. the last point turns green when it converged
draw_newton:
    cmp dword [newton_count], 0
    je .done
    push rbx
    push r12
    push r13
    xor ebx, ebx
.step:
    cmp ebx, [newton_shown]
    jae .finish
    movsd xmm0, [newton_x + rbx * 8]
    call x_column
    mov r12d, eax
    movsd xmm0, [newton_y + rbx * 8]
    call y_row
    mov r13d, eax
    lea eax, [rbx + 1]
    cmp eax, [newton_shown]
    jae .marker
    movsd xmm0, [newton_x + rbx * 8 + 8]
    call x_column
    mov [tangent_column], eax
    xorpd xmm0, xmm0
    call y_row
    mov [tangent_row], eax
    mov edi, r12d
    mov esi, r13d
    mov edx, [tangent_column]
    mov ecx, [tangent_row]
    mov r8d, COLOR_TANGENT
    mov byte [line_thick], 0
    call plot_line
    movsd xmm0, [newton_y + rbx * 8 + 8]
    call y_row
    mov edx, eax
    mov edi, [tangent_column]
    mov esi, [tangent_row]
    mov ecx, COLOR_AXIS
    call dotted_vertical
.marker:
    mov edi, r12d
    mov esi, r13d
    mov edx, COLOR_TANGENT
    mov eax, [newton_count]
    dec eax
    cmp ebx, eax
    jne .plot
    cmp byte [newton_converged], 0
    je .plot
    mov edx, COLOR_ROOT
.plot:
    call plot_marker
    inc ebx
    jmp .step
.finish:
    pop r13
    pop r12
    pop rbx
.done:
    ret

align 8
coordinate_high dq 0x40B7700000000000
coordinate_low dq 0xC0B7700000000000
five dq 0x4014000000000000
label_epsilon dq 0x3EB0C6F7A0B5ED8D
slot_colors db 14, 11, 13, 10
