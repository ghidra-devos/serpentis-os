; instruction encoding. every handler fills ins_* (width, opcode bytes, reg field, immediate)
; and calls emit_rm, which writes prefix, rex, opcode, modrm, sib, displacement, immediate

encode_reset:
    xor eax, eax
    mov [ins_width], eax
    mov [ins_reg_field], eax
    mov [ins_opcode_length], eax
    mov [ins_imm_size], eax
    mov [ins_force_rex], eax
    mov [ins_no_rexw], eax
    mov [ins_high8], eax
    mov [ins_imm], rax
    ret

set_opcode1:
    mov [ins_opcode], al
    mov dword [ins_opcode_length], 1
    ret

set_opcode2:
    mov [ins_opcode], ax
    mov dword [ins_opcode_length], 2
    ret

; rsi -> operand. r8-r15 / spl..dil need a rex, ah..dh can't have one
note_register:
    test dword [rsi + OP_REGFLAGS], REG_NEEDS_REX
    jz .high
    mov dword [ins_force_rex], 1
.high:
    test dword [rsi + OP_REGFLAGS], REG_HIGH8
    jz .done
    mov dword [ins_high8], 1
.done:
    ret

emit_rm:
    push rbx
    push r12
    mov r12, rsi

    cmp dword [ins_width], 2
    jne .rex
    mov al, 0x66                ; operand size prefix for 16 bit
    call emit_byte

.rex:
    xor ebx, ebx
    cmp dword [ins_width], 8
    jne .rex_r
    cmp dword [ins_no_rexw], 0
    jne .rex_r
    or ebx, 8
.rex_r:
    test dword [ins_reg_field], 8
    jz .rex_rm
    or ebx, 4
.rex_rm:
    cmp dword [r12 + OP_KIND], OP_REG
    jne .rex_memory
    mov rsi, r12
    call note_register
    test dword [r12 + OP_REGNUM], 8
    jz .rex_emit
    or ebx, 1
    jmp .rex_emit
.rex_memory:
    mov eax, [r12 + OP_BASE]
    cmp eax, -1
    je .rex_index
    test eax, 8
    jz .rex_index
    or ebx, 1
.rex_index:
    mov eax, [r12 + OP_INDEX]
    cmp eax, -1
    je .rex_emit
    test eax, 8
    jz .rex_emit
    or ebx, 2

.rex_emit:
    test ebx, ebx
    jnz .need_rex
    cmp dword [ins_force_rex], 0
    je .opcode
.need_rex:
    cmp dword [ins_high8], 0
    jne .high8_conflict
    mov al, bl
    or al, 0x40
    call emit_byte

.opcode:
    xor ebx, ebx
.opcode_byte:
    cmp ebx, [ins_opcode_length]
    jae .modrm
    mov al, [ins_opcode + rbx]
    call emit_byte
    inc ebx
    jmp .opcode_byte

.modrm:
    mov eax, [ins_reg_field]
    and eax, 7
    shl eax, 3
    cmp dword [r12 + OP_KIND], OP_REG
    jne .memory
    or eax, 0xC0
    mov ecx, [r12 + OP_REGNUM]
    and ecx, 7
    or eax, ecx
    call emit_byte
    jmp .immediate

.memory:
    mov ebx, eax
    mov eax, [r12 + OP_BASE]
    and eax, [r12 + OP_INDEX]
    cmp eax, -1
    jne .addressed

    ; no base and no index: [label] is always rip relative. the displacement counts from the end
    ; of the instruction, so the immediate that still follows has to be subtracted too
    mov eax, ebx
    or eax, 5
    call emit_byte
    mov rax, [r12 + OP_VALUE]
    sub rax, [origin]
    sub rax, [out_offset]
    sub rax, 4
    mov ecx, [ins_imm_size]
    sub rax, rcx
    call check_disp32
    call emit_dword
    jmp .immediate

.addressed:
    cmp dword [r12 + OP_INDEX], -1
    jne .sib
    mov eax, [r12 + OP_BASE]
    and eax, 7
    cmp eax, 4
    je .sib

    call displacement_mode
    mov eax, [r12 + OP_BASE]
    and eax, 7
    or eax, ebx
    or eax, ecx
    call emit_byte
    call emit_displacement
    jmp .immediate

