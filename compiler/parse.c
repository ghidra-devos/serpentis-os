#include "cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarScope VarScope;
typedef struct TagScope TagScope;
typedef struct Scope Scope;
typedef struct Initializer Initializer;
typedef struct Designator Designator;

struct VarScope {
    VarScope *next;
    char *name;
    Obj *var;
    Type *type_def;
    Type *enum_type;
    long long enum_value;
};

struct TagScope {
    TagScope *next;
    char *name;
    Type *type;
};

struct Scope {
    Scope *next;
    VarScope *vars;
    TagScope *tags;
};

typedef struct {
    int is_typedef;
    int is_static;
    int is_extern;
} VarAttr;

struct Initializer {
    Type *type;
    Token *tok;
    int is_flexible;
    Node *expr;
    Initializer **children;
};

struct Designator {
    Designator *next;
    int index;
    Member *member;
    Obj *var;
};

Obj *globals;
Buffer global_asm;

static Scope global_scope;
static Scope *scope = &global_scope;
static Obj *locals;
static Obj *current_fn;
static Node *gotos;
static Node *labels;
static int break_label;
static int continue_label;
static Node *current_switch;
static int unit_number;
int unique_number = 1;

static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *declarator(Token **rest, Token *tok, Type *type);
static Type *typename(Token **rest, Token *tok);
static Node *declaration(Token **rest, Token *tok, Type *base, VarAttr *attr);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *conditional(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Node *postfix_tail(Token **rest, Token *tok, Node *node);
static int is_asm(Token *tok);
static char *asm_text(Token **rest, Token *tok);
static Node *compound_literal(Token **rest, Token *tok, Type *type, Token *start);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static long long eval(Node *node);
static double eval_double(Node *node);
static long long eval_reloc(Node *node, Obj **label);
static int is_typename(Token *tok);

static char *token_name(Token *tok)
{
    return copy_text(tok->text, tok->length);
}

static int consume(Token **rest, Token *tok, char *text)
{
    if (equal(tok, text)) {
        *rest = tok->next;
        return 1;
    }
    *rest = tok;
    return 0;
}

static void enter_scope(void)
{
    Scope *s = calloc(1, sizeof(Scope));
    s->next = scope;
    scope = s;
}

static void leave_scope(void)
{
    scope = scope->next;
}

static VarScope *find_var(Token *tok)
{
    for (Scope *s = scope; s; s = s->next)
        for (VarScope *v = s->vars; v; v = v->next)
            if ((int)strlen(v->name) == tok->length && !memcmp(v->name, tok->text, tok->length))
                return v;
    return 0;
}

static Type *find_tag(Token *tok)
{
    for (Scope *s = scope; s; s = s->next)
        for (TagScope *t = s->tags; t; t = t->next)
            if ((int)strlen(t->name) == tok->length && !memcmp(t->name, tok->text, tok->length))
                return t->type;
    return 0;
}

static VarScope *push_scope(char *name)
{
    VarScope *v = calloc(1, sizeof(VarScope));
    v->name = name;
    v->next = scope->vars;
    scope->vars = v;
    return v;
}

static void push_tag(Token *tok, Type *type)
{
    TagScope *t = calloc(1, sizeof(TagScope));
    t->name = token_name(tok);
    t->type = type;
    t->next = scope->tags;
    scope->tags = t;
}

static Node *new_node(int kind, Token *tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_binary(int kind, Node *lhs, Node *rhs, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(int kind, Node *lhs, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    return node;
}

static Node *new_num(long long value, Token *tok)
{
    Node *node = new_node(ND_NUM, tok);
    node->value = value;
    return node;
}

static Node *new_long(long long value, Token *tok)
{
    Node *node = new_num(value, tok);
    node->type = ty_long;
    return node;
}

static Node *new_size(long long value, Token *tok)
{
    Node *node = new_num(value, tok);
    node->type = ty_ulong;
    return node;
}

static Node *new_var_node(Obj *var, Token *tok)
{
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

static Node *new_cast(Node *expr, Type *type)
{
    add_type(expr);
    if (type->kind != TY_VOID) {
        Node *folded = fold_cast(expr, copy_type(type));
        if (folded)
            return folded;
    }
    Node *node = new_unary(ND_CAST, expr, expr->tok);
    node->type = copy_type(type);
    return node;
}

static Obj *new_lvar(char *name, Type *type)
{
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->type = type;
    var->is_local = 1;
    var->next = locals;
    locals = var;
    push_scope(name)->var = var;
    return var;
}

static Obj *new_global(char *name, char *label, Type *type)
{
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->label = label;
    var->type = type;
    var->next = globals;
    globals = var;
    return var;
}

static Obj *find_external(char *name)
{
    for (Obj *var = globals; var; var = var->next)
        if (!var->is_static && !strcmp(var->name, name))
            return var;
    return 0;
}

static Obj *declare_global(Token *tok, char *name, Type *type, int is_static, int is_function)
{
    VarScope *existing = 0;
    for (VarScope *v = global_scope.vars; v; v = v->next)
        if (!strcmp(v->name, name)) {
            existing = v;
            break;
        }
    Obj *var = existing && existing->var ? existing->var : 0;
    if (!var && !is_static)
        var = find_external(name);
    if (var) {
        if (var->is_function != is_function)
            error_at(tok, "%s redeclared as a different kind of symbol", name);
        if (type->kind == TY_ARRAY && var->type->kind == TY_ARRAY && type->size > var->type->size)
            var->type = type;
        else if (is_function && type->prototyped)
            var->type = type;
    } else {
        char *label = is_static ? format_text("S%d_%s", unit_number, name) : format_text("_%s", name);
        var = new_global(name, label, type);
        var->is_static = is_static;
        var->is_function = is_function;
    }
    push_scope(name)->var = var;
    return var;
}

static Obj *new_string_literal(char *data, Type *type)
{
    Obj *var = new_global("", format_text("str%d", unique_number++), type);
    var->is_static = 1;
    var->is_definition = 1;
    var->has_init = 1;
    var->data = data;
    return var;
}

static int is_typename(Token *tok)
{
    static char *names[] = {
        "void", "_Bool", "char", "short", "int", "long", "struct", "union", "enum", "typedef",
        "static", "extern", "inline", "const", "volatile", "register", "auto", "restrict",
        "signed", "unsigned", "_Noreturn", "float", "double", 0
    };
    for (int i = 0; names[i]; i++)
        if (equal(tok, names[i]))
            return 1;
    if (tok->kind != TK_IDENT)
        return 0;
    VarScope *v = find_var(tok);
    return v && v->type_def;
}

static Type *struct_union_decl(Token **rest, Token *tok, int kind)
{
    Token *tag = 0;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }
    if (tag && !equal(tok, "{")) {
        *rest = tok;
        Type *type = find_tag(tag);
        if (type)
            return type;
        type = new_struct_type(kind);
        push_tag(tag, type);
        return type;
    }
    tok = skip(tok, "{");
    Type *type = 0;
    if (tag) {
        for (TagScope *t = scope->tags; t; t = t->next)
            if ((int)strlen(t->name) == tag->length && !memcmp(t->name, tag->text, tag->length))
                type = t->type;
    }
    if (!type) {
        type = new_struct_type(kind);
        if (tag)
            push_tag(tag, type);
    }
    Member head = {0};
    Member *cur = &head;
    while (!equal(tok, "}")) {
        VarAttr attr = {0};
        Type *base = declspec(&tok, tok, &attr);
        int first = 1;
        while (!consume(&tok, tok, ";")) {
            if (!first)
                tok = skip(tok, ",");
            first = 0;
            Member *member = calloc(1, sizeof(Member));
            member->type = declarator(&tok, tok, base);
            member->name = member->type->name;
            if (equal(tok, ":"))
                error_at(tok, "bit fields are not supported");
            cur = cur->next = member;
        }
    }
    *rest = tok->next;
    type->members = head.next;
    int offset = 0, align = 1, size = 0;
    for (Member *m = type->members; m; m = m->next) {
        if (m->type->kind == TY_ARRAY && m->type->length < 0)
            m->type = array_of(m->type->base, 0);
        if (kind == TY_STRUCT) {
            offset = align_to(offset, m->type->align);
            m->offset = offset;
            offset += m->type->size;
            size = offset;
        } else {
            m->offset = 0;
            if (m->type->size > size)
                size = m->type->size;
        }
        if (m->type->align > align)
            align = m->type->align;
    }
    type->align = align;
    type->size = align_to(size, align);
    type->complete = 1;
    return type;
}

static Type *enum_specifier(Token **rest, Token *tok)
{
    Type *type = copy_type(ty_int);
    type->kind = TY_ENUM;
    Token *tag = 0;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }
    if (tag && !equal(tok, "{")) {
        Type *found = find_tag(tag);
        *rest = tok;
        if (found)
            return found;
        push_tag(tag, type);
        return type;
    }
    tok = skip(tok, "{");
    long long value = 0;
    int first = 1;
    while (!equal(tok, "}")) {
        if (!first)
            tok = skip(tok, ",");
        first = 0;
        if (equal(tok, "}"))
            break;
        char *name = token_name(tok);
        tok = tok->next;
        if (equal(tok, "=")) {
            Node *value_expr = conditional(&tok, tok->next);
            value = eval(value_expr);
        }
        VarScope *v = push_scope(name);
        v->enum_type = type;
        v->enum_value = value++;
    }
    *rest = tok->next;
    if (tag)
        push_tag(tag, type);
    return type;
}

static Type *declspec(Token **rest, Token *tok, VarAttr *attr)
{
    enum {
        VOID = 1 << 0,
        BOOL = 1 << 2,
        CHAR = 1 << 4,
        SHORT = 1 << 6,
        INT = 1 << 8,
        LONG = 1 << 10,
        FLOAT = 1 << 12,
        DOUBLE = 1 << 14,
        OTHER = 1 << 16,
        SIGNED = 1 << 18,
        UNSIGNED = 1 << 19
    };
    Type *type = ty_int;
    int counter = 0;
    while (is_typename(tok)) {
        if (equal(tok, "typedef") || equal(tok, "static") || equal(tok, "extern")) {
            if (!attr)
                error_at(tok, "storage class is not allowed here");
            if (equal(tok, "typedef"))
                attr->is_typedef = 1;
            else if (equal(tok, "static"))
                attr->is_static = 1;
            else
                attr->is_extern = 1;
            tok = tok->next;
            continue;
        }
        if (equal(tok, "inline") || equal(tok, "const") || equal(tok, "volatile") || equal(tok, "register") ||
            equal(tok, "auto") || equal(tok, "restrict") || equal(tok, "_Noreturn")) {
            tok = tok->next;
            continue;
        }
        VarScope *v = tok->kind == TK_IDENT ? find_var(tok) : 0;
        if (equal(tok, "struct") || equal(tok, "union") || equal(tok, "enum") || (v && v->type_def)) {
            if (counter)
                break;
            if (equal(tok, "struct"))
                type = struct_union_decl(&tok, tok->next, TY_STRUCT);
            else if (equal(tok, "union"))
                type = struct_union_decl(&tok, tok->next, TY_UNION);
            else if (equal(tok, "enum"))
                type = enum_specifier(&tok, tok->next);
            else {
                type = v->type_def;
                tok = tok->next;
            }
            counter += OTHER;
            continue;
        }
        if (equal(tok, "void"))
            counter += VOID;
        else if (equal(tok, "_Bool"))
            counter += BOOL;
        else if (equal(tok, "char"))
            counter += CHAR;
        else if (equal(tok, "short"))
            counter += SHORT;
        else if (equal(tok, "int"))
            counter += INT;
        else if (equal(tok, "long"))
            counter += LONG;
        else if (equal(tok, "float"))
            counter += FLOAT;
        else if (equal(tok, "double"))
            counter += DOUBLE;
        else if (equal(tok, "signed"))
            counter |= SIGNED;
        else if (equal(tok, "unsigned"))
            counter |= UNSIGNED;
        switch (counter) {
        case VOID:
            type = ty_void;
            break;
        case BOOL:
            type = ty_bool;
            break;
        case CHAR:
        case SIGNED + CHAR:
            type = ty_char;
            break;
        case UNSIGNED + CHAR:
            type = ty_uchar;
            break;
        case SHORT:
        case SHORT + INT:
        case SIGNED + SHORT:
        case SIGNED + SHORT + INT:
            type = ty_short;
            break;
        case UNSIGNED + SHORT:
        case UNSIGNED + SHORT + INT:
            type = ty_ushort;
            break;
        case INT:
        case SIGNED:
        case SIGNED + INT:
            type = ty_int;
            break;
        case UNSIGNED:
        case UNSIGNED + INT:
            type = ty_uint;
            break;
        case LONG:
        case LONG + INT:
        case LONG + LONG:
        case LONG + LONG + INT:
        case SIGNED + LONG:
        case SIGNED + LONG + INT:
        case SIGNED + LONG + LONG:
        case SIGNED + LONG + LONG + INT:
            type = ty_long;
            break;
        case UNSIGNED + LONG:
        case UNSIGNED + LONG + INT:
        case UNSIGNED + LONG + LONG:
        case UNSIGNED + LONG + LONG + INT:
            type = ty_ulong;
            break;
        case FLOAT:
            type = ty_float;
            break;
        case DOUBLE:
        case LONG + DOUBLE:
            type = ty_double;
            break;
        default:
            error_at(tok, "invalid type");
        }
        tok = tok->next;
    }
    *rest = tok;
    return type;
}

static Type *func_params(Token **rest, Token *tok, Type *ret)
{
    Type *fn = func_type(ret);
    if (equal(tok, "void") && equal(tok->next, ")")) {
        *rest = tok->next->next;
        fn->prototyped = 1;
        return fn;
    }
    if (equal(tok, ")")) {
        *rest = tok->next;
        return fn;
    }
    fn->prototyped = 1;
    Type head = {0};
    Type *cur = &head;
    while (!equal(tok, ")")) {
        if (cur != &head)
            tok = skip(tok, ",");
        if (equal(tok, "...")) {
            fn->variadic = 1;
            tok = tok->next;
            break;
        }
        Type *type = declspec(&tok, tok, 0);
        type = declarator(&tok, tok, type);
        Token *name = type->name;
        if (type->kind == TY_ARRAY)
            type = pointer_to(type->base);
        else if (type->kind == TY_FUNC)
            type = pointer_to(type);
        type = copy_type(type);
        type->name = name;
        cur = cur->next = type;
    }
    fn->params = head.next;
    *rest = skip(tok, ")");
    return fn;
}

static Type *type_suffix(Token **rest, Token *tok, Type *type)
{
    if (equal(tok, "("))
        return func_params(rest, tok->next, type);
    if (equal(tok, "[")) {
        tok = tok->next;
        while (equal(tok, "static") || equal(tok, "const"))
            tok = tok->next;
        if (equal(tok, "]")) {
            type = type_suffix(rest, tok->next, type);
            Type *array = array_of(type, 0);
            array->length = -1;
            array->size = 0;
            return array;
        }
        Node *size = conditional(&tok, tok);
        tok = skip(tok, "]");
        type = type_suffix(rest, tok, type);
        long long length = eval(size);
        if (length < 0)
            error_at(size->tok, "negative array size");
        return array_of(type, (int)length);
    }
    *rest = tok;
    return type;
}

static Type *pointers(Token **rest, Token *tok, Type *type)
{
    while (consume(&tok, tok, "*")) {
        type = pointer_to(type);
        while (equal(tok, "const") || equal(tok, "volatile") || equal(tok, "restrict"))
            tok = tok->next;
    }
    *rest = tok;
    return type;
}

static Type *declarator(Token **rest, Token *tok, Type *type)
{
    type = pointers(&tok, tok, type);
    if (equal(tok, "(") && !is_typename(tok->next) && !equal(tok->next, ")")) {
        Token *start = tok;
        Type dummy = {0};
        declarator(&tok, start->next, &dummy);
        tok = skip(tok, ")");
        type = type_suffix(rest, tok, type);
        Token *ignored;
        return declarator(&ignored, start->next, type);
    }
    Token *name = 0;
    if (tok->kind == TK_IDENT) {
        name = tok;
        tok = tok->next;
    }
    type = type_suffix(rest, tok, type);
    if (type->kind != TY_STRUCT && type->kind != TY_UNION)
        type = copy_type(type);
    type->name = name;
    return type;
}

static Type *typename(Token **rest, Token *tok)
{
    Type *type = declspec(&tok, tok, 0);
    return declarator(rest, tok, type);
}

static Initializer *new_initializer(Type *type, int flexible)
{
    Initializer *init = calloc(1, sizeof(Initializer));
    init->type = type;
    if (type->kind == TY_ARRAY) {
        if (flexible && type->length < 0) {
            init->is_flexible = 1;
            return init;
        }
        init->children = calloc(type->length > 0 ? type->length : 1, sizeof(Initializer *));
        for (int i = 0; i < type->length; i++)
            init->children[i] = new_initializer(type->base, 0);
        return init;
    }
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        int count = 0;
        for (Member *m = type->members; m; m = m->next)
            count++;
        init->children = calloc(count ? count : 1, sizeof(Initializer *));
        int i = 0;
        for (Member *m = type->members; m; m = m->next)
            init->children[i++] = new_initializer(m->type, 0);
    }
    return init;
}

static void initializer2(Token **rest, Token *tok, Initializer *init);

static Token *skip_excess(Token *tok)
{
    if (equal(tok, "{")) {
        tok = skip_excess(tok->next);
        return skip(tok, "}");
    }
    assign(&tok, tok);
    return tok;
}

static void string_initializer(Token **rest, Token *tok, Initializer *init)
{
    if (init->is_flexible)
        *init = *new_initializer(array_of(init->type->base, tok->str_length), 0);
    int length = init->type->length < tok->str_length ? init->type->length : tok->str_length;
    for (int i = 0; i < length; i++)
        init->children[i]->expr = new_num(tok->str[i], tok);
    *rest = tok->next;
}

static Token *array_designator(Token *tok, int *index)
{
    Node *position = conditional(&tok, tok->next);
    *index = (int)eval(position);
    if (*index < 0)
        error_at(position->tok, "negative array index");
    tok = skip(tok, "]");
    return skip(tok, "=");
}

static int count_array_elements(Token *tok, Type *type)
{
    Initializer *dummy = new_initializer(type->base, 0);
    int count = 0, index = 0, first = 1;
    while (!equal(tok, "}")) {
        if (!first)
            tok = skip(tok, ",");
        first = 0;
        if (equal(tok, "}"))
            break;
        if (equal(tok, "["))
            tok = array_designator(tok, &index);
        initializer2(&tok, tok, dummy);
        index++;
        if (index > count)
            count = index;
    }
    return count;
}

static void array_initializer_braced(Token **rest, Token *tok, Initializer *init)
{
    tok = skip(tok, "{");
    if (init->is_flexible) {
        int length = count_array_elements(tok, init->type);
        *init = *new_initializer(array_of(init->type->base, length), 0);
    }
    int first = 1;
    for (int i = 0; !consume(rest, tok, "}"); i++) {
        if (!first)
            tok = skip(tok, ",");
        first = 0;
        if (equal(tok, "}")) {
            *rest = tok->next;
            return;
        }
        if (equal(tok, "["))
            tok = array_designator(tok, &i);
        if (i < init->type->length)
            initializer2(&tok, tok, init->children[i]);
        else
            tok = skip_excess(tok);
    }
}

static int is_end(Token *tok)
{
    return equal(tok, "}") || (equal(tok, ",") && equal(tok->next, "}"));
}

static void array_initializer_flat(Token **rest, Token *tok, Initializer *init)
{
    if (init->is_flexible)
        error_at(tok, "array size is needed here");
    for (int i = 0; i < init->type->length && !is_end(tok); i++) {
        if (i)
            tok = skip(tok, ",");
        initializer2(&tok, tok, init->children[i]);
    }
    *rest = tok;
}

static void struct_initializer_braced(Token **rest, Token *tok, Initializer *init)
{
    tok = skip(tok, "{");
    int i = 0;
    Member *m = init->type->members;
    int first = 1;
    while (!consume(rest, tok, "}")) {
        if (!first)
            tok = skip(tok, ",");
        first = 0;
        if (equal(tok, "}")) {
            *rest = tok->next;
            return;
        }
        if (equal(tok, ".")) {
            Token *name = tok->next;
            m = init->type->members;
            i = 0;
            while (m && !(m->name && m->name->length == name->length && !memcmp(m->name->text, name->text, name->length))) {
                m = m->next;
                i++;
            }
            if (!m)
                error_at(name, "no such member");
            tok = skip(name->next, "=");
        }
        if (m) {
            initializer2(&tok, tok, init->children[i]);
            m = m->next;
            i++;
            if (init->type->kind == TY_UNION)
                m = 0;
        } else {
            tok = skip_excess(tok);
        }
    }
}

static void struct_initializer_flat(Token **rest, Token *tok, Initializer *init)
{
    int i = 0;
    for (Member *m = init->type->members; m && !is_end(tok); m = m->next, i++) {
        if (i)
            tok = skip(tok, ",");
        initializer2(&tok, tok, init->children[i]);
        if (init->type->kind == TY_UNION)
            break;
    }
    *rest = tok;
}

static void initializer2(Token **rest, Token *tok, Initializer *init)
{
    if (init->type->kind == TY_ARRAY && tok->kind == TK_STR && init->type->base->size == 1) {
        string_initializer(rest, tok, init);
        return;
    }
    if (init->type->kind == TY_ARRAY && equal(tok, "{") && tok->next->kind == TK_STR && init->type->base->size == 1 &&
        equal(tok->next->next, "}")) {
        string_initializer(&tok, tok->next, init);
        *rest = skip(tok, "}");
        return;
    }
    if (init->type->kind == TY_ARRAY) {
        if (equal(tok, "{"))
            array_initializer_braced(rest, tok, init);
        else
            array_initializer_flat(rest, tok, init);
        return;
    }
    if (init->type->kind == TY_STRUCT || init->type->kind == TY_UNION) {
        if (equal(tok, "{")) {
            struct_initializer_braced(rest, tok, init);
            return;
        }
        Node *e = assign(rest, tok);
        add_type(e);
        if (e->type->kind == init->type->kind) {
            init->expr = e;
            return;
        }
        struct_initializer_flat(rest, tok, init);
        return;
    }
    if (equal(tok, "{")) {
        initializer2(&tok, tok->next, init);
        consume(&tok, tok, ",");
        *rest = skip(tok, "}");
        return;
    }
    init->expr = assign(rest, tok);
}

static Initializer *initializer(Token **rest, Token *tok, Type *type, Type **new_type)
{
    Initializer *init = new_initializer(type, 1);
    initializer2(rest, tok, init);
    *new_type = init->type;
    return init;
}

static Node *designator_expr(Designator *d, Token *tok)
{
    if (d->var)
        return new_var_node(d->var, tok);
    if (d->member) {
        Node *node = new_unary(ND_MEMBER, designator_expr(d->next, tok), tok);
        node->member = d->member;
        return node;
    }
    Node *base = designator_expr(d->next, tok);
    Node *address = new_add(base, new_num(d->index, tok), tok);
    return new_unary(ND_DEREF, address, tok);
}

static Node *local_init(Initializer *init, Type *type, Designator *d, Token *tok)
{
    if (type->kind == TY_ARRAY) {
        Node *node = new_node(ND_NULL, tok);
        for (int i = 0; i < type->length; i++) {
            Designator next = {d, i, 0, 0};
            Node *rhs = local_init(init->children[i], type->base, &next, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
        }
        return node;
    }
    if ((type->kind == TY_STRUCT || type->kind == TY_UNION) && !init->expr) {
        Node *node = new_node(ND_NULL, tok);
        int i = 0;
        for (Member *m = type->members; m; m = m->next, i++) {
            Designator next = {d, 0, m, 0};
            Node *rhs = local_init(init->children[i], m->type, &next, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
            if (type->kind == TY_UNION)
                break;
        }
        return node;
    }
    if (!init->expr)
        return new_node(ND_NULL, tok);
    return new_binary(ND_ASSIGN, designator_expr(d, tok), init->expr, tok);
}

static Node *lvar_initializer(Token **rest, Token *tok, Obj *var)
{
    Initializer *init = initializer(rest, tok, var->type, &var->type);
    Designator d = {0, 0, 0, var};
    Node *assignments = local_init(init, var->type, &d, tok);
    if (!is_struct(var->type) && var->type->kind != TY_ARRAY)
        return assignments;
    Node *zero = new_node(ND_MEMZERO, tok);
    zero->var = var;
    return new_binary(ND_COMMA, zero, assignments, tok);
}

static void write_value(char *buffer, long long value, int size)
{
    for (int i = 0; i < size; i++)
        buffer[i] = (char)(value >> (i * 8));
}

static Reloc *write_global_data(Reloc *cur, Initializer *init, Type *type, char *buffer, int offset)
{
    if (type->kind == TY_ARRAY) {
        for (int i = 0; i < type->length; i++)
            cur = write_global_data(cur, init->children[i], type->base, buffer, offset + type->base->size * i);
        return cur;
    }
    if ((type->kind == TY_STRUCT || type->kind == TY_UNION) && !init->expr) {
        int i = 0;
        for (Member *m = type->members; m; m = m->next, i++) {
            cur = write_global_data(cur, init->children[i], m->type, buffer, offset + m->offset);
            if (type->kind == TY_UNION)
                break;
        }
        return cur;
    }
    if (!init->expr)
        return cur;
    if (type->kind == TY_STRUCT || type->kind == TY_UNION)
        error_at(init->expr->tok, "struct copy is not a constant");
    if (is_flonum(type)) {
        double number = eval_double(init->expr);
        if (type->kind == TY_FLOAT) {
            float single = (float)number;
            memcpy(buffer + offset, &single, 4);
        } else {
            memcpy(buffer + offset, &number, 8);
        }
        return cur;
    }
    Obj *label = 0;
    long long value = eval_reloc(init->expr, &label);
    if (!label) {
        write_value(buffer + offset, value, type->size);
        return cur;
    }
    Reloc *rel = calloc(1, sizeof(Reloc));
    rel->offset = offset;
    rel->target = label;
    rel->addend = value;
    cur->next = rel;
    return rel;
}

static void gvar_initializer(Token **rest, Token *tok, Obj *var)
{
    Initializer *init = initializer(rest, tok, var->type, &var->type);
    Reloc head = {0};
    char *buffer = calloc(1, var->type->size ? var->type->size : 1);
    write_global_data(&head, init, var->type, buffer, 0);
    var->data = buffer;
    var->relocs = head.next;
    var->has_init = 1;
}

static Node *declaration(Token **rest, Token *tok, Type *base, VarAttr *attr)
{
    Node head = {0};
    Node *cur = &head;
    int count = 0;
    while (!equal(tok, ";")) {
        if (count++ > 0)
            tok = skip(tok, ",");
        Type *type = declarator(&tok, tok, base);
        if (!type->name)
            error_at(tok, "variable name expected");
        char *name = token_name(type->name);
        if (attr && attr->is_typedef) {
            push_scope(name)->type_def = type;
            continue;
        }
        if (type->kind == TY_FUNC) {
            declare_global(type->name, name, type, attr && attr->is_static, 1);
            continue;
        }
        if (type->kind == TY_VOID)
            error_at(type->name, "variable declared void");
        if (attr && attr->is_extern) {
            declare_global(type->name, name, type, 0, 0);
            continue;
        }
        if (attr && attr->is_static) {
            Obj *var = new_global(name, format_text("L%d_%s", unique_number++, name), type);
            var->is_static = 1;
            var->is_definition = 1;
            push_scope(name)->var = var;
            if (equal(tok, "="))
                gvar_initializer(&tok, tok->next, var);
            continue;
        }
        Obj *var = new_lvar(name, type);
        if (equal(tok, "=")) {
            Node *init = lvar_initializer(&tok, tok->next, var);
            cur = cur->next = new_unary(ND_EXPR_STMT, init, tok);
        }
        if (var->type->kind == TY_ARRAY && var->type->length < 0)
            error_at(type->name, "array size is missing");
        if ((var->type->kind == TY_STRUCT || var->type->kind == TY_UNION) && !var->type->complete)
            error_at(type->name, "variable has incomplete type");
    }
    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static Node *compound_stmt(Token **rest, Token *tok)
{
    Node *node = new_node(ND_BLOCK, tok);
    Node head = {0};
    Node *cur = &head;
    enter_scope();
    while (!equal(tok, "}")) {
        if (tok->kind == TK_EOF)
            error_at(tok, "missing '}'");
        if (is_typename(tok) && !equal(tok->next, ":")) {
            VarAttr attr = {0};
            Type *base = declspec(&tok, tok, &attr);
            if (equal(tok, ";")) {
                tok = tok->next;
                continue;
            }
            cur = cur->next = declaration(&tok, tok, base, &attr);
        } else {
            cur = cur->next = stmt(&tok, tok);
        }
        add_type(cur);
    }
    leave_scope();
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static Node *expr_stmt(Token **rest, Token *tok)
{
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}

static Node *stmt(Token **rest, Token *tok)
{
    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok);
        if (consume(rest, tok->next, ";"))
            return node;
        Node *value = expr(&tok, tok->next);
        *rest = skip(tok, ";");
        add_type(value);
        Type *ret = current_fn->type->ret;
        if (is_struct(ret) && value->type->members != ret->members)
            error_at(value->tok, "returning the wrong struct type");
        if (!is_struct(ret) && ret->kind != TY_VOID)
            value = new_cast(value, ret);
        node->lhs = value;
        return node;
    }
    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(&tok, tok);
        if (equal(tok, "else"))
            node->els = stmt(&tok, tok->next);
        *rest = tok;
        return node;
    }
    if (equal(tok, "switch")) {
        Node *node = new_node(ND_SWITCH, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        Node *saved_switch = current_switch;
        int saved_break = break_label;
        current_switch = node;
        break_label = node->break_label = unique_number++;
        node->then = stmt(rest, tok);
        current_switch = saved_switch;
        break_label = saved_break;
        return node;
    }
    if (equal(tok, "case")) {
        if (!current_switch)
            error_at(tok, "case outside of switch");
        Node *node = new_node(ND_CASE, tok);
        long long value = eval(conditional(&tok, tok->next));
        tok = skip(tok, ":");
        node->label = unique_number++;
        node->value = value;
        node->lhs = stmt(rest, tok);
        node->cases = current_switch->cases;
        current_switch->cases = node;
        return node;
    }
    if (equal(tok, "default")) {
        if (!current_switch)
            error_at(tok, "default outside of switch");
        Node *node = new_node(ND_CASE, tok);
        tok = skip(tok->next, ":");
        node->label = unique_number++;
        node->lhs = stmt(rest, tok);
        current_switch->default_case = node;
        return node;
    }
    if (equal(tok, "for")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");
        enter_scope();
        int saved_break = break_label, saved_continue = continue_label;
        break_label = node->break_label = unique_number++;
        continue_label = node->continue_label = unique_number++;
        if (is_typename(tok)) {
            VarAttr attr = {0};
            Type *base = declspec(&tok, tok, &attr);
            node->init = declaration(&tok, tok, base, &attr);
        } else {
            node->init = expr_stmt(&tok, tok);
        }
        if (!equal(tok, ";"))
            node->cond = expr(&tok, tok);
        tok = skip(tok, ";");
        if (!equal(tok, ")"))
            node->inc = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(rest, tok);
        leave_scope();
        break_label = saved_break;
        continue_label = saved_continue;
        return node;
    }
    if (equal(tok, "while")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        int saved_break = break_label, saved_continue = continue_label;
        break_label = node->break_label = unique_number++;
        continue_label = node->continue_label = unique_number++;
        node->then = stmt(rest, tok);
        break_label = saved_break;
        continue_label = saved_continue;
        return node;
    }
    if (equal(tok, "do")) {
        Node *node = new_node(ND_DO, tok);
        int saved_break = break_label, saved_continue = continue_label;
        break_label = node->break_label = unique_number++;
        continue_label = node->continue_label = unique_number++;
        node->then = stmt(&tok, tok->next);
        break_label = saved_break;
        continue_label = saved_continue;
        tok = skip(tok, "while");
        tok = skip(tok, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        *rest = skip(tok, ";");
        return node;
    }
    if (equal(tok, "goto")) {
        Node *node = new_node(ND_GOTO, tok);
        node->name = token_name(tok->next);
        node->next = gotos;
        gotos = node;
        *rest = skip(tok->next->next, ";");
        return node;
    }
    if (equal(tok, "break")) {
        if (!break_label)
            error_at(tok, "break outside of a loop or switch");
        Node *node = new_node(ND_GOTO, tok);
        node->label = break_label;
        *rest = skip(tok->next, ";");
        return node;
    }
    if (equal(tok, "continue")) {
        if (!continue_label)
            error_at(tok, "continue outside of a loop");
        Node *node = new_node(ND_GOTO, tok);
        node->label = continue_label;
        *rest = skip(tok->next, ";");
        return node;
    }
    if (tok->kind == TK_IDENT && equal(tok->next, ":")) {
        Node *node = new_node(ND_LABEL, tok);
        node->name = token_name(tok);
        node->label = unique_number++;
        node->lhs = stmt(rest, tok->next->next);
        node->next = labels;
        labels = node;
        return node;
    }
    if (equal(tok, "{"))
        return compound_stmt(rest, tok->next);
    if (is_asm(tok)) {
        Node *node = new_node(ND_ASM, tok);
        node->name = asm_text(rest, tok);
        return node;
    }
    return expr_stmt(rest, tok);
}

static int is_asm(Token *tok)
{
    return equal(tok, "asm") || equal(tok, "__asm__") || equal(tok, "__asm");
}

static char *asm_text(Token **rest, Token *tok)
{
    tok = tok->next;
    while (equal(tok, "volatile") || equal(tok, "__volatile__"))
        tok = tok->next;
    tok = skip(tok, "(");
    Buffer text = {0, 0, 0};
    if (tok->kind != TK_STR)
        error_at(tok, "asm expects a string");
    while (tok->kind == TK_STR) {
        buffer_add(&text, tok->str, tok->str_length - 1);
        tok = tok->next;
    }
    tok = skip(tok, ")");
    *rest = skip(tok, ";");
    buffer_add(&text, "", 0);
    return text.data;
}

static double eval_double(Node *node)
{
    add_type(node);
    if (is_integer(node->type)) {
        long long value = eval(node);
        if (node->type->is_unsigned && node->type->size == 8)
            return (double)(unsigned long long)value;
        return (double)value;
    }
    switch (node->kind) {
    case ND_ADD:
        return eval_double(node->lhs) + eval_double(node->rhs);
    case ND_SUB:
        return eval_double(node->lhs) - eval_double(node->rhs);
    case ND_MUL:
        return eval_double(node->lhs) * eval_double(node->rhs);
    case ND_DIV:
        return eval_double(node->lhs) / eval_double(node->rhs);
    case ND_NEG:
        return -eval_double(node->lhs);
    case ND_COND:
        return eval_double(node->cond) != 0 ? eval_double(node->then) : eval_double(node->els);
    case ND_COMMA:
        return eval_double(node->rhs);
    case ND_CAST:
        if (node->type->kind == TY_FLOAT)
            return (float)eval_double(node->lhs);
        return eval_double(node->lhs);
    case ND_NUM:
        return node->fvalue;
    }
    error_at(node->tok, "not a compile-time constant");
    return 0;
}

static long long eval(Node *node)
{
    return eval_reloc(node, 0);
}

static long long eval_address(Node *node, Obj **label)
{
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_local)
            error_at(node->tok, "not a compile-time constant");
        *label = node->var;
        return 0;
    case ND_DEREF:
        return eval_reloc(node->lhs, label);
    case ND_MEMBER:
        return eval_address(node->lhs, label) + node->member->offset;
    }
    error_at(node->tok, "not a compile-time constant");
    return 0;
}

static long long eval_reloc(Node *node, Obj **label)
{
    add_type(node);
    switch (node->kind) {
    case ND_ADD:
        return eval_reloc(node->lhs, label) + eval(node->rhs);
    case ND_SUB:
        return eval_reloc(node->lhs, label) - eval(node->rhs);
    case ND_MUL:
        return eval(node->lhs) * eval(node->rhs);
    case ND_DIV: {
        long long divisor = eval(node->rhs);
        if (!divisor)
            error_at(node->tok, "division by zero");
        if (node->type->is_unsigned)
            return (unsigned long long)eval(node->lhs) / divisor;
        return eval(node->lhs) / divisor;
    }
    case ND_MOD: {
        long long divisor = eval(node->rhs);
        if (!divisor)
            error_at(node->tok, "division by zero");
        if (node->type->is_unsigned)
            return (unsigned long long)eval(node->lhs) % divisor;
        return eval(node->lhs) % divisor;
    }
    case ND_NEG:
        return -eval(node->lhs);
    case ND_BITAND:
        return eval(node->lhs) & eval(node->rhs);
    case ND_BITOR:
        return eval(node->lhs) | eval(node->rhs);
    case ND_BITXOR:
        return eval(node->lhs) ^ eval(node->rhs);
    case ND_SHL:
        return eval(node->lhs) << eval(node->rhs);
    case ND_SHR:
        if (node->type->is_unsigned && node->type->size == 8)
            return (unsigned long long)eval(node->lhs) >> eval(node->rhs);
        if (node->type->is_unsigned)
            return (unsigned int)eval(node->lhs) >> eval(node->rhs);
        return eval(node->lhs) >> eval(node->rhs);
    case ND_EQ:
        if (is_flonum(node->lhs->type))
            return eval_double(node->lhs) == eval_double(node->rhs);
        return eval(node->lhs) == eval(node->rhs);
    case ND_NE:
        return eval(node->lhs) != eval(node->rhs);
    case ND_LT:
        if (node->lhs->type->is_unsigned)
            return (unsigned long long)eval(node->lhs) < (unsigned long long)eval(node->rhs);
        return eval(node->lhs) < eval(node->rhs);
    case ND_LE:
        if (node->lhs->type->is_unsigned)
            return (unsigned long long)eval(node->lhs) <= (unsigned long long)eval(node->rhs);
        return eval(node->lhs) <= eval(node->rhs);
    case ND_COND:
        return eval(node->cond) ? eval_reloc(node->then, label) : eval_reloc(node->els, label);
    case ND_COMMA:
        return eval_reloc(node->rhs, label);
    case ND_NOT:
        return !eval(node->lhs);
    case ND_BITNOT:
        return ~eval(node->lhs);
    case ND_LOGAND:
        return eval(node->lhs) && eval(node->rhs);
    case ND_LOGOR:
        return eval(node->lhs) || eval(node->rhs);
    case ND_CAST: {
        add_type(node->lhs);
        if (is_flonum(node->lhs->type) && is_integer(node->type))
            return (long long)eval_double(node->lhs);
        long long value = eval_reloc(node->lhs, label);
        if (is_integer(node->type)) {
            switch (node->type->size) {
            case 1:
                return node->type->is_unsigned ? (unsigned char)value : (char)value;
            case 2:
                return node->type->is_unsigned ? (unsigned short)value : (short)value;
            case 4:
                return node->type->is_unsigned ? (unsigned int)value : (int)value;
            }
        }
        return value;
    }
    case ND_ADDR:
        if (!label)
            error_at(node->tok, "not a compile-time constant");
        return eval_address(node->lhs, label);
    case ND_MEMBER:
        if (!label)
            error_at(node->tok, "not a compile-time constant");
        if (node->type->kind != TY_ARRAY)
            error_at(node->tok, "invalid initializer");
        return eval_address(node->lhs, label) + node->member->offset;
    case ND_VAR:
        if (!label)
            error_at(node->tok, "not a compile-time constant");
        if (node->var->type->kind != TY_ARRAY && node->var->type->kind != TY_FUNC)
            error_at(node->tok, "invalid initializer");
        if (node->var->is_local)
            error_at(node->tok, "not a compile-time constant");
        *label = node->var;
        return 0;
    case ND_NUM:
        return node->value;
    }
    error_at(node->tok, "not a compile-time constant");
    return 0;
}

static Node *to_assign(Node *binary)
{
    add_type(binary->lhs);
    add_type(binary->rhs);
    Token *tok = binary->tok;
    if (binary->lhs->kind == ND_VAR) {
        Node *target = binary->lhs;
        Node *value;
        if (binary->kind == ND_ADD)
            value = new_add(new_var_node(target->var, tok), binary->rhs, tok);
        else if (binary->kind == ND_SUB)
            value = new_sub(new_var_node(target->var, tok), binary->rhs, tok);
        else
            value = new_binary(binary->kind, new_var_node(target->var, tok), binary->rhs, tok);
        return new_binary(ND_ASSIGN, target, value, tok);
    }
    Obj *var = new_lvar("", pointer_to(binary->lhs->type));
    Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok), new_unary(ND_ADDR, binary->lhs, tok), tok);
    Node *expr2 = new_binary(ND_ASSIGN, new_unary(ND_DEREF, new_var_node(var, tok), tok),
                             new_binary(binary->kind, new_unary(ND_DEREF, new_var_node(var, tok), tok), binary->rhs, tok),
                             tok);
    if (binary->kind == ND_ADD)
        expr2->rhs = new_add(new_unary(ND_DEREF, new_var_node(var, tok), tok), binary->rhs, tok);
    else if (binary->kind == ND_SUB)
        expr2->rhs = new_sub(new_unary(ND_DEREF, new_var_node(var, tok), tok), binary->rhs, tok);
    return new_binary(ND_COMMA, expr1, expr2, tok);
}

