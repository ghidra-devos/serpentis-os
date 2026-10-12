#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct vec {
    int x;
    int y;
    int z;
};

struct name {
    char text[20];
    int length;
};

static int checks;
static int failures;
static jmp_buf escape;
static int depth;
static const int squares[8] = {[2] = 4, [5] = 25, [7] = 49};
static const struct vec origin = {.z = 3};

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("failed: %s\n", what);
    }
}

static struct vec add(struct vec a, struct vec b)
{
    struct vec sum = {a.x + b.x, a.y + b.y, a.z + b.z};
    return sum;
}

static struct name make_name(const char *text)
{
    struct name n;
    strcpy(n.text, text);
    n.length = (int)strlen(text);
    return n;
}

static int dot(struct vec a, int scale, struct vec b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z) * scale;
}

static void dive(int level)
{
    depth = level;
    if (level == 5)
        longjmp(escape, 42);
    dive(level + 1);
}

static int compare_ints(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static long triple(long value)
{
#ifdef __scc__
    asm("    mov rax, qword [rbp+16]\n"
        "    lea rax, [rax+rax*2]\n"
        "    jmp .done");
    return 0;
#else
    return value * 3;
#endif
}

int main(void)
{
    struct vec a = {1, 2, 3}, b = {10, 20, 30};
    struct vec c = add(a, b);
    check(c.x == 11 && c.y == 22 && c.z == 33, "struct by value");
    check(add(c, a).z == 36, "struct return member");
    check(dot(a, 2, b) == 280, "struct arguments around a scalar");
    struct name n = make_name("serpentis");
    check(n.length == 9 && !strcmp(n.text, "serpentis"), "large struct return");

    int *literal = (int[]){5, 6, 7};
    check(literal[2] == 7, "compound literal array");
    struct vec v = (struct vec){.y = 9};
    check(v.x == 0 && v.y == 9, "compound literal struct");
    check(squares[5] == 25 && squares[3] == 0 && sizeof squares == 32, "array designators");
    check(origin.z == 3 && origin.x == 0, "struct designators");

    int jumped = setjmp(escape);
    if (!jumped)
        dive(0);
    check(jumped == 42 && depth == 5, "setjmp and longjmp");

    check(triple(14) == 42, "inline assembly");

    int day, month, year;
    char word[16];
    int got = sscanf("06/10/2026 snake", "%d/%d/%d %15s", &day, &month, &year, word);
    check(got == 4 && day == 6 && month == 10 && year == 2026 && !strcmp(word, "snake"), "sscanf");

    int numbers[] = {2, 3, 5, 7, 11, 13, 17};
    int key = 11;
    int *found = bsearch(&key, numbers, 7, sizeof(int), compare_ints);
    check(found && found - numbers == 4, "bsearch");

    div_t d = div(47, 5);
    check(d.quot == 9 && d.rem == 2, "div");

    time_t epoch = 86400 * 365 + 3600 * 5;
    struct tm *parts = gmtime(&epoch);
    char stamp[40];
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M %a", parts);
    check(!strcmp(stamp, "1971-01-01 05:00 Fri"), "gmtime and strftime");
    check(time(0) > 1700000000, "time from the clock");

    errno = ENOENT;
    check(!strcmp(strerror(errno), "no such file or directory"), "errno and strerror");
    check(strcasecmp("SeRpEnT", "serpent") == 0 && strpbrk("kernel", "xyzn")[0] == 'n', "string extras");
    assert(checks > 10);

    printf("%d of %d checks passed\n", checks - failures, checks);
    return failures;
}
