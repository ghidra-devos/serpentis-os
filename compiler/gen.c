#include "cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Buffer *out;
static Obj *current_fn;
int generated_functions;

static void emit(char *format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    buffer_add(out, "\t", 1);
    buffer_add(out, line, length);
    buffer_add(out, "\n", 1);
}

static void emit_label(int label)
{
    buffer_printf(out, ".L%d:\n", label);
}

static void add_ref(Obj *from, Obj *to)
{
    for (int i = 0; i < from->ref_count; i++)
        if (from->refs[i] == to)
            return;
    if (from->ref_count == from->ref_capacity) {
        from->ref_capacity = from->ref_capacity ? from->ref_capacity * 2 : 8;
        from->refs = realloc(from->refs, from->ref_capacity * sizeof(Obj *));
    }
    from->refs[from->ref_count++] = to;
}

static int fits_imm32(long long value)
{
    return value >= -2147483647L - 1 && value <= 2147483647L;
}

static void gen_expr(Node *node);
static void gen_discarded(Node *node);
static void gen_stmt(Node *node);

static char *sse(Type *type)
{
    return type->kind == TY_FLOAT ? "ss" : "sd";
}

static void flonum_truth(Type *type)
{
    emit("movq xmm0, rax");
    emit("xorpd xmm1, xmm1");
    emit("ucomi%s xmm0, xmm1", sse(type));
    emit("setne al");
    emit("setp cl");
    emit("or al, cl");
    emit("movzx eax, al");
}

static void gen_addr(Node *node)
{
    if (node->type && is_struct(node->type) && (node->kind == ND_CALL || node->kind == ND_COND || node->kind == ND_ASSIGN)) {
        gen_expr(node);
        return;
    }
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_local) {
            int offset = node->var->offset;
            emit("lea rax, [rbp%c%d]", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset);
        } else {
            add_ref(current_fn, node->var);
            emit("lea rax, [%s]", node->var->label);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_MEMBER:
        gen_addr(node->lhs);
        if (node->member->offset)
            emit("add rax, %d", node->member->offset);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    }
    error_at(node->tok, "not an lvalue");
}

static int is_scalar(Type *type)
{
    return type->kind != TY_ARRAY && type->kind != TY_STRUCT && type->kind != TY_UNION && type->kind != TY_FUNC;
}

static char *var_address(Obj *var)
{
    if (var->is_local) {
        int offset = var->offset;
        return format_text("rbp%c%d", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset);
    }
    add_ref(current_fn, var);
    return var->label;
}

static void load_from(Type *type, char *reg, char *address)
{
    char wide = reg[1];
    if (!is_scalar(type))
        return;
    if (type->kind == TY_FLOAT)
        emit("mov e%cx, dword [%s]", wide, address);
    else if (type->size == 1 && type->is_unsigned)
        emit("movzx e%cx, byte [%s]", wide, address);
    else if (type->size == 1)
        emit("movsx r%cx, byte [%s]", wide, address);
    else if (type->size == 2 && type->is_unsigned)
        emit("movzx e%cx, word [%s]", wide, address);
    else if (type->size == 2)
        emit("movsx r%cx, word [%s]", wide, address);
    else if (type->size == 4 && type->is_unsigned)
        emit("mov e%cx, dword [%s]", wide, address);
    else if (type->size == 4)
        emit("movsxd r%cx, dword [%s]", wide, address);
    else
        emit("mov r%cx, qword [%s]", wide, address);
}

static void load(Type *type)
{
    load_from(type, "rax", "rax");
}

static void store_to(Type *type, char *address)
{
    if (type->size == 1)
        emit("mov byte [%s], al", address);
    else if (type->size == 2)
        emit("mov word [%s], ax", address);
    else if (type->size == 4)
        emit("mov dword [%s], eax", address);
    else
        emit("mov qword [%s], rax", address);
}

static void store(Type *type)
{
    emit("pop rdi");
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        emit("push rdi");
        emit("mov rsi, rax");
        emit("mov ecx, %d", type->size);
        emit("rep movsb");
        emit("pop rax");
        return;
    }
    store_to(type, "rdi");
}