.sib:
    cmp dword [r12 + OP_INDEX], 4
    je .bad_index

    cmp dword [r12 + OP_BASE], -1
    jne .sib_based

    mov eax, ebx
    or eax, 4
    call emit_byte
    call sib_scale_index
    or eax, 5
    call emit_byte
    mov rax, [r12 + OP_VALUE]
    call check_disp32
    call emit_dword
    jmp .immediate

.sib_based:
    call displacement_mode
    mov eax, ebx
    or eax, ecx
    or eax, 4
    call emit_byte
    call sib_scale_index
    mov ecx, [r12 + OP_BASE]
    and ecx, 7
    or eax, ecx
    call emit_byte
    call emit_displacement

.immediate:
    mov ecx, [ins_imm_size]
    test ecx, ecx
    jz .done
    mov rax, [ins_imm]
    cmp ecx, 1
    je .imm8
    cmp ecx, 2
    je .imm16
    call emit_dword
    jmp .done
.imm8:
    call emit_byte
    jmp .done
.imm16:
    call emit_word

.done:
    pop r12
    pop rbx
    ret

.high8_conflict:
    lea rdi, [error_high8]
    jmp asm_error

.bad_index:
    lea rdi, [error_bad_memory]
    jmp asm_error

displacement_mode:
    mov ecx, 0x80
    mov dword [displacement_size], 4
    mov rax, [r12 + OP_VALUE]
    test rax, rax
    jnz .short
    mov edx, [r12 + OP_BASE]
    and edx, 7
    cmp edx, 5
    je .short
    xor ecx, ecx
    mov dword [displacement_size], 0
    ret
.short:
    movsx rdx, al
    cmp rdx, rax
    jne .done
    mov ecx, 0x40
    mov dword [displacement_size], 1
.done:
    ret

emit_displacement:
    mov rax, [r12 + OP_VALUE]
    cmp dword [displacement_size], 1
    je .byte
    cmp dword [displacement_size], 4
    jne .none
    call check_disp32
    jmp emit_dword
.byte:
    jmp emit_byte
.none:
    ret

sib_scale_index:
    mov eax, [r12 + OP_INDEX]
    cmp eax, -1
    jne .index
    mov eax, 4
.index:
    and eax, 7
    shl eax, 3
    mov ecx, [r12 + OP_SCALE]
    cmp ecx, 2
    jb .done
    or eax, 0x40
    cmp ecx, 4
    jb .done
    xor eax, 0xC0
    cmp ecx, 8
    jb .done
    or eax, 0xC0
.done:
    ret

; range errors only make sense in the final pass, before that values are still moving
check_disp32:
    cmp byte [final_pass], 0
    je .ok
    movsxd rdx, eax
    cmp rdx, rax
    je .ok
    lea rdi, [error_out_of_range]
    jmp asm_error
.ok:
    ret

check_imm:
    cmp byte [final_pass], 0
    je .ok
    cmp ecx, 8
    jae .ok
    cmp ecx, 4
    je .dword
    cmp ecx, 2
    je .word
    movsx rdx, al
    cmp rdx, rax
    je .ok
    movzx edx, al
    cmp rdx, rax
    je .ok
    jmp .bad
.word:
    movsx rdx, ax
    cmp rdx, rax
    je .ok
    movzx edx, ax
    cmp rdx, rax
    je .ok
    jmp .bad
.dword:
    movsxd rdx, eax
    cmp rdx, rax
    je .ok
    mov edx, eax
    cmp rdx, rax
    je .ok
.bad:
    lea rdi, [error_out_of_range]
    jmp asm_error
.ok:
    ret

fits_int8:
    movsx rdx, al
    cmp rdx, rax
    ret

resolve_width:
    mov eax, [operand_1 + OP_WIDTH]
    cmp dword [operand_count], 2
    jb .check
    cmp dword [operand_2 + OP_KIND], OP_REG
    jne .check
    mov ecx, [operand_2 + OP_WIDTH]
    test eax, eax
    jz .take
    cmp eax, ecx
    jne .mismatch
.take:
    mov eax, ecx
.check:
    test eax, eax
    jz .unknown
    mov [ins_width], eax
    ret
