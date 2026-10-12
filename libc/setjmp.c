#include <setjmp.h>

#ifdef __scc__
int setjmp(jmp_buf env)
{
    asm("    mov rax, qword [rbp+16]\n"
        "    mov rcx, qword [rbp]\n"
        "    mov qword [rax+8], rcx\n"
        "    lea rcx, [rbp+16]\n"
        "    mov qword [rax+48], rcx\n"
        "    mov rcx, qword [rbp+8]\n"
        "    mov qword [rax+56], rcx");
    return 0;
}

void longjmp(jmp_buf env, int value)
{
    asm("    mov rdx, qword [rbp+16]\n"
        "    mov eax, dword [rbp+24]\n"
        "    test eax, eax\n"
        "    jnz .resume\n"
        "    mov eax, 1\n"
        ".resume:\n"
        "    mov rbp, qword [rdx+8]\n"
        "    mov rsp, qword [rdx+48]\n"
        "    jmp qword [rdx+56]");
    for (;;)
        ;
}
#else
__asm__(".globl setjmp\n"
        "setjmp:\n"
        "    mov %rbx, (%rdi)\n"
        "    mov %rbp, 8(%rdi)\n"
        "    mov %r12, 16(%rdi)\n"
        "    mov %r13, 24(%rdi)\n"
        "    mov %r14, 32(%rdi)\n"
        "    mov %r15, 40(%rdi)\n"
        "    lea 8(%rsp), %rdx\n"
        "    mov %rdx, 48(%rdi)\n"
        "    mov (%rsp), %rdx\n"
        "    mov %rdx, 56(%rdi)\n"
        "    xor %eax, %eax\n"
        "    ret\n"
        ".globl longjmp\n"
        "longjmp:\n"
        "    mov %esi, %eax\n"
        "    test %eax, %eax\n"
        "    jnz 1f\n"
        "    inc %eax\n"
        "1:  mov (%rdi), %rbx\n"
        "    mov 8(%rdi), %rbp\n"
        "    mov 16(%rdi), %r12\n"
        "    mov 24(%rdi), %r13\n"
        "    mov 32(%rdi), %r14\n"
        "    mov 40(%rdi), %r15\n"
        "    mov 48(%rdi), %rsp\n"
        "    jmp *56(%rdi)\n");
#endif
