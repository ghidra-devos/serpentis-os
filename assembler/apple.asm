SERP_STACK = 0x8000

include '../apps/runtime/program.inc'

; apple, the assembler. fasm syntax (the subset this repo uses plus macros, conditionals,
; repeat/times), x86-64 only, writes a SERP executable with the header filled in by itself.
; multi pass: every pass assembles the whole source again with the label values of the pass
; before, until nothing moves. then one final pass that is allowed to print errors

SOURCE_POOL_SIZE = 0x200000
OUTPUT_SIZE = 0x80000
PATH_POOL_SIZE = 0x2000
STRING_POOL_SIZE = 0x1000
READ_CHUNK = 0x10000
PATH_LIMIT = 240
PASS_LIMIT = 24
PASS_SETTLE = 12                ; from here unsure forward branches are forced near so the layout can't flip
BRANCH_MAX = 0x4000
SYM_FLAG_UNRESOLVED = 8
DEFINE_MAX = 16
INCLUDE_DIR_MAX = 8

main:
    push rbx
    push r12

    mov [arg_cursor], rdi
    mov dword [define_count], 0
    mov dword [include_dir_count], 0
    mov byte [quiet], 0
    mov byte [have_source], 0
    mov byte [have_output], 0

.parse:
    mov rdi, [arg_cursor]
    call arg_next
    mov [arg_cursor], rdx
    test rax, rax
    jz .parsed
    mov rbx, rax
    cmp byte [rbx], '-'
    jne .positional
    cmp byte [rbx + 1], 0
    je .positional

    mov rdi, rbx
    lea rsi, [option_help_word]
    call str_equal
    test eax, eax
    jnz .help
    mov rdi, rbx
    lea rsi, [option_version_word]
    call str_equal
    test eax, eax
    jnz .version

    movzx eax, byte [rbx + 1]
    cmp al, 'o'
    je .output
    cmp al, 'D'
    je .define
    cmp al, 'I'
    je .include_dir
    cmp al, 'q'
    je .quiet
    cmp al, 'h'
    je .help
    cmp al, 'v'
    je .version
    lea rdi, [error_unknown_option]
    jmp .complain

.output:
    call option_value
    test rax, rax
    jz .no_value
    lea rdi, [output_path]
    mov rsi, rax
    call str_copy
    mov byte [have_output], 1
    jmp .parse

.define:
    call option_value
    test rax, rax
    jz .no_value
    mov rsi, rax
    call add_define
    jc .bad_define
    jmp .parse

.include_dir:
    call option_value
    test rax, rax
    jz .no_value
    mov ecx, [include_dir_count]
    cmp ecx, INCLUDE_DIR_MAX
    jae .too_many_dirs
    mov [include_dirs + rcx * 8], rax
    inc dword [include_dir_count]
    jmp .parse

.quiet:
    mov byte [quiet], 1
    jmp .parse

.positional:
    cmp byte [have_source], 0
    jne .as_output
    lea rdi, [source_path]
    mov rsi, rbx
    call str_copy
    mov byte [have_source], 1
    jmp .parse
.as_output:
    cmp byte [have_output], 0
    jne .extra
    lea rdi, [output_path]
    mov rsi, rbx
    call str_copy
    mov byte [have_output], 1
    jmp .parse
.extra:
    lea rdi, [error_extra_arguments]
    jmp .complain

.no_value:
    lea rdi, [error_option_value]
    jmp .complain
.bad_define:
    lea rdi, [error_bad_define]
    jmp .complain
.too_many_dirs:
    lea rdi, [error_too_many_dirs]
.complain:
    call print_line
    lea rdi, [usage_text]
    call print_line
    mov eax, 2
    jmp .out

.help:
    lea rdi, [usage_text]
    call print_line
    lea rdi, [help_text]
    call print
    xor eax, eax
    jmp .out

.version:
    lea rdi, [version_text]
    call print_line
    xor eax, eax
    jmp .out

.parsed:
    cmp byte [have_source], 0
    je .usage
    cmp byte [have_output], 0
    jne .assemble
    call derive_output_path