static void truncate_to(Type *type)
{
    if (type->kind == TY_BOOL) {
        emit("test rax, rax");
        emit("setne al");
        emit("movzx eax, al");
    } else if (type->size == 1) {
        emit(type->is_unsigned ? "movzx eax, al" : "movsx rax, al");
    } else if (type->size == 2) {
        emit(type->is_unsigned ? "movzx eax, ax" : "movsx rax, ax");
    } else if (type->size == 4) {
        emit(type->is_unsigned ? "mov eax, eax" : "movsxd rax, eax");
    }
}

static void flonum_cast(Type *from, Type *to)
{
    if (is_flonum(from) && is_flonum(to)) {
        if (from->kind == to->kind)
            return;
        emit("movq xmm0, rax");
        emit(from->kind == TY_FLOAT ? "cvtss2sd xmm0, xmm0" : "cvtsd2ss xmm0, xmm0");
        emit("movq rax, xmm0");
        if (to->kind == TY_FLOAT)
            emit("mov eax, eax");
        return;
    }
    if (is_flonum(to)) {
        if (from->size == 8 && from->is_unsigned) {
            int big = unique_number++, done = unique_number++;
            emit("test rax, rax");
            emit("js .L%d", big);
            emit("cvtsi2%s xmm0, rax", sse(to));
            emit("jmp .L%d", done);
            emit_label(big);
            emit("mov rcx, rax");
            emit("shr rcx, 1");
            emit("and eax, 1");
            emit("or rcx, rax");
            emit("cvtsi2%s xmm0, rcx", sse(to));
            emit("add%s xmm0, xmm0", sse(to));
            emit_label(done);
        } else {
            emit("cvtsi2%s xmm0, rax", sse(to));
        }
        emit("movq rax, xmm0");
        if (to->kind == TY_FLOAT)
            emit("mov eax, eax");
        return;
    }
    if (to->kind == TY_BOOL) {
        flonum_truth(from);
        return;
    }
    if (to->kind == TY_VOID)
        return;
    emit("movq xmm0, rax");
    emit("cvtt%s2si rax, xmm0", sse(from));
    if (is_integer(to))
        truncate_to(to);
}

static int cast_needed(Type *from, Type *to)
{
    if (to->kind == TY_VOID || !is_integer(to))
        return 0;
    if (to->kind == TY_BOOL)
        return from->kind != TY_BOOL;
    if (to->size == 8)
        return 0;
    if (is_integer(from)) {
        if (from->size < to->size && !(to->is_unsigned && !from->is_unsigned))
            return 0;
        if (from->size == to->size && from->is_unsigned == to->is_unsigned)
            return 0;
    }
    return 1;
}

static void gen_cast(Type *from, Type *to)
{
    if (is_flonum(from) || is_flonum(to)) {
        flonum_cast(from, to);
        return;
    }
    if (cast_needed(from, to))
        truncate_to(to);
}

static Node *plain_operand(Node *node)
{
    if (is_flonum(node->type))
        return 0;
    if (node->kind == ND_CAST && !cast_needed(node->lhs->type, node->type) &&
        (node->lhs->kind == ND_VAR || node->lhs->kind == ND_NUM))
        node = node->lhs;
    if (node->kind == ND_NUM && fits_imm32(node->value))
        return node;
    if (node->kind == ND_VAR && is_scalar(node->type))
        return node;
    return 0;
}

static void load_plain(Node *node, char *reg)
{
    if (node->kind == ND_NUM)
        emit("mov %s, %lld", reg, node->value);
    else
        load_from(node->type, reg, var_address(node->var));
}

static char *gen_operands(Node *node, int allow_immediate)
{
    Node *rhs = plain_operand(node->rhs);
    if (rhs) {
        gen_expr(node->lhs);
        if (rhs->kind == ND_NUM && allow_immediate)
            return format_text("%lld", rhs->value);
        load_plain(rhs, "rcx");
        return "rcx";
    }
    Node *lhs = plain_operand(node->lhs);
    if (lhs) {
        gen_expr(node->rhs);
        emit("mov rcx, rax");
        load_plain(lhs, "rax");
        return "rcx";
    }
    gen_expr(node->lhs);
    emit("push rax");
    gen_expr(node->rhs);
    emit("mov rcx, rax");
    emit("pop rax");
    return "rcx";
}

static char *condition_code(int kind, int is_unsigned, int negate)
{
    switch (kind) {
    case ND_EQ:
        return negate ? "ne" : "e";
    case ND_NE:
        return negate ? "e" : "ne";
    case ND_LT:
        if (is_unsigned)
            return negate ? "ae" : "b";
        return negate ? "ge" : "l";
    default:
        if (is_unsigned)
            return negate ? "a" : "be";
        return negate ? "g" : "le";
    }
}

