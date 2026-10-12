#include <serpentis.h>
#include <stdio.h>
#include <string.h>

static void show_time(uint32_t stamp)
{
    printf("%04u-%02u-%02u %02u:%02u",
           2000 + (stamp >> 26), (stamp >> 22) & 15, (stamp >> 17) & 31,
           (stamp >> 12) & 31, (stamp >> 6) & 63);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : ".";
    char cwd[240];
    if (!strcmp(path, ".")) {
        serp_getcwd(cwd, sizeof cwd);
        path = cwd;
    }
    int fd = serp_open(path, SERP_OPEN_READ);
    if (fd < 0) {
        printf("cls: cannot open %s\n", path);
        return 1;
    }
    struct serp_dirent entry;
    int count = 0;
    while (serp_readdir(fd, &entry) > 0) {
        char full[480];
        snprintf(full, sizeof full, "%s/%s", strcmp(path, "/") ? path : "", entry.name);
        struct serp_stat info;
        serp_stat(full, &info);
        show_time(info.mtime);
        if (entry.flags & SERP_DIRECTORY)
            printf("  %8s  %s/\n", "<dir>", entry.name);
        else
            printf("  %8lu  %s\n", (unsigned long)info.size, entry.name);
        count++;
    }
    serp_close(fd);
    printf("%d entries\n", count);
    return 0;
}