.mismatch:
    lea rdi, [error_size_mismatch]
    jmp asm_error
.unknown:
    lea rdi, [error_size_unknown]
    jmp asm_error

require_operands:
    cmp [operand_count], eax
    jne .wrong
    ret
.wrong:
    lea rdi, [error_operand_count]
    jmp asm_error

invalid_operands:
    lea rdi, [error_invalid_operands]
    jmp asm_error

op_alu:
    mov r13d, eax
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    call resolve_width

    cmp dword [operand_2 + OP_KIND], OP_IMM
    je .immediate
    cmp dword [operand_2 + OP_KIND], OP_REG
    je .from_register

    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    mov eax, r13d
    shl eax, 3
    add eax, 2
    cmp dword [ins_width], 1
    je .reg_mem_opcode
    inc eax
.reg_mem_opcode:
    call set_opcode1
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_1]
    call note_register
    lea rsi, [operand_2]
    jmp emit_rm

.from_register:
    mov eax, r13d
    shl eax, 3
    cmp dword [ins_width], 1
    je .rm_reg_opcode
    inc eax
.rm_reg_opcode:
    call set_opcode1
    mov eax, [operand_2 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    call note_register
    lea rsi, [operand_1]
    jmp emit_rm

.immediate:
    mov [ins_reg_field], r13d
    lea eax, [r13 * 8 + 4]
    mov [ins_acc_opcode], eax
    mov rax, [operand_2 + OP_VALUE]
    mov [ins_imm], rax
    cmp dword [ins_width], 1
    je .imm8_form
    call fits_int8
    jne .imm_full
    mov al, 0x83
    call set_opcode1
    mov dword [ins_imm_size], 1
    lea rsi, [operand_1]
    jmp emit_rm
.imm8_form:
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .imm8_generic
    cmp dword [operand_1 + OP_REGNUM], 0
    je accumulator_immediate
.imm8_generic:
    mov ecx, 1
    call check_imm
    mov al, 0x80
    call set_opcode1
    mov dword [ins_imm_size], 1
    lea rsi, [operand_1]
    jmp emit_rm
.imm_full:
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .imm_generic
    cmp dword [operand_1 + OP_REGNUM], 0
    jne .imm_generic
    call accumulator_immediate
    ret
.imm_generic:
    mov ecx, [ins_width]
    cmp ecx, 4
    jbe .imm_size_ready
    mov ecx, 4
.imm_size_ready:
    push rcx
    mov ecx, [ins_width]
    cmp ecx, 8
    jne .check_small
    mov ecx, 4
    movsxd rdx, eax
    cmp byte [final_pass], 0
    je .checked
    cmp rdx, rax
    jne .out_of_range
    jmp .checked
.check_small:
    call check_imm
.checked:
    pop rcx
    mov [ins_imm_size], ecx
    mov al, 0x81
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm
.out_of_range:
    lea rdi, [error_out_of_range]
    jmp asm_error

accumulator_immediate:
    mov ecx, [ins_width]
    cmp ecx, 1
    je .size
    cmp ecx, 4
    jbe .size
    mov ecx, 4
    mov rax, [ins_imm]
    call check_disp32
    jmp .prefix
.size:
    mov rax, [ins_imm]
    call check_imm
.prefix:
    push rcx
    cmp dword [ins_width], 2
    jne .rex
    mov al, 0x66
    call emit_byte
.rex:
    cmp dword [ins_width], 8
    jne .opcode
    mov al, 0x48
    call emit_byte
.opcode:
    mov eax, [ins_acc_opcode]
    cmp dword [ins_width], 1
    je .emit
    inc eax
.emit:
    call emit_byte
    pop rcx
    mov rax, [ins_imm]
    cmp ecx, 1
    je .imm8
    cmp ecx, 2
    je .imm16
    jmp emit_dword
.imm8:
    jmp emit_byte
.imm16:
    jmp emit_word

op_mov:
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    call resolve_width

    cmp dword [operand_2 + OP_KIND], OP_IMM
    je .immediate
    cmp dword [operand_2 + OP_KIND], OP_REG
    je .from_register

    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    mov al, 0x8A
    cmp dword [ins_width], 1
    je .load
    mov al, 0x8B
.load:
    call set_opcode1
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_1]
    call note_register
    lea rsi, [operand_2]
    jmp emit_rm

