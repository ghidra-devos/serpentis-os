; gfx3d. meshes with a z buffer and the usual 3d drawing (points, lines, arrows, filled
; polygons, circles, ellipses, arcs, beziers, b-splines, text, fill, blots, a mirror plane)
; into any 8 bit surface. no perspective, x and y go straight to the screen and z only sorts.
; everything keeps rbx rbp r12-r15 and trashes the rest, xmm too. gfx3d_bss.inc goes after
; the code. one unit is g3_lens of the shorter side
;
; g3_begin        rdi -> pixels, esi = stride, edx = width, ecx = height, r8 -> z buffer
;                 (width * height dwords). resets matrix, ink, pen and mirror
; g3_clear        edi = background, -1 only clears the z buffer
;
; matrix, gl order (the one named last moves the mesh first):
;   g3_rotate_x/_y/_z  xmm0 = radians, g3_translate  g3_scale  xmm0-2, g3_multiply  rsi -> 16 floats
; the other order, acting after whatever is already there:
;   g3_turn_x/_y/_z, g3_move  g3_place (sets the translation)  g3_zoom (scales it all), g3_multiply_left
; g3_identity g3_push g3_pop g3_get g3_set, g3_matrix_off/on (points skip the matrix)
; g3_transform    xmm0-2 -> camera xmm0-2, screen xmm3 xmm4
; g3_unproject    screen xmm0 xmm1 + camera z xmm2 -> xmm0-2
;
; g3_light        xmm0-2 towards the light, any length
; g3_color        edi = color, esi = second color, edx = 0-65536 chance the second one wins
;                 on a pixel, G3_CHECKER for every other one
; g3_thick        edi = pen in pixels, esi = 1 scales it with the matrix
; g3_mirror       rsi -> 3 points on the plane, edi = 0, G3_MIRROR or G3_JUST_MIRROR
;
; g3_draw_mesh    rdi -> mesh, esi = G3_WIRE G3_TWO_SIDED G3_HALF_RANGE G3_NO_CULL
;                 a mesh: byte 24, i32 vertices, i32 triangles, i32 x y z each, i32 color and
;                 three i32 corners each, byte 0. color bit 0x1000 is half range, 0x2000 two
;                 sided. the old float 'MESH' files still load
; g3_mesh_check  g3_fit  g3_pick
; g3_point xmm0-2, g3_line3 g3_arrow xmm0-2 to xmm3-5, g3_points g3_polyline g3_poly rsi ecx,
; g3_rect corner + xmm3 xmm4, g3_circle xmm3, g3_ellipse xmm3 xmm4 turn xmm5, g3_regpoly ecx
; sides, g3_arc + xmm6 start xmm7 length, g3_bezier2/3 rsi, g3_bspline2/3 rsi ecx edx=closed,
; g3_print rdx text ecx box/diamond (draw.inc), g3_flood edi, g3_blot rsi ecx edx,
; g3_line2 g3_rect2 g3_circle2 in plain screen pixels
;
; lighting: 16 ega colors only (palette 48-63). away from the light a face is black mixed
; with its color, towards it its color mixed with color xor 8, and every pixel picks one of
; the two at random. it fizzes while it turns, thats on purpose

G3_WIRE = 1
G3_TWO_SIDED = 2
G3_HALF_RANGE = 4
G3_NO_CULL = 8
G3_ROP_HALF_RANGE = 0x1000
G3_ROP_TWO_SIDED = 0x2000
G3_SPRITE_MESH = 24
G3_MIRROR = 1
G3_JUST_MIRROR = 3
G3_CHECKER = -1
G3_BOX = 1
G3_DIAMOND = 2
G3_CLEAR = 0xFF
G3_MESH_MAGIC = 'MESH'
G3_VERTEX_MAX = 2048
G3_STACK_MAX = 8
G3_SHAPE_MAX = 64
G3_CURVE_MAX = 32
G3_CURVE_STEPS = 32
G3_FLOOD_MAX = 4096
G3_THICK_MAX = 64
G3_X = 0
G3_Y = 4
G3_Z = 8
G3_SX = 12
G3_SY = 16
G3_W = 20
G3_VERTEX_BYTES = 32
G3_MATRIX_BYTES = 64

g3_begin:
    mov [g3_pixels], rdi
    mov [g3_stride], esi
    mov [g3_width], edx
    mov [g3_height], ecx
    mov [g3_zbuffer], r8
    cvtsi2ss xmm0, edx
    movss [g3_width_f], xmm0
    cvtsi2ss xmm1, ecx
    movss [g3_height_f], xmm1
    mulss xmm0, [g3_half]
    movss [g3_center_x], xmm0
    mulss xmm1, [g3_half]
    movss [g3_center_y], xmm1
    cmp ecx, edx
    cmova ecx, edx
    cvtsi2ss xmm0, ecx
    mulss xmm0, [g3_lens]
    movss [g3_focal], xmm0
    mov dword [g3_depth], 0
    mov dword [g3_pen], 1
    mov byte [g3_ink], PALETTE_FIXED + 15
    mov byte [g3_ink2], PALETTE_FIXED + 15
    mov dword [g3_ink_chance], 0
    mov byte [g3_mirror_mode], 0
    mov byte [g3_reflecting], 0
    mov byte [g3_raw], 0

g3_identity:
    lea rdi, [g3_matrix]

g3_load_unit:
    lea rsi, [g3_unit]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
    ret

g3_clear:
    push rdi
    mov rdi, [g3_zbuffer]
    mov ecx, [g3_width]
    imul ecx, [g3_height]
    xor eax, eax
    rep stosd
    pop rax
    test eax, eax
    js .done
    mov rdx, [g3_pixels]
    mov r8d, [g3_height]
    test r8d, r8d
    jz .done
.row:
    mov rdi, rdx
    mov ecx, [g3_width]
    rep stosb
    mov ecx, [g3_stride]
    add rdx, rcx
    dec r8d
    jnz .row
.done:
    ret

g3_push:
    mov eax, [g3_depth]
    cmp eax, G3_STACK_MAX
    jae .full
    inc dword [g3_depth]
    shl eax, 6
    lea rdi, [g3_stack + rax]
    lea rsi, [g3_matrix]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
.full:
    ret

g3_pop:
    mov eax, [g3_depth]
    test eax, eax
    jz .empty
    dec eax
    mov [g3_depth], eax
    shl eax, 6
    lea rsi, [g3_stack + rax]
    lea rdi, [g3_matrix]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
.empty:
    ret

g3_get:
    lea rsi, [g3_matrix]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
    ret

g3_set:
    lea rdi, [g3_matrix]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
    ret

g3_matrix_off:
    mov byte [g3_raw], 1
    ret

g3_matrix_on:
    mov byte [g3_raw], 0
    ret

g3_multiply:
    lea rdi, [g3_matrix]
    jmp g3_compose

g3_multiply_left:
    mov rdi, rsi
    lea rsi, [g3_matrix]

g3_compose:
    xor ecx, ecx
.row:
    xor edx, edx
.column:
    movss xmm0, [rdi + rcx]
    mulss xmm0, [rsi + rdx]
    movss xmm1, [rdi + rcx + 4]
    mulss xmm1, [rsi + rdx + 16]
    addss xmm0, xmm1
    movss xmm1, [rdi + rcx + 8]
    mulss xmm1, [rsi + rdx + 32]
    addss xmm0, xmm1
    movss xmm1, [rdi + rcx + 12]
    mulss xmm1, [rsi + rdx + 48]
    addss xmm0, xmm1
    lea eax, [rcx + rdx]
    movss [g3_product + rax], xmm0
    add edx, 4
    cmp edx, 16
    jb .column
    add ecx, 16
    cmp ecx, G3_MATRIX_BYTES
    jb .row
    lea rsi, [g3_product]
    lea rdi, [g3_matrix]
    mov ecx, G3_MATRIX_BYTES
    rep movsb
    ret

g3_rotate_x:
    mov ecx, 1
    mov edx, 2
    xor r8d, r8d
    jmp g3_rotate

g3_rotate_y:
    mov ecx, 2
    xor edx, edx
    xor r8d, r8d
    jmp g3_rotate

g3_rotate_z:
    xor ecx, ecx
    mov edx, 1
    xor r8d, r8d
    jmp g3_rotate

g3_turn_x:
    mov ecx, 1
    mov edx, 2
    mov r8d, 1
    jmp g3_rotate

g3_turn_y:
    mov ecx, 2
    xor edx, edx
    mov r8d, 1
    jmp g3_rotate

g3_turn_z:
    xor ecx, ecx
    mov edx, 1
    mov r8d, 1

; ecx edx = the two axes that turn. cos on both diagonals, -sin in row ecx, sin in row edx.
; r8d = 1 puts it in front of the matrix
g3_rotate:
    push rcx
    push rdx
    lea rdi, [g3_operand]
    call g3_load_unit
    call g3_sincos
    pop rdx
    pop rcx
    imul eax, ecx, 20
    movss [g3_operand + rax], xmm1
    imul eax, edx, 20
    movss [g3_operand + rax], xmm1
    lea eax, [rdx * 4 + rcx]
    movss [g3_operand + rax * 4], xmm0
    xorps xmm2, xmm2
    subss xmm2, xmm0
    lea eax, [rcx * 4 + rdx]
    movss [g3_operand + rax * 4], xmm2
    lea rsi, [g3_operand]
    test r8d, r8d
    jnz g3_multiply_left
    jmp g3_multiply

g3_translate:
    lea rdi, [g3_operand]
    call g3_load_unit
    movss [g3_operand + 12], xmm0
    movss [g3_operand + 28], xmm1
    movss [g3_operand + 44], xmm2
    lea rsi, [g3_operand]
    jmp g3_multiply

g3_scale:
    lea rdi, [g3_operand]
    call g3_load_unit
    movss [g3_operand], xmm0
    movss [g3_operand + 20], xmm1
    movss [g3_operand + 40], xmm2
    lea rsi, [g3_operand]
    jmp g3_multiply

g3_place:
    movss [g3_matrix + 12], xmm0
    movss [g3_matrix + 28], xmm1
    movss [g3_matrix + 44], xmm2
    ret

