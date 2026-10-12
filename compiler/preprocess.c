#include "cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Macro Macro;
typedef struct Hide Hide;
typedef struct Cond Cond;
typedef struct Arg Arg;

struct Hide {
    Hide *next;
    char *name;
};

struct Macro {
    Macro *next;
    char *name;
    int function_like;
    char *params[16];
    int param_count;
    int variadic;
    int deleted;
    Token *body;
};

struct Cond {
    Cond *next;
    int taken;
    int in_else;
    Token *tok;
};

struct Arg {
    char *name;
    Token *raw;
    Token *expanded;
};

static Macro *macros;
static Macro *saved_macros;
static Cond *conds;
static char *included_once[64];
static int included_once_count;

char *include_paths[8];
int include_path_count;

static Token *expand_all(Token *tok);

typedef struct HideToken {
    Token tok;
    Hide *hideset;
} HideToken;

static Hide *hideset_of(Token *tok)
{
    return ((HideToken *)tok)->hideset;
}

static Token *copy_token(Token *tok)
{
    HideToken *copy = malloc(sizeof(HideToken));
    copy->tok = *tok;
    copy->tok.next = 0;
    copy->hideset = 0;
    return &copy->tok;
}

static Token *clone_token(Token *tok)
{
    Token *copy = copy_token(tok);
    ((HideToken *)copy)->hideset = hideset_of(tok);
    return copy;
}

static Token *convert_list(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    for (; tok; tok = tok->next)
        cur = cur->next = copy_token(tok);
    return head.next;
}

static int hidden(Token *tok, char *name)
{
    for (Hide *h = hideset_of(tok); h; h = h->next)
        if (!strcmp(h->name, name))
            return 1;
    return 0;
}

static void add_hidden(Token *tok, Hide *extra, char *name)
{
    HideToken *ht = (HideToken *)tok;
    for (Hide *h = extra; h; h = h->next) {
        if (hidden(tok, h->name))
            continue;
        Hide *copy = malloc(sizeof(Hide));
        copy->name = h->name;
        copy->next = ht->hideset;
        ht->hideset = copy;
    }
    if (name && !hidden(tok, name)) {
        Hide *own = malloc(sizeof(Hide));
        own->name = name;
        own->next = ht->hideset;
        ht->hideset = own;
    }
}

static Token *new_eof(Token *like)
{
    Token *tok = copy_token(like);
    tok->kind = TK_EOF;
    tok->length = 0;
    return tok;
}

static Token *append(Token *first, Token *second)
{
    if (first->kind == TK_EOF)
        return second;
    Token head = {0};
    Token *cur = &head;
    for (; first->kind != TK_EOF; first = first->next)
        cur = cur->next = clone_token(first);
    cur->next = second;
    return head.next;
}

static Macro *find_macro(Token *tok)
{
    if (tok->kind != TK_IDENT)
        return 0;
    for (Macro *m = macros; m; m = m->next)
        if ((int)strlen(m->name) == tok->length && !memcmp(m->name, tok->text, tok->length))
            return m->deleted ? 0 : m;
    return 0;
}

static Token *skip_line(Token *tok)
{
    while (!tok->at_bol)
        tok = tok->next;
    return tok;
}

static Token *copy_line(Token **rest, Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    for (; !tok->at_bol; tok = tok->next)
        cur = cur->next = clone_token(tok);
    cur->next = new_eof(tok);
    *rest = tok;
    return head.next;
}

static void remove_macro(char *name)
{
    Macro *m = calloc(1, sizeof(Macro));
    m->name = name;
    m->deleted = 1;
    m->next = macros;
    macros = m;
}

void save_preprocessor(void)
{
    saved_macros = macros;
}

void reset_preprocessor(void)
{
    macros = saved_macros;
    conds = 0;
    included_once_count = 0;
}

static Token *read_define(Token *tok)
{
    if (tok->kind != TK_IDENT && tok->kind != TK_KEYWORD)
        error_at(tok, "macro name expected");
    Macro *m = calloc(1, sizeof(Macro));
    m->name = copy_text(tok->text, tok->length);
    tok = tok->next;
    if (!tok->space && equal(tok, "(") && !tok->at_bol) {
        m->function_like = 1;
        tok = tok->next;
        while (!equal(tok, ")")) {
            if (m->param_count)
                tok = skip(tok, ",");
            if (equal(tok, "...")) {
                m->variadic = 1;
                m->params[m->param_count++] = "__VA_ARGS__";
                tok = tok->next;
                break;
            }
            if (m->param_count == 16)
                error_at(tok, "too many macro parameters");
            m->params[m->param_count++] = copy_text(tok->text, tok->length);
            tok = tok->next;
        }
        tok = skip(tok, ")");
    }
    m->body = copy_line(&tok, tok);
    m->next = macros;
    macros = m;
    return tok;
}

