include '/lib/start.inc'

main:
    lea rdi, [banner]
    call print_line

    lea rsi, [counted]
    mov ecx, counted_end - counted
    call sum_bytes
    call show

    lea rsi, [nested]
    mov ecx, nested_end - nested
    call sum_bytes
    call show

    lea rsi, [guarded]
    mov ecx, guarded_end - guarded
    call sum_bytes
    call show

    xor eax, eax
    ret

sum_bytes:
    xor eax, eax
.next:
    test ecx, ecx
    jz .done
    movzx edx, byte [rsi]
    add eax, edx
    inc rsi
    dec ecx
    jmp .next
.done:
    ret

show:
    mov edi, eax
    call print_u64
    mov edi, 10
    jmp print_char

banner db 'blocks:', 0

counted:
repeat 5
    db %
end repeat
counted_end:

nested:
repeat 3
    repeat 2
        db %
    end repeat
end repeat
nested_end:

guarded:
if 0
repeat 3
    db 99
end repeat
end if
repeat 4
    db 7
end repeat
guarded_end:

include '/lib/library.inc'