.assemble:
    mov edi, SOURCE_POOL_SIZE
    call sys_alloc
    test rax, rax
    js .no_memory
    mov [source_pool], rax
    mov qword [origin], USER_CODE_ADDRESS
    mov dword [current_pass], 1
    mov byte [final_pass], 0
    mov byte [branch_settle], 0

.pass:
    cmp byte [final_pass], 0
    jne .settled_branches
    cmp dword [current_pass], PASS_SETTLE
    jb .settled_branches
    mov byte [branch_settle], 1
.settled_branches:
    mov qword [out_offset], 0
    mov dword [branch_index], 0
    mov qword [initialized_end], 0
    mov qword [source_used], 0
    mov qword [path_used], 0
    mov byte [global_label], 0
    mov dword [error_count], 0
    mov byte [layout_changed], 0
    call apply_defines
    call cond_reset
    call block_reset
    call macro_reset

    lea rdi, [source_path]
    call assemble_file
    call cond_finished

    cmp byte [final_pass], 0
    jne .settled
    inc dword [current_pass]
    cmp dword [current_pass], LAST_PASS
    jb .pass
    cmp dword [current_pass], PASS_LIMIT
    jae .settle
    cmp byte [layout_changed], 0
    jne .pass
.settle:
    mov byte [final_pass], 1
    jmp .pass

.settled:
    cmp dword [error_count], 0
    jne .failed

    call write_output
    jc .write_failed

    cmp byte [quiet], 0
    jne .quiet_done
    lea rdi, [output_path]
    call print
    lea rdi, [report_code]
    call print
    mov rdi, [initialized_end]
    call print_u64
    lea rdi, [report_bss]
    call print
    mov rdi, [out_offset]
    sub rdi, [initialized_end]
    call print_u64
    lea rdi, [report_tail]
    call print_line
.quiet_done:
    xor eax, eax
    jmp .out

.failed:
    lea rdi, [report_errors]
    call print
    mov edi, [error_count]
    call print_u64
    mov edi, 10
    call print_char
    mov eax, 1
    jmp .out

.write_failed:
    lea rdi, [error_write]
    call print_line
    mov eax, 1
    jmp .out

.no_memory:
    lea rdi, [error_no_memory]
    call print_line
    mov eax, 1
    jmp .out

.usage:
    lea rdi, [usage_text]
    call print_line
    mov eax, 2

.out:
    pop r12
    pop rbx
    ret

option_value:
    cmp byte [rbx + 2], 0
    je .separate
    lea rax, [rbx + 2]
    ret
.separate:
    mov rdi, [arg_cursor]
    call arg_next
    mov [arg_cursor], rdx
    ret

; -Dname=value and -Dname both work, a missing value means 1
add_define:
    push rbx
    push r12
    mov ecx, [define_count]
    cmp ecx, DEFINE_MAX
    jae .bad
    mov rbx, rsi
    xor r12d, r12d
.scan:
    mov al, [rbx + r12]
    test al, al
    jz .plain
    cmp al, '='
    je .valued
    inc r12d
    jmp .scan
.plain:
    test r12d, r12d
    jz .bad
    mov eax, 1
    jmp .store
.valued:
    test r12d, r12d
    jz .bad
    lea rdi, [rbx + r12 + 1]
    call str_len
    test eax, eax
    jz .bad
    mov ecx, eax
    lea rsi, [rbx + r12 + 1]
    call parse_number
    jc .bad
.store:
    mov ecx, [define_count]
    mov [define_values + rcx * 8], rax
    mov [define_names + rcx * 8], rbx
    mov [define_lengths + rcx * 4], r12d
    inc dword [define_count]
    pop r12
    pop rbx
    clc
    ret
.bad:
    pop r12
    pop rbx
    stc
    ret

apply_defines:
    push rbx
    xor ebx, ebx
.next:
    cmp ebx, [define_count]
    jae .done
    mov rsi, [define_names + rbx * 8]
    mov ecx, [define_lengths + rbx * 4]
    mov rdx, [define_values + rbx * 8]
    mov dword [symbol_define_flags], SYM_FLAG_CONSTANT
    call symbol_define
    and dword [rax + SYM_FLAGS], not SYM_FLAG_UNRESOLVED
    inc ebx
    jmp .next