void define_macro(char *name, char *body)
{
    char *text = format_text("%s %s\n", name, body);
    read_define(convert_list(tokenize_text("<builtin>", text)));
}

static Token *stringize(Token *like, Token *arg)
{
    Buffer text = {0, 0, 0};
    buffer_add(&text, "\"", 1);
    for (Token *t = arg; t->kind != TK_EOF; t = t->next) {
        if (t != arg && t->space)
            buffer_add(&text, " ", 1);
        for (int i = 0; i < t->length; i++) {
            if (t->text[i] == '"' || t->text[i] == '\\')
                buffer_add(&text, "\\", 1);
            buffer_add(&text, t->text + i, 1);
        }
    }
    buffer_add(&text, "\"\n", 2);
    Token *tok = copy_token(tokenize_text(like->file, text.data));
    tok->line = like->line;
    tok->space = like->space;
    tok->at_bol = 0;
    tok->next = 0;
    return tok;
}

static Token *paste(Token *left, Token *right)
{
    char *text = format_text("%.*s%.*s\n", left->length, left->text, right->length, right->text);
    Token *pasted = tokenize_text(left->file, text);
    if (pasted->next->kind != TK_EOF)
        error_at(left, "pasting does not give a valid token");
    Token *tok = copy_token(pasted);
    tok->line = left->line;
    tok->space = left->space;
    tok->at_bol = 0;
    return tok;
}

static Arg *find_arg(Arg *args, int count, Token *tok)
{
    if (tok->kind != TK_IDENT && tok->kind != TK_KEYWORD)
        return 0;
    for (int i = 0; i < count; i++)
        if ((int)strlen(args[i].name) == tok->length && !memcmp(args[i].name, tok->text, tok->length))
            return &args[i];
    return 0;
}

static Token *read_argument(Token **rest, Token *tok, int keep_commas)
{
    Token head = {0};
    Token *cur = &head;
    int depth = 0;
    for (;;) {
        if (tok->kind == TK_EOF)
            error_at(tok, "unterminated macro call");
        if (depth == 0 && equal(tok, ")"))
            break;
        if (depth == 0 && equal(tok, ",") && !keep_commas)
            break;
        if (equal(tok, "("))
            depth++;
        else if (equal(tok, ")"))
            depth--;
        cur = cur->next = clone_token(tok);
        tok = tok->next;
    }
    cur->next = new_eof(tok);
    *rest = tok;
    return head.next;
}

static Token *substitute(Token *body, Arg *args, int count)
{
    Token head = {0};
    Token *cur = &head;
    for (Token *tok = body; tok->kind != TK_EOF; tok = tok->next) {
        if (equal(tok, "#")) {
            Arg *arg = find_arg(args, count, tok->next);
            if (!arg)
                error_at(tok->next, "'#' is not followed by a macro parameter");
            cur = cur->next = stringize(tok, arg->raw);
            tok = tok->next;
            continue;
        }
        if (equal(tok, "##")) {
            if (cur == &head)
                error_at(tok, "'##' at the start of a macro");
            Token *right = tok->next;
            Arg *arg = find_arg(args, count, right);
            if (arg) {
                Token *first = arg->raw;
                if (first->kind != TK_EOF) {
                    Token *pasted = paste(cur, first);
                    *cur = *pasted;
                    for (Token *t = first->next; t->kind != TK_EOF; t = t->next)
                        cur = cur->next = clone_token(t);
                }
            } else {
                Token *pasted = paste(cur, right);
                *cur = *pasted;
            }
            cur->next = 0;
            tok = right;
            continue;
        }
        Arg *arg = find_arg(args, count, tok);
        if (arg && equal(tok->next, "##")) {
            Token *raw = arg->raw;
            if (raw->kind == TK_EOF) {
                Arg *next_arg = find_arg(args, count, tok->next->next);
                if (next_arg) {
                    for (Token *t = next_arg->raw; t->kind != TK_EOF; t = t->next)
                        cur = cur->next = clone_token(t);
                } else {
                    cur = cur->next = clone_token(tok->next->next);
                }
                tok = tok->next->next;
                continue;
            }
            for (Token *t = raw; t->kind != TK_EOF; t = t->next)
                cur = cur->next = clone_token(t);
            continue;
        }
        if (arg) {
            Token *first = cur;
            for (Token *t = arg->expanded; t->kind != TK_EOF; t = t->next)
                cur = cur->next = clone_token(t);
            if (first->next)
                first->next->space = tok->space;
            continue;
        }
        cur = cur->next = clone_token(tok);
    }
    cur->next = new_eof(body);
    return head.next;
}