static Node *expr(Token **rest, Token *tok)
{
    Node *node = assign(&tok, tok);
    if (equal(tok, ","))
        return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);
    *rest = tok;
    return node;
}

static Node *assign(Token **rest, Token *tok)
{
    static char *ops[] = {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", 0};
    static int kinds[] = {ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD, ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR};
    Node *node = conditional(&tok, tok);
    if (equal(tok, "="))
        return new_binary(ND_ASSIGN, node, assign(rest, tok->next), tok);
    for (int i = 0; ops[i]; i++)
        if (equal(tok, ops[i]))
            return to_assign(new_binary(kinds[i], node, assign(rest, tok->next), tok));
    *rest = tok;
    return node;
}

static Node *logor(Token **rest, Token *tok);

static Node *conditional(Token **rest, Token *tok)
{
    Node *cond = logor(&tok, tok);
    if (!equal(tok, "?")) {
        *rest = tok;
        return cond;
    }
    Node *node = new_node(ND_COND, tok);
    node->cond = cond;
    node->then = expr(&tok, tok->next);
    tok = skip(tok, ":");
    node->els = conditional(rest, tok);
    return node;
}

static Node *binary_level(Token **rest, Token *tok, int level);

static Node *logor(Token **rest, Token *tok)
{
    return binary_level(rest, tok, 0);
}

static Node *binary_level(Token **rest, Token *tok, int level)
{
    static char *ops[][5] = {
        {"||"}, {"&&"}, {"|"}, {"^"}, {"&"}, {"==", "!="}, {"<", "<=", ">", ">="}, {"<<", ">>"}, {"+", "-"},
        {"*", "/", "%"}
    };
    if (level == 10)
        return cast(rest, tok);
    Node *node = binary_level(&tok, tok, level + 1);
    for (;;) {
        Token *start = tok;
        int matched = -1;
        for (int i = 0; i < 5 && ops[level][i]; i++)
            if (equal(tok, ops[level][i]))
                matched = i;
        if (matched < 0) {
            *rest = tok;
            return node;
        }
        Node *rhs = binary_level(&tok, tok->next, level + 1);
        char *op = ops[level][matched];
        if (!strcmp(op, "||"))
            node = new_binary(ND_LOGOR, node, rhs, start);
        else if (!strcmp(op, "&&"))
            node = new_binary(ND_LOGAND, node, rhs, start);
        else if (!strcmp(op, "|"))
            node = new_binary(ND_BITOR, node, rhs, start);
        else if (!strcmp(op, "^"))
            node = new_binary(ND_BITXOR, node, rhs, start);
        else if (!strcmp(op, "&"))
            node = new_binary(ND_BITAND, node, rhs, start);
        else if (!strcmp(op, "=="))
            node = new_binary(ND_EQ, node, rhs, start);
        else if (!strcmp(op, "!="))
            node = new_binary(ND_NE, node, rhs, start);
        else if (!strcmp(op, "<"))
            node = new_binary(ND_LT, node, rhs, start);
        else if (!strcmp(op, "<="))
            node = new_binary(ND_LE, node, rhs, start);
        else if (!strcmp(op, ">"))
            node = new_binary(ND_LT, rhs, node, start);
        else if (!strcmp(op, ">="))
            node = new_binary(ND_LE, rhs, node, start);
        else if (!strcmp(op, "<<"))
            node = new_binary(ND_SHL, node, rhs, start);
        else if (!strcmp(op, ">>"))
            node = new_binary(ND_SHR, node, rhs, start);
        else if (!strcmp(op, "+"))
            node = new_add(node, rhs, start);
        else if (!strcmp(op, "-"))
            node = new_sub(node, rhs, start);
        else if (!strcmp(op, "*"))
            node = new_binary(ND_MUL, node, rhs, start);
        else if (!strcmp(op, "/"))
            node = new_binary(ND_DIV, node, rhs, start);
        else
            node = new_binary(ND_MOD, node, rhs, start);
    }
}

static Node *scaled(Node *count, int size, Token *tok)
{
    if (count->kind == ND_NUM)
        return new_long(count->value * size, tok);
    Node *node = new_cast(count, ty_long);
    if (size != 1)
        node = new_binary(ND_MUL, node, new_long(size, tok), tok);
    add_type(node);
    return node;
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);
    if (is_numeric(lhs->type) && is_numeric(rhs->type))
        return new_binary(ND_ADD, lhs, rhs, tok);
    if (lhs->type->base && rhs->type->base)
        error_at(tok, "invalid operands");
    if (!lhs->type->base && rhs->type->base) {
        Node *held = lhs;
        lhs = rhs;
        rhs = held;
    }
    if (!lhs->type->base)
        error_at(tok, "invalid operands");
    rhs = scaled(rhs, lhs->type->base->size, tok);
    Node *node = new_binary(ND_ADD, lhs, rhs, tok);
    node->type = lhs->type->kind == TY_ARRAY ? pointer_to(lhs->type->base) : lhs->type;
    add_type(rhs);
    return node;
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);
    if (is_numeric(lhs->type) && is_numeric(rhs->type))
        return new_binary(ND_SUB, lhs, rhs, tok);
    if (lhs->type->base && is_integer(rhs->type)) {
        rhs = scaled(rhs, lhs->type->base->size, tok);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->type = lhs->type->kind == TY_ARRAY ? pointer_to(lhs->type->base) : lhs->type;
        return node;
    }
    if (lhs->type->base && rhs->type->base) {
        Node *node = new_binary(ND_SUB, new_cast(lhs, ty_long), new_cast(rhs, ty_long), tok);
        node->type = ty_long;
        int size = lhs->type->base->size;
        return new_binary(ND_DIV, node, new_long(size > 0 ? size : 1, tok), tok);
    }
    error_at(tok, "invalid operands");
    return 0;
}