.done:
    pop rbx
    ret

; include not found next to the file: try every -I directory. CF and rbx -> the path if found
search_include:
    push r13
    push r14
    push r15
    mov r15, [path_used]
    xor r13d, r13d
    mov rax, [include_request]
    cmp byte [rax], '/'
    je .missing
.next:
    cmp r13d, [include_dir_count]
    jae .missing
    lea rdi, [path_pool]
    add rdi, [path_used]
    mov r14, rdi
    mov rsi, [include_dirs + r13 * 8]
.directory:
    mov al, [rsi]
    test al, al
    jz .separator
    mov [rdi], al
    inc rsi
    inc rdi
    jmp .directory
.separator:
    cmp rdi, r14
    je .name
    cmp byte [rdi - 1], '/'
    je .name
    mov byte [rdi], '/'
    inc rdi
.name:
    mov rsi, [include_request]
.copy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz .copy

    mov rax, rdi
    lea rdx, [path_pool]
    sub rax, rdx
    cmp rax, PATH_POOL_SIZE - PATH_LIMIT
    jae .missing
    mov [path_used], rax

    mov rdi, r14
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    jns .found
    inc r13d
    jmp .next
.found:
    mov rbx, r14
    pop r15
    pop r14
    pop r13
    stc
    ret
.missing:
    mov [path_used], r15
    pop r15
    pop r14
    pop r13
    clc
    ret

; foo.asm -> foo.serp, anything else gets .serp appended
derive_output_path:
    lea rdi, [output_path]
    lea rsi, [source_path]
    call str_copy
    lea rdi, [output_path]
    call str_len
    cmp eax, 4
    jb .append
    lea rdi, [output_path + rax - 4]
    lea rsi, [asm_suffix]
    push rax
    call str_equal
    mov ecx, eax
    pop rax
    test ecx, ecx
    jz .append
    sub eax, 4
.append:
    lea rdi, [output_path + rax]
    lea rsi, [serp_suffix]
    jmp str_copy

assemble_file:
    push rbx
    push r12
    push r13
    sub rsp, 32

    mov [include_request], rdi
    call resolve_include_path
    mov rbx, rax

    mov rdi, rbx
    mov esi, OPEN_READ
    call sys_open
    test eax, eax
    jns .opened
    call search_include
    jnc .missing
.opened:
    mov r12d, eax

    mov r13, [source_used]
.read:
    mov edx, SOURCE_POOL_SIZE
    sub rdx, [source_used]
    jz .too_big
    cmp edx, READ_CHUNK
    jbe .chunk
    mov edx, READ_CHUNK
.chunk:
    mov edi, r12d
    mov rsi, [source_pool]
    add rsi, [source_used]
    call sys_read
    test eax, eax
    jle .loaded
    add [source_used], rax
    jmp .read

.loaded:
    mov edi, r12d
    call sys_close

    mov rax, [source_pool]
    add rax, r13
    mov [rsp], rax
    mov rax, [source_pool]
    add rax, [source_used]
    mov [rsp + 8], rax
    mov rax, [current_file]
    mov [rsp + 16], rax
    mov eax, [line_number]
    mov [rsp + 24], eax
    mov [current_file], rbx
    mov dword [line_number], 0

.line:
    mov rsi, [rsp]
    cmp rsi, [rsp + 8]
    jae .finished
    mov [line_start], rsi
.scan:
    cmp rsi, [rsp + 8]
    jae .line_ready
    cmp byte [rsi], 10
    je .line_ready
    inc rsi
    jmp .scan
.line_ready:
    mov [line_end], rsi
    inc dword [line_number]
    mov [line_rsp], rsp
    lea rax, [.line_done]
    mov [line_resume], rax
    call process_line

.line_done:
    mov rax, [line_end]
    inc rax
    mov [rsp], rax
    jmp .line