static int is_comparison(Node *node)
{
    return node->kind == ND_EQ || node->kind == ND_NE || node->kind == ND_LT || node->kind == ND_LE;
}

static void gen_branch(Node *node, int label, int when_true)
{
    add_type(node);
    if (is_comparison(node) && !is_flonum(node->lhs->type)) {
        char *operand = gen_operands(node, 1);
        emit("cmp rax, %s", operand);
        emit("j%s .L%d", condition_code(node->kind, node->lhs->type->is_unsigned, !when_true), label);
        return;
    }
    if (node->kind == ND_NOT) {
        gen_branch(node->lhs, label, !when_true);
        return;
    }
    if (node->kind == ND_LOGAND || node->kind == ND_LOGOR) {
        int is_and = node->kind == ND_LOGAND;
        if (is_and != when_true) {
            gen_branch(node->lhs, label, when_true);
            gen_branch(node->rhs, label, when_true);
        } else {
            int skip = unique_number++;
            gen_branch(node->lhs, skip, !when_true);
            gen_branch(node->rhs, label, when_true);
            emit_label(skip);
        }
        return;
    }
    gen_expr(node);
    if (is_flonum(node->type))
        flonum_truth(node->type);
    emit("test rax, rax");
    emit("j%s .L%d", when_true ? "nz" : "z", label);
}

static void gen_flonum(Node *node)
{
    gen_expr(node->lhs);
    emit("push rax");
    gen_expr(node->rhs);
    emit("movq xmm1, rax");
    emit("pop rax");
    emit("movq xmm0, rax");
    Type *type = node->lhs->type;
    char *kind = sse(type);
    switch (node->kind) {
    case ND_ADD:
        emit("add%s xmm0, xmm1", kind);
        break;
    case ND_SUB:
        emit("sub%s xmm0, xmm1", kind);
        break;
    case ND_MUL:
        emit("mul%s xmm0, xmm1", kind);
        break;
    case ND_DIV:
        emit("div%s xmm0, xmm1", kind);
        break;
    case ND_EQ:
        emit("ucomi%s xmm0, xmm1", kind);
        emit("sete al");
        emit("setnp cl");
        emit("and al, cl");
        emit("movzx eax, al");
        return;
    case ND_NE:
        emit("ucomi%s xmm0, xmm1", kind);
        emit("setne al");
        emit("setp cl");
        emit("or al, cl");
        emit("movzx eax, al");
        return;
    case ND_LT:
        emit("ucomi%s xmm1, xmm0", kind);
        emit("seta al");
        emit("movzx eax, al");
        return;
    case ND_LE:
        emit("ucomi%s xmm1, xmm0", kind);
        emit("setae al");
        emit("movzx eax, al");
        return;
    default:
        error_at(node->tok, "invalid operands for floating point");
    }
    emit("movq rax, xmm0");
    if (type->kind == TY_FLOAT)
        emit("mov eax, eax");
}

static int push_args(Node *arg)
{
    if (!arg)
        return 0;
    // own calling convention, not sysv: everything goes on the stack, last argument first.
    // slower than registers, way simpler to get right
    int bytes = push_args(arg->next);
    gen_expr(arg);
    if (is_struct(arg->type)) {
        int size = align_to(arg->type->size, 8);
        emit("sub rsp, %d", size);
        emit("mov rsi, rax");
        emit("mov rdi, rsp");
        emit("mov ecx, %d", arg->type->size);
        emit("rep movsb");
        return bytes + size;
    }
    emit("push rax");
    return bytes + 8;
}

static void gen_discarded(Node *node)
{
    while (node->unused_value)
        node = node->unused_value;
    if (node->kind == ND_COMMA) {
        gen_discarded(node->lhs);
        gen_discarded(node->rhs);
        return;
    }
    gen_expr(node);
}