g3_move:
    addss xmm0, [g3_matrix + 12]
    addss xmm1, [g3_matrix + 28]
    addss xmm2, [g3_matrix + 44]
    jmp g3_place

g3_zoom:
    xor eax, eax
.entry:
    movss xmm1, [g3_matrix + rax * 4]
    mulss xmm1, xmm0
    movss [g3_matrix + rax * 4], xmm1
    inc eax
    cmp eax, 12
    jb .entry
    ret

g3_light:
    movss xmm3, xmm0
    mulss xmm3, xmm0
    movss xmm4, xmm1
    mulss xmm4, xmm1
    addss xmm3, xmm4
    movss xmm4, xmm2
    mulss xmm4, xmm2
    addss xmm3, xmm4
    sqrtss xmm3, xmm3
    xorps xmm4, xmm4
    comiss xmm3, xmm4
    jbe .done
    divss xmm0, xmm3
    divss xmm1, xmm3
    divss xmm2, xmm3
    movss [g3_light_dir], xmm0
    movss [g3_light_dir + 4], xmm1
    movss [g3_light_dir + 8], xmm2
.done:
    ret

g3_color:
    mov [g3_ink], dil
    mov [g3_ink2], sil
    mov [g3_ink_chance], edx
    ret

g3_use_ink:
    mov al, [g3_ink]
    mov [g3_dark], al
    mov [g3_wire_color], al
    mov al, [g3_ink2]
    mov [g3_bright], al
    mov eax, [g3_ink_chance]
    mov [g3_chance], eax
    ret

g3_thick:
    test esi, esi
    jz .set
    xorps xmm3, xmm3
    xor eax, eax
.row:
    movss xmm0, [g3_matrix + rax]
    addss xmm0, [g3_matrix + rax + 4]
    addss xmm0, [g3_matrix + rax + 8]
    mulss xmm0, xmm0
    addss xmm3, xmm0
    add eax, 16
    cmp eax, 48
    jb .row
    mulss xmm3, [g3_third]
    sqrtss xmm3, xmm3
    cvtsi2ss xmm0, edi
    mulss xmm0, xmm3
    addss xmm0, [g3_half]
    cvttss2si edi, xmm0
.set:
    cmp edi, 1
    jge .low
    mov edi, 1
.low:
    cmp edi, G3_THICK_MAX
    jle .high
    mov edi, G3_THICK_MAX
.high:
    mov [g3_pen], edi
    ret

; xmm0 = radians -> sin in xmm0, cos in xmm1, eax and xmm0-4 change. folded into a quarter
; turn first, then the short taylor series is plenty for pixels
g3_sincos:
    movss xmm1, xmm0
    mulss xmm1, [g3_two_over_pi]
    cvtss2si eax, xmm1
    cvtsi2ss xmm1, eax
    mulss xmm1, [g3_half_pi]
    subss xmm0, xmm1
    movss xmm2, xmm0
    mulss xmm2, xmm0
    movss xmm3, [g3_sin_7]
    mulss xmm3, xmm2
    addss xmm3, [g3_sin_5]
    mulss xmm3, xmm2
    addss xmm3, [g3_sin_3]
    mulss xmm3, xmm2
    mulss xmm3, xmm0
    addss xmm0, xmm3
    movss xmm1, [g3_cos_6]
    mulss xmm1, xmm2
    addss xmm1, [g3_cos_4]
    mulss xmm1, xmm2
    addss xmm1, [g3_cos_2]
    mulss xmm1, xmm2
    addss xmm1, [g3_one]
    test eax, 1
    jz .even
    xorps xmm4, xmm4
    subss xmm4, xmm0
    movss xmm0, xmm1
    movss xmm1, xmm4
.even:
    test eax, 2
    jz .done
    xorps xmm4, xmm4
    subss xmm4, xmm0
    movss xmm0, xmm4
    xorps xmm4, xmm4
    subss xmm4, xmm1
    movss xmm1, xmm4
.done:
    ret

g3_abs:
    xorps xmm4, xmm4
    comiss xmm0, xmm4
    jae .done
    subss xmm4, xmm0
    movss xmm0, xmm4
.done:
    ret

g3_cross:
    movss xmm0, [rsi + 4]
    mulss xmm0, [rdi + 8]
    movss xmm3, [rsi + 8]
    mulss xmm3, [rdi + 4]
    subss xmm0, xmm3
    movss xmm1, [rsi + 8]
    mulss xmm1, [rdi]
    movss xmm3, [rsi]
    mulss xmm3, [rdi + 8]
    subss xmm1, xmm3
    movss xmm2, [rsi]
    mulss xmm2, [rdi + 4]
    movss xmm3, [rsi + 4]
    mulss xmm3, [rdi]
    subss xmm2, xmm3
    ret

g3_mix:
    xor eax, eax
.axis:
    movss xmm0, [rdx + rax * 4]
    subss xmm0, [rsi + rax * 4]
    mulss xmm0, xmm7
    addss xmm0, [rsi + rax * 4]
    movss [rdi + rax * 4], xmm0
    inc eax
    cmp eax, 3
    jb .axis
    ret

g3_project:
    movss xmm0, [g3_far]
    subss xmm0, [rdi + G3_Z]
    movss [rdi + G3_W], xmm0
    movss xmm1, [rdi + G3_X]
    mulss xmm1, [g3_focal]
    addss xmm1, [g3_center_x]
    movss [rdi + G3_SX], xmm1
    movss xmm1, [g3_center_y]
    movss xmm2, [rdi + G3_Y]
    mulss xmm2, [g3_focal]
    subss xmm1, xmm2
    movss [rdi + G3_SY], xmm1
    ret

; only rax rdx and xmm change, the mesh loop counts on that
g3_vertex:
    cmp byte [g3_raw], 0
    jne .camera
    movss xmm5, xmm0
    movss xmm6, xmm1
    movss xmm7, xmm2
    xor eax, eax
    xor edx, edx
.row:
    movss xmm0, [g3_matrix + rax]
    mulss xmm0, xmm5
    movss xmm1, [g3_matrix + rax + 4]
    mulss xmm1, xmm6
    addss xmm0, xmm1
    movss xmm1, [g3_matrix + rax + 8]
    mulss xmm1, xmm7
    addss xmm0, xmm1
    addss xmm0, [g3_matrix + rax + 12]
    movss [rdi + rdx * 4], xmm0
    add eax, 16
    inc edx
    cmp edx, 3
    jb .row
    jmp .placed
.camera:
    movss [rdi + G3_X], xmm0
    movss [rdi + G3_Y], xmm1
    movss [rdi + G3_Z], xmm2
.placed:
    cmp byte [g3_reflecting], 0
    je .project
    call g3_reflect
.project:
    movss xmm0, [rdi + G3_Z]
    comiss xmm0, [g3_near]
    jae g3_project
    ret

g3_reflect:
    xorps xmm3, xmm3
    xor eax, eax
.dot:
    movss xmm0, [rdi + rax * 4]
    subss xmm0, [g3_mirror_point + rax * 4]
    mulss xmm0, [g3_mirror_normal + rax * 4]
    addss xmm3, xmm0
    inc eax
    cmp eax, 3
    jb .dot
    addss xmm3, xmm3
    xor eax, eax
.move:
    movss xmm0, [g3_mirror_normal + rax * 4]
    mulss xmm0, xmm3
    movss xmm1, [rdi + rax * 4]
    subss xmm1, xmm0
    movss [rdi + rax * 4], xmm1
    inc eax
    cmp eax, 3
    jb .move
    ret

g3_transform:
    lea rdi, [g3_spot]
    call g3_vertex
    movss xmm0, [g3_spot + G3_X]
    movss xmm1, [g3_spot + G3_Y]
    movss xmm2, [g3_spot + G3_Z]
    movss xmm3, [g3_spot + G3_SX]
    movss xmm4, [g3_spot + G3_SY]
    ret

; rows a0 a1 a2 of the 3x3 part. the inverse has a1xa2, a2xa0, a0xa1 as its columns over
; the determinant, no gauss needed for three rows
g3_unproject:
    subss xmm0, [g3_center_x]
    divss xmm0, [g3_focal]
    movss xmm3, [g3_center_y]
    subss xmm3, xmm1
    divss xmm3, [g3_focal]
    movss xmm1, xmm3
    cmp byte [g3_raw], 0
    jne .done
    subss xmm0, [g3_matrix + 12]
    movss [g3_aim], xmm0
    subss xmm1, [g3_matrix + 28]
    movss [g3_aim + 4], xmm1
    subss xmm2, [g3_matrix + 44]
    movss [g3_aim + 8], xmm2
    lea rsi, [g3_matrix + 16]
    lea rdi, [g3_matrix + 32]
    call g3_cross
    movss [g3_inverse], xmm0
    movss [g3_inverse + 4], xmm1
    movss [g3_inverse + 8], xmm2
    lea rsi, [g3_matrix + 32]
    lea rdi, [g3_matrix]
    call g3_cross
    movss [g3_inverse + 12], xmm0
    movss [g3_inverse + 16], xmm1
    movss [g3_inverse + 20], xmm2
    lea rsi, [g3_matrix]
    lea rdi, [g3_matrix + 16]
    call g3_cross
    movss [g3_inverse + 24], xmm0
    movss [g3_inverse + 28], xmm1
    movss [g3_inverse + 32], xmm2
    movss xmm5, [g3_matrix]
    mulss xmm5, [g3_inverse]
    movss xmm0, [g3_matrix + 4]
    mulss xmm0, [g3_inverse + 4]
    addss xmm5, xmm0
    movss xmm0, [g3_matrix + 8]
    mulss xmm0, [g3_inverse + 8]
    addss xmm5, xmm0
    movss xmm0, xmm5
    call g3_abs
    comiss xmm0, [g3_tiny_det]
    xorps xmm0, xmm0
    xorps xmm1, xmm1
    xorps xmm2, xmm2
    jb .done
    xor eax, eax