.finished:
    mov rax, [rsp + 16]
    mov [current_file], rax
    mov eax, [rsp + 24]
    mov [line_number], eax
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

.too_big:
    mov edi, r12d
    call sys_close
    lea rdi, [error_source_too_big]
    jmp fatal_file_error

.missing:
    lea rdi, [error_missing_file]

fatal_file_error:
    cmp byte [final_pass], 0
    je .counted
    push rdi
    mov rdi, rbx
    call print
    lea rdi, [error_separator]
    call print
    pop rdi
    call print_line
.counted:
    inc dword [error_count]
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

resolve_include_path:
    push rbx
    mov rbx, rdi
    lea rdi, [path_pool]
    add rdi, [path_used]
    mov rax, rdi
    push rax

    cmp byte [rbx], '/'
    je .copy
    mov rsi, [current_file]
    test rsi, rsi
    jz .copy
    mov rdx, rsi
    xor ecx, ecx
.find_slash:
    mov al, [rsi + rcx]
    test al, al
    jz .prefix
    cmp al, '/'
    jne .next_char
    lea rdx, [rsi + rcx + 1]
.next_char:
    inc ecx
    jmp .find_slash
.prefix:
    cmp rsi, rdx
    jae .copy
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    jmp .prefix

.copy:
    mov al, [rbx]
    mov [rdi], al
    inc rbx
    inc rdi
    test al, al
    jnz .copy

    mov rax, rdi
    lea rdx, [path_pool]
    sub rax, rdx
    cmp rax, PATH_POOL_SIZE - PATH_LIMIT
    jae .overflow
    mov [path_used], rax
    pop rax
    pop rbx
    ret

.overflow:
    pop rax
    pop rbx
    lea rdi, [error_too_many_files]

; rdi -> message. counts the error, prints file:line and the line itself in the final pass only
; (earlier passes see lots of not yet defined symbols), then longjmps out of the line
asm_error:
    cmp byte [final_pass], 0
    je .counted
    push rdi
    mov rdi, [current_file]
    call print
    mov edi, ':'
    call print_char
    mov edi, [line_number]
    call print_u64
    lea rdi, [error_separator]
    call print
    pop rdi
    call print_line

    mov edi, FD_STDOUT
    mov rsi, [line_start]
    mov rdx, [line_end]
    sub rdx, rsi
    call sys_write
    mov edi, 10
    call print_char

.counted:
    inc dword [error_count]
    mov rsp, [line_rsp]
    jmp qword [line_resume]

emit_byte:
    mov rdx, [out_offset]
    cmp rdx, OUTPUT_SIZE
    jae .full
    mov [output_image + SERP_HEADER_SIZE + rdx], al
    inc rdx
    mov [out_offset], rdx
    mov [initialized_end], rdx
    ret
.full:
    lea rdi, [error_output_full]
    jmp asm_error

emit_word:
    push rax
    call emit_byte
    pop rax
    shr rax, 8
    jmp emit_byte

emit_dword:
    push rax
    call emit_word
    pop rax
    shr rax, 16
    jmp emit_word

emit_qword:
    push rax
    call emit_dword
    pop rax
    shr rax, 32
    jmp emit_dword

; rcx = bytes of bss. only the part still inside the output buffer gets zeroed
reserve_bytes:
    push r8
    mov rdx, [out_offset]
    add rdx, rcx
    jc .full
    mov rax, rdx
    cmp rax, OUTPUT_SIZE
    jbe .limited
    mov rax, OUTPUT_SIZE
.limited:
    mov r8, [out_offset]
    mov [out_offset], rdx
    cmp r8, rax
    jae .done
    sub rax, r8
    lea rdi, [output_image + SERP_HEADER_SIZE]
    add rdi, r8
    mov rcx, rax
    xor eax, eax
    rep stosb
.done:
    pop r8
    ret
.full:
    pop r8
    lea rdi, [error_output_full]
    jmp asm_error