static int expand_macro(Token **rest, Token *tok)
{
    Macro *m = find_macro(tok);
    if (!m || hidden(tok, m->name))
        return 0;
    if (!m->function_like) {
        Token *body = substitute(m->body, 0, 0);
        for (Token *t = body; t->kind != TK_EOF; t = t->next) {
            add_hidden(t, hideset_of(tok), m->name);
            t->file = tok->file;
            t->line = tok->line;
        }
        if (body->kind != TK_EOF) {
            body->space = tok->space;
            body->at_bol = tok->at_bol;
        }
        *rest = append(body, tok->next);
        return 1;
    }
    if (!equal(tok->next, "("))
        return 0;
    Token *name = tok;
    tok = tok->next->next;
    Arg args[16];
    int count = 0;
    while (count < m->param_count) {
        int last_variadic = m->variadic && count == m->param_count - 1;
        if (count && !last_variadic)
            tok = skip(tok, ",");
        else if (count && last_variadic && equal(tok, ","))
            tok = tok->next;
        args[count].name = m->params[count];
        args[count].raw = read_argument(&tok, tok, last_variadic);
        args[count].expanded = expand_all(args[count].raw);
        count++;
    }
    if (m->param_count == 0 && !equal(tok, ")"))
        error_at(tok, "too many arguments for macro %s", m->name);
    if (!equal(tok, ")"))
        error_at(tok, "too many arguments for macro %s", m->name);
    Token *body = substitute(m->body, args, count);
    for (Token *t = body; t->kind != TK_EOF; t = t->next) {
        add_hidden(t, hideset_of(name), m->name);
        t->file = name->file;
        t->line = name->line;
    }
    if (body->kind != TK_EOF) {
        body->space = name->space;
        body->at_bol = name->at_bol;
    }
    *rest = append(body, tok->next);
    return 1;
}

static Token *expand_all(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    while (tok->kind != TK_EOF) {
        if (expand_macro(&tok, tok))
            continue;
        cur = cur->next = clone_token(tok);
        tok = tok->next;
    }
    cur->next = tok;
    return head.next;
}

static char *directory_of(char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash)
        return "";
    if (slash == path)
        return "/";
    return copy_text(path, slash - path);
}

static char *join_path(char *directory, char *name)
{
    if (!*directory)
        return name;
    if (!strcmp(directory, "/"))
        return format_text("/%s", name);
    return format_text("%s/%s", directory, name);
}

static int file_exists(char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    fclose(file);
    return 1;
}

static char *find_include(Token *tok, char *name, int quoted)
{
    if (name[0] == '/')
        return file_exists(name) ? name : 0;
    if (quoted) {
        char *path = join_path(directory_of(tok->file), name);
        if (file_exists(path))
            return path;
    }
    for (int i = 0; i < include_path_count; i++) {
        char *path = join_path(include_paths[i], name);
        if (file_exists(path))
            return path;
    }
    return 0;
}

static Token *include_file(Token *directive_tok, Token **rest)
{
    Token *tok = directive_tok->next;
    char *name = 0;
    int quoted = 0;
    if (tok->kind == TK_STR) {
        name = copy_text(tok->str, tok->str_length - 1);
        quoted = 1;
        tok = tok->next;
    } else if (equal(tok, "<")) {
        Token *start = tok->next;
        tok = start;
        while (!equal(tok, ">")) {
            if (tok->at_bol)
                error_at(tok, "expected '>'");
            tok = tok->next;
        }
        Buffer text = {0, 0, 0};
        for (Token *t = start; t != tok; t = t->next)
            buffer_add(&text, t->text, t->length);
        name = text.data;
        quoted = 0;
        tok = tok->next;
    } else {
        error_at(tok, "expected a file name");
    }
    *rest = skip_line(tok);
    char *path = find_include(directive_tok, name, quoted);
    if (!path)
        error_at(directive_tok, "cannot find include file %s", name);
    for (int i = 0; i < included_once_count; i++)
        if (!strcmp(included_once[i], path))
            return 0;
    Token *included = tokenize_file(path);
    if (!included)
        error_at(directive_tok, "cannot read %s", path);
    return convert_list(included);
}

