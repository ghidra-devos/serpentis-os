; formula compiler. recursive descent over the text, out comes a little postfix program
; (ops + a double per op) that evaluate runs on a stack for every x. compile once, run it
; a few thousand times per frame

; grammar, lowest first:
; sum     = product { (+|-) product }
; product = power { (*|/) power | power }      2x, 3sin x, x(x+1) multiply
; power   = unary [ ^ power ]                  right associative, 2^3^2 = 2^9
; unary   = - power | + unary | primary        so -x^2 is -(x^2)
; primary = number | x | pi | e | name [(sum) | power] | (sum) | |sum|

OP_END = 0
OP_NUMBER = 1
OP_X = 2
OP_ADD = 3
OP_SUB = 4
OP_MUL = 5
OP_DIV = 6
OP_POWER = 7
OP_NEGATE = 8
OP_CALL = 9

PROGRAM_MAX = 96
STACK_MAX = 32
NAME_MAX = 8
NAME_X = 1
NAME_CONSTANT = 2
NAME_FUNCTION = 3

; rdi -> ops, rdx -> values, rsi -> text. rax = 0 or an error message
compile_function:
    push rbx
    push r12
    mov [parse_cursor], rsi
    mov [program_ops], rdi
    mov [program_values], rdx
    mov dword [program_length], 0
    mov qword [parse_error], 0
    mov dword [bar_depth], 0
    call skip_blanks
    mov rsi, [parse_cursor]
    cmp byte [rsi], 0
    jne .parse
    lea rax, [error_empty]
    mov [parse_error], rax
    jmp .finish
.parse:
    call parse_sum
    cmp qword [parse_error], 0
    jne .finish
    call skip_blanks
    mov rsi, [parse_cursor]
    cmp byte [rsi], 0
    je .finish
    lea rax, [error_trailing]
    mov [parse_error], rax
.finish:
    xor eax, eax
    call emit_op
    mov rax, [parse_error]
    pop r12
    pop rbx
    ret

emit_op:
    mov ecx, [program_length]
    cmp ecx, PROGRAM_MAX - 1
    jb .room
    test al, al
    jz .room
    lea rdx, [error_long]
    mov [parse_error], rdx
    ret
.room:
    mov rdx, [program_ops]
    mov [rdx + rcx], al
    mov rdx, [program_values]
    movsd [rdx + rcx * 8], xmm0
    inc dword [program_length]
    ret

skip_blanks:
    mov rsi, [parse_cursor]
.next:
    cmp byte [rsi], ' '
    jne .done
    inc rsi
    jmp .next
.done:
    mov [parse_cursor], rsi
    ret

peek_char:
    call skip_blanks
    movzx eax, byte [rsi]
    ret

parse_sum:
    call parse_product
.next:
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    cmp al, '+'
    je .add
    cmp al, '-'
    jne .done
    inc qword [parse_cursor]
    call parse_product
    mov eax, OP_SUB
    call emit_op
    jmp .next
.add:
    inc qword [parse_cursor]
    call parse_product
    mov eax, OP_ADD
    call emit_op
    jmp .next
.done:
    ret

parse_product:
    call parse_power
.next:
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    cmp al, '*'
    je .multiply
    cmp al, '/'
    je .divide
    ; implicit multiplication, a factor right after a factor
    call starts_factor
    jnc .done
    call parse_power
    mov eax, OP_MUL
    call emit_op
    jmp .next
.multiply:
    inc qword [parse_cursor]
    call parse_power
    mov eax, OP_MUL
    call emit_op
    jmp .next
.divide:
    inc qword [parse_cursor]
    call parse_power
    mov eax, OP_DIV
    call emit_op
    jmp .next
.done:
    ret

starts_factor:
    cmp al, '('
    je .yes
    cmp al, '|'
    jne .digit
    cmp dword [bar_depth], 0
    je .yes
    clc
    ret
.digit:
    cmp al, '.'
    je .yes
    cmp al, '0'
    jb .letter
    cmp al, '9'
    jbe .yes
.letter:
    or al, 0x20
    cmp al, 'a'
    jb .no
    cmp al, 'z'
    ja .no
.yes:
    stc
    ret
.no:
    clc
    ret

parse_power:
    call parse_unary
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    cmp al, '^'
    jne .done
    inc qword [parse_cursor]
    call parse_power
    mov eax, OP_POWER
    call emit_op
.done:
    ret

parse_unary:
    call peek_char
    cmp al, '-'
    je .negate
    cmp al, '+'
    jne parse_primary
    inc qword [parse_cursor]
    jmp parse_unary