static void gen_expr(Node *node)
{
    switch (node->kind) {
    case ND_NULL:
        return;
    case ND_NUM:
        if (node->type->kind == TY_DOUBLE) {
            double number = node->fvalue;
            long long bits;
            memcpy(&bits, &number, 8);
            emit("mov rax, %lld", bits);
            return;
        }
        if (node->type->kind == TY_FLOAT) {
            float number = (float)node->fvalue;
            unsigned int bits;
            memcpy(&bits, &number, 4);
            emit("mov eax, %u", bits);
            return;
        }
        if (node->value == 0)
            emit("xor eax, eax");
        else
            emit("mov rax, %lld", node->value);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        if (node->type->kind == TY_DOUBLE) {
            emit("btc rax, 63");
            return;
        }
        if (node->type->kind == TY_FLOAT) {
            emit("btc eax, 31");
            return;
        }
        emit("neg rax");
        truncate_to(node->type);
        return;
    case ND_VAR:
        if (is_scalar(node->type)) {
            load_from(node->type, "rax", var_address(node->var));
            return;
        }
        gen_addr(node);
        return;
    case ND_MEMBER:
        gen_addr(node);
        load(node->type);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->type);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN:
        if (node->lhs->kind == ND_VAR && is_scalar(node->type)) {
            gen_expr(node->rhs);
            store_to(node->type, var_address(node->lhs->var));
            return;
        }
        if (is_scalar(node->type) && plain_operand(node->rhs)) {
            Node *value = plain_operand(node->rhs);
            gen_addr(node->lhs);
            emit("mov rdi, rax");
            if (value->kind == ND_NUM)
                emit("mov rax, %lld", value->value);
            else
                load_from(value->type, "rax", var_address(value->var));
            store_to(node->type, "rdi");
            return;
        }
        gen_addr(node->lhs);
        emit("push rax");
        gen_expr(node->rhs);
        store(node->type);
        return;
    case ND_MEMZERO: {
        int offset = node->var->offset;
        emit("lea rdi, [rbp%c%d]", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset);
        emit("mov ecx, %d", node->var->type->size);
        emit("xor eax, eax");
        emit("rep stosb");
        return;
    }
    case ND_COND: {
        int else_label = unique_number++, end_label = unique_number++;
        gen_branch(node->cond, else_label, 0);
        gen_expr(node->then);
        emit("jmp .L%d", end_label);
        emit_label(else_label);
        gen_expr(node->els);
        emit_label(end_label);
        return;
    }
    case ND_NOT:
        gen_expr(node->lhs);
        if (is_flonum(node->lhs->type))
            flonum_truth(node->lhs->type);
        emit("test rax, rax");
        emit("sete al");
        emit("movzx eax, al");
        return;
    case ND_BITNOT:
        gen_expr(node->lhs);
        emit("not rax");
        truncate_to(node->type);
        return;
    case ND_LOGAND:
    case ND_LOGOR: {
        int is_and = node->kind == ND_LOGAND;
        int short_label = unique_number++, end_label = unique_number++;
        gen_branch(node->lhs, short_label, !is_and);
        gen_branch(node->rhs, short_label, !is_and);
        emit("mov eax, %d", is_and);
        emit("jmp .L%d", end_label);
        emit_label(short_label);
        emit("mov eax, %d", !is_and);
        emit_label(end_label);
        return;
    }
    case ND_CAST:
        gen_expr(node->lhs);
        gen_cast(node->lhs->type, node->type);
        return;
    case ND_COMMA:
        gen_discarded(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_CALL: {
        int bytes = push_args(node->args);
        if (node->var) {
            emit("lea rax, [%s]", var_address(node->var));
            emit("push rax");
            bytes += 8;
        }
        if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
            add_ref(current_fn, node->lhs->var);
            emit("call %s", node->lhs->var->label);
        } else {
            gen_expr(node->lhs);
            emit("call rax");
        }
        if (bytes)
            emit("add rsp, %d", bytes);
        return;
    }
    case ND_SYSCALL: {
        static char *registers[] = {"rax", "rdi", "rsi", "rdx", "r10"};
        push_args(node->args);
        for (int i = 0; i < node->value; i++)
            emit("pop %s", registers[i]);
        emit("int 0x80");
        return;
    }
    }

    if (node->lhs && is_flonum(node->lhs->type)) {
        gen_flonum(node);
        return;
    }
    int alu = node->kind == ND_ADD || node->kind == ND_SUB || node->kind == ND_BITAND || node->kind == ND_BITOR ||
              node->kind == ND_BITXOR || is_comparison(node);
    char *operand = gen_operands(node, alu);
    int is_unsigned = node->lhs->type->is_unsigned;
    switch (node->kind) {
    case ND_ADD:
        emit("add rax, %s", operand);
        break;
    case ND_SUB:
        emit("sub rax, %s", operand);
        break;
    case ND_MUL:
        emit("imul rax, rcx");
        break;
    case ND_DIV:
    case ND_MOD:
        if (is_unsigned) {
            emit("xor edx, edx");
            emit("div rcx");
        } else {
            emit("cqo");
            emit("idiv rcx");
        }
        if (node->kind == ND_MOD)
            emit("mov rax, rdx");
        break;
    case ND_BITAND:
        emit("and rax, %s", operand);
        break;
    case ND_BITOR:
        emit("or rax, %s", operand);
        break;
    case ND_BITXOR:
        emit("xor rax, %s", operand);
        break;
    case ND_SHL:
        emit("shl rax, cl");
        break;
    case ND_SHR:
        emit(is_unsigned ? "shr rax, cl" : "sar rax, cl");
        break;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        emit("cmp rax, %s", operand);
        emit("set%s al", condition_code(node->kind, is_unsigned, 0));
        emit("movzx eax, al");
        return;
    default:
        error_at(node->tok, "invalid expression");
    }
    int keeps_form = node->kind == ND_BITAND || node->kind == ND_BITOR || node->kind == ND_BITXOR;
    if (node->type->size < 8 && node->type->kind != TY_PTR && !keeps_form)
        truncate_to(node->type);
}

