; owners and rwx bits. no groups, the middle octal digit is kept on disk but nobody looks
; at it. root (uid 0) walks through everything like on any unix

ACCESS_READ = 4
ACCESS_WRITE = 2
ACCESS_EXEC = 1

MODE_FILE = 644o
MODE_DIRECTORY = 755o
MODE_PROGRAM = 755o

process_current_uid:
    call process_current_record
    mov eax, [rax + PROCESS_UID]
    ret

fs_access:
    push rax
    push rbx
    push rdx
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    call process_current_uid
    test eax, eax
    jz .allowed
    movzx edx, word [rbx + FS_ENTRY_MODE]
    cmp ax, [rbx + FS_ENTRY_OWNER]
    jne .other
    shr edx, 6
.other:
    and dl, 7
    and dl, cl
    cmp dl, cl
    jne .denied
.allowed:
    pop rdx
    pop rbx
    pop rax
    clc
    ret
.denied:
    mov byte [fs_denied], 1
    pop rdx
    pop rbx
    pop rax
    stc
    ret

fs_access_parent:
    push rax
    shl eax, FS_ENTRY_SHIFT
    movzx eax, word [fs_entries + rax + FS_ENTRY_PARENT]
    call fs_access
    pop rax
    ret

fs_owns:
    push rax
    push rbx
    shl eax, FS_ENTRY_SHIFT
    lea rbx, [fs_entries + rax]
    call process_current_uid
    test eax, eax
    jz .yes
    cmp ax, [rbx + FS_ENTRY_OWNER]
    jne .no
.yes:
    pop rbx
    pop rax
    clc
    ret
.no:
    mov byte [fs_denied], 1
    pop rbx
    pop rax
    stc
    ret

; rbx -> fresh entry with its flags and path set. new files belong to whoever made them.
; .serp files come out executable, otherwise every program apple or cc builds would need
; a chmod before it runs
fs_stamp_owner:
    push rax
    push rsi
    call process_current_uid
    mov [rbx + FS_ENTRY_OWNER], ax
    mov word [rbx + FS_ENTRY_MODE], MODE_DIRECTORY
    test byte [rbx + FS_ENTRY_FLAGS], FS_FLAG_DIRECTORY
    jnz .done
    mov word [rbx + FS_ENTRY_MODE], MODE_FILE
    lea rsi, [rbx + FS_ENTRY_PATH]
.end:
    cmp byte [rsi], 0
    je .ends
    inc rsi
    jmp .end
.ends:
    lea rax, [rbx + FS_ENTRY_PATH + 5]
    cmp rsi, rax
    jb .done
    cmp dword [rsi - 4], 'serp'
    jne .done
    cmp byte [rsi - 5], '.'
    jne .done
    mov word [rbx + FS_ENTRY_MODE], MODE_PROGRAM
.done:
    pop rsi
    pop rax
    ret

need_root:
    push rax
    call process_current_uid
    test eax, eax
    pop rax
    jz .root
    mov byte [fs_denied], 1
    stc
    ret
.root:
    clc
    ret

fs_denied db 0
