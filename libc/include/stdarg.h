#ifndef STDARG_H
#define STDARG_H

#ifdef __scc__
typedef char *va_list;
#define va_start(list, last) ((list) = (char *)&(last) + 8)
#define va_arg(list, type) (*(type *)(((list) += 8) - 8))
#define va_copy(to, from) ((to) = (from))
#define va_end(list) ((void)0)
#else
typedef __builtin_va_list va_list;
#define va_start(list, last) __builtin_va_start(list, last)
#define va_arg(list, type) __builtin_va_arg(list, type)
#define va_copy(to, from) __builtin_va_copy(to, from)
#define va_end(list) __builtin_va_end(list)
#endif

#endif