process_line:
    push rbx
    push r12
    push r13

    mov rax, [origin]
    add rax, [out_offset]
    mov [line_address], rax

    call tokenize_line
    lea rax, [tokens]
    mov [token_ptr], rax

    cmp dword [tokens + TOKEN_TYPE], TOKEN_END
    je .done
    call cond_emitting
    jc .live
    call cond_skip_line
    jmp .done
.live:
    cmp dword [tokens + TOKEN_TYPE], TOKEN_IDENT
    jne .statement

    lea rdi, [tokens + TOKEN_SIZE]
    mov sil, ':'
    call token_is_punct
    jc .label

    lea rdi, [tokens + TOKEN_SIZE]
    mov sil, '='
    call token_is_punct
    jc .constant

    lea rdi, [tokens + TOKEN_SIZE]
    lea rsi, [kw_equ]
    call token_equals
    jc .constant

    lea rdi, [tokens + TOKEN_SIZE]
    call find_data_directive
    jnc .statement
    call define_line_label
    call token_advance
    jmp .statement

.label:
    call define_line_label
    call token_advance
    call token_advance
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .done
    jmp .statement

.constant:
    add qword [token_ptr], TOKEN_SIZE * 2
    call expr_parse
    mov r12, rax
    call expect_end
    mov rsi, [tokens + TOKEN_TEXT]
    mov ecx, [tokens + TOKEN_LENGTH]
    mov rdx, r12
    mov dword [symbol_define_flags], SYM_FLAG_CONSTANT
    call symbol_define
    and dword [rax + SYM_FLAGS], not SYM_FLAG_UNRESOLVED
    cmp byte [expr_known], 0
    jne .done
    or dword [rax + SYM_FLAGS], SYM_FLAG_UNRESOLVED
    mov dword [rax + SYM_PASS], 0
    jmp .done

.statement:
    call process_statement

.done:
    pop r13
    pop r12
    pop rbx
    ret

; labels not starting with a dot become the new global scope for .local ones
define_line_label:
    mov rsi, [tokens + TOKEN_TEXT]
    mov ecx, [tokens + TOKEN_LENGTH]
    cmp byte [rsi], '.'
    je .define
    cmp ecx, SYM_NAME_MAX - 1
    jae .define
    lea rdi, [global_label]
    push rcx
    rep movsb
    pop rcx
    mov byte [rdi], 0
    mov rsi, [tokens + TOKEN_TEXT]
.define:
    mov rdx, [line_address]
    mov dword [symbol_define_flags], SYM_FLAG_LABEL
    jmp symbol_define

expect_end:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    jne .junk
    ret
.junk:
    lea rdi, [error_extra_tokens]
    jmp asm_error

process_statement:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .unknown

    call find_data_directive
    jc .data

    lea rsi, [directive_table]
    call keyword_lookup
    jc .directive

    lea rsi, [prefix_table]
    call keyword_lookup
    jc .prefix

    lea rsi, [mnemonic_table]
    call keyword_lookup
    jc .instruction

    call condition_mnemonic
    jc .instruction

    mov rdi, [token_ptr]
    call macro_lookup
    jc macro_expand

.unknown:
    lea rdi, [error_unknown_instruction]
    jmp asm_error

.data:
    call token_advance
    jmp rax

.directive:
    push rdx
    call token_advance
    pop rdx
    jmp rdx

.prefix:
    push rax
    call token_advance
    pop rax
    call emit_byte
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .missing
    jmp process_statement
.missing:
    lea rdi, [error_prefix_alone]
    jmp asm_error

.instruction:
    push rax
    push rdx
    call token_advance
    call encode_reset
    call parse_operands
    pop rdx
    pop rax
    jmp rdx

keyword_lookup:
    mov r8d, [rdi + TOKEN_LENGTH]
    mov r9, [rdi + TOKEN_TEXT]
.entry:
    movzx ecx, byte [rsi]
    test ecx, ecx
    jz .missing
    cmp ecx, r8d
    jne .skip
    xor edx, edx
.compare:
    mov al, [r9 + rdx]
    or al, 0x20
    cmp al, [rsi + 1 + rdx]
    jne .skip
    inc edx
    cmp edx, ecx
    jb .compare
    mov rdx, [rsi + 1 + rcx]
    mov eax, [rsi + 1 + rcx + 8]
    stc
    ret
