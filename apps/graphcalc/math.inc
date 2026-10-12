; double precision math for graphcalc, sse2 only (no x87, no libm in here). value in xmm0,
; second argument in xmm1, result in xmm0. everything not finite comes out as nan or +-inf

SERIES_TERMS = 14
ATAN_TERMS = 24
LOG_TERMS = 24

nan_value:
    mov rax, 0x7FF8000000000000
    movq xmm0, rax
    ret

is_finite:
    movq rax, xmm0
    shr rax, 52
    and eax, 0x7FF
    cmp eax, 0x7FF
    je .no
    stc
    ret
.no:
    clc
    ret

math_abs:
    andpd xmm0, [abs_mask]
    ret

math_sqrt:
    sqrtsd xmm0, xmm0
    ret

math_trunc:
    movsd xmm1, xmm0
    andpd xmm1, [abs_mask]
    ucomisd xmm1, [large_integer]
    jae .done
    cvttsd2si rax, xmm0
    cvtsi2sd xmm0, rax
.done:
    ret

math_floor:
    movsd xmm2, xmm0
    call math_trunc
    ucomisd xmm0, xmm2
    jbe .done
    subsd xmm0, [one]
.done:
    ret

math_ceil:
    movsd xmm2, xmm0
    call math_trunc
    ucomisd xmm0, xmm2
    jae .done
    addsd xmm0, [one]
.done:
    ret

math_round:
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jb .negative
    addsd xmm0, [half]
    jmp math_floor
.negative:
    subsd xmm0, [half]
    jmp math_ceil

reduce_quadrant:
    movsd xmm3, xmm0
    mulsd xmm0, [two_over_pi]
    call math_round
    cvttsd2si rax, xmm0
    and eax, 3
    mov [quadrant], eax
    movsd xmm1, xmm0
    mulsd xmm1, [pio2_high]
    subsd xmm3, xmm1
    mulsd xmm0, [pio2_low]
    subsd xmm3, xmm0
    movsd xmm0, xmm3
    ret

sin_series:
    movsd xmm1, xmm0
    mulsd xmm1, xmm0
    movsd xmm2, xmm0
    movsd xmm3, xmm0
    mov ecx, 1
.term:
    lea eax, [rcx * 2]
    lea edx, [rax + 1]
    imul eax, edx
    cvtsi2sd xmm4, eax
    mulsd xmm2, xmm1
    divsd xmm2, xmm4
    xorpd xmm2, [sign_mask]
    addsd xmm3, xmm2
    inc ecx
    cmp ecx, SERIES_TERMS
    jb .term
    movsd xmm0, xmm3
    ret

cos_series:
    movsd xmm1, xmm0
    mulsd xmm1, xmm0
    movsd xmm2, [one]
    movsd xmm3, [one]
    mov ecx, 1
.term:
    lea eax, [rcx * 2]
    lea edx, [rax - 1]
    imul eax, edx
    cvtsi2sd xmm4, eax
    mulsd xmm2, xmm1
    divsd xmm2, xmm4
    xorpd xmm2, [sign_mask]
    addsd xmm3, xmm2
    inc ecx
    cmp ecx, SERIES_TERMS
    jb .term
    movsd xmm0, xmm3
    ret

math_sin:
    call is_finite
    jnc nan_value
    call reduce_quadrant
quadrant_value:
    test dword [quadrant], 1
    jnz .cosine
    call sin_series
    jmp .sign
.cosine:
    call cos_series
.sign:
    test dword [quadrant], 2
    jz .done
    xorpd xmm0, [sign_mask]
.done:
    ret

math_cos:
    call is_finite
    jnc nan_value
    call reduce_quadrant
    inc dword [quadrant]
    jmp quadrant_value

math_tan:
    movsd [tan_angle], xmm0
    call math_cos
    movsd [tan_cosine], xmm0
    movsd xmm0, [tan_angle]
    call math_sin
    divsd xmm0, [tan_cosine]
    ret

math_atan:
    call is_finite
    jc .finite
    movq rax, xmm0
    test rax, rax
    js .minus_infinity
    movsd xmm0, [half_pi]
    ret
.minus_infinity:
    movsd xmm0, [half_pi]
    xorpd xmm0, [sign_mask]
    ret
.finite:
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jae .positive
    xorpd xmm0, [sign_mask]
    call .positive
    xorpd xmm0, [sign_mask]
    ret
.positive:
    ucomisd xmm0, [one]
    jbe .small
    movsd xmm1, [one]
    divsd xmm1, xmm0
    movsd xmm0, xmm1
    call .small
    movsd xmm1, [half_pi]
    subsd xmm1, xmm0
    movsd xmm0, xmm1
    ret
; two halvings bring x under tan(pi/16) so the series converges fast, times 4 at the end
.small:
    call halve_angle
    call halve_angle
    movsd xmm1, xmm0
    mulsd xmm1, xmm0
    movsd xmm2, xmm0
    xorpd xmm3, xmm3
    xor ecx, ecx
