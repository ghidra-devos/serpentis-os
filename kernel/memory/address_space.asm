; per process page tables and the physical page pool.

; every process gets: pml4 -> pdpt -> pd. the pd starts as the kernel's identity map of the
; first gb (2 MB pages, supervisor only) and then two entries are swapped for user page tables:
; pd[2] 4-6 MB   code + data + bss, AS_MAX_CODE_PAGES 4 KB pages
; pd[4] 8-10 MB  stack, growing down from page 0x100 (9 MB)
; pdpt[1] is the heap at 1 GB, built lazily by heap_grow.
; all tables live in the identity mapped pool, so physical == virtual for the kernel

address_space_init:
    mov dword [as_free_count], 0
    mov dword [as_next_page], AS_POOL_START

    mov qword [as_pml4], 0
    mov qword [as_pdpt], 0
    mov qword [as_pd], 0
    mov qword [as_pt_code], 0
    mov qword [as_pt_stack], 0

    mov dword [as_code_page_count], 0

    mov rdi, as_code_pages
    xor eax, eax
    mov ecx, AS_MAX_CODE_PAGES
    cld
    rep stosq
    ret

; out: rax = zeroed physical page, CF when the pool is exhausted.
; reuses freed pages first, otherwise bumps as_next_page
as_page_alloc:
    push rbx
    push rcx
    push rdi
    push rdx
    pushfq
    cli

    cmp dword [as_free_count], 0
    je .allocate_new

    dec dword [as_free_count]
    mov ecx, [as_free_count]
    mov rax, [as_free_stack + rcx * 8]
    jmp .zero

.allocate_new:
    mov eax, [as_next_page]
    add dword [as_next_page], AS_PAGE_SIZE

    cmp eax, AS_POOL_END
    jae .failed

.zero:
    mov rbx, rax
    mov rdi, rbx
    xor eax, eax
    mov ecx, AS_PAGE_SIZE / 8
    cld
    rep stosq

    mov rax, rbx

    popfq
    pop rdx
    pop rdi
    pop rcx
    pop rbx
    clc
    ret

.failed:
    popfq
    pop rdx
    pop rdi
    pop rcx
    pop rbx
    xor eax, eax
    stc
    ret

as_page_free:
    push rbx
    push rcx
    push rdx
    pushfq
    cli

    mov rbx, rax

    test rbx, rbx
    jz .failed

    cmp rbx, AS_POOL_START
    jb .failed

    cmp rbx, AS_POOL_END
    jae .failed

    test rbx, AS_PAGE_SIZE - 1
    jnz .failed

    mov ecx, [as_free_count]
    cmp ecx, AS_FREE_STACK_SIZE
    jae .failed

    mov [as_free_stack + rcx * 8], rbx
    inc dword [as_free_count]

    popfq
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.failed:
    popfq
    pop rdx
    pop rcx
    pop rbx
    stc
    ret

address_space_create:
    push rbx
    push rcx
    push rdx
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi
    mov r13d, esi
    mov r14d, edx

    mov qword [as_pml4], 0
    mov qword [as_pdpt], 0
    mov qword [as_pd], 0
    mov qword [as_pt_code], 0
    mov qword [as_pt_stack], 0
    mov qword [as_stack_page], 0
    mov dword [as_code_page_count], 0

    test r12d, r12d
    jz .failed

    test r14d, r14d
    jz .failed
    cmp r14d, USER_STACK_PAGE_INDEX + 1
    ja .failed

    add r12d, r13d
    jc .failed
    cmp r12d, AS_MAX_CODE_PAGES
    ja .failed

    mov [as_code_page_count], r12d

    mov rdi, as_code_pages
    xor eax, eax
    mov ecx, AS_MAX_CODE_PAGES
    cld
    rep stosq

    call as_page_alloc
    jc .failed
    mov [as_pml4], rax

    call as_page_alloc
    jc .failed
    mov [as_pdpt], rax

    call as_page_alloc
    jc .failed
    mov [as_pd], rax

    call as_page_alloc
    jc .failed
    mov [as_pt_code], rax

    call as_page_alloc
    jc .failed
    mov [as_pt_stack], rax

    mov rdi, [as_pd]
    mov rax, AS_PDE_KERNEL_FLAGS
    xor ecx, ecx

.build_kernel_pd:
    mov [rdi + rcx * 8], rax
    add rax, 0x200000
    inc ecx
    cmp ecx, 512
    jb .build_kernel_pd
    mov rax, [PD_ADDRESS + FRAMEBUFFER_PD_INDEX * 8]
    mov [rdi + FRAMEBUFFER_PD_INDEX * 8], rax

    mov rdi, [as_pt_code]
    xor r12d, r12d