.skip:
    lea rsi, [rsi + rcx + 13]
    jmp .entry
.missing:
    clc
    ret

find_data_directive:
    cmp dword [rdi + TOKEN_TYPE], TOKEN_IDENT
    jne .no
    lea rsi, [data_table]
    call keyword_lookup
    jnc .no
    mov rax, rdx
    stc
    ret
.no:
    clc
    ret

condition_mnemonic:
    mov r8d, [rdi + TOKEN_LENGTH]
    mov r9, [rdi + TOKEN_TEXT]
    lea r10, [condition_forms]
.form:
    movzx ecx, byte [r10]
    test ecx, ecx
    jz .missing
    cmp r8d, ecx
    jbe .next_form
    xor edx, edx
.prefix:
    mov al, [r9 + rdx]
    or al, 0x20
    cmp al, [r10 + 1 + rdx]
    jne .next_form
    inc edx
    cmp edx, ecx
    jb .prefix

    lea rsi, [condition_table]
.condition:
    movzx eax, byte [rsi]
    test eax, eax
    jz .next_form
    mov r11d, r8d
    sub r11d, ecx
    cmp eax, r11d
    jne .skip_condition
    xor edx, edx
.compare:
    push rcx
    add rcx, rdx
    mov al, [r9 + rcx]
    pop rcx
    or al, 0x20
    cmp al, [rsi + 1 + rdx]
    jne .skip_condition_reload
    inc edx
    cmp edx, r11d
    jb .compare
    movzx eax, byte [rsi + 1 + r11]
    mov rdx, [r10 + 1 + rcx]
    stc
    ret
.skip_condition_reload:
    movzx eax, byte [rsi]
.skip_condition:
    lea rsi, [rsi + rax + 2]
    jmp .condition
.next_form:
    movzx ecx, byte [r10]
    lea r10, [r10 + rcx + 9]
    jmp .form
.missing:
    clc
    ret

directive_db:
    mov r12d, 1
    jmp data_values
directive_dw:
    mov r12d, 2
    jmp data_values
directive_dd:
    mov r12d, 4
    jmp data_values
directive_dq:
    mov r12d, 8

data_values:
    mov rdi, [token_ptr]
    cmp r12d, 1
    jne .value
    cmp dword [rdi + TOKEN_TYPE], TOKEN_STRING
    jne .value
    lea rdi, [rdi + TOKEN_SIZE]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .string
    mov sil, ','
    call token_is_punct
    jnc .value
.string:
    mov rdi, [token_ptr]
    mov rsi, [rdi + TOKEN_TEXT]
    mov r13d, [rdi + TOKEN_LENGTH]
.string_byte:
    test r13d, r13d
    jz .string_done
    mov al, [rsi]
    push rsi
    call emit_byte
    pop rsi
    inc rsi
    dec r13d
    jmp .string_byte
.string_done:
    call token_advance
    jmp .separator

.value:
    call expr_parse
    mov ecx, r12d
    push rax
    call check_imm
    pop rax
    cmp r12d, 1
    je .byte
    cmp r12d, 2
    je .word
    cmp r12d, 4
    je .dword
    call emit_qword
    jmp .separator
.byte:
    call emit_byte
    jmp .separator
.word:
    call emit_word
    jmp .separator
.dword:
    call emit_dword

.separator:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .done
    mov sil, ','
    call token_is_punct
    jnc .junk
    call token_advance
    jmp data_values
.done:
    ret
.junk:
    lea rdi, [error_extra_tokens]
    jmp asm_error

directive_rb:
    mov r12d, 1
    jmp reserve_values
directive_rw:
    mov r12d, 2
    jmp reserve_values
directive_rd:
    mov r12d, 4
    jmp reserve_values
directive_rq:
    mov r12d, 8

reserve_values:
    call constant_count
    imul rcx, r12
    call reserve_bytes
    jmp expect_end

constant_count:
    call expr_parse
    cmp byte [expr_known], 0
    je .not_constant
    test rax, rax
    js .negative
    mov rcx, rax
    ret