static void gen_switch(Node *node)
{
    gen_expr(node->cond);
    for (Node *c = node->cases; c; c = c->cases) {
        if (fits_imm32(c->value)) {
            emit("cmp rax, %lld", c->value);
        } else {
            emit("mov rcx, %lld", c->value);
            emit("cmp rax, rcx");
        }
        emit("je .L%d", c->label);
    }
    emit("jmp .L%d", node->default_case ? node->default_case->label : node->break_label);
    gen_stmt(node->then);
    emit_label(node->break_label);
}

static void gen_stmt(Node *node)
{
    switch (node->kind) {
    case ND_IF: {
        int else_label = unique_number++;
        gen_branch(node->cond, else_label, 0);
        gen_stmt(node->then);
        if (node->els) {
            int end_label = unique_number++;
            emit("jmp .L%d", end_label);
            emit_label(else_label);
            gen_stmt(node->els);
            emit_label(end_label);
        } else {
            emit_label(else_label);
        }
        return;
    }
    case ND_FOR: {
        int begin = unique_number++;
        if (node->init)
            gen_stmt(node->init);
        emit_label(begin);
        if (node->cond)
            gen_branch(node->cond, node->break_label, 0);
        gen_stmt(node->then);
        emit_label(node->continue_label);
        if (node->inc)
            gen_discarded(node->inc);
        emit("jmp .L%d", begin);
        emit_label(node->break_label);
        return;
    }
    case ND_DO: {
        int begin = unique_number++;
        emit_label(begin);
        gen_stmt(node->then);
        emit_label(node->continue_label);
        gen_branch(node->cond, begin, 1);
        emit_label(node->break_label);
        return;
    }
    case ND_SWITCH:
        gen_switch(node);
        return;
    case ND_CASE:
        emit_label(node->label);
        gen_stmt(node->lhs);
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_GOTO:
        emit("jmp .L%d", node->label);
        return;
    case ND_LABEL:
        emit_label(node->label);
        gen_stmt(node->lhs);
        return;
    case ND_RETURN:
        if (node->lhs)
            gen_expr(node->lhs);
        if (node->lhs && is_struct(node->lhs->type)) {
            emit("mov rsi, rax");
            emit("mov rdi, qword [rbp+16]");
            emit("mov ecx, %d", node->lhs->type->size);
            emit("rep movsb");
            emit("mov rax, qword [rbp+16]");
        }
        emit("jmp .done");
        return;
    case ND_EXPR_STMT:
        gen_discarded(node->lhs);
        return;
    case ND_ASM:
        buffer_add(out, node->name, strlen(node->name));
        buffer_add(out, "\n", 1);
        return;
    }
    error_at(node->tok, "invalid statement");
}

static char *inverse_jumps[] = {
    "e", "ne", "z", "nz", "l", "ge", "le", "g", "b", "ae", "be", "a", "s", "ns", "p", "np", 0
};

