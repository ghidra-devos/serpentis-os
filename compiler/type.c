#include "cc.h"
#include <stdlib.h>

static Type type_void = {.kind = TY_VOID, .size = 1, .align = 1};
static Type type_bool = {.kind = TY_BOOL, .size = 1, .align = 1, .is_unsigned = 1};
static Type type_char = {.kind = TY_CHAR, .size = 1, .align = 1};
static Type type_short = {.kind = TY_SHORT, .size = 2, .align = 2};
static Type type_int = {.kind = TY_INT, .size = 4, .align = 4};
static Type type_long = {.kind = TY_LONG, .size = 8, .align = 8};
static Type type_uchar = {.kind = TY_CHAR, .size = 1, .align = 1, .is_unsigned = 1};
static Type type_ushort = {.kind = TY_SHORT, .size = 2, .align = 2, .is_unsigned = 1};
static Type type_uint = {.kind = TY_INT, .size = 4, .align = 4, .is_unsigned = 1};
static Type type_ulong = {.kind = TY_LONG, .size = 8, .align = 8, .is_unsigned = 1};
static Type type_float = {.kind = TY_FLOAT, .size = 4, .align = 4};
static Type type_double = {.kind = TY_DOUBLE, .size = 8, .align = 8};

Type *ty_void = &type_void;
Type *ty_bool = &type_bool;
Type *ty_char = &type_char;
Type *ty_short = &type_short;
Type *ty_int = &type_int;
Type *ty_long = &type_long;
Type *ty_uchar = &type_uchar;
Type *ty_ushort = &type_ushort;
Type *ty_uint = &type_uint;
Type *ty_ulong = &type_ulong;
Type *ty_float = &type_float;
Type *ty_double = &type_double;

int align_to(int value, int align)
{
    return (value + align - 1) / align * align;
}

int is_integer(Type *type)
{
    int k = type->kind;
    return k == TY_BOOL || k == TY_CHAR || k == TY_SHORT || k == TY_INT || k == TY_LONG || k == TY_ENUM;
}

int is_flonum(Type *type)
{
    return type->kind == TY_FLOAT || type->kind == TY_DOUBLE;
}

int is_numeric(Type *type)
{
    return is_integer(type) || is_flonum(type);
}

int is_struct(Type *type)
{
    return type->kind == TY_STRUCT || type->kind == TY_UNION;
}

int is_pointer_like(Type *type)
{
    return type->kind == TY_PTR || type->kind == TY_ARRAY;
}

Type *copy_type(Type *type)
{
    Type *copy = malloc(sizeof(Type));
    *copy = *type;
    return copy;
}

static Type *new_type(int kind, int size, int align)
{
    Type *type = calloc(1, sizeof(Type));
    type->kind = kind;
    type->size = size;
    type->align = align;
    return type;
}

Type *pointer_to(Type *base)
{
    Type *type = new_type(TY_PTR, 8, 8);
    type->base = base;
    type->is_unsigned = 1;
    return type;
}

Type *array_of(Type *base, int length)
{
    Type *type = new_type(TY_ARRAY, base->size * length, base->align);
    type->base = base;
    type->length = length;
    return type;
}

Type *func_type(Type *ret)
{
    Type *type = new_type(TY_FUNC, 1, 1);
    type->ret = ret;
    return type;
}

Type *new_struct_type(int kind)
{
    return new_type(kind, 0, 1);
}

static Type *promoted(Type *type)
{
    if (type->kind == TY_ENUM || type->size < 4)
        return ty_int;
    return type;
}

static Type *common_type(Type *a, Type *b)
{
    if (a->base)
        return pointer_to(a->base);
    if (a->kind == TY_FUNC)
        return pointer_to(a);
    if (b->kind == TY_FUNC)
        return pointer_to(b);
    if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE)
        return ty_double;
    if (a->kind == TY_FLOAT || b->kind == TY_FLOAT)
        return ty_float;
    a = promoted(a);
    b = promoted(b);
    if (a->size != b->size)
        return a->size < b->size ? b : a;
    return b->is_unsigned ? b : a;
}

