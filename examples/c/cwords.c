#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct word {
    char *text;
    int count;
};

static struct word *words;
static int word_count;
static int word_capacity;

static void add_word(const char *text)
{
    for (int i = 0; i < word_count; i++) {
        if (!strcmp(words[i].text, text)) {
            words[i].count++;
            return;
        }
    }
    if (word_count == word_capacity) {
        word_capacity = word_capacity ? word_capacity * 2 : 64;
        words = realloc(words, word_capacity * sizeof *words);
    }
    words[word_count].text = strdup(text);
    words[word_count].count = 1;
    word_count++;
}

static int by_count(const void *a, const void *b)
{
    const struct word *left = a;
    const struct word *right = b;
    if (left->count != right->count)
        return right->count - left->count;
    return strcmp(left->text, right->text);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: cwords FILE\n");
        return 1;
    }
    FILE *input = fopen(argv[1], "r");
    if (!input) {
        printf("cwords: cannot open %s\n", argv[1]);
        return 1;
    }
    char current[64];
    int length = 0, lines = 0, total = 0;
    for (;;) {
        int c = fgetc(input);
        if (c == '\n')
            lines++;
        if (c != EOF && (isalnum(c) || c == '_')) {
            if (length < (int)sizeof current - 1)
                current[length++] = (char)tolower(c);
            continue;
        }
        if (length) {
            current[length] = 0;
            add_word(current);
            total++;
            length = 0;
        }
        if (c == EOF)
            break;
    }
    fclose(input);
    qsort(words, word_count, sizeof *words, by_count);
    printf("%d lines, %d words, %d different\n", lines, total, word_count);
    for (int i = 0; i < word_count && i < 5; i++)
        printf("%-12s %d\n", words[i].text, words[i].count);
    return 0;
}
