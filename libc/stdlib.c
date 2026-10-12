#include <ctype.h>
#include <serpentis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GROW_BYTES 65536
#define HANDLER_MAX 16

struct block {
    size_t size;
    struct block *next;
};

int errno;

static struct block *free_list;
static void (*exit_handlers[HANDLER_MAX])(void);
static int exit_handler_count;
static unsigned long random_state = 1;

static size_t round_up(size_t value)
{
    return (value + sizeof(struct block) - 1) & ~(sizeof(struct block) - 1);
}

static void release(struct block *freed)
{
    struct block **link = &free_list;
    while (*link && *link < freed)
        link = &(*link)->next;
    freed->next = *link;
    *link = freed;
    if (freed->next && (char *)freed + freed->size == (char *)freed->next) {
        freed->size += freed->next->size;
        freed->next = freed->next->next;
    }
    if (link != &free_list) {
        struct block *previous = (struct block *)((char *)link - offsetof(struct block, next));
        if ((char *)previous + previous->size == (char *)freed) {
            previous->size += freed->size;
            previous->next = freed->next;
        }
    }
}

static int grow(size_t needed)
{
    size_t length = needed > GROW_BYTES ? needed : GROW_BYTES;
    length = (length + 4095) & ~(size_t)4095;
    struct block *fresh = serp_alloc(length);
    if ((long)fresh < 0)
        return 0;
    fresh->size = length;
    release(fresh);
    return 1;
}

void *malloc(size_t length)
{
    if (!length)
        return 0;
    size_t needed = round_up(length) + sizeof(struct block);
    for (int attempt = 0; attempt < 2; attempt++) {
        struct block **link = &free_list;
        while (*link) {
            struct block *candidate = *link;
            if (candidate->size >= needed) {
                if (candidate->size - needed >= 2 * sizeof(struct block)) {
                    struct block *rest = (struct block *)((char *)candidate + needed);
                    rest->size = candidate->size - needed;
                    rest->next = candidate->next;
                    *link = rest;
                    candidate->size = needed;
                } else {
                    *link = candidate->next;
                }
                return candidate + 1;
            }
            link = &candidate->next;
        }
        if (!grow(needed))
            return 0;
    }
    return 0;
}

void free(void *pointer)
{
    if (pointer)
        release((struct block *)pointer - 1);
}

void *calloc(size_t count, size_t length)
{
    size_t total = count * length;
    if (length && total / length != count)
        return 0;
    void *pointer = malloc(total);
    if (pointer)
        memset(pointer, 0, total);
    return pointer;
}

void *realloc(void *pointer, size_t length)
{
    if (!pointer)
        return malloc(length);
    if (!length) {
        free(pointer);
        return 0;
    }
    struct block *current = (struct block *)pointer - 1;
    size_t available = current->size - sizeof(struct block);
    if (available >= length)
        return pointer;
    void *moved = malloc(length);
    if (moved) {
        memcpy(moved, pointer, available);
        free(pointer);
    }
    return moved;
}

int atexit(void (*handler)(void))
{
    if (exit_handler_count >= HANDLER_MAX)
        return -1;
    exit_handlers[exit_handler_count++] = handler;
    return 0;
}

void exit(int code)
{
    while (exit_handler_count)
        exit_handlers[--exit_handler_count]();
    fflush(0);
    serp_exit(code);
}

void abort(void)
{
    serp_exit(134);
}

long strtol(const char *text, char **end, int base)
{
    while (isspace(*text))
        text++;
    int negative = 0;
    if (*text == '-' || *text == '+')
        negative = *text++ == '-';
    unsigned long value = strtoul(text, end, base);
    return negative ? -(long)value : (long)value;
}

unsigned long strtoul(const char *text, char **end, int base)
{
    while (isspace(*text))
        text++;
    if ((base == 0 || base == 16) && text[0] == '0' && (text[1] | 32) == 'x') {
        text += 2;
        base = 16;
    } else if (base == 0) {
        base = text[0] == '0' ? 8 : 10;
    }
    unsigned long value = 0;
    for (;; text++) {
        int digit;
        if (isdigit(*text))
            digit = *text - '0';
        else if (isalpha(*text))
            digit = (*text | 32) - 'a' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * base + digit;
    }
    if (end)
        *end = (char *)text;
    return value;
}

int atoi(const char *text)
{
    return (int)strtol(text, 0, 10);
}