typedef struct {
    Token *tok;
} Reader;

static long long eval_expression(Reader *r);

static long long eval_primary(Reader *r)
{
    Token *tok = r->tok;
    if (equal(tok, "(")) {
        r->tok = tok->next;
        long long value = eval_expression(r);
        r->tok = skip(r->tok, ")");
        return value;
    }
    if (equal(tok, "!")) {
        r->tok = tok->next;
        return !eval_primary(r);
    }
    if (equal(tok, "-")) {
        r->tok = tok->next;
        return -eval_primary(r);
    }
    if (equal(tok, "+")) {
        r->tok = tok->next;
        return eval_primary(r);
    }
    if (equal(tok, "~")) {
        r->tok = tok->next;
        return ~eval_primary(r);
    }
    if (tok->kind == TK_NUM) {
        r->tok = tok->next;
        return tok->value;
    }
    if (tok->kind == TK_IDENT || tok->kind == TK_KEYWORD) {
        r->tok = tok->next;
        return 0;
    }
    error_at(tok, "invalid expression in #if");
    return 0;
}

static int precedence(Token *tok)
{
    static char *levels[][4] = {
        {"||"}, {"&&"}, {"|"}, {"^"}, {"&"}, {"==", "!="}, {"<", "<=", ">", ">="},
        {"<<", ">>"}, {"+", "-"}, {"*", "/", "%"}
    };
    if (tok->kind != TK_PUNCT)
        return -1;
    for (int level = 0; level < 10; level++)
        for (int i = 0; i < 4 && levels[level][i]; i++)
            if (equal(tok, levels[level][i]))
                return level;
    return -1;
}

static long long apply(Token *op, long long a, long long b)
{
    if (equal(op, "||")) return a || b;
    if (equal(op, "&&")) return a && b;
    if (equal(op, "|")) return a | b;
    if (equal(op, "^")) return a ^ b;
    if (equal(op, "&")) return a & b;
    if (equal(op, "==")) return a == b;
    if (equal(op, "!=")) return a != b;
    if (equal(op, "<")) return a < b;
    if (equal(op, "<=")) return a <= b;
    if (equal(op, ">")) return a > b;
    if (equal(op, ">=")) return a >= b;
    if (equal(op, "<<")) return a << b;
    if (equal(op, ">>")) return a >> b;
    if (equal(op, "+")) return a + b;
    if (equal(op, "-")) return a - b;
    if (equal(op, "*")) return a * b;
    if (!b)
        error_at(op, "division by zero in #if");
    if (equal(op, "/")) return a / b;
    return a % b;
}

static long long eval_binary(Reader *r, int level)
{
    long long value = eval_primary(r);
    for (;;) {
        Token *op = r->tok;
        int p = precedence(op);
        if (p < level)
            return value;
        r->tok = op->next;
        long long right = eval_binary(r, p + 1);
        value = apply(op, value, right);
    }
}

static long long eval_expression(Reader *r)
{
    long long value = eval_binary(r, 0);
    if (equal(r->tok, "?")) {
        r->tok = r->tok->next;
        long long yes = eval_expression(r);
        r->tok = skip(r->tok, ":");
        long long no = eval_expression(r);
        return value ? yes : no;
    }
    return value;
}

static long long eval_condition(Token **rest, Token *tok)
{
    Token *line = copy_line(rest, tok);
    Token head = {0};
    Token *cur = &head;
    for (Token *t = line; t->kind != TK_EOF; t = t->next) {
        if (equal(t, "defined")) {
            Token *name = t->next;
            int paren = equal(name, "(");
            if (paren)
                name = name->next;
            Token *value = copy_token(name);
            value->kind = TK_NUM;
            value->value = find_macro(name) != 0;
            t = name;
            if (paren) {
                t = name->next;
                if (!equal(t, ")"))
                    error_at(t, "expected ')'");
            }
            cur = cur->next = value;
            continue;
        }
        cur = cur->next = clone_token(t);
    }
    cur->next = new_eof(tok);
    Reader r;
    r.tok = expand_all(head.next);
    long long value = eval_expression(&r);
    if (r.tok->kind != TK_EOF)
        error_at(r.tok, "extra tokens in #if");
    return value;
}

