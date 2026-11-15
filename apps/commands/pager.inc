; less / more: the same pager, PAGER_NAME is the only difference. opens a window (or takes
; the screen). vi and less keys: j k space b d u g G, / to search, n N, q to quit

SERP_STACK = 0x4000

include '../runtime/program.inc'

LINE_MAX = 65536
PAGER_WIDTH = 78 * 8 + SCREEN_PAD_X * 2
PAGER_HEIGHT = 26 * 16 + SCREEN_PAD_Y * 2
QUERY_MAX = 60
KEY_ESCAPE = 27

ATTR_PAGER_TEXT = 0x07
ATTR_PAGER_FILLER = 0x09
ATTR_PAGER_STATUS = 0x70
ATTR_PAGER_MATCH = 0x60

main:
    push rbx
    call collect_args
    xor ecx, ecx
    call use_stdin
    cmp dword [arg_count], 1
    jne .usage
    mov rdi, [arg_list]
    call load_input
    jc .failed
    call index_lines

    mov rsi, [arg_list]
    lea rax, [piped_title]
    cmp word [rsi], '-'
    je .titled
    call base_name
.titled:
    mov [title], rax
    mov rdi, rax
    mov esi, PAGER_WIDTH
    mov edx, PAGER_HEIGHT
    call screen_open
    mov byte [running], 1

.loop:
    call render
    cmp byte [running], 0
    je .exit
    xor esi, esi
    call screen_wait
    cmp eax, SCREEN_KEY
    je .key
    cmp eax, SCREEN_RESIZE
    je .resize
    cmp eax, SCREEN_CLOSE
    je .exit
    jmp .loop
.key:
    mov edi, [screen_key]
    call handle_key
    jmp .loop
.resize:
    call render
    call screen_acknowledge_resize
    jmp .loop

.exit:
    call screen_close
    xor eax, eax
    pop rbx
    ret
.failed:
    mov eax, 1
    pop rbx
    ret
.usage:
    pop rbx
    lea rdi, [usage_text]
    jmp usage

index_lines:
    lea rdi, [file_data]
    mov esi, [file_size]
    add rsi, rdi
    xor ecx, ecx
.next:
    cmp rdi, rsi
    jae .done
    cmp ecx, LINE_MAX
    jae .done
    mov rax, rdi
    sub rax, file_data
    mov [line_starts + rcx * 4], eax
    inc ecx
    push rcx
    call line_end_at
    pop rcx
    lea rdi, [rax + 1]
    jmp .next
.done:
    mov [line_total], ecx
    ret

page_rows:
    mov eax, [screen_rows]
    dec eax
    jg .done
    mov eax, 1
.done:
    ret

; top line that still fills the screen, so G doesn't scroll into emptiness
last_top:
    call page_rows
    mov ecx, eax
    mov eax, [line_total]
    sub eax, ecx
    jns .done
    xor eax, eax
.done:
    ret

scroll_by:
    mov eax, [top]
    add eax, edi
    jns .clamp
    xor eax, eax
.clamp:
    push rax
    call last_top
    pop rcx
    cmp ecx, eax
    jbe .store
    mov ecx, eax
.store:
    mov [top], ecx
    ret

handle_key:
    mov byte [message_shown], 0
    cmp byte [searching], 0
    jne search_key
    mov eax, edi
    cmp eax, 'q'
    je .quit
    cmp eax, 'Q'
    je .quit
    cmp eax, KEY_ESCAPE
    je .quit
    cmp eax, 'j'
    je .down
    cmp eax, 'e'
    je .down
    cmp eax, 10
    je .down
    cmp eax, KEY_DOWN
    je .down
    cmp eax, 'k'
    je .up
    cmp eax, 'y'
    je .up
    cmp eax, KEY_UP
    je .up
    cmp eax, ' '
    je .page_down
    cmp eax, 'f'
    je .page_down
    cmp eax, 6
    je .page_down
    cmp eax, KEY_PAGE_DOWN
    je .page_down
    cmp eax, 'b'
    je .page_up
    cmp eax, 2
    je .page_up
    cmp eax, KEY_PAGE_UP
    je .page_up
    cmp eax, 'd'
    je .half_down
    cmp eax, 4
    je .half_down
    cmp eax, 'u'
    je .half_up
    cmp eax, 21
    je .half_up
    cmp eax, 'g'
    je .first
    cmp eax, '<'
    je .first
    cmp eax, KEY_HOME
    je .first
    cmp eax, 'G'
    je .last
    cmp eax, '>'
    je .last
    cmp eax, KEY_END
    je .last
    cmp eax, '/'
    je .search
    cmp eax, 'n'
    je .next_match
    cmp eax, 'N'
    je .previous_match
    ret
