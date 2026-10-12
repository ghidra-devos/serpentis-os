#include "cc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *keywords[] = {
    "return", "if", "else", "for", "while", "do", "switch", "case", "default", "break",
    "continue", "goto", "sizeof", "_Alignof", "struct", "union", "enum", "typedef", "static",
    "extern", "inline", "const", "volatile", "register", "auto", "restrict", "signed",
    "unsigned", "void", "_Bool", "char", "short", "int", "long", "_Noreturn", "float", "double", "asm",
    "__asm__", "__asm", 0
};

static char *punctuators[] = {
    "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=", "%=", "&=", "|=",
    "^=", "&&", "||", "++", "--", "<<", ">>", "##", 0
};

void fatal(char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fputs("cc: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    exit(1);
}

static void report(Token *tok, char *kind, char *format, va_list arguments)
{
    fprintf(stderr, "%s:%d: %s: ", tok->file, tok->line, kind);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    char *start = tok->line_start;
    if (!start || tok->text < start)
        return;
    int length = 0;
    while (start[length] && start[length] != '\n')
        length++;
    if (tok->text > start + length)
        return;
    fprintf(stderr, "%5d | %.*s\n      | ", tok->line, length, start);
    for (char *p = start; p < tok->text; p++)
        fputc(*p == '\t' ? '\t' : ' ', stderr);
    int width = tok->length > 0 ? tok->length : 1;
    while (width--)
        fputc('^', stderr);
    fputc('\n', stderr);
}

void error_at(Token *tok, char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    report(tok, "error", format, arguments);
    va_end(arguments);
    exit(1);
}

void warn_at(Token *tok, char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    report(tok, "warning", format, arguments);
    va_end(arguments);
}

char *copy_text(char *text, int length)
{
    char *copy = malloc(length + 1);
    memcpy(copy, text, length);
    copy[length] = 0;
    return copy;
}

char *format_text(char *format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    return copy_text(line, length);
}

void buffer_add(Buffer *buffer, char *text, int length)
{
    if (buffer->length + length + 1 > buffer->capacity) {
        int capacity = buffer->capacity ? buffer->capacity * 2 : 4096;
        while (capacity < buffer->length + length + 1)
            capacity *= 2;
        buffer->data = realloc(buffer->data, capacity);
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = 0;
}

void buffer_printf(Buffer *buffer, char *format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    buffer_add(buffer, line, length);
}

char *read_file(char *path, int *length)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    Buffer buffer = {0, 0, 0};
    char chunk[512];
    for (;;) {
        int count = fread(chunk, 1, sizeof chunk, file);
        if (count <= 0)
            break;
        buffer_add(&buffer, chunk, count);
    }
    fclose(file);
    if (!buffer.data)
        buffer_add(&buffer, "", 0);
    if (length)
        *length = buffer.length;
    return buffer.data;
}

int equal(Token *tok, char *text)
{
    return tok->kind != TK_STR && (int)strlen(text) == tok->length && !memcmp(tok->text, text, tok->length);
}

Token *skip(Token *tok, char *text)
{
    if (!equal(tok, text))
        error_at(tok, "expected '%s'", text);
    return tok->next;
}

static Token *new_token(int kind, char *start, char *end)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->text = start;
    tok->length = end - start;
    return tok;
}

static int is_ident_start(int c)
{
    return isalpha(c) || c == '_' || c == '$';
}

static int read_escape(char **cursor)
{
    char *p = *cursor;
    int c = *p++;
    if (c >= '0' && c <= '7') {
        int value = c - '0';
        if (*p >= '0' && *p <= '7') {
            value = value * 8 + *p++ - '0';
            if (*p >= '0' && *p <= '7')
                value = value * 8 + *p++ - '0';
        }
        *cursor = p;
        return value;
    }
    if (c == 'x') {
        int value = 0;
        while (isxdigit(*p)) {
            int digit = isdigit(*p) ? *p - '0' : (*p | 32) - 'a' + 10;
            value = value * 16 + digit;
            p++;
        }
        *cursor = p;
        return value;
    }
    *cursor = p;
    switch (c) {
    case 'a': return 7;
    case 'b': return 8;
    case 't': return 9;
    case 'n': return 10;
    case 'v': return 11;
    case 'f': return 12;
    case 'r': return 13;
    case 'e': return 27;
    }
    return c;
}

static Token *read_string(char *start, char *file, int line)
{
    char *p = start + 1;
    Buffer text = {0, 0, 0};
    while (*p != '"') {
        if (!*p || *p == '\n') {
            Token where = {0};
            where.file = file;
            where.line = line;
            where.kind = TK_EOF;
            error_at(&where, "unterminated string");
        }
        char c;
        if (*p == '\\') {
            p++;
            c = (char)read_escape(&p);
        } else {
            c = *p++;
        }
        buffer_add(&text, &c, 1);
    }
    buffer_add(&text, "", 0);
    Token *tok = new_token(TK_STR, start, p + 1);
    tok->str = text.data;
    tok->str_length = text.length + 1;
    tok->type = array_of(ty_char, tok->str_length);
    return tok;
}

