#include <serpentis.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define STREAM_READ 1
#define STREAM_WRITE 2
#define STREAM_END 4
#define STREAM_LINE 8
#define STREAM_TTY 16
#define STREAM_MAX 16

struct file {
    int fd;
    int flags;
    size_t used;
    size_t position;
    unsigned char buffer[BUFSIZ];
};

static struct file streams[STREAM_MAX];

FILE *stdin = &streams[0];
FILE *stdout = &streams[1];
FILE *stderr = &streams[2];

void serp_stdio_init(void)
{
    streams[0].fd = 0;
    streams[0].flags = STREAM_READ | (serp_isatty(0) > 0 ? STREAM_TTY : 0);
    streams[1].fd = 1;
    streams[1].flags = STREAM_WRITE | STREAM_LINE;
    streams[2].fd = 2;
    streams[2].flags = STREAM_WRITE;
}

static int flush_stream(FILE *stream)
{
    if (!(stream->flags & STREAM_WRITE) || !stream->used)
        return 0;
    long written = serp_write(stream->fd, stream->buffer, stream->used);
    stream->used = 0;
    return written < 0 ? EOF : 0;
}

int fflush(FILE *stream)
{
    if (stream)
        return flush_stream(stream);
    for (int i = 0; i < STREAM_MAX; i++)
        if (streams[i].flags)
            flush_stream(&streams[i]);
    return 0;
}

