#include <serpentis.h>
#include <stdlib.h>

#define ARGUMENT_MAX 32

int main(int argc, char **argv);
void serp_stdio_init(void);

static char argument_text[256];
static char *argument_list[ARGUMENT_MAX + 2];
static char program_name[] = "program";

// clang builds need their own _start, under cc gen.c writes it
#ifndef __scc__
__asm__(".section .text.start,\"ax\"\n"
        ".globl _start\n"
        "_start:\n"
        "    and $-16, %rsp\n"
        "    call serp_start\n"
        "    ud2\n"
        ".text\n");
#endif

// the kernel hands us the arguments as one string, argv gets cut out of a copy of it.
// quotes work, escapes don't
static int split_arguments(const char *source)
{
    int count = 0;
    size_t length = 0;
    argument_list[count++] = program_name;
    if (!source)
        return count;
    while (source[length] && length < sizeof argument_text - 1) {
        argument_text[length] = source[length];
        length++;
    }
    argument_text[length] = 0;
    char *cursor = argument_text;
    while (*cursor && count <= ARGUMENT_MAX) {
        while (*cursor == ' ')
            cursor++;
        if (!*cursor)
            break;
        char quote = 0;
        if (*cursor == '"' || *cursor == '\'')
            quote = *cursor++;
        argument_list[count++] = cursor;
        while (*cursor && (quote ? *cursor != quote : *cursor != ' '))
            cursor++;
        if (*cursor)
            *cursor++ = 0;
    }
    argument_list[count] = 0;
    return count;
}

__attribute__((noreturn, used)) void serp_start(const char *arguments)
{
    serp_stdio_init();
    int count = split_arguments(arguments);
    exit(main(count, argument_list));
}

// busy waits with yield, there is no sleep syscall that takes ticks. good enough
void serp_sleep(unsigned long ticks)
{
    unsigned long start = serp_ticks();
    while (serp_ticks() - start < ticks)
        serp_yield();
}
