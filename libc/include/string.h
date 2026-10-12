#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length);
void *memmove(void *destination, const void *source, size_t length);
void *memset(void *destination, int value, size_t length);
int memcmp(const void *left, const void *right, size_t length);
void *memchr(const void *data, int value, size_t length);
size_t strlen(const char *text);
size_t strnlen(const char *text, size_t limit);
char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t limit);
char *strcat(char *destination, const char *source);
char *strncat(char *destination, const char *source, size_t limit);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t limit);
char *strchr(const char *text, int value);
char *strrchr(const char *text, int value);
char *strstr(const char *text, const char *needle);
size_t strspn(const char *text, const char *accept);
size_t strcspn(const char *text, const char *reject);
char *strdup(const char *text);
char *strtok(char *text, const char *separators);
char *strpbrk(const char *text, const char *accept);
char *strndup(const char *text, size_t limit);
int strcasecmp(const char *left, const char *right);
int strncasecmp(const char *left, const char *right, size_t limit);
char *strerror(int code);

#endif