.not_constant:
    lea rdi, [error_not_constant]
    jmp asm_error
.negative:
    lea rdi, [error_out_of_range]
    jmp asm_error

directive_times:
    call constant_count
    mov r12, rcx
    mov r13, [token_ptr]
    mov rdi, r13
    cmp dword [rdi + TOKEN_TYPE], TOKEN_END
    je .missing
.repeat:
    test r12, r12
    jz .done
    mov [token_ptr], r13
    push r12
    push r13
    call process_statement
    pop r13
    pop r12
    dec r12
    jmp .repeat
.done:
    ret
.missing:
    lea rdi, [error_expression]
    jmp asm_error

directive_align:
    call constant_count
    test rcx, rcx
    jz .bad
    lea rdx, [rcx - 1]
    test rdx, rcx
    jnz .bad
    mov rax, [origin]
    add rax, [out_offset]
    and rax, rdx
    jz .aligned
    sub rcx, rax
    mov rax, [out_offset]
    cmp rax, [initialized_end]
    jne .reserve
.pad:
    mov rdx, [out_offset]
    lea rax, [rdx + rcx]
    cmp rax, OUTPUT_SIZE
    ja .reserve
    mov [out_offset], rax
    lea rdi, [output_image + SERP_HEADER_SIZE]
    add rdi, rdx
    mov al, 0x90
    rep stosb
    jmp .aligned
.reserve:
    call reserve_bytes
.aligned:
    jmp expect_end
.bad:
    lea rdi, [error_out_of_range]
    jmp asm_error

directive_include:
    mov rdi, [token_ptr]
    cmp dword [rdi + TOKEN_TYPE], TOKEN_STRING
    jne .bad
    mov rsi, [rdi + TOKEN_TEXT]
    mov ecx, [rdi + TOKEN_LENGTH]
    cmp ecx, PATH_LIMIT - 1
    jae .bad
    lea rdi, [include_name]
    rep movsb
    mov byte [rdi], 0
    call token_advance
    call expect_end

    mov rax, [line_rsp]
    push rax
    push qword [line_start]
    push qword [line_end]
    lea rdi, [include_name]
    call assemble_file
    pop qword [line_end]
    pop qword [line_start]
    pop rax
    mov [line_rsp], rax
    ret
.bad:
    lea rdi, [error_include]
    jmp asm_error

directive_ignore:
    ret

write_output:
    push rbx
    mov rax, [initialized_end]
    test rax, rax
    jz .empty

    lea rdi, [output_image]
    mov dword [rdi + SERP_HEADER_MAGIC], SERP_MAGIC
    mov word [rdi + SERP_HEADER_VERSION], SERP_VERSION
    mov word [rdi + SERP_HEADER_FLAGS], SERP_FLAG_USER
    mov qword [rdi + SERP_HEADER_HEADER_SIZE], SERP_HEADER_SIZE
    mov qword [rdi + SERP_HEADER_CODE_OFFSET], SERP_HEADER_SIZE
    mov [rdi + SERP_HEADER_CODE_SIZE], rax
    mov qword [rdi + SERP_HEADER_DATA_OFFSET], 0
    mov qword [rdi + SERP_HEADER_DATA_SIZE], 0
    mov rdx, [out_offset]
    sub rdx, rax
    mov [rdi + SERP_HEADER_BSS_SIZE], rdx
    add rax, 0xFFF
    shr rax, 12
    mov [rdi + SERP_HEADER_REQUIRED_PAGES], rax

    lea rsi, [entry_symbol]
    mov ecx, entry_symbol_end - entry_symbol - 1
    call symbol_lookup
    xor edx, edx
    test dword [rax + SYM_FLAGS], SYM_FLAG_LABEL
    jz .entry_ready
    mov rdx, [rax + SYM_VALUE]
    sub rdx, [origin]
.entry_ready:
    mov [output_image + SERP_HEADER_ENTRY], rdx

    lea rsi, [stack_symbol]
    mov ecx, stack_symbol_end - stack_symbol - 1
    call symbol_lookup
    mov edx, 0x1000
    test dword [rax + SYM_FLAGS], SYM_FLAG_CONSTANT
    jz .stack_ready
    mov rdx, [rax + SYM_VALUE]