int fputc(int c, FILE *stream)
{
    if (!(stream->flags & STREAM_WRITE))
        return EOF;
    if (stream->used == BUFSIZ && flush_stream(stream))
        return EOF;
    stream->buffer[stream->used++] = (unsigned char)c;
    if ((stream->flags & STREAM_LINE) && c == '\n')
        flush_stream(stream);
    return (unsigned char)c;
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int fputs(const char *text, FILE *stream)
{
    while (*text)
        if (fputc(*text++, stream) == EOF)
            return EOF;
    return 0;
}

int puts(const char *text)
{
    if (fputs(text, stdout) == EOF)
        return EOF;
    return putchar('\n');
}

static long read_typed_line(FILE *stream)
{
    size_t used = 0;
    for (;;) {
        char c;
        long count = serp_read(stream->fd, &c, 1);
        if (count <= 0)
            return used ? (long)used : count;
        if (c == 8 || c == 127) {
            if (used) {
                used--;
                serp_write(1, "\b", 1);
            }
            continue;
        }
        if (c == SERP_KEY_END_OF_INPUT && !used)
            return 0;
        if ((unsigned char)c < ' ' && c != '\n' && c != '\t')
            continue;
        if (used == BUFSIZ - 1 && c != '\n')
            continue;
        stream->buffer[used++] = c;
        serp_write(1, &c, 1);
        if (c == '\n')
            return (long)used;
    }
}

int fgetc(FILE *stream)
{
    if (!(stream->flags & STREAM_READ) || (stream->flags & STREAM_END))
        return EOF;
    if (stream == stdin)
        fflush(stdout);
    if (stream->position == stream->used) {
        long count = stream->flags & STREAM_TTY ? read_typed_line(stream) : serp_read(stream->fd, stream->buffer, BUFSIZ);
        if (count <= 0) {
            stream->flags |= STREAM_END;
            return EOF;
        }
        stream->used = (size_t)count;
        stream->position = 0;
    }
    return stream->buffer[stream->position++];
}

int getchar(void)
{
    return fgetc(stdin);
}

char *fgets(char *buffer, int length, FILE *stream)
{
    int count = 0;
    while (count < length - 1) {
        int c = fgetc(stream);
        if (c == EOF)
            break;
        buffer[count++] = (char)c;
        if (c == '\n')
            break;
    }
    if (!count)
        return 0;
    buffer[count] = 0;
    return buffer;
}

char *gets_s(char *buffer, size_t length)
{
    if (!fgets(buffer, (int)length, stdin))
        return 0;
    size_t end = strlen(buffer);
    if (end && buffer[end - 1] == '\n')
        buffer[end - 1] = 0;
    return buffer;
}

int feof(FILE *stream)
{
    return (stream->flags & STREAM_END) != 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int flags = mode[0] == 'r' ? SERP_OPEN_READ : SERP_OPEN_WRITE | SERP_OPEN_CREATE;
    if (strchr(mode, '+'))
        flags |= SERP_OPEN_READ | SERP_OPEN_WRITE;
    for (int i = 3; i < STREAM_MAX; i++) {
        if (streams[i].flags)
            continue;
        int fd = serp_open(path, flags);
        if (fd < 0)
            return 0;
        streams[i].fd = fd;
        streams[i].flags = (flags & SERP_OPEN_READ ? STREAM_READ : 0) | (flags & SERP_OPEN_WRITE ? STREAM_WRITE : 0);
        streams[i].used = 0;
        streams[i].position = 0;
        if (mode[0] == 'a')
            serp_seek(fd, 0, SERP_SEEK_END);
        return &streams[i];
    }
    return 0;
}

int fclose(FILE *stream)
{
    int result = flush_stream(stream);
    serp_close(stream->fd);
    stream->flags = 0;
    return result;
}

size_t fwrite(const void *data, size_t size, size_t count, FILE *stream)
{
    const unsigned char *bytes = data;
    size_t total = size * count;
    if (total >= BUFSIZ && (stream->flags & STREAM_WRITE) && !(stream->flags & STREAM_LINE)) {
        if (flush_stream(stream))
            return 0;
        long written = serp_write(stream->fd, bytes, total);
        if (written < 0)
            return 0;
        return size ? (size_t)written / size : 0;
    }
    for (size_t i = 0; i < total; i++)
        if (fputc(bytes[i], stream) == EOF)
            return size ? i / size : 0;
    return count;
}

size_t fread(void *data, size_t size, size_t count, FILE *stream)
{
    unsigned char *bytes = data;
    size_t total = size * count;
    for (size_t i = 0; i < total; i++) {
        int c = fgetc(stream);
        if (c == EOF)
            return size ? i / size : 0;
        bytes[i] = (unsigned char)c;
    }
    return count;
}

int fseek(FILE *stream, long offset, int whence)
{
    flush_stream(stream);
    stream->used = stream->position = 0;
    stream->flags &= ~STREAM_END;
    return serp_seek(stream->fd, offset, whence) < 0 ? -1 : 0;
}

long ftell(FILE *stream)
{
    long here = serp_seek(stream->fd, 0, SERP_SEEK_CUR);
    if (stream->flags & STREAM_WRITE)
        return here + (long)stream->used;
    return here - (long)(stream->used - stream->position);
}

int remove(const char *path)
{
    return serp_unlink(path) < 0 ? -1 : 0;
}

int rename(const char *from, const char *to)
{
    return serp_rename(from, to) < 0 ? -1 : 0;
}

struct sink {
    FILE *stream;
    char *buffer;
    size_t limit;
    size_t count;
};

static void emit(struct sink *sink, char c)
{
    if (sink->stream)
        fputc(c, sink->stream);
    else if (sink->count + 1 < sink->limit)
        sink->buffer[sink->count] = c;
    sink->count++;
}

static void emit_padded(struct sink *sink, const char *text, size_t length, int width, int left, char pad)
{
    int fill = width > (int)length ? width - (int)length : 0;
    if (!left)
        while (fill-- > 0)
            emit(sink, pad);
    for (size_t i = 0; i < length; i++)
        emit(sink, text[i]);
    if (left)
        while (fill-- > 0)
            emit(sink, ' ');
}

static double power_of_ten(int count)
{
    double power = 1;
    while (count > 22) {
        power *= 1e22;
        count -= 22;
    }
    while (count-- > 0)
        power *= 10;
    return power;
}

static int write_unsigned(char *out, unsigned long value, int minimum)
{
    char reversed[24];
    int count = 0, length = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value || count < minimum);
    while (count)
        out[length++] = reversed[--count];
    return length;
}

static int fixed_digits(char *out, double value, int precision)
{
    if (precision > 17)
        precision = 17;
    double scaled = value * power_of_ten(precision) + 0.5;
    if (scaled >= 9.2e18) {
        int exponent = 0;
        while (value >= 1e18) {
            value /= 10;
            exponent++;
        }
        int length = write_unsigned(out, (unsigned long)value, 1);
        while (exponent--)
            out[length++] = '0';
        if (precision > 0) {
            out[length++] = '.';
            while (precision--)
                out[length++] = '0';
        }
        return length;
    }
    unsigned long units = (unsigned long)scaled;
    unsigned long divisor = 1;
    for (int i = 0; i < precision; i++)
        divisor *= 10;
    int length = write_unsigned(out, units / divisor, 1);
    if (precision > 0) {
        out[length++] = '.';
        length += write_unsigned(out + length, units % divisor, precision);
    }
    return length;
}