.axis:
    movss xmm3, [g3_aim]
    mulss xmm3, [g3_inverse + rax * 4]
    movss xmm4, [g3_aim + 4]
    mulss xmm4, [g3_inverse + rax * 4 + 12]
    addss xmm3, xmm4
    movss xmm4, [g3_aim + 8]
    mulss xmm4, [g3_inverse + rax * 4 + 24]
    addss xmm3, xmm4
    divss xmm3, xmm5
    movss [g3_aim + rax * 4 + 12], xmm3
    inc eax
    cmp eax, 3
    jb .axis
    movss xmm0, [g3_aim + 12]
    movss xmm1, [g3_aim + 16]
    movss xmm2, [g3_aim + 20]
.done:
    ret

g3_mirror:
    mov byte [g3_mirror_mode], 0
    test edi, edi
    jz .done
    push rbx
    push r12
    mov r12, rsi
    mov ebx, edi
    mov byte [g3_reflecting], 0
    lea rdi, [g3_mirror_corners]
.corner:
    movss xmm0, [r12]
    movss xmm1, [r12 + 4]
    movss xmm2, [r12 + 8]
    call g3_vertex
    add r12, 12
    add rdi, G3_VERTEX_BYTES
    lea rax, [g3_mirror_corners + G3_VERTEX_BYTES * 3]
    cmp rdi, rax
    jb .corner
    xor eax, eax
.side:
    movss xmm0, [g3_mirror_corners + rax * 4]
    subss xmm0, [g3_mirror_corners + G3_VERTEX_BYTES + rax * 4]
    movss [g3_u + rax * 4], xmm0
    movss xmm0, [g3_mirror_corners + G3_VERTEX_BYTES * 2 + rax * 4]
    subss xmm0, [g3_mirror_corners + G3_VERTEX_BYTES + rax * 4]
    movss [g3_v + rax * 4], xmm0
    movss xmm0, [g3_mirror_corners + rax * 4]
    movss [g3_mirror_point + rax * 4], xmm0
    inc eax
    cmp eax, 3
    jb .side
    lea rsi, [g3_u]
    lea rdi, [g3_v]
    call g3_cross
    movss xmm3, xmm0
    mulss xmm3, xmm0
    movss xmm4, xmm1
    mulss xmm4, xmm1
    addss xmm3, xmm4
    movss xmm4, xmm2
    mulss xmm4, xmm2
    addss xmm3, xmm4
    sqrtss xmm3, xmm3
    comiss xmm3, [g3_tiny_det]
    jb .flat
    divss xmm0, xmm3
    divss xmm1, xmm3
    divss xmm2, xmm3
    movss [g3_mirror_normal], xmm0
    movss [g3_mirror_normal + 4], xmm1
    movss [g3_mirror_normal + 8], xmm2
    mov [g3_mirror_mode], bl
.flat:
    pop r12
    pop rbx
.done:
    ret

g3_twice:
    push rbx
    mov rbx, rax
    test byte [g3_mirror_mode], G3_MIRROR
    jz .real
    mov byte [g3_reflecting], 1
    call rbx
    mov byte [g3_reflecting], 0
    test byte [g3_mirror_mode], 2
    jnz .done
.real:
    call rbx
.done:
    pop rbx
    ret

g3_draw_mesh:
    mov [g3_mesh_ptr], rdi
    mov [g3_mesh_flags], esi
    mov dword [g3_drawn], 0
    lea rax, [g3_mesh_pass]
    call g3_twice
    mov eax, [g3_drawn]
    ret

g3_mesh_pass:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov rdi, [g3_mesh_ptr]
    mov ebp, [g3_mesh_flags]
    cmp byte [rdi], G3_SPRITE_MESH
    je .sprite
    cmp dword [rdi], G3_MESH_MAGIC
    jne .done
    mov byte [g3_mesh_kind], 0
    movzx r13d, word [rdi + 4]
    movzx ebx, word [rdi + 6]
    lea r12, [rdi + 8]
    jmp .counted
.sprite:
    mov byte [g3_mesh_kind], 1
    mov r13d, [rdi + 1]
    mov ebx, [rdi + 5]
    lea r12, [rdi + 9]
.counted:
    cmp r13d, G3_VERTEX_MAX
    ja .done
    mov [g3_vertex_count], r13d
    imul eax, r13d, 12
    lea r14, [r12 + rax]
    imul eax, ebx, 6
    lea r15, [r14 + rax]
    lea rdi, [g3_vertices]
    test r13d, r13d
    jz .triangles

.vertex:
    call g3_mesh_vertex
    call g3_vertex
    add r12, 12
    add rdi, G3_VERTEX_BYTES
    dec r13d
    jnz .vertex

.triangles:
    test ebx, ebx
    jz .done
.triangle:
    mov edx, ebp
    xor ecx, ecx
    cmp byte [g3_mesh_kind], 0
    jne .sprite_corner
.corner:
    movzx eax, word [r14 + rcx * 2]
    call g3_mesh_corner
    jc .next
    inc ecx
    cmp ecx, 3
    jb .corner
    movzx eax, byte [r15]
    jmp .draw
.sprite_corner:
    mov eax, [r14 + rcx * 4 + 4]
    call g3_mesh_corner
    jc .next
    inc ecx
    cmp ecx, 3
    jb .sprite_corner
    mov eax, [r14]
    test eax, G3_ROP_HALF_RANGE
    jz .full_range
    or edx, G3_HALF_RANGE
.full_range:
    test eax, G3_ROP_TWO_SIDED
    jz .draw
    or edx, G3_TWO_SIDED
.draw:
    call g3_triangle
.next:
    cmp byte [g3_mesh_kind], 0
    jne .sprite_next
    add r14, 6
    inc r15
    jmp .counting
.sprite_next:
    add r14, 16
.counting:
    dec ebx
    jnz .triangle

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_mesh_vertex:
    cmp byte [g3_mesh_kind], 0
    jne .whole
    movss xmm0, [r12]
    movss xmm1, [r12 + 4]
    movss xmm2, [r12 + 8]
    ret
.whole:
    mov eax, [r12]
    cvtsi2ss xmm0, eax
    mov eax, [r12 + 4]
    cvtsi2ss xmm1, eax
    mov eax, [r12 + 8]
    cvtsi2ss xmm2, eax
    ret

g3_mesh_corner:
    cmp eax, [g3_vertex_count]
    jae .bad
    shl eax, 5
    lea rax, [g3_vertices + rax]
    mov [g3_corners + rcx * 8], rax
    clc
    ret
.bad:
    stc
    ret

g3_mesh_check:
    xor eax, eax
    cmp esi, 9
    jb .done
    cmp byte [rdi], G3_SPRITE_MESH
    je .sprite
    cmp dword [rdi], G3_MESH_MAGIC
    jne .done
    movzx eax, word [rdi + 4]
    imul eax, eax, 12
    movzx ecx, word [rdi + 6]
    imul ecx, ecx, 7
    lea eax, [rax + rcx + 8]
    jmp .fits
.sprite:
    mov eax, [rdi + 1]
    cmp eax, G3_VERTEX_MAX
    ja .bad
    mov ecx, [rdi + 5]
    cmp ecx, 0x100000
    ja .bad
    imul eax, eax, 12
    shl ecx, 4
    lea eax, [rax + rcx + 10]
.fits:
    cmp eax, esi
    jbe .done
.bad:
    xor eax, eax
.done:
    ret

; meshes are in pixels, this puts any of them in the unit ball. call it last
g3_fit:
    push r12
    mov r12, rdi
    movss xmm0, [g3_big]
    xorps xmm1, xmm1
    subss xmm1, xmm0
    xor eax, eax
.reset:
    movss [g3_low + rax * 4], xmm0
    movss [g3_high + rax * 4], xmm1
    inc eax
    cmp eax, 3
    jb .reset
    mov byte [g3_fit_pass], 0
    call g3_fit_walk
    jc .done
    xor eax, eax
.middle:
    movss xmm0, [g3_low + rax * 4]
    addss xmm0, [g3_high + rax * 4]
    mulss xmm0, [g3_half]
    movss [g3_middle + rax * 4], xmm0
    inc eax
    cmp eax, 3
    jb .middle
    xorps xmm0, xmm0
    movss [g3_reach], xmm0
    mov byte [g3_fit_pass], 1
    call g3_fit_walk
    movss xmm0, [g3_reach]
    sqrtss xmm0, xmm0
    comiss xmm0, [g3_tiny]
    jbe .done
    movss xmm1, [g3_one]
    divss xmm1, xmm0
    movss xmm0, xmm1
    movss xmm2, xmm1
    call g3_scale
    xorps xmm0, xmm0
    subss xmm0, [g3_middle]
    xorps xmm1, xmm1
    subss xmm1, [g3_middle + 4]
    xorps xmm2, xmm2
    subss xmm2, [g3_middle + 8]
    call g3_translate
.done:
    pop r12
    ret

g3_fit_walk:
    push r12
    push r13
    mov rdi, r12
    cmp byte [rdi], G3_SPRITE_MESH
    je .sprite
    cmp dword [rdi], G3_MESH_MAGIC
    jne .bad
    mov byte [g3_mesh_kind], 0
    movzx r13d, word [rdi + 4]
    lea r12, [rdi + 8]
    jmp .walk
.sprite:
    mov byte [g3_mesh_kind], 1
    mov r13d, [rdi + 1]
    lea r12, [rdi + 9]
.walk:
    test r13d, r13d
    jz .bad
    cmp r13d, G3_VERTEX_MAX
    ja .bad
.vertex:
    call g3_mesh_vertex
    movss [g3_aim], xmm0
    movss [g3_aim + 4], xmm1
    movss [g3_aim + 8], xmm2
    xor eax, eax
    cmp byte [g3_fit_pass], 0
    jne .reach
.axis:
    movss xmm0, [g3_aim + rax * 4]
    comiss xmm0, [g3_low + rax * 4]
    jae .not_lower
    movss [g3_low + rax * 4], xmm0
.not_lower:
    comiss xmm0, [g3_high + rax * 4]
    jbe .not_higher
    movss [g3_high + rax * 4], xmm0
.not_higher:
    inc eax
    cmp eax, 3
    jb .axis
    jmp .next
.reach:
    xorps xmm1, xmm1
