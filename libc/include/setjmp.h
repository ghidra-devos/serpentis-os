#ifndef SETJMP_H
#define SETJMP_H

typedef long jmp_buf[8];

int setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int value);

#endif