.from_register:
    mov al, 0x88
    cmp dword [ins_width], 1
    je .store
    mov al, 0x89
.store:
    call set_opcode1
    mov eax, [operand_2 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    call note_register
    lea rsi, [operand_1]
    jmp emit_rm

.immediate:
    mov rax, [operand_2 + OP_VALUE]
    mov [ins_imm], rax
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .memory_immediate

    mov ecx, [ins_width]
    cmp ecx, 8
    jne .register_short
    movsxd rdx, eax
    cmp rdx, rax
    je .sign_extended

    mov dword [ins_width], 8
    mov eax, [operand_1 + OP_REGNUM]
    call emit_register_opcode_b8
    mov rax, [ins_imm]
    call emit_qword
    ret

.register_short:
    mov rax, [ins_imm]
    call check_imm
    mov eax, [operand_1 + OP_REGNUM]
    call emit_register_opcode_b8
    mov rax, [ins_imm]
    mov ecx, [ins_width]
    cmp ecx, 1
    je .emit_imm8
    cmp ecx, 2
    je .emit_imm16
    jmp emit_dword
.emit_imm8:
    jmp emit_byte
.emit_imm16:
    jmp emit_word

.sign_extended:
    mov rax, [ins_imm]
    call check_disp32
    mov al, 0xC7
    call set_opcode1
    mov dword [ins_imm_size], 4
    lea rsi, [operand_1]
    jmp emit_rm

.memory_immediate:
    mov ecx, [ins_width]
    cmp ecx, 4
    jbe .memory_size
    mov ecx, 4
    mov rax, [ins_imm]
    call check_disp32
    jmp .memory_opcode
.memory_size:
    mov rax, [ins_imm]
    call check_imm
.memory_opcode:
    mov [ins_imm_size], ecx
    mov al, 0xC6
    cmp dword [ins_width], 1
    je .memory_emit
    mov al, 0xC7
.memory_emit:
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm

emit_register_opcode_b8:
    push rbx
    mov ebx, eax
    cmp dword [ins_width], 2
    jne .rex
    mov al, 0x66
    call emit_byte
.rex:
    lea rsi, [operand_1]
    call note_register
    xor eax, eax
    cmp dword [ins_width], 8
    jne .rex_b
    or eax, 8
.rex_b:
    test ebx, 8
    jz .rex_ready
    or eax, 1
.rex_ready:
    test eax, eax
    jnz .emit_rex
    cmp dword [ins_force_rex], 0
    je .opcode
.emit_rex:
    cmp dword [ins_high8], 0
    jne .conflict
    or al, 0x40
    call emit_byte
.opcode:
    mov eax, ebx
    and eax, 7
    add eax, 0xB0
    cmp dword [ins_width], 1
    je .emit
    add eax, 8
.emit:
    call emit_byte
    pop rbx
    ret
.conflict:
    lea rdi, [error_high8]
    jmp asm_error

op_test:
    mov eax, 2
    call require_operands
    call resolve_width
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je .immediate
    cmp dword [operand_2 + OP_KIND], OP_REG
    jne invalid_operands
    mov al, 0x84
    cmp dword [ins_width], 1
    je .register
    mov al, 0x85
.register:
    call set_opcode1
    mov eax, [operand_2 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    call note_register
    lea rsi, [operand_1]
    jmp emit_rm
.immediate:
    mov rax, [operand_2 + OP_VALUE]
    mov [ins_imm], rax
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .generic
    cmp dword [operand_1 + OP_REGNUM], 0
    jne .generic
    mov dword [ins_acc_opcode], 0xA8
    jmp accumulator_immediate
.generic:
    mov ecx, [ins_width]
    cmp ecx, 4
    jbe .size
    mov ecx, 4
    call check_disp32
    jmp .opcode
.size:
    call check_imm
.opcode:
    mov [ins_imm_size], ecx
    mov al, 0xF6
    cmp dword [ins_width], 1
    je .emit
    mov al, 0xF7
.emit:
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm

op_lea:
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_MEM
    jne invalid_operands
    mov eax, [operand_1 + OP_WIDTH]
    mov [ins_width], eax
    mov al, 0x8D
    call set_opcode1
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    jmp emit_rm

op_unary:
    mov r13d, eax
    mov eax, 1
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    call resolve_width
    mov eax, r13d
    and eax, 7
    mov [ins_reg_field], eax
    mov eax, r13d
    shr eax, 8
    cmp dword [ins_width], 1
    je .emit
    inc eax
.emit:
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm

op_shift:
    mov r13d, eax
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    mov eax, [operand_1 + OP_WIDTH]
    test eax, eax
    jz .unknown
    mov [ins_width], eax
    mov [ins_reg_field], r13d

    cmp dword [operand_2 + OP_KIND], OP_REG
    je .by_cl
    cmp dword [operand_2 + OP_KIND], OP_IMM
    jne invalid_operands
    cmp dword [operand_2 + OP_KNOWN], 0
    je .not_constant
    mov rax, [operand_2 + OP_VALUE]
    cmp rax, 1
    je .by_one
    cmp rax, 63
    ja .range
    mov [ins_imm], rax
    mov dword [ins_imm_size], 1
    mov al, 0xC0
    jmp .opcode
.by_one:
    mov al, 0xD0
    jmp .opcode
.by_cl:
    cmp dword [operand_2 + OP_REGNUM], 1
    jne invalid_operands
    cmp dword [operand_2 + OP_WIDTH], 1
    jne invalid_operands
    mov al, 0xD2
.opcode:
    cmp dword [ins_width], 1
    je .emit
    inc al
.emit:
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm
.unknown:
    lea rdi, [error_size_unknown]
    jmp asm_error
.not_constant:
    lea rdi, [error_not_constant]
    jmp asm_error
.range:
    lea rdi, [error_out_of_range]
    jmp asm_error

op_push_pop:
    mov r13d, eax
    mov eax, 1
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_REG
    je .register
    cmp r13d, 0x50
    jne invalid_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    jne invalid_operands
    mov rax, [operand_1 + OP_VALUE]
    call fits_int8
    jne .imm32
    push rax
    mov al, 0x6A
    call emit_byte
    pop rax
    jmp emit_byte
.imm32:
    push rax
    call check_disp32
    mov al, 0x68
    call emit_byte
    pop rax
    jmp emit_dword
.register:
    cmp dword [operand_1 + OP_WIDTH], 8
    jne invalid_operands
    mov eax, [operand_1 + OP_REGNUM]
    test eax, 8
    jz .opcode
    push rax
    mov al, 0x41
    call emit_byte
    pop rax
.opcode:
    and eax, 7
    add eax, r13d
    jmp emit_byte

op_branch:
    mov r13d, eax
    mov eax, 1
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je .relative
    test r13d, 0xFF
    jz invalid_operands
    mov eax, r13d
    shr eax, 16
    and eax, 0xFF
    mov [ins_reg_field], eax
    mov dword [ins_width], 8
    mov dword [ins_no_rexw], 1
    mov al, 0xFF
    call set_opcode1
    lea rsi, [operand_1]
    jmp emit_rm

.relative:
    mov rbx, [operand_1 + OP_VALUE]
    test r13d, 0xFF00
    jz .near
    mov ecx, [branch_index]
    inc dword [branch_index]
    mov [branch_slot], ecx
    cmp dword [operand_1 + OP_KNOWN], 0
    jne .measure
    cmp dword [operand_1 + OP_ESTIMATED], 0
    je .first_guess
    cmp byte [branch_settle], 0
    jne .near
    jmp .measure
.first_guess:
    xor eax, eax
    jmp .short

; short or near, the classic relaxation problem. a backward target is known exactly. a forward
; target only has last pass's value, so its distance is measured inside last pass's layout:
; last target - last position of this branch - last size of this branch. measuring against
; the current position mixes two layouts and once made whole chains go near for nothing,
; that version never matched fasm on the editor. took me way too long to find that one
.measure:
    xor r9d, r9d
    mov rax, rbx
    sub rax, [origin]
    cmp dword [operand_1 + OP_KNOWN], 0
    jne .current
    mov ecx, [branch_slot]
    cmp ecx, BRANCH_MAX
    jae .current
    movzx edx, byte [branch_size + rcx]
    test edx, edx
    jz .current
    mov r8d, [branch_position + rcx * 4]
    sub rax, r8
    sub rax, rdx
    call fits_int8
    jne .near
    mov r9d, 1
    mov rax, rbx
    sub rax, [origin]
.current:
    sub rax, [out_offset]
    sub rax, 2
    call fits_int8
    jne .estimate_short
    jmp .short
.estimate_short:
    test r9d, r9d
    jz .near

.short:
    mov edx, 2
    call remember_branch
    push rax
    mov eax, r13d
    shr eax, 8
    call emit_byte
    pop rax
    jmp emit_byte

.near:
    test r13d, 0xFF00
    jz .near_emit
    mov edx, 5
    test r13d, 0xFF
    jnz .remember_near
    inc edx
.remember_near:
    call remember_branch
.near_emit:
    mov eax, r13d
    test al, al
    jnz .opcode_ready
    mov al, 0x0F
    call emit_byte
    mov eax, r13d
    shr eax, 24
.opcode_ready:
    call emit_byte
    mov rax, rbx
    sub rax, [origin]
    sub rax, [out_offset]
    sub rax, 4
    call check_disp32
    jmp emit_dword

remember_branch:
    mov ecx, [branch_slot]
    cmp ecx, BRANCH_MAX
    jae .done
    mov [branch_size + rcx], dl
    mov r8, [out_offset]
    mov [branch_position + rcx * 4], r8d
.done:
    ret

op_jcc:
    mov ecx, eax
    add ecx, 0x80
    shl ecx, 24
    add eax, 0x70
    shl eax, 8
    or eax, ecx
    jmp op_branch

op_setcc:
    push rax
    mov eax, 1
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    mov dword [ins_width], 1
    cmp dword [operand_1 + OP_WIDTH], 0
    je .sized
    cmp dword [operand_1 + OP_WIDTH], 1
    jne invalid_operands
.sized:
    pop rax
    add eax, 0x90
    shl eax, 8
    or eax, 0x0F
    call set_opcode2
    lea rsi, [operand_1]
    jmp emit_rm

op_cmovcc:
    push rax
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je invalid_operands
    call resolve_width
    pop rax
    add eax, 0x40

op_two_byte_reg_rm:
    shl eax, 8
    or eax, 0x0F
    call set_opcode2
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_1]
    call note_register
    lea rsi, [operand_2]
    jmp emit_rm

op_extend:
    mov r13d, eax
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je invalid_operands
    mov eax, [operand_1 + OP_WIDTH]
    mov [ins_width], eax
    mov ecx, [operand_2 + OP_WIDTH]
    cmp ecx, 1
    je .byte_source
    cmp ecx, 2
    jne invalid_operands
    lea eax, [r13 + 1]
    jmp op_two_byte_reg_rm
.byte_source:
    lea rsi, [operand_2]
    call note_register
    mov eax, r13d
    jmp op_two_byte_reg_rm

op_movsxd:
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [operand_1 + OP_WIDTH], 8
    jne invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je invalid_operands
    mov dword [ins_width], 8
    mov al, 0x63
    call set_opcode1
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    jmp emit_rm

op_imul:
    cmp dword [operand_count], 1
    jne .multi
    mov eax, 0xF605
    jmp op_unary
.multi:
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [operand_count], 3
    je .three
    mov eax, 2
    call require_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je .two_immediate
    call resolve_width
    mov eax, 0xAF
    jmp op_two_byte_reg_rm
.two_immediate:
    lea rsi, [operand_2]
    lea rdi, [operand_3]
    mov ecx, OP_SIZE
    rep movsb
    lea rdi, [operand_2]
    lea rsi, [operand_1]
    mov ecx, OP_SIZE
    rep movsb
.three:
    cmp dword [operand_3 + OP_KIND], OP_IMM
    jne invalid_operands
    mov eax, [operand_1 + OP_WIDTH]
    mov [ins_width], eax
    mov rax, [operand_3 + OP_VALUE]
    mov [ins_imm], rax
    mov eax, [operand_1 + OP_REGNUM]
    mov [ins_reg_field], eax
    mov rax, [ins_imm]
    call fits_int8
    jne .imm32
    mov dword [ins_imm_size], 1
    mov al, 0x6B
    jmp .emit
.imm32:
    mov rax, [ins_imm]
    call check_disp32
    mov dword [ins_imm_size], 4
    mov al, 0x69
.emit:
    call set_opcode1
    lea rsi, [operand_2]
    jmp emit_rm

op_xchg:
    mov eax, 2
    call require_operands
    call resolve_width
    cmp dword [ins_width], 1
    je .general
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .general
    cmp dword [operand_2 + OP_KIND], OP_REG
    jne .general
    mov eax, [operand_2 + OP_REGNUM]
    cmp dword [operand_1 + OP_REGNUM], 0
    je .short
    mov eax, [operand_1 + OP_REGNUM]
    cmp dword [operand_2 + OP_REGNUM], 0
    jne .general
.short:
    push rax
    cmp dword [ins_width], 2
    jne .short_rex
    mov al, 0x66
    call emit_byte
.short_rex:
    mov ecx, [rsp]
    xor eax, eax
    cmp dword [ins_width], 8
    jne .short_b
    or eax, 8
.short_b:
    test ecx, 8
    jz .short_emit_rex
    or eax, 1
.short_emit_rex:
    test eax, eax
    jz .short_opcode
    or al, 0x40
    call emit_byte
.short_opcode:
    pop rax
    and eax, 7
    add eax, 0x90
    jmp emit_byte
.general:
    cmp dword [operand_1 + OP_KIND], OP_REG
    je .swap
    cmp dword [operand_2 + OP_KIND], OP_REG
    je .ordered
    jmp invalid_operands
.swap:
    lea rsi, [operand_1]
    lea rdi, [operand_3]
    mov ecx, OP_SIZE
    rep movsb
    lea rsi, [operand_2]
    lea rdi, [operand_1]
    mov ecx, OP_SIZE
    rep movsb
    lea rsi, [operand_3]
    lea rdi, [operand_2]
    mov ecx, OP_SIZE
    rep movsb
.ordered:
    mov al, 0x86
    cmp dword [ins_width], 1
    je .emit
    mov al, 0x87
.emit:
    call set_opcode1
    mov eax, [operand_2 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_2]
    call note_register
    lea rsi, [operand_1]
    jmp emit_rm

op_bit:
    mov r13d, eax
    mov eax, 2
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    je invalid_operands
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je .immediate
    call resolve_width
    mov eax, r13d
    shl eax, 3
    add eax, 0xA3
    shl eax, 8
    or eax, 0x0F
    call set_opcode2
    mov eax, [operand_2 + OP_REGNUM]
    mov [ins_reg_field], eax
    lea rsi, [operand_1]
    jmp emit_rm
.immediate:
    mov eax, [operand_1 + OP_WIDTH]
    test eax, eax
    jz .unknown
    mov [ins_width], eax
    mov rax, [operand_2 + OP_VALUE]
    mov [ins_imm], rax
    mov dword [ins_imm_size], 1
    lea eax, [r13 + 4]
    mov [ins_reg_field], eax
    mov eax, 0xBA0F
    call set_opcode2
    lea rsi, [operand_1]
    jmp emit_rm
.unknown:
    lea rdi, [error_size_unknown]
    jmp asm_error

op_int:
    mov r13d, eax
    mov eax, 1
    call require_operands
    cmp dword [operand_1 + OP_KIND], OP_IMM
    jne invalid_operands
    mov al, 0xCD
    call emit_byte
    mov rax, [operand_1 + OP_VALUE]
    mov ecx, 1
    call check_imm
    jmp emit_byte

op_ret:
    cmp dword [operand_count], 0
    jne .immediate
    mov al, 0xC3
    jmp emit_byte
.immediate:
    mov al, 0xC2
    call emit_byte
    mov rax, [operand_1 + OP_VALUE]
    jmp emit_word

SSE_RM = 0
SSE_MOVE = 1
SSE_FROM_INTEGER = 2
SSE_TO_INTEGER = 3
SSE_MOVQ = 4

op_sse:
    mov r13d, eax
    shr eax, 16
    mov r14d, eax
    cmp r14d, SSE_MOVE
    jne .operands
    cmp dword [operand_count], 0
    jne .operands
    mov al, 0xA5
    jmp emit_byte
.operands:
    mov eax, 2
    call require_operands
    mov dword [ins_width], 4
    cmp dword [operand_2 + OP_KIND], OP_IMM
    je invalid_operands
    cmp r14d, SSE_MOVQ
    je .movq
    cmp r14d, SSE_TO_INTEGER
    je .to_integer
    lea rsi, [operand_1]
    cmp r14d, SSE_MOVE
    jne .load
    cmp dword [operand_1 + OP_KIND], OP_MEM
    jne .load
    lea rsi, [operand_2]
    call require_xmm
    mov eax, [operand_2 + OP_REGNUM]
    lea rsi, [operand_1]
    add r13d, 0x100
    jmp sse_encode
.load:
    call require_xmm
    cmp r14d, SSE_FROM_INTEGER
    jne .xmm_source
    cmp dword [operand_2 + OP_WIDTH], XMM_WIDTH
    je invalid_operands
    cmp dword [operand_2 + OP_WIDTH], 8
    jne .source_ready
    mov dword [ins_width], 8
    jmp .source_ready
.xmm_source:
    cmp dword [operand_2 + OP_KIND], OP_REG
    jne .source_ready
    lea rsi, [operand_2]
    call require_xmm
.source_ready:
    mov eax, [operand_1 + OP_REGNUM]
    lea rsi, [operand_2]
    jmp sse_encode

.to_integer:
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne invalid_operands
    mov eax, [operand_1 + OP_WIDTH]
    cmp eax, 4
    je .integer_width
    cmp eax, 8
    jne invalid_operands
.integer_width:
    mov [ins_width], eax
    cmp dword [operand_2 + OP_KIND], OP_REG
    jne .integer_ready
    lea rsi, [operand_2]
    call require_xmm
.integer_ready:
    mov eax, [operand_1 + OP_REGNUM]
    lea rsi, [operand_2]
    jmp sse_encode

.movq:
    cmp dword [operand_1 + OP_WIDTH], XMM_WIDTH
    jne .movq_out
    cmp dword [operand_1 + OP_KIND], OP_REG
    jne .movq_out
    cmp dword [operand_2 + OP_KIND], OP_MEM
    je .movq_load
    cmp dword [operand_2 + OP_WIDTH], 8
    jne invalid_operands
    mov dword [ins_width], 8
    mov r13d, 0x6E66
    mov eax, [operand_1 + OP_REGNUM]
    lea rsi, [operand_2]
    jmp sse_encode
.movq_load:
    mov r13d, 0x7EF3
    mov eax, [operand_1 + OP_REGNUM]
    lea rsi, [operand_2]
    jmp sse_encode
.movq_out:
    lea rsi, [operand_2]
    call require_xmm
    cmp dword [operand_1 + OP_KIND], OP_MEM
    je .movq_store
    cmp dword [operand_1 + OP_WIDTH], 8
    jne invalid_operands
    mov dword [ins_width], 8
    mov r13d, 0x7E66
    mov eax, [operand_2 + OP_REGNUM]
    lea rsi, [operand_1]
    jmp sse_encode
.movq_store:
    mov r13d, 0xD666
    mov eax, [operand_2 + OP_REGNUM]
    lea rsi, [operand_1]
    jmp sse_encode

require_xmm:
    cmp dword [rsi + OP_KIND], OP_REG
    jne invalid_operands
    cmp dword [rsi + OP_WIDTH], XMM_WIDTH
    jne invalid_operands
    ret

sse_encode:
    mov [ins_reg_field], eax
    push rsi
    mov eax, r13d
    test al, al
    jz .opcode
    call emit_byte
.opcode:
    mov eax, r13d
    and eax, 0xFF00
    or eax, 0x0F
    call set_opcode2
    pop rsi
    jmp emit_rm

op_plain:
    mov r13d, eax
    mov eax, 0
    call require_operands
    mov eax, r13d
.next:
    test eax, eax
    jz .done
    push rax
    call emit_byte
    pop rax
    shr eax, 8
    jmp .next
.done:
    ret