.square:
    movss xmm0, [g3_aim + rax * 4]
    subss xmm0, [g3_middle + rax * 4]
    mulss xmm0, xmm0
    addss xmm1, xmm0
    inc eax
    cmp eax, 3
    jb .square
    comiss xmm1, [g3_reach]
    jbe .next
    movss [g3_reach], xmm1
.next:
    add r12, 12
    dec r13d
    jnz .vertex
    clc
    pop r13
    pop r12
    ret
.bad:
    stc
    pop r13
    pop r12
    ret

g3_triangle:
    push rbx
    push r12
    mov ebx, eax
    mov r12d, edx
    mov rsi, [g3_corners]
    mov rdi, [g3_corners + 8]
    mov rdx, [g3_corners + 16]
    xor ecx, ecx
.sides:
    movss xmm0, [rdi + rcx * 4]
    subss xmm0, [rsi + rcx * 4]
    movss [g3_u + rcx * 4], xmm0
    movss xmm0, [rdx + rcx * 4]
    subss xmm0, [rsi + rcx * 4]
    movss [g3_v + rcx * 4], xmm0
    inc ecx
    cmp ecx, 3
    jb .sides
    lea rsi, [g3_u]
    lea rdi, [g3_v]
    call g3_cross

    cmp byte [g3_reflecting], 0
    je .facing
    xorps xmm3, xmm3                ; mirrored faces wind the other way round
    subss xmm3, xmm0
    movss xmm0, xmm3
    xorps xmm3, xmm3
    subss xmm3, xmm1
    movss xmm1, xmm3
    xorps xmm3, xmm3
    subss xmm3, xmm2
    movss xmm2, xmm3
.facing:
    test r12d, G3_TWO_SIDED + G3_NO_CULL
    jnz .lit
    xorps xmm4, xmm4
    comiss xmm2, xmm4
    jae .done

.lit:
    movss xmm3, xmm0
    mulss xmm3, xmm0
    movss xmm4, xmm1
    mulss xmm4, xmm1
    addss xmm3, xmm4
    movss xmm4, xmm2
    mulss xmm4, xmm2
    addss xmm3, xmm4
    sqrtss xmm3, xmm3
    xorps xmm4, xmm4
    comiss xmm3, xmm4
    jbe .done
    mulss xmm0, [g3_light_dir]
    mulss xmm1, [g3_light_dir + 4]
    mulss xmm2, [g3_light_dir + 8]
    addss xmm0, xmm1
    addss xmm0, xmm2
    divss xmm0, xmm3
    mulss xmm0, [g3_chance_scale]
    test r12d, G3_TWO_SIDED
    jz .one_side
    call g3_abs
    addss xmm0, xmm0
    jmp .scaled
.one_side:
    addss xmm0, [g3_chance_scale]
.scaled:
    cvttss2si eax, xmm0
    and ebx, 15
    test r12d, G3_HALF_RANGE
    jz .ranged
    sar eax, 1
    cmp ebx, 8
    jb .ranged
    sub ebx, 8
    add eax, 0x10000
.ranged:
    lea ecx, [rbx + PALETTE_FIXED]
    mov [g3_wire_color], cl
    cmp eax, 0x10000
    jae .bright
    test eax, eax
    jns .dim
    xor eax, eax
.dim:
    mov byte [g3_dark], PALETTE_FIXED
    mov [g3_bright], cl
    jmp .chance
.bright:
    sub eax, 0x10000
    cmp eax, 0x10000
    jbe .top
    mov eax, 0x10000
.top:
    mov [g3_dark], cl
    xor ebx, 8
    add ebx, PALETTE_FIXED
    mov [g3_bright], bl
.chance:
    mov [g3_chance], eax

    test r12d, G3_WIRE
    jnz .wire
    call g3_flat
    jmp .done

.wire:
    call g3_clip
    cmp eax, 3
    jb .done
    inc dword [g3_drawn]
    mov ebx, eax
    lea r12, [g3_polygon]
.edge:
    mov rsi, r12
    lea rdi, [r12 + G3_VERTEX_BYTES]
    dec ebx
    jnz .line
    lea rdi, [g3_polygon]
.line:
    call g3_line
    add r12, G3_VERTEX_BYTES
    test ebx, ebx
    jnz .edge

.done:
    pop r12
    pop rbx
    ret

g3_flat:
    push rbx
    call g3_clip
    cmp eax, 3
    jb .done
    inc dword [g3_drawn]
    mov ebx, eax
    lea rsi, [g3_polygon]
    lea rdi, [g3_polygon + G3_VERTEX_BYTES]
    lea rdx, [g3_polygon + G3_VERTEX_BYTES * 2]
    call g3_fill
    cmp ebx, 4
    jb .done
    lea rsi, [g3_polygon]
    lea rdi, [g3_polygon + G3_VERTEX_BYTES * 2]
    lea rdx, [g3_polygon + G3_VERTEX_BYTES * 3]
    call g3_fill
.done:
    pop rbx
    ret

; near plane only, the scan loops clip at the screen edges. out: eax = 0, 3 or 4 corners
g3_clip:
    push rbx
    push r12
    lea rdi, [g3_polygon]
    xor ebx, ebx
.corner:
    mov rsi, [g3_corners + rbx * 8]
    lea eax, [rbx + 1]
    cmp eax, 3
    jb .follow
    xor eax, eax
.follow:
    mov r12, [g3_corners + rax * 8]
    movss xmm0, [rsi + G3_Z]
    comiss xmm0, [g3_near]
    jb .outside
    push rsi
    mov ecx, G3_VERTEX_BYTES
    rep movsb
    pop rsi
    movss xmm1, [r12 + G3_Z]
    comiss xmm1, [g3_near]
    jb .cross
    jmp .next
.outside:
    movss xmm1, [r12 + G3_Z]
    comiss xmm1, [g3_near]
    jb .next
.cross:
    movss xmm2, [g3_near]
    subss xmm2, xmm0
    subss xmm1, xmm0
    divss xmm2, xmm1
    xor ecx, ecx
.blend:
    movss xmm0, [r12 + rcx * 4]
    subss xmm0, [rsi + rcx * 4]
    mulss xmm0, xmm2
    addss xmm0, [rsi + rcx * 4]
    movss [rdi + rcx * 4], xmm0
    inc ecx
    cmp ecx, 3
    jb .blend
    movss xmm0, [g3_near]
    movss [rdi + G3_Z], xmm0
    call g3_project
    add rdi, G3_VERTEX_BYTES
.next:
    inc ebx
    cmp ebx, 3
    jb .corner
    lea rax, [g3_polygon]
    sub rdi, rax
    shr edi, 5
    mov eax, edi
    pop r12
    pop rbx
    ret

g3_ceil:
    comiss xmm0, [g3_minus_one]
    jae .low
    movss xmm0, [g3_minus_one]
.low:
    comiss xmm0, xmm1
    jbe .high
    movss xmm0, xmm1
.high:
    cvttss2si eax, xmm0
    cvtsi2ss xmm2, eax
    comiss xmm2, xmm0
    jae .done
    inc eax
.done:
    ret

; a pixel is drawn when its center is inside, so two triangles on one edge never both touch
; it. far - z goes in the z buffer so a cleared buffer of zeroes is infinitely far
g3_fill:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    movss xmm0, [rdi + G3_SY]
    comiss xmm0, [rsi + G3_SY]
    jae .first
    xchg rsi, rdi
.first:
    movss xmm0, [rdx + G3_SY]
    comiss xmm0, [rdi + G3_SY]
    jae .sorted
    xchg rdi, rdx
    movss xmm0, [rdi + G3_SY]
    comiss xmm0, [rsi + G3_SY]
    jae .sorted
    xchg rsi, rdi
.sorted:
    mov r12, rsi
    mov r13, rdi
    mov r14, rdx

    movss xmm0, [r13 + G3_SX]
    subss xmm0, [r12 + G3_SX]
    movss xmm1, [r13 + G3_SY]
    subss xmm1, [r12 + G3_SY]
    movss xmm2, [r13 + G3_W]
    subss xmm2, [r12 + G3_W]
    movss xmm3, [r14 + G3_SX]
    subss xmm3, [r12 + G3_SX]
    movss xmm4, [r14 + G3_SY]
    subss xmm4, [r12 + G3_SY]
    movss xmm5, [r14 + G3_W]
    subss xmm5, [r12 + G3_W]

    movss xmm6, xmm0
    mulss xmm6, xmm4
    movss xmm7, xmm3
    mulss xmm7, xmm1
    subss xmm6, xmm7
    movss xmm7, xmm6
    comiss xmm7, [g3_tiny]
    ja .area
    xorps xmm7, xmm7
    subss xmm7, xmm6
    comiss xmm7, [g3_tiny]
    jbe .done
.area:
    movss xmm7, xmm2
    mulss xmm7, xmm4
    movss [g3_grad_x], xmm7
    movss xmm7, xmm5
    mulss xmm7, xmm1
    movss [g3_grad_y], xmm7
    movss xmm7, [g3_grad_x]
    subss xmm7, [g3_grad_y]
    divss xmm7, xmm6
    movss [g3_grad_x], xmm7
    movss xmm7, xmm0
    mulss xmm7, xmm5
    mulss xmm2, xmm3
    subss xmm7, xmm2
    divss xmm7, xmm6
    movss [g3_grad_y], xmm7

    divss xmm3, xmm4
    movss [g3_slope_long], xmm3
    xorps xmm7, xmm7
    comiss xmm1, xmm7
    jbe .flat_top
    divss xmm0, xmm1
    movss xmm7, xmm0
.flat_top:
    movss [g3_slope_upper], xmm7
    movss xmm0, [r14 + G3_SX]
    subss xmm0, [r13 + G3_SX]
    movss xmm1, [r14 + G3_SY]
    subss xmm1, [r13 + G3_SY]
    xorps xmm7, xmm7
    comiss xmm1, xmm7
    jbe .flat_bottom
    divss xmm0, xmm1
    movss xmm7, xmm0
.flat_bottom:
    movss [g3_slope_lower], xmm7

    movss xmm0, [r14 + G3_SY]
    subss xmm0, [g3_half]
    movss xmm1, [g3_height_f]
    call g3_ceil
    cmp eax, [g3_height]
    jle .last_row
    mov eax, [g3_height]