.quit:
    mov byte [running], 0
    ret
.down:
    mov edi, 1
    jmp scroll_by
.up:
    mov edi, -1
    jmp scroll_by
.page_down:
    call page_rows
    mov edi, eax
    jmp scroll_by
.page_up:
    call page_rows
    neg eax
    mov edi, eax
    jmp scroll_by
.half_down:
    call page_rows
    shr eax, 1
    mov edi, eax
    jmp scroll_by
.half_up:
    call page_rows
    shr eax, 1
    neg eax
    mov edi, eax
    jmp scroll_by
.first:
    mov dword [top], 0
    ret
.last:
    call last_top
    mov [top], eax
    ret
.search:
    mov byte [searching], 1
    mov dword [query_length], 0
    mov byte [query], 0
    ret
.next_match:
    mov edi, 1
    jmp find_match
.previous_match:
    mov edi, -1
    jmp find_match

search_key:
    mov eax, edi
    cmp eax, KEY_ESCAPE
    je .cancel
    cmp eax, 10
    je .run
    cmp eax, 8
    je .erase
    cmp eax, ' '
    jb .done
    cmp eax, 126
    ja .done
    mov ecx, [query_length]
    cmp ecx, QUERY_MAX
    jae .done
    mov [query + rcx], al
    inc ecx
    mov [query_length], ecx
    mov byte [query + rcx], 0
.done:
    ret
.erase:
    mov ecx, [query_length]
    test ecx, ecx
    jz .cancel
    dec ecx
    mov [query_length], ecx
    mov byte [query + rcx], 0
    ret
.cancel:
    mov byte [searching], 0
    ret
.run:
    mov byte [searching], 0
    mov eax, [query_length]
    mov [pattern_length], eax
    lea rdi, [pattern]
    lea rsi, [query]
    call str_copy
    mov edi, 1

; edi = 1 forward, -1 back, starting next to the top line. stops at the ends, no wrap
find_match:
    cmp dword [pattern_length], 0
    je .done
    push rbx
    push r12
    mov r12d, edi
    mov ebx, [top]
.line:
    add ebx, r12d
    js .missing
    cmp ebx, [line_total]
    jae .missing
    mov edi, ebx
    call line_bounds
    call contains_pattern
    jnc .line
    mov [top], ebx
    xor edi, edi
    call scroll_by
    pop r12
    pop rbx
.done:
    ret
.missing:
    mov byte [message_shown], 1
    pop r12
    pop rbx
    ret

line_bounds:
    mov eax, [line_starts + rdi * 4]
    lea rsi, [file_data + rax]
    mov ecx, [file_size]
    lea rdx, [file_data + rcx]
    push rsi
    mov rdi, rsi
    mov rsi, rdx
    call line_end_at
    mov rdx, rax
    pop rsi
    ret

contains_pattern:
    mov ecx, [pattern_length]
.start:
    mov rax, rdx
    sub rax, rsi
    cmp rax, rcx
    jb .no
    xor r8d, r8d
.compare:
    cmp r8d, ecx
    jae .yes
    mov al, [rsi + r8]
    cmp al, [pattern + r8]
    jne .advance
    inc r8d
    jmp .compare
.advance:
    inc rsi
    jmp .start
.yes:
    stc
    ret
.no:
    clc
    ret

render:
    push rbx
    push r12
    push r13
    push r14
    call screen_clear
    xor r12d, r12d
.row:
    call page_rows
    cmp r12d, eax
    jae .status
    mov ebx, [top]
    add ebx, r12d
    cmp ebx, [line_total]
    jae .filler
    mov edi, ebx
    call line_bounds
    mov r13, rsi
    mov r14, rdx
    xor ebx, ebx
.char:
    cmp r13, r14
    jae .next_row
    cmp ebx, [screen_columns]
    jae .next_row
    mov cl, ATTR_PAGER_TEXT
    call match_here
    jnc .plain
    mov cl, ATTR_PAGER_MATCH
