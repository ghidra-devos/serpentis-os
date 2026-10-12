#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

void *malloc(size_t length);
void *calloc(size_t count, size_t length);
void *realloc(void *block, size_t length);
void free(void *block);
__attribute__((noreturn)) void exit(int code);
__attribute__((noreturn)) void abort(void);
int atexit(void (*handler)(void));
int atoi(const char *text);
long atol(const char *text);
long strtol(const char *text, char **end, int base);
unsigned long strtoul(const char *text, char **end, int base);
int abs(int value);
long labs(long value);
int rand(void);
void srand(unsigned int seed);
void qsort(void *items, size_t count, size_t size, int (*compare)(const void *, const void *));
void *bsearch(const void *key, const void *items, size_t count, size_t size,
              int (*compare)(const void *, const void *));
long long atoll(const char *text);
double strtod(const char *text, char **end);
double atof(const char *text);
long long strtoll(const char *text, char **end, int base);
unsigned long long strtoull(const char *text, char **end, int base);
long long llabs(long long value);
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
char *getenv(const char *name);
int system(const char *command);

#endif