.negate:
    inc qword [parse_cursor]
    call parse_power
    mov eax, OP_NEGATE
    jmp emit_op

parse_primary:
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    cmp al, '('
    je .group
    cmp al, '|'
    je .absolute
    cmp al, '.'
    je .number
    cmp al, '0'
    jb .name
    cmp al, '9'
    jbe .number
.name:
    mov ecx, eax
    or ecx, 0x20
    cmp ecx, 'a'
    jb .unexpected
    cmp ecx, 'z'
    ja .unexpected
    jmp parse_name
.group:
    inc qword [parse_cursor]
    call parse_sum
    jmp expect_close
.absolute:
    inc qword [parse_cursor]
    inc dword [bar_depth]
    call parse_sum
    dec dword [bar_depth]
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    cmp al, '|'
    jne .missing_bar
    inc qword [parse_cursor]
    mov eax, OP_CALL
    lea rdx, [function_abs]
    jmp emit_call
.missing_bar:
    lea rax, [error_bar]
    mov [parse_error], rax
    ret
.number:
    call parse_number
    mov eax, OP_NUMBER
    jmp emit_op
.unexpected:
    lea rax, [error_unexpected]
    mov rcx, [parse_cursor]
    cmp byte [rcx], 0
    jne .store
    lea rax, [error_end]
.store:
    mov [parse_error], rax
.done:
    ret

expect_close:
    cmp qword [parse_error], 0
    jne .done
    call peek_char
    test al, al
    jz .done
    cmp al, ')'
    jne .missing
    inc qword [parse_cursor]
.done:
    ret
.missing:
    lea rax, [error_close]
    mov [parse_error], rax
    ret

emit_call:
    mov rcx, rdx
    lea rax, [function_names]
    sub rcx, rax
    shr rcx, 4
    cvtsi2sd xmm0, rcx
    mov eax, OP_CALL
    jmp emit_op

parse_name:
    push rbx
    push r12
    mov rsi, [parse_cursor]
    xor ecx, ecx
.copy:
    mov al, [rsi + rcx]
    mov dl, al
    or dl, 0x20
    cmp dl, '0'
    jb .copied
    cmp dl, '9'
    ja .letter
    test ecx, ecx
    jz .copied
    jmp .store
.letter:
    cmp dl, 'a'
    jb .copied
    cmp dl, 'z'
    ja .copied
.store:
    cmp ecx, NAME_MAX - 1
    jae .copied
    mov [name_text + rcx], dl
    inc ecx
    jmp .copy
.copied:
    mov r12d, ecx
.try:
    test r12d, r12d
    jz .unknown
    mov byte [name_text + r12], 0
    call match_name
    test eax, eax
    jnz .matched
    dec r12d
    jmp .try
.matched:
    add [parse_cursor], r12
    cmp eax, NAME_FUNCTION
    je .function
    cmp eax, NAME_X
    je .variable
    mov eax, OP_NUMBER
    call emit_op
    jmp .out
.variable:
    mov eax, OP_X
    call emit_op
    jmp .out
.function:
    push rbx
    call peek_char
    cmp al, '('
    jne .call_power
    inc qword [parse_cursor]
    call parse_sum
    call expect_close
    jmp .emit
.call_power:
    call parse_power
.emit:
    pop rdx
    call emit_call
    jmp .out
.unknown:
    lea rax, [error_name]
    mov [parse_error], rax
.out:
    pop r12
    pop rbx
    ret

match_name:
    cmp r12d, 1
    jne .constants
    cmp byte [name_text], 'x'
    jne .constants
    mov eax, NAME_X
    ret
.constants:
    lea rdi, [name_text]
    lea rsi, [pi_name]
    call str_equal
    test eax, eax
    jz .not_pi
    movsd xmm0, [pi]
    mov eax, NAME_CONSTANT
    ret
.not_pi:
    lea rdi, [name_text]
    lea rsi, [e_name]
    call str_equal
    test eax, eax
    jz .not_e
    movsd xmm0, [euler]
    mov eax, NAME_CONSTANT
    ret
.not_e:
    lea rbx, [function_names]
.search:
    cmp byte [rbx], 0
    je .none
    lea rdi, [name_text]
    mov rsi, rbx
    call str_equal
    test eax, eax
    jnz .found
    add rbx, 16
    jmp .search
.found:
    mov eax, NAME_FUNCTION
    ret
.none:
    xor eax, eax
    ret

; digits with an optional fraction, no exponent. the fraction is summed as an integer and
; divided once at the end, fewer rounding steps
parse_number:
    mov rsi, [parse_cursor]
    xorpd xmm0, xmm0
    movsd xmm2, [ten]