long atol(const char *text)
{
    return strtol(text, 0, 10);
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

long labs(long value)
{
    return value < 0 ? -value : value;
}

int rand(void)
{
    random_state = random_state * 1103515245 + 12345;
    return (int)((random_state >> 16) & RAND_MAX);
}

void srand(unsigned int seed)
{
    random_state = seed;
}

static void swap(char *a, char *b, size_t size)
{
    while (size--) {
        char held = *a;
        *a++ = *b;
        *b++ = held;
    }
}

void qsort(void *items, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    char *base = items;
    for (size_t gap = count / 2; gap; gap /= 2)
        for (size_t i = gap; i < count; i++)
            for (size_t j = i; j >= gap && compare(base + (j - gap) * size, base + j * size) > 0; j -= gap)
                swap(base + (j - gap) * size, base + j * size, size);
}

void *bsearch(const void *key, const void *items, size_t count, size_t size,
              int (*compare)(const void *, const void *))
{
    const char *base = items;
    size_t low = 0, high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const void *item = base + middle * size;
        int order = compare(key, item);
        if (order == 0)
            return (void *)item;
        if (order < 0)
            high = middle;
        else
            low = middle + 1;
    }
    return 0;
}

long long atoll(const char *text)
{
    return strtol(text, 0, 10);
}

long long strtoll(const char *text, char **end, int base)
{
    return strtol(text, end, base);
}

unsigned long long strtoull(const char *text, char **end, int base)
{
    return strtoul(text, end, base);
}

long long llabs(long long value)
{
    return value < 0 ? -value : value;
}

div_t div(int numerator, int denominator)
{
    div_t result;
    result.quot = numerator / denominator;
    result.rem = numerator % denominator;
    return result;
}

ldiv_t ldiv(long numerator, long denominator)
{
    ldiv_t result;
    result.quot = numerator / denominator;
    result.rem = numerator % denominator;
    return result;
}

char *getenv(const char *name)
{
    (void)name;
    return 0;
}

int system(const char *command)
{
    char path[256];
    size_t length = 0;
    while (command[length] && command[length] != ' ' && length < 200)
        length++;
    if (!length)
        return -1;
    if (memchr(command, '/', length)) {
        memcpy(path, command, length);
        path[length] = 0;
    } else {
        memcpy(path, "/bin/", 5);
        memcpy(path + 5, command, length);
        memcpy(path + 5 + length, ".serp", 6);
    }
    while (command[length] == ' ')
        length++;
    fflush(0);
    int pid = serp_spawn(path, command + length);
    if (pid < 0)
        return -1;
    return serp_waitpid(pid, 0);
}

void serp_assert_failed(const char *condition, const char *file, int line)
{
    fflush(stdout);
    fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, condition);
    exit(134);
}

static double power_of_ten(int count)
{
    double power = 1;
    while (count-- > 0)
        power *= 10;
    return power;
}

static double scale_decimal(unsigned long mantissa, int exponent)
{
    double value = (double)mantissa;
    while (exponent > 22) {
        value *= 1e22;
        exponent -= 22;
    }
    while (exponent < -22) {
        value /= 1e22;
        exponent += 22;
    }
    if (exponent >= 0)
        return value * power_of_ten(exponent);
    return value / power_of_ten(-exponent);
}

double strtod(const char *text, char **end)
{
    const char *p = text;
    while (isspace(*p))
        p++;
    int negative = 0;
    if (*p == '+' || *p == '-')
        negative = *p++ == '-';
    double value = 0;
    int digits = 0;
    if ((p[0] | 32) == 'i' && (p[1] | 32) == 'n' && (p[2] | 32) == 'f') {
        value = 1e300 * 1e300;
        p += 3;
        digits = 1;
    } else if ((p[0] | 32) == 'n' && (p[1] | 32) == 'a' && (p[2] | 32) == 'n') {
        value = (1e300 * 1e300) - (1e300 * 1e300);
        p += 3;
        digits = 1;
    } else {
        unsigned long mantissa = 0;
        int significant = 0, exponent = 0;
        while (isdigit(*p)) {
            if (significant < 19) {
                mantissa = mantissa * 10 + (*p - '0');
                if (mantissa)
                    significant++;
            } else {
                exponent++;
            }
            p++;
            digits++;
        }
        if (*p == '.') {
            p++;
            while (isdigit(*p)) {
                if (significant < 19) {
                    mantissa = mantissa * 10 + (*p - '0');
                    if (mantissa)
                        significant++;
                    exponent--;
                }
                p++;
                digits++;
            }
        }
        if (digits && (*p | 32) == 'e') {
            const char *mark = p++;
            int exponent_negative = 0, written = 0;
            if (*p == '+' || *p == '-')
                exponent_negative = *p++ == '-';
            if (isdigit(*p)) {
                while (isdigit(*p)) {
                    if (written < 10000)
                        written = written * 10 + (*p - '0');
                    p++;
                }
                exponent += exponent_negative ? -written : written;
            } else {
                p = mark;
            }
        }
        value = scale_decimal(mantissa, exponent);
    }
    if (end)
        *end = (char *)(digits ? p : text);
    return negative ? -value : value;
}

double atof(const char *text)
{
    return strtod(text, 0);
}