.allocate_code_pages:
    cmp r12d, [as_code_page_count]
    jae .allocate_stack_page

    call as_page_alloc
    jc .failed

    mov [as_code_pages + r12 * 8], rax

    mov rbx, rax
    or rbx, AS_USER_PAGE_FLAGS
    mov [rdi + r12 * 8], rbx

    inc r12d
    jmp .allocate_code_pages

.allocate_stack_page:
    mov rdi, [as_pt_stack]
    mov r15d, USER_STACK_PAGE_INDEX

.next_stack_page:
    call as_page_alloc
    jc .failed

    cmp r15d, USER_STACK_PAGE_INDEX
    jne .map_stack_page
    mov [as_stack_page], rax    ; the topmost page, args are copied into it

.map_stack_page:
    mov rbx, rax
    or rbx, AS_USER_PAGE_FLAGS
    mov [rdi + r15 * 8], rbx
    dec r15d
    dec r14d
    jnz .next_stack_page

    mov rdi, [as_pml4]
    mov rax, [as_pdpt]
    or rax, AS_USER_PAGE_FLAGS
    mov [rdi], rax

    mov rdi, [as_pdpt]
    mov rax, [as_pd]
    or rax, AS_USER_PAGE_FLAGS
    mov [rdi], rax

    mov rdi, [as_pd]
    mov rax, [as_pt_code]
    or rax, AS_USER_PAGE_FLAGS
    mov [rdi + 2 * 8], rax

    mov rax, [as_pt_stack]
    or rax, AS_USER_PAGE_FLAGS
    mov [rdi + 4 * 8], rax

    mov rax, [as_pml4]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.failed:
    call as_scratch_free

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdx
    pop rcx
    pop rbx
    xor eax, eax
    stc
    ret

as_scratch_free:
    push rax
    push rbx
    push rcx
    push rsi
    push r12

    xor r12d, r12d

.free_code_pages:
    cmp r12d, [as_code_page_count]
    jae .free_stack

    mov rax, [as_code_pages + r12 * 8]
    test rax, rax
    jz .next_code

    call as_page_free
    mov qword [as_code_pages + r12 * 8], 0

.next_code:
    inc r12d
    jmp .free_code_pages

.free_stack:
    mov qword [as_stack_page], 0
    mov rsi, [as_pt_stack]
    test rsi, rsi
    jz .free_pt_code

    call as_free_pt_stack
    mov qword [as_pt_stack], 0

.free_pt_code:
    mov rax, [as_pt_code]
    test rax, rax
    jz .free_pd

    call as_page_free
    mov qword [as_pt_code], 0

.free_pd:
    mov rax, [as_pd]
    test rax, rax
    jz .free_pdpt

    call as_page_free
    mov qword [as_pd], 0

.free_pdpt:
    mov rax, [as_pdpt]
    test rax, rax
    jz .free_pml4

    call as_page_free
    mov qword [as_pdpt], 0

.free_pml4:
    mov rax, [as_pml4]
    test rax, rax
    jz .reset

    call as_page_free
    mov qword [as_pml4], 0

.reset:
    mov dword [as_code_page_count], 0

    pop r12
    pop rsi
    pop rcx
    pop rbx
    pop rax
    ret

address_space_destroy:
    push rax
    push rbx
    push rdx
    push rsi
    push r12
    push r13

    mov r13, rdi
    test r13, r13
    jz .done

    xor r12d, r12d
    xor edx, edx

    mov rax, [r13]
    test al, 1
    jz .free_pml4

    mov rbx, rax
    and rbx, 0xFFFFFFFFFFFFF000
    mov r12, rbx

    mov rax, [r12]
    test al, 1
    jz .free_pdpt

    mov rbx, rax
    and rbx, 0xFFFFFFFFFFFFF000
    mov rdx, rbx

    mov rax, [rdx + 2 * 8]
    test al, 1
    jz .no_pt_code
    mov rsi, rax
    and rsi, 0xFFFFFFFFFFFFF000
    call as_free_pt_code

.no_pt_code:
    mov rax, [rdx + 4 * 8]
    test al, 1
    jz .no_pt_stack
    mov rsi, rax
    and rsi, 0xFFFFFFFFFFFFF000
    call as_free_pt_stack

