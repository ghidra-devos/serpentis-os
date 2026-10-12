#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_MAX 16
#define VARIABLE_MAX 32

struct variable {
    char name[NAME_MAX];
    long value;
};

static struct variable variables[VARIABLE_MAX];
static int variable_count;
static const char *cursor;
static int failed;

static long expression(void);

static void fail(const char *message)
{
    if (!failed)
        printf("error: %s\n", message);
    failed = 1;
}

static void skip_spaces(void)
{
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
}

static struct variable *find(const char *name, int create)
{
    for (int i = 0; i < variable_count; i++)
        if (!strcmp(variables[i].name, name))
            return &variables[i];
    if (!create)
        return 0;
    if (variable_count == VARIABLE_MAX) {
        fail("too many variables");
        return 0;
    }
    struct variable *fresh = &variables[variable_count++];
    strcpy(fresh->name, name);
    fresh->value = 0;
    return fresh;
}

static int read_name(char *name)
{
    int length = 0;
    while (isalnum(*cursor) || *cursor == '_') {
        if (length < NAME_MAX - 1)
            name[length++] = *cursor;
        cursor++;
    }
    name[length] = 0;
    return length;
}

static long power(long base, long exponent)
{
    long result = 1;
    while (exponent-- > 0)
        result *= base;
    return result;
}

static long call(const char *name)
{
    long argument = expression();
    skip_spaces();
    if (*cursor != ')') {
        fail("missing ')'");
        return 0;
    }
    cursor++;
    if (!strcmp(name, "abs"))
        return argument < 0 ? -argument : argument;
    if (!strcmp(name, "sqrt")) {
        long root = 0;
        while ((root + 1) * (root + 1) <= argument)
            root++;
        return root;
    }
    if (!strcmp(name, "fact")) {
        long result = 1;
        for (long i = 2; i <= argument; i++)
            result *= i;
        return result;
    }
    fail("unknown function");
    return 0;
}

static long primary(void)
{
    skip_spaces();
    if (*cursor == '(') {
        cursor++;
        long value = expression();
        skip_spaces();
        if (*cursor != ')')
            fail("missing ')'");
        else
            cursor++;
        return value;
    }
    if (*cursor == '-') {
        cursor++;
        return -primary();
    }
    if (isdigit(*cursor)) {
        char *end;
        long value = strtol(cursor, &end, 0);
        cursor = end;
        return value;
    }
    if (isalpha(*cursor) || *cursor == '_') {
        char name[NAME_MAX];
        read_name(name);
        skip_spaces();
        if (*cursor == '(') {
            cursor++;
            return call(name);
        }
        struct variable *v = find(name, 0);
        if (!v) {
            fail("unknown variable");
            return 0;
        }
        return v->value;
    }
    fail("expected a number");
    return 0;
}

static long factor(void)
{
    long value = primary();
    skip_spaces();
    if (*cursor == '^') {
        cursor++;
        return power(value, factor());
    }
    return value;
}

static long term(void)
{
    long value = factor();
    for (;;) {
        skip_spaces();
        char op = *cursor;
        if (op != '*' && op != '/' && op != '%')
            return value;
        cursor++;
        long right = factor();
        if (op == '*') {
            value *= right;
        } else if (!right) {
            fail("division by zero");
            return 0;
        } else {
            value = op == '/' ? value / right : value % right;
        }
    }
}

static long expression(void)
{
    long value = term();
    for (;;) {
        skip_spaces();
        if (*cursor == '+') {
            cursor++;
            value += term();
        } else if (*cursor == '-') {
            cursor++;
            value -= term();
        } else {
            return value;
        }
    }
}

static void statement(const char *line)
{
    cursor = line;
    failed = 0;
    skip_spaces();
    const char *start = cursor;
    char name[NAME_MAX];
    struct variable *target = 0;
    if (isalpha(*cursor) && read_name(name)) {
        skip_spaces();
        if (*cursor == '=') {
            cursor++;
            target = find(name, 1);
        } else {
            cursor = start;
        }
    }
    long value = expression();
    skip_spaces();
    if (*cursor && *cursor != '\n')
        fail("unexpected text");
    if (failed)
        return;
    if (target) {
        target->value = value;
        printf("%s = %ld\n", target->name, value);
    } else {
        printf("%ld\n", value);
    }
}

static void list_variables(void)
{
    if (!variable_count)
        puts("no variables yet");
    for (int i = 0; i < variable_count; i++)
        printf("  %-10s %ld\n", variables[i].name, variables[i].value);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        char line[256] = "";
        for (int i = 1; i < argc; i++) {
            strncat(line, argv[i], sizeof line - strlen(line) - 2);
            strcat(line, " ");
        }
        statement(line);
        return failed;
    }
    puts("calculator - integers, variables and functions, written in C");
    puts("try: x = 6 * 7, x ^ 2, fact(10), sqrt(1000), vars, quit");
    char line[256];
    for (;;) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin))
            break;
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, "quit") || !strcmp(line, "exit"))
            break;
        if (!strcmp(line, "vars"))
            list_variables();
        else if (line[0])
            statement(line);
    }
    return 0;
}