static Node *cast(Token **rest, Token *tok)
{
    if (equal(tok, "(") && is_typename(tok->next)) {
        Token *start = tok;
        Type *type = typename(&tok, tok->next);
        tok = skip(tok, ")");
        if (equal(tok, "{")) {
            Node *literal = compound_literal(&tok, tok, type, start);
            return postfix_tail(rest, tok, literal);
        }
        Node *node = new_cast(cast(rest, tok), type);
        node->tok = start;
        return node;
    }
    return unary(rest, tok);
}

static Node *unary(Token **rest, Token *tok)
{
    if (equal(tok, "+"))
        return cast(rest, tok->next);
    if (equal(tok, "-"))
        return new_unary(ND_NEG, cast(rest, tok->next), tok);
    if (equal(tok, "&")) {
        Node *lhs = cast(rest, tok->next);
        add_type(lhs);
        return new_unary(ND_ADDR, lhs, tok);
    }
    if (equal(tok, "*")) {
        Node *lhs = cast(rest, tok->next);
        add_type(lhs);
        if (lhs->type->kind == TY_FUNC)
            return lhs;
        return new_unary(ND_DEREF, lhs, tok);
    }
    if (equal(tok, "!"))
        return new_unary(ND_NOT, cast(rest, tok->next), tok);
    if (equal(tok, "~"))
        return new_unary(ND_BITNOT, cast(rest, tok->next), tok);
    if (equal(tok, "++"))
        return to_assign(new_binary(ND_ADD, unary(rest, tok->next), new_num(1, tok), tok));
    if (equal(tok, "--"))
        return to_assign(new_binary(ND_SUB, unary(rest, tok->next), new_num(1, tok), tok));
    if (equal(tok, "sizeof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
        Type *type = typename(&tok, tok->next->next);
        *rest = skip(tok, ")");
        return new_size(type->size, tok);
    }
    if (equal(tok, "sizeof")) {
        Node *node = unary(rest, tok->next);
        add_type(node);
        return new_size(node->type->size, tok);
    }
    if (equal(tok, "_Alignof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
        Type *type = typename(&tok, tok->next->next);
        *rest = skip(tok, ")");
        return new_size(type->align, tok);
    }
    return postfix(rest, tok);
}

static Member *find_member(Type *type, Token *tok)
{
    for (Member *m = type->members; m; m = m->next)
        if (m->name && m->name->length == tok->length && !memcmp(m->name->text, tok->text, tok->length))
            return m;
    error_at(tok, "no such member");
    return 0;
}

static Node *struct_ref(Node *lhs, Token *tok)
{
    add_type(lhs);
    if (lhs->type->kind != TY_STRUCT && lhs->type->kind != TY_UNION)
        error_at(lhs->tok, "not a struct or union");
    if (!lhs->type->complete)
        error_at(lhs->tok, "incomplete struct type");
    Node *node = new_unary(ND_MEMBER, lhs, tok);
    node->member = find_member(lhs->type, tok);
    return node;
}

static Node *new_inc_dec(Node *node, Token *tok, int addend)
{
    // x++ becomes (x += 1) - 1. gen.c throws the - 1 away when nobody reads the value
    add_type(node);
    Node *changed = to_assign(new_binary(ND_ADD, node, new_num(addend, tok), tok));
    Node *back = new_add(changed, new_num(-addend, tok), tok);
    Node *result = new_cast(back, node->type);
    result->unused_value = changed;
    return result;
}

static Node *funcall(Token **rest, Token *tok, Node *fn)
{
    add_type(fn);
    Type *type = fn->type;
    if (type->kind == TY_PTR && type->base->kind == TY_FUNC)
        type = type->base;
    if (type->kind != TY_FUNC)
        error_at(fn->tok, "not a function");
    Type *param = type->params;
    Node head = {0};
    Node *cur = &head;
    while (!equal(tok, ")")) {
        if (cur != &head)
            tok = skip(tok, ",");
        Node *arg = assign(&tok, tok);
        add_type(arg);
        if (param) {
            if (is_struct(param) != is_struct(arg->type))
                error_at(arg->tok, "argument does not match the parameter type");
            if (!is_struct(param))
                arg = new_cast(arg, param);
            param = param->next;
        } else if (type->prototyped && !type->variadic) {
            error_at(arg->tok, "too many arguments");
        } else if (arg->type->kind == TY_FLOAT) {
            arg = new_cast(arg, ty_double);
        } else if (is_integer(arg->type) && arg->type->size < 4) {
            arg = new_cast(arg, ty_int);
        }
        cur = cur->next = arg;
    }
    if (param && type->prototyped)
        error_at(tok, "too few arguments");
    *rest = skip(tok, ")");
    Node *node = new_node(ND_CALL, fn->tok);
    node->lhs = fn;
    node->args = head.next;
    node->type = type->ret;
    if (is_struct(node->type)) {
        if (!current_fn)
            error_at(fn->tok, "a struct returning call is not a constant");
        node->var = new_lvar("", node->type);
    }
    return node;
}

static Node *postfix(Token **rest, Token *tok)
{
    Node *node = primary(&tok, tok);
    return postfix_tail(rest, tok, node);
}

static Node *compound_literal(Token **rest, Token *tok, Type *type, Token *start)
{
    if (!current_fn) {
        Obj *var = new_global("", format_text("L%d_literal", unique_number++), type);
        var->is_static = 1;
        var->is_definition = 1;
        gvar_initializer(rest, tok, var);
        return new_var_node(var, start);
    }
    Obj *var = new_lvar("", type);
    Node *init = lvar_initializer(rest, tok, var);
    return new_binary(ND_COMMA, init, new_var_node(var, start), start);
}

static Node *postfix_tail(Token **rest, Token *tok, Node *node)
{
    for (;;) {
        if (equal(tok, "(")) {
            node = funcall(&tok, tok->next, node);
            continue;
        }
        if (equal(tok, "[")) {
            Token *start = tok;
            Node *index = expr(&tok, tok->next);
            tok = skip(tok, "]");
            node = new_unary(ND_DEREF, new_add(node, index, start), start);
            continue;
        }
        if (equal(tok, ".")) {
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        if (equal(tok, "->")) {
            node = new_unary(ND_DEREF, node, tok);
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        if (equal(tok, "++")) {
            node = new_inc_dec(node, tok, 1);
            tok = tok->next;
            continue;
        }
        if (equal(tok, "--")) {
            node = new_inc_dec(node, tok, -1);
            tok = tok->next;
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *syscall_builtin(Token **rest, Token *tok)
{
    Node *node = new_node(ND_SYSCALL, tok);
    tok = skip(tok->next, "(");
    Node head = {0};
    Node *cur = &head;
    int count = 0;
    while (!equal(tok, ")")) {
        if (count++)
            tok = skip(tok, ",");
        Node *arg = assign(&tok, tok);
        add_type(arg);
        cur = cur->next = new_cast(arg, ty_long);
    }
    if (count < 1 || count > 5)
        error_at(tok, "__syscall takes between 1 and 5 arguments");
    node->args = head.next;
    node->value = count;
    node->type = ty_long;
    *rest = skip(tok, ")");
    return node;
}

static Node *primary(Token **rest, Token *tok)
{
    if (equal(tok, "(")) {
        if (equal(tok->next, "{"))
            error_at(tok, "statement expressions are not supported");
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    if (tok->kind == TK_IDENT && equal(tok, "__syscall"))
        return syscall_builtin(rest, tok);
    if (tok->kind == TK_IDENT) {
        if (tok->text[0] >= '0' && tok->text[0] <= '9')
            error_at(tok, "floating point is not supported");
        VarScope *v = find_var(tok);
        *rest = tok->next;
        if (v && v->var)
            return new_var_node(v->var, tok);
        if (v && v->enum_type)
            return new_num(v->enum_value, tok);
        if (equal(tok->next, "(")) {
            warn_at(tok, "implicit declaration of function");
            Type *type = func_type(ty_int);
            Obj *fn = declare_global(tok, token_name(tok), type, 0, 1);
            return new_var_node(fn, tok);
        }
        error_at(tok, "undefined variable");
    }
    if (tok->kind == TK_STR) {
        Buffer text = {0, 0, 0};
        Token *start = tok;
        while (tok->kind == TK_STR) {
            buffer_add(&text, tok->str, tok->str_length - 1);
            tok = tok->next;
        }
        buffer_add(&text, "", 1);
        Obj *var = new_string_literal(text.data, array_of(ty_char, text.length));
        *rest = tok;
        return new_var_node(var, start);
    }
    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->value, tok);
        node->type = tok->type;
        node->fvalue = tok->fvalue;
        *rest = tok->next;
        return node;
    }
    error_at(tok, "expected an expression");
    return 0;
}

static void resolve_gotos(void)
{
    for (Node *x = gotos; x; x = x->next) {
        Node *y = labels;
        while (y && strcmp(x->name, y->name))
            y = y->next;
        if (!y)
            error_at(x->tok, "label is not defined");
        x->label = y->label;
    }
    gotos = labels = 0;
}

static Token *function(Token *tok, Type *type, VarAttr *attr)
{
    char *name = token_name(type->name);
    Obj *fn = declare_global(type->name, name, type, attr->is_static, 1);
    fn->type = type;
    if (!consume(&tok, tok, ";")) {
        if (fn->is_definition)
            error_at(type->name, "%s is defined twice", name);
        fn->is_definition = 1;
        current_fn = fn;
        locals = 0;
        enter_scope();
        int offset = 16;
        if (is_struct(type->ret)) {
            fn->return_slot = new_lvar("", pointer_to(type->ret));
            fn->return_slot->offset = offset;
            offset += 8;
        }
        for (Type *param = type->params; param; param = param->next) {
            if (!param->name)
                error_at(tok, "parameter name omitted");
            if (is_struct(param) && !param->complete)
                error_at(param->name, "parameter has incomplete type");
            Obj *var = new_lvar(token_name(param->name), param);
            var->offset = offset;
            offset += align_to(param->size, 8);
        }
        tok = skip(tok, "{");
        fn->body = compound_stmt(&tok, tok);
        fn->locals = locals;
        leave_scope();
        resolve_gotos();
        current_fn = 0;
    }
    return tok;
}

static int is_function(Token *tok)
{
    if (equal(tok, ";"))
        return 0;
    Type dummy = {0};
    Type *type = declarator(&tok, tok, &dummy);
    return type->kind == TY_FUNC;
}

static Token *global_variable(Token *tok, Type *base, VarAttr *attr)
{
    int first = 1;
    while (!consume(&tok, tok, ";")) {
        if (!first)
            tok = skip(tok, ",");
        first = 0;
        Type *type = declarator(&tok, tok, base);
        if (!type->name)
            error_at(tok, "variable name expected");
        char *name = token_name(type->name);
        if (type->kind == TY_FUNC) {
            declare_global(type->name, name, type, attr->is_static, 1);
            continue;
        }
        Obj *var = declare_global(type->name, name, type, attr->is_static, 0);
        if (!attr->is_extern)
            var->is_definition = 1;
        if (equal(tok, "=")) {
            if (var->has_init)
                error_at(type->name, "%s is initialized twice", name);
            var->is_definition = 1;
            var->type = type;
            gvar_initializer(&tok, tok->next, var);
        }
    }
    return tok;
}

void parse_unit(Token *tok, int unit)
{
    unit_number = unit;
    Scope *saved = global_scope.next;
    VarScope *saved_vars = global_scope.vars;
    TagScope *saved_tags = global_scope.tags;
    while (tok->kind != TK_EOF) {
        if (is_asm(tok)) {
            char *text = asm_text(&tok, tok);
            buffer_add(&global_asm, text, strlen(text));
            buffer_add(&global_asm, "\n", 1);
            continue;
        }
        VarAttr attr = {0};
        Type *base = declspec(&tok, tok, &attr);
        if (attr.is_typedef) {
            declaration(&tok, tok, base, &attr);
            continue;
        }
        if (is_function(tok)) {
            Type *type = declarator(&tok, tok, base);
            if (!type->name)
                error_at(tok, "function name expected");
            tok = function(tok, type, &attr);
            continue;
        }
        tok = global_variable(tok, base, &attr);
    }
    global_scope.next = saved;
    global_scope.vars = saved_vars;
    global_scope.tags = saved_tags;
}