static char *inverse_condition(char *condition)
{
    for (int i = 0; inverse_jumps[i]; i++) {
        if (!strcmp(inverse_jumps[i], condition))
            return inverse_jumps[i ^ 1];
    }
    return 0;
}

static int starts_with(char *text, char *prefix)
{
    return !strncmp(text, prefix, strlen(prefix));
}

static int is_label_line(char *line, char *label)
{
    int length = strlen(label);
    return !strncmp(line, label, length) && line[length] == ':' && line[length + 1] == 0;
}

static int same_slot(char *store, char *store_prefix, char *store_suffix, char *load, char *load_prefix)
{
    if (!starts_with(store, store_prefix) || !starts_with(load, load_prefix))
        return 0;
    char *slot = store + strlen(store_prefix);
    char *end = strstr(slot, store_suffix);
    if (!end || end[strlen(store_suffix)])
        return 0;
    int length = end - slot;
    char *loaded = load + strlen(load_prefix);
    return (int)strlen(loaded) == length + 1 && !strncmp(loaded, slot, length) && loaded[length] == ']';
}

// chibicc style stack machine output is full of push rax / pop rdi pairs, this cleans up
// the worst of it. text in, text out, nothing clever
static void peephole(Buffer *code)
{
    char **lines = malloc((code->length / 2 + 2) * sizeof(char *));
    int count = 0;
    char *p = code->data;
    while (*p) {
        lines[count++] = p;
        char *end = strchr(p, '\n');
        if (!end)
            break;
        *end = 0;
        p = end + 1;
    }
    int changed = 1;
    while (changed) {
        changed = 0;
        int kept = 0;
        for (int i = 0; i < count; i++) {
            char *line = lines[i];
            char *next = i + 1 < count ? lines[i + 1] : "";
            char *after = i + 2 < count ? lines[i + 2] : "";
            if (!strcmp(line, "\tpush rax") && !strcmp(next, "\tpop rdi")) {
                lines[kept++] = "\tmov rdi, rax";
                i++;
                changed = 1;
                continue;
            }
            if (!strcmp(line, "\tpush rax") && !strcmp(next, "\tpop rax")) {
                i++;
                changed = 1;
                continue;
            }
            if (starts_with(line, "\tjmp .") && is_label_line(next, line + 5)) {
                changed = 1;
                continue;
            }
            if (line[0] == '\t' && line[1] == 'j' && starts_with(next, "\tjmp .")) {
                char *space = strchr(line, ' ');
                if (space && space[1] == '.' && is_label_line(after, space + 1)) {
                    char condition[8];
                    int length = space - line - 2;
                    if (length > 0 && length < 8) {
                        memcpy(condition, line + 2, length);
                        condition[length] = 0;
                        char *inverse = inverse_condition(condition);
                        if (inverse) {
                            lines[kept++] = format_text("\tj%s %s", inverse, next + 5);
                            i++;
                            changed = 1;
                            continue;
                        }
                    }
                }
            }
            if (same_slot(line, "\tmov qword [", "], rax", next, "\tmov rax, qword [") ||
                same_slot(line, "\tmov dword [", "], eax", next, "\tmovsxd rax, dword [") ||
                same_slot(line, "\tmov dword [", "], eax", next, "\tmov eax, dword [")) {
                lines[kept++] = line;
                i++;
                changed = 1;
                continue;
            }
            lines[kept++] = line;
        }
        count = kept;
    }
    Buffer result = {0, 0, 0};
    for (int i = 0; i < count; i++) {
        buffer_add(&result, lines[i], strlen(lines[i]));
        buffer_add(&result, "\n", 1);
    }
    free(lines);
    free(code->data);
    *code = result;
}

static void gen_function(Obj *fn)
{
    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
        if (var->offset > 0)
            continue;
        offset += var->type->size;
        offset = align_to(offset, var->type->align > 8 ? var->type->align : 8);
        var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);

    Buffer code = {0, 0, 0};
    out = &code;
    current_fn = fn;
    buffer_printf(out, "%s:\n", fn->label);
    emit("push rbp");
    emit("mov rbp, rsp");
    if (fn->stack_size)
        emit("sub rsp, %d", fn->stack_size);
    gen_stmt(fn->body);
    char *tail = "\tjmp .done\n";
    int tail_length = strlen(tail);
    if (code.length > tail_length && !strcmp(code.data + code.length - tail_length, tail))
        code.length -= tail_length;
    else if (!strcmp(fn->name, "main"))
        emit("xor eax, eax");
    buffer_add(out, ".done:\n", 7);
    emit("leave");
    emit("ret");
    buffer_add(out, "\n", 1);
    peephole(&code);
    fn->code = code.data;
    fn->code_length = code.length;
}

