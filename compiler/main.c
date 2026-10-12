// no linker here. every .c plus the whole libc gets parsed into one big program, then
// generate() writes a single fasm file and apple assembles it. dumb, but it works and it
// means i never had to write a linker
#include "cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __serpentis__
#include <serpentis.h>
#endif

static char *library_sources[] = {"start.c", "string.c", "ctype.c", "stdlib.c", "stdio.c", "graphics.c", "setjmp.c", "time.c", "math.c", 0};
static char *library_dir = "/lib/c";
static char *sources[32];
static int source_count;
static char *defines[16];
static int define_count;
static char *output_path;
static int assembly_only;
static int no_library;
static int verbose;
static int assembly_lines;

static void usage(int code)
{
    fprintf(code ? stderr : stdout,
            "usage: cc [-S] [-v] [-o program.serp] [-I dir] [-D name[=value]] file.c ...\n"
            "  -S        keep the assembly and stop before apple\n"
            "  -v        show every step\n"
            "  -o FILE   write the program to FILE\n"
            "  -I DIR    also look for include files in DIR\n"
            "  -D NAME   define a macro\n"
            "  -L DIR    take the C library from DIR instead of /lib/c\n"
            "  -nostdlib do not compile the C library in\n");
    exit(code);
}

static char *replace_extension(char *path, char *extension)
{
    char *slash = strrchr(path, '/');
    char *dot = strrchr(path, '.');
    int length = dot && (!slash || dot > slash) ? dot - path : (int)strlen(path);
    return format_text("%.*s%s", length, path, extension);
}

static char *local_name(char *path, char *extension)
{
    char *slash = strrchr(path, '/');
    return replace_extension(slash ? slash + 1 : path, extension);
}

static void parse_arguments(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            usage(0);
        if (!strcmp(arg, "-S")) {
            assembly_only = 1;
        } else if (!strcmp(arg, "-v")) {
            verbose = 1;
        } else if (!strcmp(arg, "-nostdlib")) {
            no_library = 1;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "-I") || !strcmp(arg, "-D") || !strcmp(arg, "-L")) {
            if (i + 1 >= argc)
                usage(1);
            char *value = argv[++i];
            if (arg[1] == 'o')
                output_path = value;
            else if (arg[1] == 'I' && include_path_count < 7)
                include_paths[include_path_count++] = value;
            else if (arg[1] == 'D' && define_count < 16)
                defines[define_count++] = value;
            else if (arg[1] == 'L')
                library_dir = value;
        } else if (arg[0] == '-') {
            fprintf(stderr, "cc: unknown option %s\n", arg);
            usage(1);
        } else if (source_count < 32) {
            sources[source_count++] = arg;
        }
    }
    if (!source_count)
        usage(1);
}

static void predefine(void)
{
    define_macro("__scc__", "1");
    define_macro("__serpentis__", "1");
    define_macro("__STDC__", "1");
    define_macro("__x86_64__", "1");
    define_macro("__LP64__", "1");
    define_macro("__attribute__(x)", "");
    define_macro("__builtin_unreachable()", "");
    for (int i = 0; i < define_count; i++) {
        char *equals = strchr(defines[i], '=');
        if (equals)
            define_macro(copy_text(defines[i], equals - defines[i]), equals + 1);
        else
            define_macro(defines[i], "1");
    }
    save_preprocessor();
}

static unsigned long now(void)
{
#ifdef __serpentis__
    return serp_ticks();
#else
    return 0;
#endif
}

static long file_size(char *path)
{
#ifdef __serpentis__
    struct serp_stat info;
    if (serp_stat(path, &info) < 0)
        return -1;
    return (long)info.size;
#else
    FILE *file = fopen(path, "rb");
    if (!file)
        return -1;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    return size;
#endif
}

static void step(char *format, char *name)
{
    if (verbose)
        printf(format, name);
}

static void compile(char *path, int unit)
{
    step("cc: compiling %s\n", path);
    Token *tok = tokenize_file(path);
    if (!tok)
        fatal("cannot open %s", path);
    reset_preprocessor();
    tok = preprocess(tok);
    mark_keywords(tok);
    parse_unit(tok, unit);
}

static void write_file(char *path, Buffer *buffer)
{
    FILE *file = fopen(path, "w");
    if (!file)
        fatal("cannot write %s", path);
    fwrite(buffer->data, 1, buffer->length, file);
    if (fclose(file))
        fatal("cannot write %s", path);
}

static int assemble(char *assembly, char *program)
{
#ifdef __serpentis__
    char *arguments = format_text("-q %s -o %s", assembly, program);
    int pid = serp_spawn("/bin/apple.serp", arguments);  // yes, cc calls my own assembler
    if (pid < 0)
        fatal("cannot start /bin/apple.serp");
    return serp_waitpid(pid, 0);
#else
    fprintf(stderr, "cc: assembling %s into %s needs SERPENTIS\n", assembly, program);
    return 1;
#endif
}

int main(int argc, char **argv)
{
    unsigned long started = now();
    parse_arguments(argc, argv);
    include_paths[include_path_count++] = format_text("%s/include", library_dir);
    predefine();

    int unit = 0;
    for (int i = 0; i < source_count; i++)
        compile(sources[i], unit++);
    if (!no_library)
        for (int i = 0; library_sources[i]; i++)
            compile(format_text("%s/%s", library_dir, library_sources[i]), unit++);

    Buffer assembly = {0, 0, 0};
    generate(&assembly);
    for (int i = 0; i < assembly.length; i++)
        assembly_lines += assembly.data[i] == '\n';
    if (verbose)
        printf("cc: %d functions, %d lines of assembly\n", generated_functions, assembly_lines);

    char *program = output_path ? output_path : local_name(sources[0], ".serp");
    if (assembly_only) {
        char *path = output_path ? output_path : local_name(sources[0], ".asm");
        write_file(path, &assembly);
        printf("cc: wrote %s, %d lines of assembly\n", path, assembly_lines);
        return 0;
    }
    char *assembly_path = replace_extension(program, ".asm");
    write_file(assembly_path, &assembly);
    step("cc: assembling %s with apple\n", assembly_path);
    int status = assemble(assembly_path, program);
    remove(assembly_path);
    if (status) {
        fprintf(stderr, "cc: apple failed on the generated assembly\n");
        return 1;
    }
    unsigned long ticks = now() - started;
    printf("cc: built %s from %d file%s, %d functions, %ld bytes, %lu.%02lu seconds\n", program, source_count,
           source_count == 1 ? "" : "s", generated_functions, file_size(program), ticks / 100, ticks % 100);
    return 0;
}
