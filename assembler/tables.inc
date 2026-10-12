; keyword tables. one entry = length byte, name, handler, a dword the handler gets in eax

macro keyword name, handler, param
{
    local .name_start, .name_end
    db .name_end - .name_start
.name_start:
    db name
.name_end:
    dq handler
    dd param
}

macro condition_form name, handler
{
    local .name_start, .name_end
    db .name_end - .name_start
.name_start:
    db name
.name_end:
    dq handler
}

macro condition name, code
{
    local .name_start, .name_end
    db .name_end - .name_start
.name_start:
    db name
.name_end:
    db code
}

data_table:
    keyword 'db', directive_db, 0
    keyword 'dw', directive_dw, 0
    keyword 'dd', directive_dd, 0
    keyword 'dq', directive_dq, 0
    keyword 'rb', directive_rb, 0
    keyword 'rw', directive_rw, 0
    keyword 'rd', directive_rd, 0
    keyword 'rq', directive_rq, 0
    db 0

; what a dead conditional branch still has to look at
cond_table:
    keyword 'if', directive_if, 0
    keyword 'else', directive_else, 0
    keyword 'end', cond_skip_end, 0
    keyword 'endif', directive_endif, 0
    db 0

directive_table:
    keyword 'if', directive_if, 0
    keyword 'else', directive_else, 0
    keyword 'end', directive_end, 0
    keyword 'endif', directive_endif, 0
    keyword 'define', directive_define, 0
    keyword 'repeat', directive_repeat, 0
    keyword 'macro', directive_macro, 0
    keyword 'times', directive_times, 0
    keyword 'align', directive_align, 0
    keyword 'include', directive_include, 0
    keyword 'format', directive_ignore, 0
    keyword 'use64', directive_ignore, 0
    keyword 'org', directive_ignore, 0
    db 0

prefix_table:
    keyword 'rep', 0, 0xF3
    keyword 'repe', 0, 0xF3
    keyword 'repz', 0, 0xF3
    keyword 'repne', 0, 0xF2
    keyword 'repnz', 0, 0xF2
    keyword 'lock', 0, 0xF0
    db 0

