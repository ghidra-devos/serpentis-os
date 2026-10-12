#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQUARE(x) ((x) * (x))
#define NAME(a, b) a##b
#define TEXT(x) #x
#define COUNT(array) (int)(sizeof(array) / sizeof((array)[0]))

typedef struct point {
    int x;
    int y;
} Point;

struct shape {
    char tag;
    long area;
    Point corners[2];
    union {
        int whole;
        unsigned char bytes[4];
    } value;
};

enum color { RED, GREEN = 5, BLUE };

static int failures;
static int checks;
static int table[] = {3, 1, 4, 1, 5, 9, 2, 6};
static const char *words[] = {"pear", "apple", "fig"};
static Point origin = {.y = 7, .x = -2};
static struct shape global_shape = {'s', 12, {{1, 2}, {3, 4}}, {0x01020304}};
int counter;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("failed: %s\n", what);
    }
}

static int fibonacci(int n)
{
    return n < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2);
}

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }

static int apply(int (*op)(int, int), int a, int b)
{
    return op(a, b);
}

static int compare_ints(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int compare_words(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static int next_id(void)
{
    static int id = 100;
    return id++;
}

static long sum(int count, ...)
{
    va_list list;
    va_start(list, count);
    long total = 0;
    for (int i = 0; i < count; i++)
        total += va_arg(list, long);
    va_end(list);
    return total;
}

static char *classify(int n)
{
    switch (n) {
    case 0:
        return "zero";
    case 1:
    case 2:
        return "small";
    case -1:
        return "negative";
    default:
        break;
    }
    return "big";
}

static void move(Point *p, int dx, int dy)
{
    p->x += dx;
    p->y += dy;
}

int main(void)
{
    char buffer[64];

    check(SQUARE(7) == 49, "macro");
    check(NAME(coun, ter) == 0, "token paste");
    check(!strcmp(TEXT(hello world), "hello world"), "stringize");
    check(COUNT(table) == 8, "sizeof array");
    check(sizeof(struct shape) == 40, "struct size");
    check(sizeof(Point) == 8 && sizeof(long) == 8 && sizeof(short) == 2, "type sizes");

    check(fibonacci(15) == 610, "recursion");
    check(apply(add, 2, 3) == 5 && apply(sub, 2, 3) == -1, "function pointers");

    int numbers[8];
    memcpy(numbers, table, sizeof numbers);
    qsort(numbers, 8, sizeof(int), compare_ints);
    check(numbers[0] == 1 && numbers[7] == 9 && numbers[4] == 4, "qsort ints");
    const char *sorted[3] = {words[0], words[1], words[2]};
    qsort(sorted, 3, sizeof(char *), compare_words);
    check(!strcmp(sorted[0], "apple") && !strcmp(sorted[2], "pear"), "qsort strings");

    check(origin.x == -2 && origin.y == 7, "designated initializer");
    check(global_shape.corners[1].y == 4 && global_shape.tag == 's', "nested initializer");
    check(global_shape.value.bytes[0] == 4 && global_shape.value.bytes[3] == 1, "union");

    Point p = {1, 1};
    move(&p, 4, -3);
    check(p.x == 5 && p.y == -2, "struct pointer");
    Point q = p;
    q.x = 0;
    check(p.x == 5 && q.y == -2, "struct copy");

    check(next_id() == 100 && next_id() == 101, "static local");
    check(sum(4, 10L, 20L, 30L, 40L) == 100, "varargs");
    check(!strcmp(classify(0), "zero") && !strcmp(classify(2), "small") && !strcmp(classify(-1), "negative") &&
              !strcmp(classify(50), "big"),
          "switch");
    check(BLUE == 6 && GREEN == 5 && RED == 0, "enum");

    unsigned int u = 0xFFFFFFFF;
    check(u + 1 == 0, "unsigned wrap");
    check(u > 1, "unsigned compare");
    int negative = -7;
    check(negative / 2 == -3 && negative % 2 == -1, "signed division");
    check((negative >> 1) == -4, "arithmetic shift");
    check((u >> 28) == 15, "logical shift");
    check(INT_MAX + 1L == 2147483648L, "long promotion");
    char c = (char)200;
    check(c < 0, "signed char");
    unsigned char uc = 200;
    check(uc == 200 && uc + uc == 400, "unsigned char");
    short s = -2;
    check(s * 3 == -6, "short");
    long big = 1L << 40;
    check(big == 1099511627776L, "long shift");
    check((5 & 3) == 1 && (5 | 3) == 7 && (5 ^ 3) == 6 && ~0 == -1, "bitwise");

    int i = 0, total = 0;
    while (1) {
        if (++i > 10)
            break;
        if (i % 2)
            continue;
        total += i;
    }
    check(total == 30, "while break continue");
    do {
        total--;
    } while (total > 25);
    check(total == 25, "do while");
    int x = 5;
    x *= 3;
    x -= 1;
    x <<= 2;
    x |= 1;
    check(x == 57, "compound assignment");
    int *pointer = numbers;
    pointer += 2;
    check(*pointer++ == 2 && *pointer == 3 && pointer - numbers == 3, "pointer arithmetic");
    int k = 3;
    check(k++ == 3 && k == 4 && --k == 3, "increment");
    check((k > 2 ? 10 : 20) == 10 && (k > 5 || k == 3) && !(k < 0 && k > -5), "logic");

    for (int row = 0; row < 3; row++)
        for (int column = 0; column < 3; column++)
            if (row == 1 && column == 1)
                goto found;
    check(0, "goto");
found:

    snprintf(buffer, sizeof buffer, "%d|%5s|%-4d|%x|%c|%ld|%03d", -42, "ab", 7, 255, 'z', 123456789012L, 9);
    check(!strcmp(buffer, "-42|   ab|7   |ff|z|123456789012|009"), "printf");
    char *copy = malloc(32);
    strcpy(copy, "serpent");
    strcat(copy, "is");
    check(strlen(copy) == 9 && strchr(copy, 'p') == copy + 3, "malloc and strings");
    copy = realloc(copy, 4096);
    check(!strcmp(copy, "serpentis"), "realloc");
    free(copy);
    check(atoi("-315") == -315 && strtol("ff", 0, 16) == 255, "number parsing");

    printf("%d of %d checks passed\n", checks - failures, checks);
    return failures;
}