.last_row:
    mov [g3_row_end], eax
    movss xmm0, [r12 + G3_SY]
    subss xmm0, [g3_half]
    movss xmm1, [g3_height_f]
    call g3_ceil
    test eax, eax
    jns .first_row
    xor eax, eax
.first_row:
    mov ebp, eax

.row:
    cmp ebp, [g3_row_end]
    jge .done
    cvtsi2ss xmm3, ebp
    addss xmm3, [g3_half]
    movss xmm4, xmm3
    subss xmm4, [r12 + G3_SY]
    mulss xmm4, [g3_slope_long]
    addss xmm4, [r12 + G3_SX]
    comiss xmm3, [r13 + G3_SY]
    jae .lower
    movss xmm5, xmm3
    subss xmm5, [r12 + G3_SY]
    mulss xmm5, [g3_slope_upper]
    addss xmm5, [r12 + G3_SX]
    jmp .span
.lower:
    movss xmm5, xmm3
    subss xmm5, [r13 + G3_SY]
    mulss xmm5, [g3_slope_lower]
    addss xmm5, [r13 + G3_SX]
.span:
    comiss xmm4, xmm5
    jbe .ordered
    movss xmm6, xmm4
    movss xmm4, xmm5
    movss xmm5, xmm6
.ordered:
    movss xmm0, xmm4
    subss xmm0, [g3_half]
    movss xmm1, [g3_width_f]
    call g3_ceil
    test eax, eax
    jns .left
    xor eax, eax
.left:
    mov ebx, eax
    movss xmm0, xmm5
    subss xmm0, [g3_half]
    movss xmm1, [g3_width_f]
    call g3_ceil
    cmp eax, [g3_width]
    jle .right
    mov eax, [g3_width]
.right:
    mov ecx, eax
    sub ecx, ebx
    jle .next_row

    cvtsi2ss xmm0, ebx
    addss xmm0, [g3_half]
    subss xmm0, [r12 + G3_SX]
    mulss xmm0, [g3_grad_x]
    movss xmm1, xmm3
    subss xmm1, [r12 + G3_SY]
    mulss xmm1, [g3_grad_y]
    addss xmm0, xmm1
    addss xmm0, [r12 + G3_W]
    movss xmm1, [g3_grad_x]
    mov eax, ebp
    imul eax, [g3_stride]
    add eax, ebx
    mov rdi, [g3_pixels]
    add rdi, rax
    mov eax, ebp
    imul eax, [g3_width]
    add eax, ebx
    mov rsi, [g3_zbuffer]
    lea rsi, [rsi + rax * 4]
    movzx r8d, byte [g3_dark]
    movzx r10d, byte [g3_bright]
    mov r11d, [g3_chance]
    cmp r11d, G3_CHECKER
    je .checker
    mov r9d, [g3_seed]
.pixel:
    comiss xmm0, [rsi]
    jbe .hidden
    movss [rsi], xmm0
    mov eax, r9d
    shl eax, 13
    xor r9d, eax
    mov eax, r9d
    shr eax, 17
    xor r9d, eax
    mov eax, r9d
    shl eax, 5
    xor r9d, eax
    mov eax, r9d
    shr eax, 16
    mov edx, r8d
    cmp eax, r11d
    cmovb edx, r10d
    mov [rdi], dl
.hidden:
    addss xmm0, xmm1
    inc rdi
    add rsi, 4
    dec ecx
    jnz .pixel
    mov [g3_seed], r9d
    jmp .next_row

.checker:
    lea eax, [rbx + rbp]
    and eax, 1
.square:
    comiss xmm0, [rsi]
    jbe .covered
    movss [rsi], xmm0
    mov edx, r8d
    test eax, eax
    cmovnz edx, r10d
    mov [rdi], dl
.covered:
    xor eax, 1
    addss xmm0, xmm1
    inc rdi
    add rsi, 4
    dec ecx
    jnz .square

.next_row:
    inc ebp
    jmp .row

.done:
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_coord:
    comiss xmm0, [g3_far_left]
    jae .low
    movss xmm0, [g3_far_left]
.low:
    comiss xmm0, [g3_far_right]
    jbe .high
    movss xmm0, [g3_far_right]
.high:
    cvttss2si eax, xmm0
    ret

g3_line:
    push rbx
    push r12
    push r13
    movss xmm0, [rsi + G3_SX]
    call g3_coord
    mov r8d, eax
    movss xmm0, [rsi + G3_SY]
    call g3_coord
    mov r9d, eax
    movss xmm0, [rdi + G3_SX]
    call g3_coord
    mov r10d, eax
    movss xmm0, [rdi + G3_SY]
    call g3_coord
    mov r11d, eax

    mov eax, r10d
    sub eax, r8d
    mov r12d, 1
    jns .right
    neg eax
    neg r12d
.right:
    mov ebx, eax
    mov eax, r11d
    sub eax, r9d
    mov r13d, 1
    jns .down
    neg eax
    neg r13d
.down:
    neg eax
    mov edx, eax
    lea ecx, [rbx + rdx]
.plot:
    cmp r8d, [g3_width]
    jae .step
    cmp r9d, [g3_height]
    jae .step
    mov esi, r9d
    imul esi, [g3_stride]
    add esi, r8d
    add rsi, [g3_pixels]
    mov al, [g3_wire_color]
    mov [rsi], al
.step:
    cmp r8d, r10d
    jne .move
    cmp r9d, r11d
    je .done
.move:
    lea eax, [rcx + rcx]
    cmp eax, edx
    jl .column
    add ecx, edx
    add r8d, r12d
.column:
    cmp eax, ebx
    jg .plot
    add ecx, ebx
    add r9d, r13d
    jmp .plot
.done:
    pop r13
    pop r12
    pop rbx
    ret

; eax edx = pixel, xmm0 = depth. g3_pixel takes the ink, g3_put the color in r10d
g3_pixel:
    mov r10d, -1

g3_put:
    cmp eax, [g3_width]
    jae .out
    cmp edx, [g3_height]
    jae .out
    mov r8d, edx
    imul r8d, [g3_width]
    add r8d, eax
    mov r9, [g3_zbuffer]
    lea r8, [r9 + r8 * 4]
    cmp byte [g3_no_depth], 0
    jne .drawn
    comiss xmm0, [r8]
    jbe .out
.drawn:
    movss [r8], xmm0
    lea r8d, [rax + rdx]
    imul edx, [g3_stride]
    add edx, eax
    mov r9, [g3_pixels]
    add r9, rdx
    test r10d, r10d
    js .ink
    mov [r9], r10b
.out:
    ret
.ink:
    mov eax, [g3_chance]
    cmp eax, G3_CHECKER
    je .checker
    mov edx, [g3_seed]
    mov r8d, edx
    shl r8d, 13
    xor edx, r8d
    mov r8d, edx
    shr r8d, 17
    xor edx, r8d
    mov r8d, edx
    shl r8d, 5
    xor edx, r8d
    mov [g3_seed], edx
    shr edx, 16
    cmp edx, eax
    jb .bright
.dark:
    mov al, [g3_dark]
    mov [r9], al
    ret
.checker:
    test r8d, 1
    jz .dark
.bright:
    mov al, [g3_bright]
    mov [r9], al
    ret

g3_dot:
    push rbx
    push r12
    push r13
    push r14
    movss xmm0, [rdi + G3_SX]
    call g3_coord
    mov r12d, eax
    movss xmm0, [rdi + G3_SY]
    call g3_coord
    mov r13d, eax
    mov eax, [g3_pen]
    shr eax, 1
    sub r12d, eax
    sub r13d, eax
    movss xmm0, [rdi + G3_W]
    xor ebx, ebx
.row:
    xor r14d, r14d
.column:
    lea eax, [r12 + r14]
    lea edx, [rbx + r13]
    call g3_pixel
    inc r14d
    cmp r14d, [g3_pen]
    jb .column
    inc ebx
    cmp ebx, [g3_pen]
    jb .row
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

g3_seg:
    push rbx
    push rbp
    push r12
    push r13
    mov r12, rsi
    mov r13, rdi
    movss xmm0, [r13 + G3_SX]
    subss xmm0, [r12 + G3_SX]
    movss [g3_slope], xmm0
    movss xmm0, [r13 + G3_SY]
    subss xmm0, [r12 + G3_SY]
    movss [g3_slope + 4], xmm0
    movss xmm0, [r13 + G3_W]
    subss xmm0, [r12 + G3_W]
    movss [g3_slope + 8], xmm0
    movss xmm0, [g3_slope]
    call g3_abs
    movss xmm1, xmm0
    movss xmm0, [g3_slope + 4]
    call g3_abs
    comiss xmm0, xmm1
    jae .longer
    movss xmm0, xmm1
.longer:
    comiss xmm0, [g3_far_right]
    jbe .count
    movss xmm0, [g3_far_right]
.count:
    cvttss2si ebx, xmm0
    inc ebx
    cvtsi2ss xmm1, ebx
    movss xmm0, [g3_one]
    divss xmm0, xmm1
    movss [g3_step], xmm0
    xor ebp, ebp
.dot:
    cvtsi2ss xmm7, ebp
    mulss xmm7, [g3_step]
    movss xmm0, [g3_slope]
    mulss xmm0, xmm7
    addss xmm0, [r12 + G3_SX]
    movss [g3_spot + G3_SX], xmm0
    movss xmm0, [g3_slope + 4]
    mulss xmm0, xmm7
    addss xmm0, [r12 + G3_SY]
    movss [g3_spot + G3_SY], xmm0
    movss xmm0, [g3_slope + 8]
    mulss xmm0, xmm7
    addss xmm0, [r12 + G3_W]
    movss [g3_spot + G3_W], xmm0
    lea rdi, [g3_spot]
    call g3_dot
    inc ebp
    cmp ebp, ebx
    jbe .dot
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_cut:
    movss xmm2, [g3_near]
    subss xmm2, [rsi + G3_Z]
    movss xmm1, [rdi + G3_Z]
    subss xmm1, [rsi + G3_Z]
    divss xmm2, xmm1
    xor ecx, ecx