.no_pt_stack:
    mov rax, rdx
    call as_page_free

    mov rax, [r12 + 8]
    test al, 1
    jz .free_pdpt
    mov rsi, rax
    and rsi, 0xFFFFFFFFFFFFF000
    call as_free_heap

.free_pdpt:
    test r12, r12
    jz .free_pml4
    mov rax, r12
    call as_page_free

.free_pml4:
    mov rax, r13
    call as_page_free

.done:
    pop r13
    pop r12
    pop rsi
    pop rdx
    pop rbx
    pop rax
    ret

as_free_pt_code:
    push rax
    push rcx

    xor ecx, ecx

.loop:
    mov rax, [rsi + rcx * 8]
    test al, 1
    jz .next

    and rax, 0xFFFFFFFFFFFFF000
    call as_page_free

.next:
    inc ecx
    cmp ecx, AS_MAX_CODE_PAGES
    jb .loop

    mov rax, rsi
    call as_page_free

    pop rcx
    pop rax
    ret

as_free_heap:
    push rax
    push rcx
    xor ecx, ecx
.table:
    mov rax, [rsi + rcx * 8]
    test al, 1
    jz .next
    push rsi
    mov rsi, rax
    and rsi, 0xFFFFFFFFFFFFF000
    call as_free_pt_stack
    pop rsi
.next:
    inc ecx
    cmp ecx, 512
    jb .table
    mov rax, rsi
    call as_page_free
    pop rcx
    pop rax
    ret

heap_grow:
    push rbx
    push r12
    push r13
    push r14
    call process_current_record
    mov r14, rax
    mov rbx, [r14 + PROCESS_HEAP_TOP]
    test rbx, rbx
    jnz .sized
    mov rbx, HEAP_BASE
.sized:
    mov r12, rbx
    add rdi, AS_PAGE_SIZE - 1
    and rdi, not (AS_PAGE_SIZE - 1)
    lea r13, [rbx + rdi]
    mov rax, HEAP_BASE + HEAP_LIMIT
    cmp r13, rax
    ja .no_memory
.page:
    cmp rbx, r13
    jae .grown
    mov rdi, rbx
    call heap_map_page
    jc .partial
    add rbx, AS_PAGE_SIZE
    jmp .page
.grown:
    mov [r14 + PROCESS_HEAP_TOP], r13
    mov rax, r12
    jmp .out
.partial:
    mov [r14 + PROCESS_HEAP_TOP], rbx
.no_memory:
    mov rax, SYS_ERR_NO_MEMORY
.out:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

heap_map_page:
    push rbx
    push r12
    mov r12, rdi
    mov rbx, cr3
    and rbx, 0xFFFFFFFFFFFFF000
    mov rbx, [rbx]
    and rbx, 0xFFFFFFFFFFFFF000
    lea rdi, [rbx + 8]          ; pdpt[1]
    call heap_table_at
    jc .out
    mov rbx, rax
    mov rax, r12
    shr rax, 21
    and eax, 511
    lea rdi, [rbx + rax * 8]
    call heap_table_at
    jc .out
    mov rbx, rax
    call as_page_alloc
    jc .out
    or rax, AS_USER_PAGE_FLAGS
    mov rcx, r12
    shr rcx, 12
    and ecx, 511
    mov [rbx + rcx * 8], rax
    clc
.out:
    pop r12
    pop rbx
    ret

; rdi -> table entry. returns the table it points to, allocating it if it isn't there
heap_table_at:
    mov rax, [rdi]
    test al, 1
    jnz .present
    push rdi
    call as_page_alloc
    pop rdi
    jc .failed
    or rax, AS_USER_PAGE_FLAGS
    mov [rdi], rax
.present:
    and rax, 0xFFFFFFFFFFFFF000
    clc
    ret
.failed:
    stc
    ret

as_free_pt_stack:
    push rax
    push rcx

    xor ecx, ecx

.loop:
    mov rax, [rsi + rcx * 8]
    test al, 1
    jz .next

    and rax, 0xFFFFFFFFFFFFF000
    call as_page_free

.next:
    inc ecx
    cmp ecx, 512
    jb .loop

    mov rax, rsi
    call as_page_free

    pop rcx
    pop rax
    ret

as_free_count dd 0
as_next_page dd AS_POOL_START
as_free_stack = AS_FREE_STACK_ADDRESS

align 8
as_pml4 dq 0
as_pdpt dq 0
as_pd dq 0
as_pt_code dq 0
as_pt_stack dq 0
as_stack_page dq 0
as_code_page_count dd 0

align 8
as_code_pages:
    times AS_MAX_CODE_PAGES dq 0