.whole:
    movzx eax, byte [rsi]
    sub eax, '0'
    cmp eax, 9
    ja .point
    mulsd xmm0, xmm2
    cvtsi2sd xmm1, eax
    addsd xmm0, xmm1
    inc rsi
    jmp .whole
.point:
    cmp byte [rsi], '.'
    jne .done
    inc rsi
    movsd xmm3, [one]
    xorpd xmm4, xmm4
.fraction:
    movzx eax, byte [rsi]
    sub eax, '0'
    cmp eax, 9
    ja .scale
    mulsd xmm4, xmm2
    cvtsi2sd xmm1, eax
    addsd xmm4, xmm1
    mulsd xmm3, xmm2
    inc rsi
    jmp .fraction
.scale:
    divsd xmm4, xmm3
    addsd xmm0, xmm4
.done:
    mov [parse_cursor], rsi
    ret

evaluate:
    push rbx
    push r12
    push r13
    movsd [eval_x], xmm0
    mov r12, rdi
    mov r13, rsi
    xor ebx, ebx
    mov dword [eval_depth], 0
.next:
    movzx eax, byte [r12 + rbx]
    cmp eax, OP_END
    je .result
    cmp eax, OP_NUMBER
    je .number
    cmp eax, OP_X
    je .variable
    cmp eax, OP_NEGATE
    je .negate
    cmp eax, OP_CALL
    je .call
    mov ecx, [eval_depth]
    sub ecx, 2
    js .broken
    mov [eval_depth], ecx
    movsd xmm0, [eval_stack + rcx * 8]
    movsd xmm1, [eval_stack + rcx * 8 + 8]
    cmp eax, OP_ADD
    je .add
    cmp eax, OP_SUB
    je .subtract
    cmp eax, OP_MUL
    je .multiply
    cmp eax, OP_DIV
    je .divide
    call math_power
    jmp .push
.add:
    addsd xmm0, xmm1
    jmp .push
.subtract:
    subsd xmm0, xmm1
    jmp .push
.multiply:
    mulsd xmm0, xmm1
    jmp .push
.divide:
    divsd xmm0, xmm1
    jmp .push
.number:
    movsd xmm0, [r13 + rbx * 8]
    jmp .push
.variable:
    movsd xmm0, [eval_x]
    jmp .push
.negate:
    call pop_value
    jc .broken
    xorpd xmm0, [sign_mask]
    jmp .push
.call:
    cvttsd2si rax, qword [r13 + rbx * 8]
    mov rax, [function_handlers + rax * 8]
    mov [call_target], rax
    call pop_value
    jc .broken
    call qword [call_target]
.push:
    mov ecx, [eval_depth]
    cmp ecx, STACK_MAX
    jae .broken
    movsd [eval_stack + rcx * 8], xmm0
    inc dword [eval_depth]
    inc ebx
    jmp .next
.result:
    cmp dword [eval_depth], 1
    jne .broken
    movsd xmm0, [eval_stack]
    jmp .out
.broken:
    call nan_value
.out:
    pop r13
    pop r12
    pop rbx
    ret

pop_value:
    mov ecx, [eval_depth]
    test ecx, ecx
    jz .empty
    dec ecx
    mov [eval_depth], ecx
    movsd xmm0, [eval_stack + rcx * 8]
    clc
    ret
.empty:
    stc
    ret

align 16
function_names:
    db 'sin', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'cos', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'tan', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'asin', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'acos', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'atan', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'sqrt', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
function_abs db 'abs', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'ln', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'log', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'exp', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'floor', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'ceil', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'round', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'sinh', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'cosh', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'tanh', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'sec', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'csc', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'cot', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'sign', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'cbrt', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 'log2', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 0

align 8
function_handlers dq math_sin, math_cos, math_tan, math_asin, math_acos, math_atan, math_sqrt, math_abs
                  dq math_ln, math_log10, math_exp, math_floor, math_ceil, math_round
                  dq math_sinh, math_cosh, math_tanh, math_sec, math_csc, math_cot, math_sign, math_cbrt
                  dq math_log2

pi_name db 'pi', 0
e_name db 'e', 0
error_empty db 'type a formula in x', 0
error_trailing db 'unexpected text at the end', 0
error_long db 'formula is too long', 0
error_close db 'missing )', 0
error_bar db 'missing |', 0
error_name db 'unknown name', 0
error_unexpected db 'unexpected character', 0
error_end db 'formula ends too early', 0