mnemonic_table:
    keyword 'mov', op_mov, 0
    keyword 'add', op_alu, 0
    keyword 'or', op_alu, 1
    keyword 'adc', op_alu, 2
    keyword 'sbb', op_alu, 3
    keyword 'and', op_alu, 4
    keyword 'sub', op_alu, 5
    keyword 'xor', op_alu, 6
    keyword 'cmp', op_alu, 7
    keyword 'test', op_test, 0
    keyword 'lea', op_lea, 0
    keyword 'not', op_unary, 0xF602  ; opcode << 8 | /r
    keyword 'neg', op_unary, 0xF603
    keyword 'mul', op_unary, 0xF604
    keyword 'div', op_unary, 0xF606
    keyword 'idiv', op_unary, 0xF607
    keyword 'inc', op_unary, 0xFE00
    keyword 'dec', op_unary, 0xFE01
    keyword 'imul', op_imul, 0
    keyword 'rol', op_shift, 0
    keyword 'ror', op_shift, 1
    keyword 'rcl', op_shift, 2
    keyword 'rcr', op_shift, 3
    keyword 'shl', op_shift, 4
    keyword 'sal', op_shift, 4
    keyword 'shr', op_shift, 5
    keyword 'sar', op_shift, 7
    keyword 'push', op_push_pop, 0x50
    keyword 'pop', op_push_pop, 0x58
    keyword 'jmp', op_branch, 0x0004EBE9
    keyword 'call', op_branch, 0x000200E8
    keyword 'int', op_int, 0
    keyword 'ret', op_ret, 0
    keyword 'movzx', op_extend, 0xB6
    keyword 'movsx', op_extend, 0xBE
    keyword 'movsxd', op_movsxd, 0
    keyword 'xchg', op_xchg, 0
    keyword 'bt', op_bit, 0
    keyword 'bts', op_bit, 1
    keyword 'btr', op_bit, 2
    keyword 'btc', op_bit, 3
    keyword 'nop', op_plain, 0x90  ; raw bytes, low byte first: 0x9866 = 66 98
    keyword 'hlt', op_plain, 0xF4
    keyword 'cld', op_plain, 0xFC
    keyword 'clc', op_plain, 0xF8
    keyword 'stc', op_plain, 0xF9
    keyword 'cmc', op_plain, 0xF5
    keyword 'lahf', op_plain, 0x9F
    keyword 'sahf', op_plain, 0x9E
    keyword 'cbw', op_plain, 0x9866
    keyword 'cwde', op_plain, 0x98
    keyword 'cwd', op_plain, 0x9966
    keyword 'rdtsc', op_plain, 0x310F
    keyword 'cpuid', op_plain, 0xA20F
    keyword 'std', op_plain, 0xFD
    keyword 'cli', op_plain, 0xFA
    keyword 'sti', op_plain, 0xFB
    keyword 'syscall', op_plain, 0x050F
    keyword 'cqo', op_plain, 0x9948
    keyword 'cdq', op_plain, 0x99
    keyword 'cdqe', op_plain, 0x9848
    keyword 'leave', op_plain, 0xC9
    keyword 'int3', op_plain, 0xCC
    keyword 'ud2', op_plain, 0x0B0F
    keyword 'pushfq', op_plain, 0x9C
    keyword 'popfq', op_plain, 0x9D
    keyword 'iretq', op_plain, 0xCF48
    keyword 'pause', op_plain, 0x90F3
    keyword 'movsb', op_plain, 0xA4
    keyword 'movsw', op_plain, 0xA566
    keyword 'movsq', op_plain, 0xA548
    keyword 'stosb', op_plain, 0xAA
    keyword 'stosw', op_plain, 0xAB66
    keyword 'stosd', op_plain, 0xAB
    keyword 'stosq', op_plain, 0xAB48
    keyword 'lodsb', op_plain, 0xAC
    keyword 'lodsw', op_plain, 0xAD66
    keyword 'lodsd', op_plain, 0xAD
    keyword 'lodsq', op_plain, 0xAD48
    keyword 'cmpsb', op_plain, 0xA6
    keyword 'cmpsw', op_plain, 0xA766
    keyword 'cmpsd', op_plain, 0xA7
    keyword 'cmpsq', op_plain, 0xA748
    keyword 'scasb', op_plain, 0xAE
    keyword 'scasw', op_plain, 0xAF66
    keyword 'scasd', op_plain, 0xAF
    keyword 'scasq', op_plain, 0xAF48
    keyword 'movsd', op_sse, 0x0110F2
    keyword 'movss', op_sse, 0x0110F3
    keyword 'addsd', op_sse, 0x0058F2
    keyword 'addss', op_sse, 0x0058F3
    keyword 'subsd', op_sse, 0x005CF2
    keyword 'subss', op_sse, 0x005CF3
    keyword 'mulsd', op_sse, 0x0059F2
    keyword 'mulss', op_sse, 0x0059F3
    keyword 'divsd', op_sse, 0x005EF2
    keyword 'divss', op_sse, 0x005EF3
    keyword 'sqrtsd', op_sse, 0x0051F2
    keyword 'sqrtss', op_sse, 0x0051F3
    keyword 'minsd', op_sse, 0x005DF2
    keyword 'maxsd', op_sse, 0x005FF2
    keyword 'ucomisd', op_sse, 0x002E66
    keyword 'ucomiss', op_sse, 0x002E00
    keyword 'comisd', op_sse, 0x002F66
    keyword 'comiss', op_sse, 0x002F00
    keyword 'cvtsd2ss', op_sse, 0x005AF2
    keyword 'cvtss2sd', op_sse, 0x005AF3
    keyword 'cvtsi2sd', op_sse, 0x022AF2
    keyword 'cvtsi2ss', op_sse, 0x022AF3
    keyword 'cvttsd2si', op_sse, 0x032CF2
    keyword 'cvttss2si', op_sse, 0x032CF3
    keyword 'cvtsd2si', op_sse, 0x032DF2
    keyword 'cvtss2si', op_sse, 0x032DF3
    keyword 'movapd', op_sse, 0x002866
    keyword 'movaps', op_sse, 0x002800
    keyword 'andpd', op_sse, 0x005466
    keyword 'andnpd', op_sse, 0x005566
    keyword 'orpd', op_sse, 0x005666
    keyword 'xorpd', op_sse, 0x005766
    keyword 'xorps', op_sse, 0x005700
    keyword 'pxor', op_sse, 0x00EF66
    keyword 'movq', op_sse, 0x040000
    db 0

condition_forms:
    condition_form 'j', op_jcc
    condition_form 'set', op_setcc
    condition_form 'cmov', op_cmovcc
    db 0