Node *fold_cast(Node *expr, Type *type)
{
    if (expr->kind != ND_NUM || !expr->type)
        return 0;
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->tok = expr->tok;
    node->type = type;
    if (is_flonum(type)) {
        double value = is_flonum(expr->type) ? expr->fvalue
                       : expr->type->is_unsigned && expr->type->size == 8 ? (double)(unsigned long long)expr->value
                                                                           : (double)expr->value;
        node->fvalue = type->kind == TY_FLOAT ? (float)value : value;
        return node;
    }
    if (!is_integer(type) && type->kind != TY_PTR) {
        free(node);
        return 0;
    }
    long long value = is_flonum(expr->type) ? (long long)expr->fvalue : expr->value;
    if (type->kind == TY_BOOL)
        value = is_flonum(expr->type) ? expr->fvalue != 0 : value != 0;
    else if (type->size == 1)
        value = type->is_unsigned ? (long long)(unsigned char)value : (long long)(signed char)value;
    else if (type->size == 2)
        value = type->is_unsigned ? (long long)(unsigned short)value : (long long)(short)value;
    else if (type->size == 4)
        value = type->is_unsigned ? (long long)(unsigned int)value : (long long)(int)value;
    node->value = value;
    return node;
}

static Node *cast_to(Node *expr, Type *type)
{
    Node *folded = fold_cast(expr, type);
    if (folded)
        return folded;
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_CAST;
    node->tok = expr->tok;
    node->lhs = expr;
    node->type = type;
    return node;
}

static void usual_arith_conv(Node **lhs, Node **rhs)
{
    Type *type = common_type((*lhs)->type, (*rhs)->type);
    *lhs = cast_to(*lhs, type);
    *rhs = cast_to(*rhs, type);
}

void add_type(Node *node)
{
    if (!node || node->type)
        return;
    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);
    for (Node *n = node->body; n; n = n->next)
        add_type(n);
    for (Node *n = node->args; n; n = n->next)
        add_type(n);

    switch (node->kind) {
    case ND_NUM:
        node->type = ty_int;
        return;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
    case ND_BITAND:
    case ND_BITOR:
    case ND_BITXOR:
        usual_arith_conv(&node->lhs, &node->rhs);
        node->type = node->lhs->type;
        return;
    case ND_NEG: {
        Type *type = is_flonum(node->lhs->type) ? node->lhs->type : promoted(node->lhs->type);
        node->lhs = cast_to(node->lhs, type);
        node->type = type;
        return;
    }
    case ND_ASSIGN:
        if (node->lhs->type->kind == TY_ARRAY)
            error_at(node->tok, "cannot assign to an array");
        if (node->lhs->type->kind != TY_STRUCT && node->lhs->type->kind != TY_UNION)
            node->rhs = cast_to(node->rhs, node->lhs->type);
        node->type = node->lhs->type;
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        usual_arith_conv(&node->lhs, &node->rhs);
        node->type = ty_int;
        return;
    case ND_CALL:
    case ND_SYSCALL:
        if (!node->type)
            node->type = ty_long;
        return;
    case ND_NOT:
    case ND_LOGOR:
    case ND_LOGAND:
        node->type = ty_int;
        return;
    case ND_BITNOT:
    case ND_SHL:
    case ND_SHR: {
        Type *type = promoted(node->lhs->type);
        node->lhs = cast_to(node->lhs, type);
        node->type = type;
        return;
    }
    case ND_VAR:
        node->type = node->var->type;
        return;
    case ND_COND:
        if (node->then->type->kind == TY_VOID || node->els->type->kind == TY_VOID) {
            node->type = ty_void;
        } else if (node->then->type->kind == TY_STRUCT || node->then->type->kind == TY_UNION) {
            node->type = node->then->type;
        } else if (is_numeric(node->then->type) && is_numeric(node->els->type)) {
            usual_arith_conv(&node->then, &node->els);
            node->type = node->then->type;
        } else {
            Type *type = node->then->type;
            if (type->kind == TY_ARRAY)
                type = pointer_to(type->base);
            else if (type->kind == TY_FUNC)
                type = pointer_to(type);
            else if (!is_pointer_like(type))
                type = node->els->type->kind == TY_ARRAY ? pointer_to(node->els->type->base) : node->els->type;
            node->type = type;
        }
        return;
    case ND_COMMA:
        node->type = node->rhs->type;
        return;
    case ND_MEMBER:
        node->type = node->member->type;
        return;
    case ND_ADDR: {
        Type *type = node->lhs->type;
        if (type->kind == TY_ARRAY)
            node->type = pointer_to(type->base);
        else
            node->type = pointer_to(type);
        return;
    }
    case ND_DEREF:
        if (node->lhs->type->kind == TY_FUNC) {
            node->type = node->lhs->type;
            return;
        }
        if (!node->lhs->type->base)
            error_at(node->tok, "invalid pointer dereference");
        if (node->lhs->type->base->kind == TY_VOID)
            error_at(node->tok, "dereferencing a void pointer");
        node->type = node->lhs->type->base;
        return;
    }
}
