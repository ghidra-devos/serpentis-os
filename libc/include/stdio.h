#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define BUFSIZ 4096
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int putchar(int c);
int puts(const char *text);
int getchar(void);
char *gets_s(char *buffer, size_t length);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t length, const char *format, ...);
int vprintf(const char *format, va_list arguments);
int vfprintf(FILE *stream, const char *format, va_list arguments);
int vsnprintf(char *buffer, size_t length, const char *format, va_list arguments);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *data, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *data, size_t size, size_t count, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *text, FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *buffer, int length, FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);
int remove(const char *path);
int rename(const char *from, const char *to);
int ungetc(int c, FILE *stream);
int getc(FILE *stream);
int putc(int c, FILE *stream);
void perror(const char *prefix);
int sscanf(const char *text, const char *format, ...);
int vsscanf(const char *text, const char *format, va_list arguments);
int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);

#endif