condition_table:
    condition 'o', 0
    condition 'no', 1
    condition 'b', 2
    condition 'c', 2
    condition 'nae', 2
    condition 'ae', 3
    condition 'nb', 3
    condition 'nc', 3
    condition 'e', 4
    condition 'z', 4
    condition 'ne', 5
    condition 'nz', 5
    condition 'be', 6
    condition 'na', 6
    condition 'a', 7
    condition 'nbe', 7
    condition 's', 8
    condition 'ns', 9
    condition 'p', 10
    condition 'pe', 10
    condition 'np', 11
    condition 'po', 11
    condition 'l', 12
    condition 'nge', 12
    condition 'ge', 13
    condition 'nl', 13
    condition 'le', 14
    condition 'ng', 14
    condition 'g', 15
    condition 'nle', 15
    db 0

kw_or db 'or', 0
kw_xor db 'xor', 0
kw_and db 'and', 0
kw_shl db 'shl', 0
kw_shr db 'shr', 0
kw_mod db 'mod', 0
kw_not db 'not', 0
kw_ptr db 'ptr', 0
kw_if db 'if', 0
kw_equ db 'equ', 0
kw_defined db 'defined', 0
kw_end db 'end', 0
kw_repeat db 'repeat', 0
kw_local db 'local', 0

asm_suffix db '.asm', 0
serp_suffix db '.serp', 0
entry_symbol db '_start', 0
entry_symbol_end:
stack_symbol db 'SERP_STACK', 0
stack_symbol_end:

usage_text db 'usage: apple [options] <source.asm> [output.serp]', 0
help_text db 10
          db '  -o FILE          write the output to FILE', 10
          db '  -D NAME[=VALUE]  define a constant, VALUE defaults to 1', 10
          db '  -I DIR           also look for include files in DIR', 10
          db '  -q               print nothing unless something fails', 10
          db '  -h, --help       show this text', 10
          db '  -v, --version    show the version', 10
          db 10
          db 'With no output name the source name is used with .serp.', 10, 0
version_text db 'Apple 1.0 - the serpentis-os assembler', 0
option_help_word db '--help', 0
option_version_word db '--version', 0
report_code db ': ', 0
report_bss db ' bytes, bss ', 0
report_tail db 0
report_errors db 'apple: errors: ', 0

error_line_too_long db 'too many tokens on line', 0
error_bad_number db 'invalid number', 0
error_unterminated db 'unterminated string', 0
error_name_too_long db 'symbol name too long', 0
error_symbols_full db 'symbol table full', 0
error_duplicate_label db 'label already defined', 0
error_phase db 'label moved between passes', 0
error_divide_zero db 'division by zero', 0
error_expression db 'invalid expression', 0
error_undefined db 'undefined symbol', 0
error_size_mismatch db 'operand sizes do not match', 0
error_bad_memory db 'invalid memory operand', 0
error_extra_tokens db 'extra characters on line', 0
error_high8 db 'ah, bh, ch, dh cannot be used with REX', 0
error_out_of_range db 'value out of range', 0
error_size_unknown db 'operand size not specified', 0
error_operand_count db 'wrong number of operands', 0
error_invalid_operands db 'invalid operands', 0
error_not_constant db 'value must be constant', 0
error_source_too_big db 'source too big', 0
error_no_memory db 'apple: out of memory', 0
error_missing_file db 'cannot open file', 0
error_too_many_files db 'too many include files', 0
error_output_full db 'output too big', 0
error_unknown_instruction db 'unknown instruction', 0
error_prefix_alone db 'prefix without instruction', 0
error_include db 'invalid include', 0
error_cond_deep db 'conditional blocks nested too deeply', 0
error_cond_orphan db 'else or end if without if', 0
error_cond_open db 'missing end if', 0
error_repeat_open db 'missing end repeat', 0
error_block_orphan db 'block terminator without a block', 0
error_macro_header db 'invalid macro declaration', 0
error_macro_full db 'too many macros', 0
error_macro_open db 'macro body is not closed', 0
error_macro_deep db 'macros nested too deeply', 0
error_macro_locals db 'too many local names in a macro', 0
error_write db 'apple: cannot write output', 0
error_empty db 'apple: nothing to write', 0
error_unknown_option db 'apple: unknown option', 0
error_option_value db 'apple: that option needs a value', 0
error_bad_define db 'apple: -D wants NAME or NAME=VALUE', 0
error_too_many_dirs db 'apple: too many -I directories', 0
error_extra_arguments db 'apple: too many file names', 0
