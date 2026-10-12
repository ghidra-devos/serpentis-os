#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <serpentis.h>

int main(int argc, char **argv)
{
    printf("Hello from C on serpentis-os!\n");
    printf("argc = %d\n", argc);
    for (int i = 1; i < argc; i++)
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    char *copy = strdup("heap works");
    printf("%s, pid %d, ticks %lu\n", copy, serp_getpid(), serp_ticks());
    free(copy);
    return 7;
}