.stack_ready:
    mov [output_image + SERP_HEADER_STACK_SIZE], rdx

    lea rdi, [output_path]
    call sys_unlink
    lea rdi, [output_path]
    mov esi, OPEN_WRITE or OPEN_CREATE
    call sys_open
    test eax, eax
    js .failed
    mov ebx, eax
    mov edi, eax
    lea rsi, [output_image]
    mov rdx, [initialized_end]
    add rdx, SERP_HEADER_SIZE
    push rdx
    call sys_write
    pop rdx
    push rax
    mov edi, ebx
    call sys_close
    pop rax
    cmp rax, rdx
    jne .failed
    pop rbx
    clc
    ret

.empty:
    lea rdi, [error_empty]
    call print_line
.failed:
    pop rbx
    stc
    ret

include 'lexer.inc'
include 'symbols.inc'
include 'expr.inc'
include 'cond.inc'
include 'blocks.inc'
include 'macros.inc'
include 'operand.inc'
include 'encode.inc'
include 'tables.inc'

include '../apps/runtime/library.inc'

align 8
origin rq 1
out_offset rq 1
initialized_end rq 1
line_address rq 1
line_start rq 1
line_end rq 1
line_rsp rq 1
token_ptr rq 1
current_file rq 1
source_used rq 1
source_pool rq 1
path_used rq 1
string_pool_used rq 1
ins_imm rq 1
current_pass rd 1
line_number rd 1
error_count rd 1
token_count rd 1
operand_count rd 1
symbol_define_flags rd 1
ins_width rd 1
ins_reg_field rd 1
ins_opcode_length rd 1
ins_imm_size rd 1
ins_force_rex rd 1
ins_no_rexw rd 1
ins_high8 rd 1
displacement_size rd 1
ins_acc_opcode rd 1
ins_opcode:
    rb 4
repeat_counter rq 1
line_resume rq 1
arg_cursor rq 1
include_request rq 1
define_names rq DEFINE_MAX
define_values rq DEFINE_MAX
include_dirs rq INCLUDE_DIR_MAX
define_lengths rd DEFINE_MAX
define_count rd 1
include_dir_count rd 1
quiet rb 1
have_source rb 1
have_output rb 1
final_pass rb 1
branch_settle rb 1
branch_index rd 1
branch_slot rd 1
layout_changed rb 1
expr_estimated rb 1
align 8
macro_entry rq 1
macro_active_frame rq 1
macro_body_start rq 1
macro_body_end rq 1
macro_body_tail rq 1
macro_arg_start rq 1
macro_arg_end rq 1
macro_line_src rq 1
macro_line_stop rq 1
macro_count rd 1
macro_depth rd 1
macro_unique rd 1
cond_depth rd 1
cond_index rd 1
block_word_length rd 1
cond_active:
    rb COND_MAX
cond_taken:
    rb COND_MAX
expr_known rb 1
align 8
operand_1:
    rb OP_SIZE
operand_2:
    rb OP_SIZE
operand_3:
    rb OP_SIZE
tokens:
    rb TOKEN_SIZE * TOKEN_MAX
symbol_name:
    rb SYM_NAME_MAX
global_label:
    rb SYM_NAME_MAX
source_path:
    rb PATH_LIMIT
output_path:
    rb PATH_LIMIT + 8
include_name:
    rb PATH_LIMIT
string_pool:
    rb STRING_POOL_SIZE
path_pool:
    rb PATH_POOL_SIZE
macro_table:
    rb MACRO_MAX * MACRO_ENTRY
macro_frames:
    rb MACRO_DEPTH_MAX * FRAME_SIZE
symbol_table:
    rb SYM_SLOTS * SYM_SIZE
branch_size:
    rb BRANCH_MAX
branch_position:
    rd BRANCH_MAX
output_image:
    rb SERP_HEADER_SIZE + OUTPUT_SIZE