.blend:
    movss xmm0, [rdi + rcx * 4]
    subss xmm0, [rsi + rcx * 4]
    mulss xmm0, xmm2
    addss xmm0, [rsi + rcx * 4]
    movss [rsi + rcx * 4], xmm0
    inc ecx
    cmp ecx, 3
    jb .blend
    movss xmm0, [g3_near]
    movss [rsi + G3_Z], xmm0
    push rdi
    mov rdi, rsi
    call g3_project
    pop rdi
    ret

g3_segment3:
    movss xmm0, [rsi + G3_Z]
    comiss xmm0, [g3_near]
    jae .first
    movss xmm0, [rdi + G3_Z]
    comiss xmm0, [g3_near]
    jb .gone
    call g3_cut
    jmp g3_seg
.first:
    movss xmm0, [rdi + G3_Z]
    comiss xmm0, [g3_near]
    jae g3_seg
    xchg rsi, rdi
    call g3_cut
    xchg rsi, rdi
    jmp g3_seg
.gone:
    ret

g3_point:
    movss [g3_args], xmm0
    movss [g3_args + 4], xmm1
    movss [g3_args + 8], xmm2
    lea rax, [g3_point_pass]
    jmp g3_twice

g3_point_pass:
    call g3_use_ink
    movss xmm0, [g3_args]
    movss xmm1, [g3_args + 4]
    movss xmm2, [g3_args + 8]
    lea rdi, [g3_spot]
    call g3_vertex
    movss xmm0, [g3_spot + G3_Z]
    comiss xmm0, [g3_near]
    jae g3_dot
    ret

g3_keep_line:
    movss [g3_args], xmm0
    movss [g3_args + 4], xmm1
    movss [g3_args + 8], xmm2
    movss [g3_args + 12], xmm3
    movss [g3_args + 16], xmm4
    movss [g3_args + 20], xmm5
    ret

g3_line3:
    call g3_keep_line
    lea rax, [g3_line_pass]
    jmp g3_twice

g3_line_pass:
    call g3_use_ink
    movss xmm0, [g3_args]
    movss xmm1, [g3_args + 4]
    movss xmm2, [g3_args + 8]
    lea rdi, [g3_ends]
    call g3_vertex
    movss xmm0, [g3_args + 12]
    movss xmm1, [g3_args + 16]
    movss xmm2, [g3_args + 20]
    lea rdi, [g3_ends + G3_VERTEX_BYTES]
    call g3_vertex
    lea rsi, [g3_ends]
    lea rdi, [g3_ends + G3_VERTEX_BYTES]
    jmp g3_segment3

g3_arrow:
    call g3_keep_line
    lea rax, [g3_arrow_pass]
    jmp g3_twice

; 2.75 * thick is a smudge at thick 1, so one more thick
g3_arrow_pass:
    call g3_line_pass
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_Z]
    comiss xmm0, [g3_near]
    jb .done
    movss xmm5, [g3_ends + G3_VERTEX_BYTES + G3_SX]
    subss xmm5, [g3_ends + G3_SX]
    movss xmm6, [g3_ends + G3_VERTEX_BYTES + G3_SY]
    subss xmm6, [g3_ends + G3_SY]
    movss xmm7, xmm5
    mulss xmm7, xmm5
    movss xmm0, xmm6
    mulss xmm0, xmm6
    addss xmm7, xmm0
    sqrtss xmm7, xmm7
    comiss xmm7, [g3_tiny]
    jbe .done
    mov eax, [g3_pen]
    inc eax
    cvtsi2ss xmm0, eax
    mulss xmm0, [g3_arrow_size]
    divss xmm0, xmm7
    mulss xmm5, xmm0
    mulss xmm6, xmm0
    movss [g3_arrow_side], xmm5
    movss [g3_arrow_side + 4], xmm6
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_W]
    movss [g3_head + G3_W], xmm0
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_SX]
    subss xmm0, xmm5
    addss xmm0, xmm6
    movss [g3_head + G3_SX], xmm0
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_SY]
    subss xmm0, xmm6
    subss xmm0, xmm5
    movss [g3_head + G3_SY], xmm0
    lea rsi, [g3_head]
    lea rdi, [g3_ends + G3_VERTEX_BYTES]
    call g3_seg
    movss xmm5, [g3_arrow_side]
    movss xmm6, [g3_arrow_side + 4]
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_SX]
    subss xmm0, xmm5
    subss xmm0, xmm6
    movss [g3_head + G3_SX], xmm0
    movss xmm0, [g3_ends + G3_VERTEX_BYTES + G3_SY]
    subss xmm0, xmm6
    addss xmm0, xmm5
    movss [g3_head + G3_SY], xmm0
    lea rsi, [g3_head]
    lea rdi, [g3_ends + G3_VERTEX_BYTES]
    jmp g3_seg
.done:
    ret

g3_points:
    push rbx
    push r12
    mov r12, rsi
    mov ebx, ecx
    test ebx, ebx
    jz .done
.next:
    movss xmm0, [r12]
    movss xmm1, [r12 + 4]
    movss xmm2, [r12 + 8]
    call g3_point
    add r12, 12
    dec ebx
    jnz .next
.done:
    pop r12
    pop rbx
    ret

g3_polyline:
    push rbx
    push r12
    mov r12, rsi
    mov ebx, ecx
    cmp ebx, 2
    jb .done
    dec ebx
.next:
    movss xmm0, [r12]
    movss xmm1, [r12 + 4]
    movss xmm2, [r12 + 8]
    movss xmm3, [r12 + 12]
    movss xmm4, [r12 + 16]
    movss xmm5, [r12 + 20]
    call g3_line3
    add r12, 12
    dec ebx
    jnz .next
.done:
    pop r12
    pop rbx
    ret

g3_poly:
    mov [g3_shape_src], rsi
    mov [g3_shape_count], ecx
    lea rax, [g3_poly_pass]
    jmp g3_twice

g3_poly_pass:
    push rbx
    push r12
    push r13
    call g3_use_ink
    mov ebx, [g3_shape_count]
    cmp ebx, 3
    jb .done
    cmp ebx, G3_SHAPE_MAX
    jbe .counted
    mov ebx, G3_SHAPE_MAX
.counted:
    mov r12, [g3_shape_src]
    lea rdi, [g3_shape]
    mov r13d, ebx
.load:
    movss xmm0, [r12]
    movss xmm1, [r12 + 4]
    movss xmm2, [r12 + 8]
    call g3_vertex
    add r12, 12
    add rdi, G3_VERTEX_BYTES
    dec r13d
    jnz .load
    mov r13d, 1
.fan:
    lea rax, [g3_shape]
    mov [g3_corners], rax
    mov eax, r13d
    shl eax, 5
    lea rax, [g3_shape + rax]
    mov [g3_corners + 8], rax
    add rax, G3_VERTEX_BYTES
    mov [g3_corners + 16], rax
    call g3_flat
    inc r13d
    lea eax, [r13 + 1]
    cmp eax, ebx
    jb .fan
.done:
    pop r13
    pop r12
    pop rbx
    ret

g3_rect:
    movss xmm5, xmm0
    addss xmm5, xmm3
    movss xmm6, xmm1
    addss xmm6, xmm4
    movss [g3_box], xmm0
    movss [g3_box + 4], xmm1
    movss [g3_box + 8], xmm2
    movss [g3_box + 12], xmm5
    movss [g3_box + 16], xmm1
    movss [g3_box + 20], xmm2
    movss [g3_box + 24], xmm5
    movss [g3_box + 28], xmm6
    movss [g3_box + 32], xmm2
    movss [g3_box + 36], xmm0
    movss [g3_box + 40], xmm6
    movss [g3_box + 44], xmm2
    lea rsi, [g3_box]
    mov ecx, 4
    jmp g3_poly

g3_circle:
    movss xmm4, xmm3
    xorps xmm5, xmm5

g3_ellipse:
    xor ecx, ecx

g3_regpoly:
    xorps xmm6, xmm6
    xorps xmm7, xmm7

; center + turn * (rx cos t, ry sin t, 0). 48 pieces for a round one, one per side otherwise
g3_arc:
    push rbx
    push r12
    movss [g3_arc_args], xmm0
    movss [g3_arc_args + 4], xmm1
    movss [g3_arc_args + 8], xmm2
    movss [g3_arc_args + 12], xmm3
    movss [g3_arc_args + 16], xmm4
    movss [g3_arc_args + 24], xmm6
    xorps xmm0, xmm0
    comiss xmm7, xmm0
    ja .length
    movss xmm7, [g3_two_pi]
.length:
    test ecx, ecx
    jnz .sides
    movss xmm0, xmm7
    mulss xmm0, [g3_round_rate]
    cvttss2si ebx, xmm0
    inc ebx
    cvtsi2ss xmm1, ebx
    movss xmm0, xmm7
    divss xmm0, xmm1
    jmp .stepped
.sides:
    cvtsi2ss xmm1, ecx
    movss xmm0, [g3_two_pi]
    divss xmm0, xmm1
    movss xmm2, xmm7
    divss xmm2, xmm0
    addss xmm2, [g3_tiny]
    cvttss2si ebx, xmm2
    cmp ebx, 1
    jge .stepped
    mov ebx, 1
.stepped:
    movss [g3_arc_args + 28], xmm0
    movss xmm0, xmm5
    call g3_sincos
    movss [g3_arc_args + 32], xmm0
    movss [g3_arc_args + 36], xmm1
    xor r12d, r12d
    call g3_arc_point
.next:
    movss xmm0, [g3_arc_at]
    movss xmm1, [g3_arc_at + 4]
    movss xmm2, [g3_arc_at + 8]
    movss [g3_arc_from], xmm0
    movss [g3_arc_from + 4], xmm1
    movss [g3_arc_from + 8], xmm2
    inc r12d
    call g3_arc_point
    movss xmm0, [g3_arc_from]
    movss xmm1, [g3_arc_from + 4]
    movss xmm2, [g3_arc_from + 8]
    movss xmm3, [g3_arc_at]
    movss xmm4, [g3_arc_at + 4]
    movss xmm5, [g3_arc_at + 8]
    call g3_line3
    cmp r12d, ebx
    jb .next
    pop r12
    pop rbx
    ret