.term:
    lea eax, [rcx * 2 + 1]
    cvtsi2sd xmm4, eax
    movsd xmm5, xmm2
    divsd xmm5, xmm4
    addsd xmm3, xmm5
    mulsd xmm2, xmm1
    xorpd xmm2, [sign_mask]
    inc ecx
    cmp ecx, ATAN_TERMS
    jb .term
    mulsd xmm3, [four]
    movsd xmm0, xmm3
    ret

halve_angle:
    movsd xmm1, xmm0
    mulsd xmm1, xmm0
    addsd xmm1, [one]
    sqrtsd xmm1, xmm1
    addsd xmm1, [one]
    divsd xmm0, xmm1
    ret

math_asin:
    movsd xmm1, xmm0
    andpd xmm1, [abs_mask]
    ucomisd xmm1, [one]
    ja nan_value
    jb .inside
    movsd xmm1, [half_pi]
    movq rax, xmm0
    test rax, rax
    jns .edge
    xorpd xmm1, [sign_mask]
.edge:
    movsd xmm0, xmm1
    ret
.inside:
    movsd xmm1, xmm0
    mulsd xmm1, xmm0
    movsd xmm2, [one]
    subsd xmm2, xmm1
    sqrtsd xmm2, xmm2
    divsd xmm0, xmm2
    jmp math_atan

math_acos:
    call math_asin
    movsd xmm1, [half_pi]
    subsd xmm1, xmm0
    movsd xmm0, xmm1
    ret

math_exp:
    call is_finite
    jc .finite
    movq rax, xmm0
    test rax, rax
    jns .done
    xorpd xmm0, xmm0
.done:
    ret
.finite:
    ucomisd xmm0, [exp_high]
    ja .overflow
    ucomisd xmm0, [exp_low]
    jb .underflow
    movsd xmm6, xmm0
    mulsd xmm0, [inverse_ln2]
    call math_round
    cvttsd2si r8, xmm0
    mulsd xmm0, [ln2]
    subsd xmm6, xmm0
    movsd xmm2, [one]
    movsd xmm3, [one]
    mov ecx, 1
.term:
    cvtsi2sd xmm4, ecx
    mulsd xmm2, xmm6
    divsd xmm2, xmm4
    addsd xmm3, xmm2
    inc ecx
    cmp ecx, 22
    jb .term
    movsd xmm0, xmm3
    jmp scale_power_of_two
.overflow:
    mov rax, 0x7FF0000000000000
    movq xmm0, rax
    ret
.underflow:
    xorpd xmm0, xmm0
    ret

; xmm0 * 2^r8. the exponent field can't take more than about +-1022 at once, so big
; shifts go in steps of 2^1000
scale_power_of_two:
    cmp r8, -1000
    jge .high
    add r8, 1000
    mulsd xmm0, [tiny_scale]
    jmp scale_power_of_two
.high:
    cmp r8, 1000
    jle .apply
    sub r8, 1000
    mulsd xmm0, [huge_scale]
    jmp .high
.apply:
    lea rax, [r8 + 1023]
    shl rax, 52
    movq xmm1, rax
    mulsd xmm0, xmm1
    ret

math_ln:
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jp nan_value
    jb nan_value
    jne .positive
    mov rax, 0xFFF0000000000000
    movq xmm0, rax
    ret
.positive:
    call is_finite
    jnc .done
    movq rax, xmm0
    mov rcx, rax
    shr rcx, 52
    and ecx, 0x7FF
    jnz .normal
    mulsd xmm0, [huge_scale]
    movq rax, xmm0
    mov rcx, rax
    shr rcx, 52
    and ecx, 0x7FF
    sub ecx, 1000
.normal:
    sub ecx, 1022
    mov rdx, 0x800FFFFFFFFFFFFF
    and rax, rdx
    mov rdx, 0x3FE0000000000000  ; exponent of 0.5, mantissa kept -> m in [0.5, 1)
    or rax, rdx
    movq xmm0, rax
    ucomisd xmm0, [sqrt_half]
    jae .series
    addsd xmm0, xmm0
    dec ecx
.series:
    cvtsi2sd xmm7, ecx
    movsd xmm1, xmm0
    subsd xmm1, [one]
    addsd xmm0, [one]
    divsd xmm1, xmm0
    movsd xmm2, xmm1
    mulsd xmm1, xmm1
    xorpd xmm3, xmm3
    xor ecx, ecx
.term:
    lea eax, [rcx * 2 + 1]
    cvtsi2sd xmm4, eax
    movsd xmm5, xmm2
    divsd xmm5, xmm4
    addsd xmm3, xmm5
    mulsd xmm2, xmm1
    inc ecx
    cmp ecx, LOG_TERMS
    jb .term
    addsd xmm3, xmm3
    mulsd xmm7, [ln2]
    addsd xmm3, xmm7
    movsd xmm0, xmm3