.plain:
    mov dl, [r13]
    cmp dl, 9
    jne .put
    mov dl, ' '
    mov edi, ebx
    mov esi, r12d
    call screen_put
    inc ebx
    test ebx, 3
    jnz .plain
    inc r13
    jmp .char
.put:
    mov edi, ebx
    mov esi, r12d
    call screen_put
    inc ebx
    inc r13
    jmp .char
.filler:
    xor edi, edi
    mov esi, r12d
    mov dl, '~'
    mov cl, ATTR_PAGER_FILLER
    call screen_put
.next_row:
    inc r12d
    jmp .row

.status:
    call render_status
    call screen_present
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

match_here:
    mov eax, [pattern_length]
    test eax, eax
    jz .no
    mov r8, r13
    mov r9d, eax
.back:
    mov rsi, r8
    mov rdx, r14
    push rcx
    call contains_at
    pop rcx
    jc .yes
    dec r9d
    jz .no
    dec r8
    cmp r8, file_data
    jb .no
    jmp .back
.yes:
    stc
    ret
.no:
    clc
    ret

contains_at:
    mov ecx, [pattern_length]
    mov rax, rdx
    sub rax, rsi
    cmp rax, rcx
    jb .no
    mov rax, rsi
    add rax, rcx
    cmp rax, r13
    jbe .no
    xor eax, eax
.compare:
    cmp eax, ecx
    jae .yes
    mov r10b, [rsi + rax]
    cmp r10b, [pattern + rax]
    jne .no
    inc eax
    jmp .compare
.yes:
    stc
    ret
.no:
    clc
    ret

render_status:
    call page_rows
    mov r12d, eax
    xor ebx, ebx
.fill:
    cmp ebx, [screen_columns]
    jae .text
    mov edi, ebx
    mov esi, r12d
    mov dl, ' '
    mov cl, ATTR_PAGER_STATUS
    call screen_put
    inc ebx
    jmp .fill
.text:
    cmp byte [searching], 0
    jne .query
    cmp byte [message_shown], 0
    jne .not_found
    lea rdi, [status_text]
    mov rsi, [title]
    call str_copy
    lea rsi, [lines_text]
    call status_append
    mov edi, [top]
    inc edi
    call status_number
    lea rsi, [dash_text]
    call status_append
    call page_rows
    add eax, [top]
    cmp eax, [line_total]
    jbe .shown
    mov eax, [line_total]
.shown:
    mov edi, eax
    call status_number
    lea rsi, [of_text]
    call status_append
    mov edi, [line_total]
    call status_number
    call last_top
    cmp [top], eax
    jb .draw
    lea rsi, [end_text]
    call status_append
    jmp .draw
.query:
    lea rdi, [status_text]
    lea rsi, [slash_text]
    call str_copy
    lea rsi, [query]
    call status_append
    jmp .draw
.not_found:
    lea rdi, [status_text]
    lea rsi, [not_found_text]
    call str_copy
.draw:
    lea rbx, [status_text]
    xor r13d, r13d
.draw_char:
    mov dl, [rbx]
    test dl, dl
    jz .done
    lea edi, [r13 + 1]
    mov esi, r12d
    mov cl, ATTR_PAGER_STATUS
    call screen_put
    inc rbx
    inc r13d
    jmp .draw_char
.done:
    ret

status_number:
    call u64_to_dec
    mov rsi, rax

status_append:
    push rsi
    lea rdi, [status_text]
    call str_len
    pop rsi
    lea rdi, [status_text + rax]
    jmp str_copy

program_name db PAGER_NAME, 0
usage_text db 'usage: ', PAGER_NAME, ' file', 0
piped_title db 'standard input', 0
lines_text db '  lines ', 0
dash_text db '-', 0
of_text db ' of ', 0
end_text db '  (END)', 0
slash_text db '/', 0
not_found_text db 'Pattern not found  (press any key)', 0

include 'common.inc'
include '../runtime/draw.inc'
include '../runtime/gui.inc'
include '../runtime/screen.inc'
include '../runtime/library.inc'
include 'common_bss.inc'
include '../runtime/screen_bss.inc'

align 8
title rq 1
line_total rd 1
top rd 1
query_length rd 1
pattern_length rd 1
running rb 1
searching rb 1
message_shown rb 1
query rb QUERY_MAX + 2
pattern rb QUERY_MAX + 2
status_text rb 160
line_starts rd LINE_MAX