g3_arc_point:
    cvtsi2ss xmm0, r12d
    mulss xmm0, [g3_arc_args + 28]
    addss xmm0, [g3_arc_args + 24]
    call g3_sincos
    mulss xmm1, [g3_arc_args + 12]
    mulss xmm0, [g3_arc_args + 16]
    movss xmm2, xmm1
    mulss xmm2, [g3_arc_args + 36]
    movss xmm3, xmm0
    mulss xmm3, [g3_arc_args + 32]
    subss xmm2, xmm3
    addss xmm2, [g3_arc_args]
    movss [g3_arc_at], xmm2
    mulss xmm1, [g3_arc_args + 32]
    mulss xmm0, [g3_arc_args + 36]
    addss xmm1, xmm0
    addss xmm1, [g3_arc_args + 4]
    movss [g3_arc_at + 4], xmm1
    movss xmm0, [g3_arc_args + 8]
    movss [g3_arc_at + 8], xmm0
    ret

; de casteljau, mix neighbours by t until one point is left
g3_casteljau:
    push rcx
    lea rdi, [g3_bez]
    imul ecx, ecx, 12
    rep movsb
    pop rcx
    dec ecx
.level:
    test ecx, ecx
    jz .done
    lea eax, [rcx + rcx * 2]
    xor edx, edx
.mix:
    movss xmm0, [g3_bez + rdx * 4 + 12]
    subss xmm0, [g3_bez + rdx * 4]
    mulss xmm0, xmm7
    addss xmm0, [g3_bez + rdx * 4]
    movss [g3_bez + rdx * 4], xmm0
    inc edx
    cmp edx, eax
    jb .mix
    dec ecx
    jmp .level
.done:
    ret

g3_bezier2:
    mov ecx, 3
    jmp g3_bezier

g3_bezier3:
    mov ecx, 4

; the real bernstein weights. the version i copied the idea from leaves out the 2 and the 3
g3_bezier:
    push rbx
    push r12
    push r13
    mov r12, rsi
    mov r13d, ecx
    movss xmm0, [r12]
    movss [g3_curve_from], xmm0
    movss xmm0, [r12 + 4]
    movss [g3_curve_from + 4], xmm0
    movss xmm0, [r12 + 8]
    movss [g3_curve_from + 8], xmm0
    mov ebx, 1
.next:
    cvtsi2ss xmm7, ebx
    mulss xmm7, [g3_curve_rate]
    mov rsi, r12
    mov ecx, r13d
    call g3_casteljau
    movss xmm0, [g3_curve_from]
    movss xmm1, [g3_curve_from + 4]
    movss xmm2, [g3_curve_from + 8]
    movss xmm3, [g3_bez]
    movss xmm4, [g3_bez + 4]
    movss xmm5, [g3_bez + 8]
    movss [g3_curve_from], xmm3
    movss [g3_curve_from + 4], xmm4
    movss [g3_curve_from + 8], xmm5
    call g3_line3
    inc ebx
    cmp ebx, G3_CURVE_STEPS
    jbe .next
    pop r13
    pop r12
    pop rbx
    ret

g3_bspline2:
    mov r8d, 2
    jmp g3_bspline

g3_bspline3:
    mov r8d, 3

; d-1 points on every side at 1/d steps, joints halfway between their neighbours, then
; one bezier of degree d per side
g3_bspline:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    cmp ecx, 3
    jb .done
    cmp ecx, G3_CURVE_MAX
    jbe .counted
    mov ecx, G3_CURVE_MAX
.counted:
    mov r12, rsi
    mov r13d, ecx
    mov r14d, r8d
    mov ebp, edx
    lea ebx, [rcx - 1]
    test ebp, ebp
    jz .pieces
    mov ebx, ecx
.pieces:
    xor r15d, r15d
.side:
    mov ecx, 1
.third:
    push rcx
    cvtsi2ss xmm7, ecx
    cvtsi2ss xmm0, r14d
    divss xmm7, xmm0
    mov eax, r14d
    imul eax, r15d
    add eax, ecx
    call g3_curve_slot
    mov rdi, rax
    imul eax, r15d, 12
    lea rsi, [r12 + rax]
    lea eax, [r15 + 1]
    cmp eax, r13d
    jb .wrap
    xor eax, eax
.wrap:
    imul eax, eax, 12
    lea rdx, [r12 + rax]
    call g3_mix
    pop rcx
    inc ecx
    cmp ecx, r14d
    jb .third
    inc r15d
    cmp r15d, ebx
    jb .side

    movss xmm7, [g3_half]
    mov r15d, 1
.joint:
    cmp r15d, ebx
    jae .ends
    mov eax, r14d
    imul eax, r15d
    call g3_curve_slot
    mov rdi, rax
    lea rsi, [rax - 12]
    lea rdx, [rax + 12]
    call g3_mix
    inc r15d
    jmp .joint

.ends:
    mov eax, r14d
    imul eax, ebx
    call g3_curve_slot
    mov r15, rax
    test ebp, ebp
    jnz .closed
    lea rdi, [g3_curve]
    mov rsi, r12
    mov ecx, 12
    rep movsb
    mov rdi, r15
    lea eax, [r13 - 1]
    imul eax, eax, 12
    lea rsi, [r12 + rax]
    mov ecx, 12
    rep movsb
    jmp .draw
.closed:
    lea rdi, [g3_curve]
    lea rsi, [g3_curve + 12]
    lea rdx, [r15 - 12]
    call g3_mix
    mov rdi, r15
    lea rsi, [g3_curve]
    mov ecx, 12
    rep movsb

.draw:
    xor r15d, r15d
.piece:
    mov eax, r14d
    imul eax, r15d
    call g3_curve_slot
    mov rsi, rax
    lea ecx, [r14 + 1]
    call g3_bezier
    inc r15d
    cmp r15d, ebx
    jb .piece
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_curve_slot:
    imul eax, eax, 12
    lea rax, [g3_curve + rax]
    ret

g3_print:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov r12, rdx
    mov r15d, ecx
    call g3_use_ink
    lea rdi, [g3_spot]
    call g3_vertex
    movss xmm0, [g3_spot + G3_Z]
    comiss xmm0, [g3_near]
    jb .done
    movss xmm0, [g3_spot + G3_SX]
    call g3_coord
    mov r13d, eax
    movss xmm0, [g3_spot + G3_SY]
    call g3_coord
    mov r14d, eax
    movss xmm0, [g3_spot + G3_W]
    movss [g3_frame + 32], xmm0
    mov rbx, r12
    mov ebp, r13d
.char:
    movzx eax, byte [rbx]
    test eax, eax
    jz .written
    shl eax, 4
    mov rsi, [draw_font]
    add rsi, rax
    xor edi, edi
.glyph_row:
    mov cl, [rsi + rdi]
    xor r11d, r11d
.glyph_column:
    test cl, 0x80
    jz .blank
    lea eax, [r11 + rbp]
    lea edx, [r14 + rdi]
    call g3_pixel
.blank:
    shl cl, 1
    inc r11d
    cmp r11d, 8
    jb .glyph_column
    inc edi
    cmp edi, FONT_HEIGHT_PIXELS
    jb .glyph_row
    add ebp, 8
    inc rbx
    jmp .char

.written:
    sub rbx, r12
    shl ebx, 3
    test r15d, G3_BOX
    jz .diamond
    lea eax, [r13 - 2]
    lea edx, [r14 - 2]
    lea ecx, [r13 + rbx + 1]
    lea r8d, [r14 + FONT_HEIGHT_PIXELS + 1]
    mov [g3_frame], eax
    mov [g3_frame + 4], edx
    mov [g3_frame + 8], ecx
    mov [g3_frame + 12], edx
    mov [g3_frame + 16], ecx
    mov [g3_frame + 20], r8d
    mov [g3_frame + 24], eax
    mov [g3_frame + 28], r8d
    call g3_frame_lines
.diamond:
    test r15d, G3_DIAMOND
    jz .done
    mov eax, ebx
    shr eax, 1
    lea ecx, [rax + r13]
    lea edx, [r14 + FONT_HEIGHT_PIXELS / 2]
    lea r8d, [rax + 12]
    mov eax, ecx
    sub eax, r8d
    mov [g3_frame], eax
    mov [g3_frame + 4], edx
    mov [g3_frame + 8], ecx
    lea eax, [rdx - FONT_HEIGHT_PIXELS]
    mov [g3_frame + 12], eax
    lea eax, [rcx + r8]
    mov [g3_frame + 16], eax
    mov [g3_frame + 20], edx
    mov [g3_frame + 24], ecx
    lea eax, [rdx + FONT_HEIGHT_PIXELS]
    mov [g3_frame + 28], eax
    call g3_frame_lines
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_frame_lines:
    push rbx
    xor ebx, ebx
.corner:
    mov eax, [g3_frame + rbx * 8]
    cvtsi2ss xmm0, eax
    movss [g3_shape + G3_SX], xmm0
    mov eax, [g3_frame + rbx * 8 + 4]
    cvtsi2ss xmm0, eax
    movss [g3_shape + G3_SY], xmm0
    lea eax, [rbx + 1]
    and eax, 3
    mov edx, [g3_frame + rax * 8]
    cvtsi2ss xmm0, edx
    movss [g3_shape + G3_VERTEX_BYTES + G3_SX], xmm0
    mov edx, [g3_frame + rax * 8 + 4]
    cvtsi2ss xmm0, edx
    movss [g3_shape + G3_VERTEX_BYTES + G3_SY], xmm0
    movss xmm0, [g3_frame + 32]
    movss [g3_shape + G3_W], xmm0
    movss [g3_shape + G3_VERTEX_BYTES + G3_W], xmm0
    lea rsi, [g3_shape]
    lea rdi, [g3_shape + G3_VERTEX_BYTES]
    call g3_seg
    inc ebx
    cmp ebx, 4
    jb .corner
    pop rbx
    ret

g3_flood:
    movss [g3_args], xmm0
    movss [g3_args + 4], xmm1
    movss [g3_args + 8], xmm2
    mov [g3_flood_mode], dil
    lea rax, [g3_flood_pass]
    jmp g3_twice