static Token *read_char(char *start)
{
    char *p = start + 1;
    int c;
    if (*p == '\\') {
        p++;
        c = read_escape(&p);
    } else {
        c = (unsigned char)*p++;
    }
    while (*p && *p != '\'' && *p != '\n')
        p++;
    Token *tok = new_token(TK_NUM, start, p + 1);
    tok->value = (char)c;
    tok->type = ty_int;
    return tok;
}

static Token *read_float(char *start)
{
    char *end;
    double value = strtod(start, &end);
    Type *type = ty_double;
    if ((*end | 32) == 'f') {
        type = ty_float;
        end++;
    } else if ((*end | 32) == 'l') {
        end++;
    }
    Token *tok = new_token(TK_NUM, start, end);
    tok->fvalue = value;
    tok->type = type;
    return tok;
}

static int is_float_literal(char *p)
{
    if (p[0] == '0' && (p[1] | 32) == 'x')
        return 0;
    while (isdigit(*p))
        p++;
    return *p == '.' || (*p | 32) == 'e';
}

static Token *read_number(char *start)
{
    if (is_float_literal(start))
        return read_float(start);
    char *p = start;
    unsigned long long value = 0;
    int base = 10;
    if (p[0] == '0' && (p[1] | 32) == 'x') {
        base = 16;
        p += 2;
    } else if (p[0] == '0') {
        base = 8;
    }
    for (;; p++) {
        int digit;
        if (isdigit(*p))
            digit = *p - '0';
        else if (base == 16 && isxdigit(*p))
            digit = (*p | 32) - 'a' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * base + digit;
    }
    int is_long = 0, is_unsigned = 0;
    for (;; p++) {
        if ((*p | 32) == 'l')
            is_long = 1;
        else if ((*p | 32) == 'u')
            is_unsigned = 1;
        else
            break;
    }
    Token *tok = new_token(TK_NUM, start, p);
    tok->value = (long long)value;
    if (is_long || value >> 31)
        tok->type = is_unsigned ? ty_ulong : ty_long;
    else
        tok->type = is_unsigned ? ty_uint : ty_int;
    if (!is_long && !is_unsigned && base != 10 && value >> 31 && !(value >> 31 >> 1))
        tok->type = ty_uint;
    return tok;
}

static void join_lines(char *p)
{
    char *out = p;
    while (*p) {
        if (p[0] == '\\' && p[1] == '\n') {
            p += 2;
        } else if (p[0] == '\\' && p[1] == '\r' && p[2] == '\n') {
            p += 3;
        } else if (*p == '\r') {
            p++;
        } else {
            *out++ = *p++;
        }
    }
    *out = 0;
}

static Token *tokenize(char *file, char *p)
{
    Token head = {0};
    Token *cur = &head;
    int line = 1;
    char *line_start = p;
    int at_bol = 1, space = 0;
    while (*p) {
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
            space = 1;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n')
                    line++;
                p++;
            }
            if (*p)
                p += 2;
            space = 1;
            continue;
        }
        if (*p == '\n') {
            p++;
            line++;
            line_start = p;
            at_bol = 1;
            space = 0;
            continue;
        }
        if (isspace(*p)) {
            p++;
            space = 1;
            continue;
        }
        Token *tok;
        if (isdigit(*p) || (*p == '.' && isdigit(p[1]))) {
            tok = read_number(p);
            if (isalnum(tok->text[tok->length]) || tok->text[tok->length] == '.')
                tok->kind = TK_IDENT;
        } else if (*p == '"') {
            tok = read_string(p, file, line);
        } else if (*p == '\'') {
            tok = read_char(p);
        } else if (is_ident_start(*p)) {
            char *q = p;
            while (is_ident_start(*q) || isdigit(*q))
                q++;
            tok = new_token(TK_IDENT, p, q);
        } else {
            int length = 1;
            for (int i = 0; punctuators[i]; i++) {
                int n = strlen(punctuators[i]);
                if (!strncmp(p, punctuators[i], n)) {
                    length = n;
                    break;
                }
            }
            tok = new_token(TK_PUNCT, p, p + length);
        }
        tok->file = file;
        tok->line = line;
        tok->line_start = line_start;
        tok->at_bol = at_bol;
        tok->space = space;
        at_bol = space = 0;
        p = tok->text + tok->length;
        cur = cur->next = tok;
    }
    Token *end = new_token(TK_EOF, p, p);
    end->file = file;
    end->line = line;
    end->at_bol = 1;
    cur->next = end;
    return head.next;
}

Token *tokenize_file(char *path)
{
    char *text = read_file(path, 0);
    if (!text)
        return 0;
    join_lines(text);
    return tokenize(path, text);
}

void mark_keywords(Token *tok)
{
    for (; tok; tok = tok->next) {
        if (tok->kind != TK_IDENT)
            continue;
        for (int i = 0; keywords[i]; i++)
            if (equal(tok, keywords[i]))
                tok->kind = TK_KEYWORD;
    }
}

Token *tokenize_text(char *file, char *text)
{
    return tokenize(file, text);
}