.done:
    ret

math_log10:
    call math_ln
    divsd xmm0, [ln10]
    ret

math_log2:
    call math_ln
    mulsd xmm0, [inverse_ln2]
    ret

math_sinh:
    call math_exp
    movsd xmm1, [one]
    divsd xmm1, xmm0
    subsd xmm0, xmm1
    mulsd xmm0, [half]
    ret

math_cosh:
    call math_exp
    movsd xmm1, [one]
    divsd xmm1, xmm0
    addsd xmm0, xmm1
    mulsd xmm0, [half]
    ret

; past 20 tanh is +-1 in doubles and e^2x would only make inf/inf
math_tanh:
    movsd xmm1, xmm0
    andpd xmm1, [abs_mask]
    ucomisd xmm1, [tanh_limit]
    ja math_sign
    addsd xmm0, xmm0
    call math_exp
    movsd xmm1, xmm0
    subsd xmm0, [one]
    addsd xmm1, [one]
    divsd xmm0, xmm1
    ret

math_sec:
    call math_cos
    jmp reciprocal

math_csc:
    call math_sin
    jmp reciprocal

math_cot:
    call math_tan
reciprocal:
    movsd xmm1, [one]
    divsd xmm1, xmm0
    movsd xmm0, xmm1
    ret

math_sign:
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jp .done
    je .done
    movsd xmm1, [one]
    jb .negative
    movsd xmm0, xmm1
.done:
    ret
.negative:
    xorpd xmm1, [sign_mask]
    movsd xmm0, xmm1
    ret

math_cbrt:
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jp .done
    je .done
    call is_finite
    jnc .done
    movsd [root_target], xmm0
    andpd xmm0, [abs_mask]
    call math_ln
    divsd xmm0, [three]
    call math_exp
    movsd xmm1, [root_target]
    movq rax, xmm1
    test rax, rax
    jns .refine
    xorpd xmm0, [sign_mask]
.refine:
    movsd xmm2, xmm0
    mulsd xmm2, xmm0
    movsd xmm3, xmm2
    mulsd xmm3, xmm0
    subsd xmm3, [root_target]
    mulsd xmm2, [three]
    divsd xmm3, xmm2
    subsd xmm0, xmm3
.done:
    ret

math_power:
    movsd [power_base], xmm0
    movsd [power_exponent], xmm1
    xorpd xmm2, xmm2
    ucomisd xmm1, xmm2
    jne .general
    movsd xmm0, [one]
    ret
.general:
    movsd xmm0, xmm1
    call math_trunc
    ucomisd xmm0, [power_exponent]
    jne .real
    movsd xmm1, xmm0
    andpd xmm1, [abs_mask]
    ucomisd xmm1, [integer_power_limit]
    ja .real
    cvttsd2si rcx, xmm0
    mov r9, rcx
    test rcx, rcx
    jns .count
    neg rcx
.count:
    movsd xmm0, [one]
    movsd xmm1, [power_base]
.square:
    test rcx, rcx
    jz .integer_done
    test rcx, 1
    jz .skip
    mulsd xmm0, xmm1
.skip:
    mulsd xmm1, xmm1
    shr rcx, 1
    jmp .square
.integer_done:
    test r9, r9
    jns .done
    movsd xmm1, [one]
    divsd xmm1, xmm0
    movsd xmm0, xmm1
.done:
    ret
.real:
    movsd xmm0, [power_base]
    xorpd xmm2, xmm2
    ucomisd xmm0, xmm2
    jb nan_value
    jne .logarithm
    xorpd xmm0, xmm0
    ret
.logarithm:
    call math_ln
    mulsd xmm0, [power_exponent]
    jmp math_exp

align 16
abs_mask:
    dq 0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF
sign_mask:
    dq 0x8000000000000000, 0
one dq 0x3FF0000000000000
half dq 0x3FE0000000000000
three dq 0x4008000000000000
four dq 0x4010000000000000
ten dq 0x4024000000000000
pi dq 0x400921FB54442D18
half_pi dq 0x3FF921FB54442D18
pio2_high dq 0x3FF921FB54400000
pio2_low dq 0x3DD0B4611A626331
two_over_pi dq 0x3FE45F306DC9C883
euler dq 0x4005BF0A8B145769
ln2 dq 0x3FE62E42FEFA39EF
inverse_ln2 dq 0x3FF71547652B82FE
ln10 dq 0x40026BB1BBB55516
sqrt_half dq 0x3FE6A09E667F3BCD
large_integer dq 0x43CBC16D674EC800
integer_power_limit dq 0x408F400000000000
exp_high dq 0x40862E42FEFA39EF
exp_low dq 0xC0874385446D71C3
huge_scale dq 0x7E70000000000000  ; 2^1000
tiny_scale dq 0x0170000000000000
tanh_limit dq 0x4034000000000000