static int decimal_exponent(double *value)
{
    if (*value == 0)
        return 0;
    int exponent = 0;
    while (*value >= 1e22) {
        *value /= 1e22;
        exponent += 22;
    }
    while (*value < 1e-22) {
        *value *= 1e22;
        exponent -= 22;
    }
    int step = 0;
    if (*value >= 1) {
        while (*value >= power_of_ten(step + 1))
            step++;
        *value /= power_of_ten(step);
        return exponent + step;
    }
    while (*value * power_of_ten(step) < 1)
        step++;
    *value *= power_of_ten(step);
    return exponent - step;
}

static int exponent_digits(char *out, double value, int precision, char letter)
{
    int exponent = decimal_exponent(&value);
    double rounding = 0.5;
    for (int i = 0; i < precision; i++)
        rounding /= 10;
    if (value + rounding >= 10) {
        value /= 10;
        exponent++;
    }
    int length = fixed_digits(out, value, precision);
    out[length++] = letter;
    out[length++] = exponent < 0 ? '-' : '+';
    if (exponent < 0)
        exponent = -exponent;
    if (exponent >= 100)
        out[length++] = (char)('0' + exponent / 100);
    out[length++] = (char)('0' + exponent / 10 % 10);
    out[length++] = (char)('0' + exponent % 10);
    return length;
}

static int strip_zeros(char *out, int length)
{
    int point = -1, mark = length;
    for (int i = 0; i < length; i++) {
        if (out[i] == '.')
            point = i;
        if (out[i] == 'e' || out[i] == 'E') {
            mark = i;
            break;
        }
    }
    if (point < 0)
        return length;
    int end = mark;
    while (end > point + 1 && out[end - 1] == '0')
        end--;
    if (end == point + 1)
        end = point;
    for (int i = mark; i < length; i++)
        out[end++] = out[i];
    return end;
}

static int format_double(char *out, double value, int precision, char kind, int alternate)
{
    int length = 0;
    if (value != value) {
        memcpy(out, "nan", 3);
        return 3;
    }
    if (value < 0 || (value == 0 && 1 / value < 0)) {
        out[length++] = '-';
        value = -value;
    }
    if (value > 1.7976931348623157e308) {
        memcpy(out + length, "inf", 3);
        return length + 3;
    }
    if (precision < 0)
        precision = 6;
    char lower = kind | 32;
    if (lower == 'f')
        return length + fixed_digits(out + length, value, precision);
    if (lower == 'e')
        return length + exponent_digits(out + length, value, precision, kind == 'E' ? 'E' : 'e');
    if (precision == 0)
        precision = 1;
    double probe = value;
    int exponent = decimal_exponent(&probe);
    int count;
    if (exponent < -4 || exponent >= precision)
        count = exponent_digits(out + length, value, precision - 1, kind == 'G' ? 'E' : 'e');
    else
        count = fixed_digits(out + length, value, precision - 1 - exponent);
    if (!alternate)
        count = strip_zeros(out + length, count);
    return length + count;
}