static Token *skip_conditional(Token *tok)
{
    int depth = 0;
    while (tok->kind != TK_EOF) {
        if (tok->at_bol && equal(tok, "#")) {
            Token *d = tok->next;
            if (equal(d, "if") || equal(d, "ifdef") || equal(d, "ifndef")) {
                depth++;
            } else if (equal(d, "endif")) {
                if (!depth)
                    return tok;
                depth--;
            } else if (!depth && (equal(d, "elif") || equal(d, "else"))) {
                return tok;
            }
        }
        tok = tok->next;
    }
    return tok;
}

static void push_cond(Token *tok, int taken)
{
    Cond *c = calloc(1, sizeof(Cond));
    c->tok = tok;
    c->taken = taken;
    c->next = conds;
    conds = c;
}

static void directive(Token *hash, Token **rest_out)
{
    Token *tok = hash->next;
    if (tok->at_bol) {
        *rest_out = tok;
        return;
    }
    if (equal(tok, "include")) {
        Token *after;
        Token *included = include_file(tok, &after);
        if (included)
            after = append(included, after);
        *rest_out = after;
        return;
    }
    if (equal(tok, "define")) {
        *rest_out = read_define(tok->next);
        return;
    }
    if (equal(tok, "undef")) {
        remove_macro(copy_text(tok->next->text, tok->next->length));
        *rest_out = skip_line(tok->next);
        return;
    }
    if (equal(tok, "ifdef") || equal(tok, "ifndef")) {
        int defined = find_macro(tok->next) != 0;
        int taken = equal(tok, "ifdef") ? defined : !defined;
        push_cond(tok, taken);
        tok = skip_line(tok->next->next);
        *rest_out = taken ? tok : skip_conditional(tok);
        return;
    }
    if (equal(tok, "if")) {
        Token *after;
        int taken = eval_condition(&after, tok->next) != 0;
        push_cond(tok, taken);
        *rest_out = taken ? after : skip_conditional(after);
        return;
    }
    if (equal(tok, "elif")) {
        if (!conds || conds->in_else)
            error_at(tok, "#elif without #if");
        if (conds->taken) {
            *rest_out = skip_conditional(skip_line(tok->next));
            return;
        }
        Token *after;
        int taken = eval_condition(&after, tok->next) != 0;
        conds->taken = taken;
        *rest_out = taken ? after : skip_conditional(after);
        return;
    }
    if (equal(tok, "else")) {
        if (!conds || conds->in_else)
            error_at(tok, "#else without #if");
        conds->in_else = 1;
        tok = skip_line(tok->next);
        *rest_out = conds->taken ? skip_conditional(tok) : tok;
        conds->taken = 1;
        return;
    }
    if (equal(tok, "endif")) {
        if (!conds)
            error_at(tok, "#endif without #if");
        conds = conds->next;
        *rest_out = skip_line(tok->next);
        return;
    }
    if (equal(tok, "pragma")) {
        if (equal(tok->next, "once") && included_once_count < 64)
            included_once[included_once_count++] = tok->file;
        *rest_out = skip_line(tok->next);
        return;
    }
    if (equal(tok, "error")) {
        error_at(tok, "#error");
    }
    if (equal(tok, "warning") || equal(tok, "line")) {
        *rest_out = skip_line(tok->next);
        return;
    }
    error_at(tok, "unknown directive");
}

static Token *line_number_token(Token *tok, int file)
{
    Token *t = copy_token(tok);
    if (file) {
        char *text = format_text("\"%s\"\n", tok->file);
        Token *s = tokenize_text(tok->file, text);
        t->kind = TK_STR;
        t->str = s->str;
        t->str_length = s->str_length;
        t->type = s->type;
        t->text = s->text;
        t->length = s->length;
    } else {
        t->kind = TK_NUM;
        t->value = tok->line;
        t->type = ty_int;
    }
    return t;
}

Token *preprocess(Token *tok)
{
    tok = convert_list(tok);
    Token head = {0};
    Token *cur = &head;
    while (tok->kind != TK_EOF) {
        if (tok->at_bol && equal(tok, "#")) {
            directive(tok, &tok);
            continue;
        }
        if (equal(tok, "__LINE__") || equal(tok, "__FILE__")) {
            cur = cur->next = line_number_token(tok, equal(tok, "__FILE__"));
            tok = tok->next;
            continue;
        }
        if (expand_macro(&tok, tok))
            continue;
        cur = cur->next = tok;
        tok = tok->next;
    }
    if (conds)
        error_at(conds->tok, "unterminated conditional");
    cur->next = tok;
    return head.next;
}