static void mark(Obj *var)
{
    if (var->reachable)
        return;
    var->reachable = 1;
    for (int i = 0; i < var->ref_count; i++)
        mark(var->refs[i]);
    for (Reloc *rel = var->relocs; rel; rel = rel->next)
        mark(rel->target);
}

static int printable(char c)
{
    return c >= 32 && c < 127 && c != '\'';
}

static void emit_bytes(char *data, int length)
{
    int i = 0;
    while (i < length) {
        Buffer line = {0, 0, 0};
        buffer_add(&line, "\tdb ", 4);
        int items = 0;
        while (i < length && line.length < 72) {
            if (items++)
                buffer_add(&line, ", ", 2);
            if (printable(data[i])) {
                buffer_add(&line, "'", 1);
                while (i < length && printable(data[i]) && line.length < 90)
                    buffer_add(&line, &data[i++], 1);
                buffer_add(&line, "'", 1);
            } else {
                buffer_printf(&line, "%d", (unsigned char)data[i++]);
            }
        }
        buffer_add(&line, "\n", 1);
        buffer_add(out, line.data, line.length);
        free(line.data);
    }
}

static void emit_data(Obj *var)
{
    int align = var->type->align > 1 ? var->type->align : 1;
    if (align > 1)
        emit("align %d", align);
    buffer_printf(out, "%s:\n", var->label);
    int size = var->type->size;
    int position = 0;
    Reloc *rel = var->relocs;
    while (position < size) {
        if (rel && rel->offset == position) {
            if (rel->addend)
                emit("dq %s %c %lld", rel->target->label, rel->addend < 0 ? '-' : '+',
                     rel->addend < 0 ? -rel->addend : rel->addend);
            else
                emit("dq %s", rel->target->label);
            position += 8;
            rel = rel->next;
            continue;
        }
        int end = rel ? rel->offset : size;
        emit_bytes(var->data + position, end - position);
        position = end;
    }
}

void generate(Buffer *output)
{
    for (Obj *var = globals; var; var = var->next)
        if (var->is_function && var->is_definition)
            gen_function(var);

    Obj *start = 0;
    for (Obj *var = globals; var; var = var->next)
        if (var->is_function && !var->is_static && !strcmp(var->name, "serp_start"))
            start = var;
    if (!start || !start->is_definition)
        fatal("serp_start is missing, the C library was not found");
    mark(start);

    int missing = 0;
    for (Obj *var = globals; var; var = var->next) {
        if (!var->reachable || var->is_definition)
            continue;
        fprintf(stderr, "cc: undefined reference to %s\n", var->name);
        missing = 1;
    }
    if (missing)
        exit(1);

    out = output;
    buffer_printf(out, "SERP_STACK = 0x10000\n\n_start:\n");
    emit("and rsp, -16");
    emit("push rdi");
    emit("call %s", start->label);
    buffer_add(out, "\n", 1);

    int count = 0;
    for (Obj *var = globals; var; var = var->next)
        count++;
    Obj **list = malloc((count + 1) * sizeof(Obj *));
    int index = count;
    for (Obj *var = globals; var; var = var->next)
        list[--index] = var;

    for (int i = 0; i < count; i++) {
        if (list[i]->reachable && list[i]->is_function) {
            buffer_add(out, list[i]->code, list[i]->code_length);
            generated_functions++;
        }
    }
    if (global_asm.length)
        buffer_add(out, global_asm.data, global_asm.length);
    for (int i = 0; i < count; i++)
        if (list[i]->reachable && !list[i]->is_function && list[i]->has_init)
            emit_data(list[i]);
    buffer_add(out, "\n", 1);
    for (int i = 0; i < count; i++) {
        Obj *var = list[i];
        if (!var->reachable || var->is_function || var->has_init)
            continue;
        if (var->type->align > 1)
            emit("align %d", var->type->align);
        buffer_printf(out, "%s rb %d\n", var->label, var->type->size ? var->type->size : 1);
    }
}