static int format(struct sink *sink, const char *pattern, va_list arguments)
{
    char digits[72];
    for (; *pattern; pattern++) {
        if (*pattern != '%') {
            emit(sink, *pattern);
            continue;
        }
        pattern++;
        int left = 0, plus = 0, space = 0, alternate = 0;
        char pad = ' ';
        for (;; pattern++) {
            if (*pattern == '-')
                left = 1;
            else if (*pattern == '0')
                pad = '0';
            else if (*pattern == '+')
                plus = 1;
            else if (*pattern == ' ')
                space = 1;
            else if (*pattern == '#')
                alternate = 1;
            else
                break;
        }
        int width = 0;
        if (*pattern == '*') {
            width = va_arg(arguments, int);
            pattern++;
        }
        while (*pattern >= '0' && *pattern <= '9')
            width = width * 10 + *pattern++ - '0';
        int precision = -1;
        if (*pattern == '.') {
            precision = 0;
            pattern++;
            if (*pattern == '*') {
                precision = va_arg(arguments, int);
                pattern++;
            }
            while (*pattern >= '0' && *pattern <= '9')
                precision = precision * 10 + *pattern++ - '0';
        }
        int size = 0;
        while (*pattern == 'l' || *pattern == 'z' || *pattern == 'h' || *pattern == 'L') {
            if (*pattern != 'h')
                size = 1;
            pattern++;
        }
        char kind = *pattern;
        if (kind == 's') {
            const char *text = va_arg(arguments, const char *);
            if (!text)
                text = "(null)";
            size_t length = precision >= 0 ? strnlen(text, (size_t)precision) : strlen(text);
            emit_padded(sink, text, length, width, left, ' ');
        } else if (kind == 'c') {
            char c = (char)va_arg(arguments, int);
            emit_padded(sink, &c, 1, width, left, ' ');
        } else if (kind == 'f' || kind == 'F' || kind == 'e' || kind == 'E' || kind == 'g' || kind == 'G') {
            double value = va_arg(arguments, double);
            char number[400];
            int length = format_double(number, value, precision, kind, alternate);
            int prefix = 0;
            char sign[1];
            if (number[0] != '-' && (plus || space)) {
                sign[0] = plus ? '+' : ' ';
                prefix = 1;
            }
            if (pad == '0' && !left) {
                int start = number[0] == '-' ? 1 : 0;
                if (start)
                    emit(sink, '-');
                else if (prefix)
                    emit(sink, sign[0]);
                int fill = width - length - prefix;
                while (fill-- > 0)
                    emit(sink, '0');
                for (int i = start; i < length; i++)
                    emit(sink, number[i]);
            } else {
                if (prefix && !left)
                    for (int fill = width - length - 1; fill > 0; fill--)
                        emit(sink, ' ');
                if (prefix)
                    emit(sink, sign[0]);
                emit_padded(sink, number, (size_t)length, prefix ? (left ? width - 1 : 0) : width, left, ' ');
            }
        } else if (kind == '%') {
            emit(sink, '%');
        } else if (kind == 'd' || kind == 'i' || kind == 'u' || kind == 'x' || kind == 'X' || kind == 'o' || kind == 'p') {
            unsigned long value;
            int negative = 0;
            if (kind == 'p') {
                value = (unsigned long)va_arg(arguments, void *);
                alternate = 1;
            } else if (kind == 'd' || kind == 'i') {
                long signed_value = size ? va_arg(arguments, long) : va_arg(arguments, int);
                negative = signed_value < 0;
                value = negative ? 0 - (unsigned long)signed_value : (unsigned long)signed_value;
            } else {
                value = size ? va_arg(arguments, unsigned long) : va_arg(arguments, unsigned int);
            }
            unsigned base = kind == 'o' ? 8 : (kind == 'x' || kind == 'X' || kind == 'p') ? 16 : 10;
            const char *alphabet = kind == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            int length = 0;
            char reversed[24];
            do {
                reversed[length++] = alphabet[value % base];
                value /= base;
            } while (value);
            while (length < precision && length < 23)
                reversed[length++] = '0';
            int prefix = 0;
            if (negative)
                digits[prefix++] = '-';
            else if (plus)
                digits[prefix++] = '+';
            else if (space)
                digits[prefix++] = ' ';
            if (alternate && base == 16) {
                digits[prefix++] = '0';
                digits[prefix++] = kind == 'X' ? 'X' : 'x';
            }
            if (pad == '0' && !left && precision < 0)
                while (prefix + length < width && length < 23)
                    reversed[length++] = '0';
            int total = prefix;
            while (length)
                digits[total++] = reversed[--length];
            emit_padded(sink, digits, (size_t)total, width, left, ' ');
        } else if (kind) {
            emit(sink, '%');
            emit(sink, kind);
        } else {
            break;
        }
    }
    if (!sink->stream && sink->limit)
        sink->buffer[sink->count < sink->limit ? sink->count : sink->limit - 1] = 0;
    return (int)sink->count;
}

int vfprintf(FILE *stream, const char *pattern, va_list arguments)
{
    struct sink sink = {stream, 0, 0, 0};
    return format(&sink, pattern, arguments);
}

int vprintf(const char *pattern, va_list arguments)
{
    return vfprintf(stdout, pattern, arguments);
}

int vsnprintf(char *buffer, size_t length, const char *pattern, va_list arguments)
{
    struct sink sink = {0, buffer, length, 0};
    return format(&sink, pattern, arguments);
}

int printf(const char *pattern, ...)
{
    va_list arguments;
    va_start(arguments, pattern);
    int count = vprintf(pattern, arguments);
    va_end(arguments);
    return count;
}

int fprintf(FILE *stream, const char *pattern, ...)
{
    va_list arguments;
    va_start(arguments, pattern);
    int count = vfprintf(stream, pattern, arguments);
    va_end(arguments);
    return count;
}

int snprintf(char *buffer, size_t length, const char *pattern, ...)
{
    va_list arguments;
    va_start(arguments, pattern);
    int count = vsnprintf(buffer, length, pattern, arguments);
    va_end(arguments);
    return count;
}