; TODO the seed stack just drops seeds when full, a huge ragged area can come out with holes
g3_flood_pass:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    call g3_use_ink
    movss xmm0, [g3_args]
    movss xmm1, [g3_args + 4]
    movss xmm2, [g3_args + 8]
    lea rdi, [g3_spot]
    call g3_vertex
    movss xmm0, [g3_spot + G3_Z]
    comiss xmm0, [g3_near]
    jb .done
    movss xmm0, [g3_spot + G3_SX]
    call g3_coord
    mov r12d, eax
    movss xmm0, [g3_spot + G3_SY]
    call g3_coord
    mov r13d, eax
    cmp r12d, [g3_width]
    jae .done
    cmp r13d, [g3_height]
    jae .done
    mov eax, r12d
    mov edx, r13d
    call g3_flood_at
    mov al, [rax]
    mov [g3_flood_target], al
    cmp al, [g3_ink]
    je .done
    xor r14d, r14d
    mov eax, r12d
    mov edx, r13d
    call g3_flood_push

.pop:
    test r14d, r14d
    jz .done
    dec r14d
    mov r12d, [g3_flood_stack + r14 * 8]
    mov r13d, [g3_flood_stack + r14 * 8 + 4]
    mov eax, r12d
    mov edx, r13d
    call g3_flood_match
    jnc .pop
    mov ebx, r12d
.left:
    test ebx, ebx
    jz .right_start
    lea eax, [rbx - 1]
    mov edx, r13d
    call g3_flood_match
    jnc .right_start
    dec ebx
    jmp .left
.right_start:
    mov ebp, r12d
.right:
    lea eax, [rbp + 1]
    cmp eax, [g3_width]
    jae .fill
    mov edx, r13d
    call g3_flood_match
    jnc .fill
    inc ebp
    jmp .right
.fill:
    mov eax, ebx
    mov edx, r13d
    call g3_flood_at
    mov rdi, rax
    lea ecx, [rbp + 1]
    sub ecx, ebx
    mov al, [g3_ink]
    rep stosb
    lea r15d, [r13 - 1]
    call g3_flood_row
    lea r15d, [r13 + 1]
    call g3_flood_row
    jmp .pop

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

g3_flood_row:
    cmp r15d, [g3_height]
    jae .done
    push r12
    mov r12d, ebx
.scan:
    cmp r12d, ebp
    jg .scanned
    mov eax, r12d
    mov edx, r15d
    call g3_flood_match
    jnc .skip
    cmp r12d, ebx
    je .seed
    lea eax, [r12 - 1]
    mov edx, r15d
    call g3_flood_match
    jc .skip
.seed:
    mov eax, r12d
    mov edx, r15d
    call g3_flood_push
.skip:
    inc r12d
    jmp .scan
.scanned:
    pop r12
.done:
    ret

g3_flood_push:
    cmp r14d, G3_FLOOD_MAX
    jae .full
    mov [g3_flood_stack + r14 * 8], eax
    mov [g3_flood_stack + r14 * 8 + 4], edx
    inc r14d
.full:
    ret

g3_flood_at:
    imul edx, [g3_stride]
    add edx, eax
    mov rax, [g3_pixels]
    add rax, rdx
    ret

g3_flood_match:
    call g3_flood_at
    mov al, [rax]
    cmp byte [g3_flood_mode], 0
    jne .border
    cmp al, [g3_flood_target]
    je .yes
    clc
    ret
.border:
    cmp al, [g3_ink]
    jne .yes
    clc
    ret
.yes:
    stc
    ret

g3_blot:
    movss [g3_args], xmm0
    movss [g3_args + 4], xmm1
    movss [g3_args + 8], xmm2
    mov [g3_shape_src], rsi
    mov [g3_blot_size], ecx
    mov [g3_blot_size + 4], edx
    lea rax, [g3_blot_pass]
    jmp g3_twice

g3_blot_pass:
    push rbx
    push r12
    push r13
    push r14
    push r15
    movss xmm0, [g3_args]
    movss xmm1, [g3_args + 4]
    movss xmm2, [g3_args + 8]
    lea rdi, [g3_spot]
    call g3_vertex
    movss xmm0, [g3_spot + G3_Z]
    comiss xmm0, [g3_near]
    jb .done
    movss xmm0, [g3_spot + G3_SX]
    call g3_coord
    mov r12d, eax
    movss xmm0, [g3_spot + G3_SY]
    call g3_coord
    mov r13d, eax
    movss xmm0, [g3_spot + G3_W]
    mov r15, [g3_shape_src]
    xor ebx, ebx
.row:
    cmp ebx, [g3_blot_size + 4]
    jae .done
    xor r14d, r14d
.column:
    cmp r14d, [g3_blot_size]
    jae .next_row
    movzx r10d, byte [r15]
    inc r15
    cmp r10d, G3_CLEAR
    je .clear
    lea eax, [r12 + r14]
    lea edx, [rbx + r13]
    call g3_put
.clear:
    inc r14d
    jmp .column
.next_row:
    inc ebx
    jmp .row
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

g3_pick:
    cvtsi2ss xmm6, esi
    cvtsi2ss xmm7, edx
    movss xmm5, [g3_big]
    mov eax, -1
    xor ecx, ecx
    lea rsi, [g3_vertices]
.vertex:
    cmp ecx, [g3_vertex_count]
    jae .done
    movss xmm0, [rsi + G3_Z]
    comiss xmm0, [g3_near]
    jb .next
    movss xmm0, [rsi + G3_SX]
    subss xmm0, xmm6
    mulss xmm0, xmm0
    movss xmm1, [rsi + G3_SY]
    subss xmm1, xmm7
    mulss xmm1, xmm1
    addss xmm0, xmm1
    comiss xmm0, xmm5
    jae .next
    movss xmm5, xmm0
    mov eax, ecx
.next:
    inc ecx
    add rsi, G3_VERTEX_BYTES
    jmp .vertex
.done:
    movss xmm0, xmm5
    ret

; 2d over everything, no depth test. line2 edi esi to edx ecx, rect2 corner edi esi size edx ecx,
; circle2 center edi esi radius edx
g3_line2:
    call g3_use_ink
    cvtsi2ss xmm0, edi
    movss [g3_ends + G3_SX], xmm0
    cvtsi2ss xmm0, esi
    movss [g3_ends + G3_SY], xmm0
    cvtsi2ss xmm0, edx
    movss [g3_ends + G3_VERTEX_BYTES + G3_SX], xmm0
    cvtsi2ss xmm0, ecx
    movss [g3_ends + G3_VERTEX_BYTES + G3_SY], xmm0
    movss xmm0, [g3_front]
    movss [g3_ends + G3_W], xmm0
    movss [g3_ends + G3_VERTEX_BYTES + G3_W], xmm0
    lea rsi, [g3_ends]
    lea rdi, [g3_ends + G3_VERTEX_BYTES]
    mov byte [g3_no_depth], 1
    call g3_seg
    mov byte [g3_no_depth], 0
    ret

g3_rect2:
    push rbx
    push r12
    push r13
    push r14
    push r15
    call g3_use_ink
    mov r12d, edi
    mov r13d, esi
    mov r14d, edx
    mov r15d, ecx
    movss xmm0, [g3_front]
    mov byte [g3_no_depth], 1
    xor ebx, ebx
.row:
    cmp ebx, r15d
    jge .done
    xor ecx, ecx
.column:
    cmp ecx, r14d
    jge .next_row
    lea eax, [r12 + rcx]
    lea edx, [rbx + r13]
    call g3_pixel
    inc ecx
    jmp .column
.next_row:
    inc ebx
    jmp .row
.done:
    mov byte [g3_no_depth], 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

g3_circle2:
    push rbx
    push r12
    push r13
    push r14
    mov r12d, edi
    mov r13d, esi
    cvtsi2ss xmm0, edx
    movss [g3_arc_args + 12], xmm0
    lea eax, [r12 + rdx]
    mov [g3_frame], eax
    mov [g3_frame + 4], r13d
    mov ebx, 1
.next:
    cvtsi2ss xmm0, ebx
    mulss xmm0, [g3_two_pi]
    mulss xmm0, [g3_circle_rate]
    call g3_sincos
    mulss xmm1, [g3_arc_args + 12]
    mulss xmm0, [g3_arc_args + 12]
    cvtss2si eax, xmm1
    add eax, r12d
    cvtss2si r14d, xmm0
    neg r14d
    add r14d, r13d
    mov edi, [g3_frame]
    mov esi, [g3_frame + 4]
    mov edx, eax
    mov ecx, r14d
    mov [g3_frame], edx
    mov [g3_frame + 4], ecx
    call g3_line2
    inc ebx
    cmp ebx, 24
    jbe .next
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

align 4
g3_unit dd 0x3F800000, 0, 0, 0, 0, 0x3F800000, 0, 0, 0, 0, 0x3F800000, 0, 0, 0, 0, 0x3F800000
g3_one dd 0x3F800000
g3_half dd 0x3F000000
g3_third dd 0x3EAAAAAB
g3_minus_one dd 0xBF800000
g3_tiny dd 0x3C23D70A
g3_tiny_det dd 0x358637BD
g3_big dd 0x7149F2CA
g3_lens dd 0x3ED70A3D
g3_far dd 0x44800000
g3_chance_scale dd 0x47800000
g3_near dd 0x3D4CCCCD
g3_two_over_pi dd 0x3F22F983
g3_half_pi dd 0x3FC90FDB
g3_two_pi dd 0x40C90FDB
g3_round_rate dd 0x40F47645
g3_curve_rate dd 0x3D000000
g3_arrow_size dd 0x40300000
g3_sin_3 dd 0xBE2AAAAB
g3_sin_5 dd 0x3C088889
g3_sin_7 dd 0xB9500D01
g3_cos_2 dd 0xBF000000
g3_cos_4 dd 0x3D2AAAAB
g3_cos_6 dd 0xBAB60B61
g3_far_left dd 0xC5800000
g3_far_right dd 0x45800000
g3_light_dir dd 0xBF13CD3A, 0x3F13CD3A, 0xBF13CD3A
g3_seed dd 0x2545F491
g3_front dd 0x4E6E6B28
g3_circle_rate dd 0x3D2AAAAB
