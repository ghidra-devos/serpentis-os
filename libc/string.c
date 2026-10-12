#include <stdlib.h>
#include <string.h>

void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = destination;
    const unsigned char *from = source;
    while (length--)
        *to++ = *from++;
    return destination;
}

void *memmove(void *destination, const void *source, size_t length)
{
    unsigned char *to = destination;
    const unsigned char *from = source;
    if (to < from) {
        while (length--)
            *to++ = *from++;
    } else {
        while (length--)
            to[length] = from[length];
    }
    return destination;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = destination;
    while (length--)
        *to++ = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t length)
{
    const unsigned char *a = left;
    const unsigned char *b = right;
    for (; length; length--, a++, b++)
        if (*a != *b)
            return *a - *b;
    return 0;
}

void *memchr(const void *data, int value, size_t length)
{
    const unsigned char *cursor = data;
    for (; length; length--, cursor++)
        if (*cursor == (unsigned char)value)
            return (void *)cursor;
    return 0;
}

size_t strlen(const char *text)
{
    size_t length = 0;
    while (text[length])
        length++;
    return length;
}

size_t strnlen(const char *text, size_t limit)
{
    size_t length = 0;
    while (length < limit && text[length])
        length++;
    return length;
}

char *strcpy(char *destination, const char *source)
{
    char *to = destination;
    while ((*to++ = *source++))
        ;
    return destination;
}

char *strncpy(char *destination, const char *source, size_t limit)
{
    size_t i = 0;
    for (; i < limit && source[i]; i++)
        destination[i] = source[i];
    for (; i < limit; i++)
        destination[i] = 0;
    return destination;
}

char *strcat(char *destination, const char *source)
{
    strcpy(destination + strlen(destination), source);
    return destination;
}

char *strncat(char *destination, const char *source, size_t limit)
{
    char *to = destination + strlen(destination);
    while (limit-- && *source)
        *to++ = *source++;
    *to = 0;
    return destination;
}

int strcmp(const char *left, const char *right)
{
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t limit)
{
    for (; limit; limit--, left++, right++) {
        if (*left != *right || !*left)
            return (unsigned char)*left - (unsigned char)*right;
    }
    return 0;
}

char *strchr(const char *text, int value)
{
    for (;; text++) {
        if (*text == (char)value)
            return (char *)text;
        if (!*text)
            return 0;
    }
}

char *strrchr(const char *text, int value)
{
    const char *found = 0;
    for (;; text++) {
        if (*text == (char)value)
            found = text;
        if (!*text)
            return (char *)found;
    }
}

char *strstr(const char *text, const char *needle)
{
    size_t length = strlen(needle);
    if (!length)
        return (char *)text;
    for (; *text; text++)
        if (!strncmp(text, needle, length))
            return (char *)text;
    return 0;
}

char *strdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy)
        memcpy(copy, text, length);
    return copy;
}

char *strtok(char *text, const char *separators)
{
    static char *saved;
    if (text)
        saved = text;
    if (!saved)
        return 0;
    while (*saved && strchr(separators, *saved))
        saved++;
    if (!*saved) {
        saved = 0;
        return 0;
    }
    char *token = saved;
    while (*saved && !strchr(separators, *saved))
        saved++;
    if (*saved)
        *saved++ = 0;
    else
        saved = 0;
    return token;
}

size_t strspn(const char *text, const char *accept)
{
    size_t length = 0;
    while (text[length] && strchr(accept, text[length]))
        length++;
    return length;
}

size_t strcspn(const char *text, const char *reject)
{
    size_t length = 0;
    while (text[length] && !strchr(reject, text[length]))
        length++;
    return length;
}

char *strpbrk(const char *text, const char *accept)
{
    for (; *text; text++)
        if (strchr(accept, *text))
            return (char *)text;
    return 0;
}

char *strndup(const char *text, size_t limit)
{
    size_t length = strnlen(text, limit);
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, text, length);
        copy[length] = 0;
    }
    return copy;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int strcasecmp(const char *left, const char *right)
{
    for (;; left++, right++) {
        int a = lower((unsigned char)*left), b = lower((unsigned char)*right);
        if (a != b || !a)
            return a - b;
    }
}

int strncasecmp(const char *left, const char *right, size_t limit)
{
    for (; limit; limit--, left++, right++) {
        int a = lower((unsigned char)*left), b = lower((unsigned char)*right);
        if (a != b || !a)
            return a - b;
    }
    return 0;
}

char *strerror(int code)
{
    switch (code) {
    case 0: return "no error";
    case 1: return "operation not permitted";
    case 2: return "no such file or directory";
    case 5: return "input or output error";
    case 9: return "bad file descriptor";
    case 12: return "out of memory";
    case 13: return "permission denied";
    case 14: return "bad address";
    case 16: return "device busy";
    case 17: return "file exists";
    case 20: return "not a directory";
    case 21: return "is a directory";
    case 22: return "invalid argument";
    case 28: return "no space left on device";
    case 33: return "argument out of domain";
    case 34: return "result out of range";
    }
    return "unknown error";
}