int sprintf(char *buffer, const char *pattern, ...)
{
    va_list arguments;
    va_start(arguments, pattern);
    int count = vsnprintf(buffer, (size_t)-1 >> 1, pattern, arguments);
    va_end(arguments);
    return count;
}

int ungetc(int c, FILE *stream)
{
    if (c == EOF || !(stream->flags & STREAM_READ) || !stream->position)
        return EOF;
    stream->buffer[--stream->position] = (unsigned char)c;
    stream->flags &= ~STREAM_END;
    return (unsigned char)c;
}

int getc(FILE *stream)
{
    return fgetc(stream);
}

int putc(int c, FILE *stream)
{
    return fputc(c, stream);
}

void perror(const char *prefix)
{
    if (prefix && *prefix)
        fprintf(stderr, "%s: %s\n", prefix, strerror(errno));
    else
        fprintf(stderr, "%s\n", strerror(errno));
}

static int is_space(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

static int digit_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'z')
        return (c | 32) - 'a' + 10;
    return 99;
}

int vsscanf(const char *text, const char *format, va_list arguments)
{
    const char *start = text;
    int assigned = 0;
    for (; *format; format++) {
        if (is_space(*format)) {
            while (is_space(*text))
                text++;
            continue;
        }
        if (*format != '%' || format[1] == '%') {
            if (*format == '%')
                format++;
            if (*text != *format)
                return assigned;
            text++;
            continue;
        }
        format++;
        int skip = 0, width = 0, size = 0;
        if (*format == '*') {
            skip = 1;
            format++;
        }
        while (*format >= '0' && *format <= '9')
            width = width * 10 + *format++ - '0';
        while (*format == 'l' || *format == 'h' || *format == 'z') {
            size = *format == 'h' ? -1 : 1;
            format++;
        }
        char kind = *format;
        if (kind == 'n') {
            if (!skip)
                *va_arg(arguments, int *) = (int)(text - start);
            continue;
        }
        if (kind != 'c')
            while (is_space(*text))
                text++;
        if (!*text)
            return assigned ? assigned : EOF;
        if (kind == 'c') {
            int count = width ? width : 1;
            char *out = skip ? 0 : va_arg(arguments, char *);
            for (int i = 0; i < count && *text; i++, text++)
                if (out)
                    out[i] = *text;
            assigned += !skip;
        } else if (kind == 's') {
            char *out = skip ? 0 : va_arg(arguments, char *);
            int count = 0;
            while (*text && !is_space(*text) && (!width || count < width)) {
                if (out)
                    out[count] = *text;
                count++;
                text++;
            }
            if (out)
                out[count] = 0;
            assigned += !skip;
        } else if (kind == 'd' || kind == 'i' || kind == 'u' || kind == 'x' || kind == 'X' || kind == 'o') {
            int base = kind == 'x' || kind == 'X' ? 16 : kind == 'o' ? 8 : 10;
            int negative = 0, digits = 0;
            if (*text == '-' || *text == '+')
                negative = *text++ == '-';
            if (kind == 'i' && text[0] == '0') {
                base = (text[1] | 32) == 'x' ? 16 : 8;
                if (base == 16)
                    text += 2;
            } else if (base == 16 && text[0] == '0' && (text[1] | 32) == 'x') {
                text += 2;
            }
            unsigned long value = 0;
            while (digit_value(*text) < base && (!width || digits < width)) {
                value = value * base + digit_value(*text++);
                digits++;
            }
            if (!digits)
                return assigned;
            if (negative)
                value = 0 - value;
            if (!skip) {
                if (size > 0)
                    *va_arg(arguments, long *) = (long)value;
                else if (size < 0)
                    *va_arg(arguments, short *) = (short)value;
                else
                    *va_arg(arguments, int *) = (int)value;
                assigned++;
            }
        } else {
            return assigned;
        }
    }
    return assigned;
}

int sscanf(const char *text, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int count = vsscanf(text, format, arguments);
    va_end(arguments);
    return count;
}

int fscanf(FILE *stream, const char *format, ...)
{
    char line[512];
    if (!fgets(line, sizeof line, stream))
        return EOF;
    va_list arguments;
    va_start(arguments, format);
    int count = vsscanf(line, format, arguments);
    va_end(arguments);
    return count;
}

int scanf(const char *format, ...)
{
    char line[512];
    if (!fgets(line, sizeof line, stdin))
        return EOF;
    va_list arguments;
    va_start(arguments, format);
    int count = vsscanf(line, format, arguments);
    va_end(arguments);
    return count;
}
