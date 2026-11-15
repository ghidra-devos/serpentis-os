align 16
g3_vertices rb G3_VERTEX_MAX * G3_VERTEX_BYTES
g3_polygon rb 4 * G3_VERTEX_BYTES
g3_matrix rd 16
g3_operand rd 16
g3_product rd 16
g3_stack rd G3_STACK_MAX * 16
g3_pixels rq 1
g3_zbuffer rq 1
g3_corners rq 3
g3_u rd 3
g3_v rd 3
g3_stride rd 1
g3_width rd 1
g3_height rd 1
g3_width_f rd 1
g3_height_f rd 1
g3_center_x rd 1
g3_center_y rd 1
g3_focal rd 1
g3_depth rd 1
g3_vertex_count rd 1
g3_drawn rd 1
g3_grad_x rd 1
g3_grad_y rd 1
g3_slope_long rd 1
g3_slope_upper rd 1
g3_slope_lower rd 1
g3_row_end rd 1
g3_chance rd 1
g3_dark rb 1
g3_bright rb 1
g3_wire_color rb 1
g3_shape rd G3_SHAPE_MAX * G3_VERTEX_BYTES / 4
g3_ends rd G3_VERTEX_BYTES / 2
g3_spot rd G3_VERTEX_BYTES / 4
g3_head rd G3_VERTEX_BYTES / 4
g3_mirror_corners rd 3 * G3_VERTEX_BYTES / 4
g3_curve rd (G3_CURVE_MAX * 3 + 1) * 3
g3_bez rd 8 * 3
g3_flood_stack rd G3_FLOOD_MAX * 2
g3_mesh_ptr rq 1
g3_shape_src rq 1
g3_args rd 6
g3_box rd 12
g3_arc_args rd 10
g3_arc_at rd 3
g3_arc_from rd 3
g3_curve_from rd 3
g3_aim rd 6
g3_inverse rd 9
g3_mirror_point rd 3
g3_mirror_normal rd 3
g3_slope rd 3
g3_arrow_side rd 2
g3_frame rd 9
g3_blot_size rd 2
g3_mesh_flags rd 1
g3_shape_count rd 1
g3_pen rd 1
g3_step rd 1
g3_ink_chance rd 1
g3_ink rb 1
g3_ink2 rb 1
g3_mirror_mode rb 1
g3_reflecting rb 1
g3_raw rb 1
g3_flood_mode rb 1
g3_flood_target rb 1
g3_low rd 3
g3_high rd 3
g3_middle rd 3
g3_reach rd 1
g3_mesh_kind rb 1
g3_fit_pass rb 1
g3_no_depth rb 1
