#ifndef CC_H
// most of this comes from chibicc by Rui Ueyama (see LICENSE.chibicc), cut down and bent
// to fit. the fasm output, the peephole and the whole program linking are mine
#define CC_H

#include <stdarg.h>
#include <stddef.h>

typedef struct Token Token;
typedef struct Type Type;
typedef struct Member Member;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Reloc Reloc;
typedef struct Buffer Buffer;

enum {
    TK_IDENT,
    TK_PUNCT,
    TK_KEYWORD,
    TK_NUM,
    TK_STR,
    TK_EOF
};

struct Token {
    int kind;
    Token *next;
    char *text;
    int length;
    long long value;
    double fvalue;
    Type *type;
    char *str;
    int str_length;
    char *file;
    int line;
    char *line_start;
    int at_bol;
    int space;
};

enum {
    TY_VOID,
    TY_BOOL,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_ENUM,
    TY_FLOAT,
    TY_DOUBLE,
    TY_PTR,
    TY_ARRAY,
    TY_FUNC,
    TY_STRUCT,
    TY_UNION
};

struct Type {
    int kind;
    int size;
    int align;
    int is_unsigned;
    Type *base;
    int length;
    Token *name;
    Member *members;
    int complete;
    Type *ret;
    Type *params;
    Type *next;
    int variadic;
    int prototyped;
};

struct Member {
    Member *next;
    Type *type;
    Token *name;
    int offset;
};

enum {
    ND_NUM,
    ND_VAR,
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_BITAND,
    ND_BITOR,
    ND_BITXOR,
    ND_SHL,
    ND_SHR,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_LOGAND,
    ND_LOGOR,
    ND_NOT,
    ND_BITNOT,
    ND_NEG,
    ND_ADDR,
    ND_DEREF,
    ND_MEMBER,
    ND_ASSIGN,
    ND_COND,
    ND_COMMA,
    ND_CAST,
    ND_CALL,
    ND_SYSCALL,
    ND_MEMZERO,
    ND_RETURN,
    ND_IF,
    ND_FOR,
    ND_DO,
    ND_SWITCH,
    ND_CASE,
    ND_BLOCK,
    ND_GOTO,
    ND_LABEL,
    ND_EXPR_STMT,
    ND_ASM,
    ND_NULL
};

struct Node {
    int kind;
    Node *next;
    Type *type;
    Token *tok;
    Node *lhs;
    Node *rhs;
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;
    Node *body;
    Node *args;
    Member *member;
    Obj *var;
    long long value;
    double fvalue;
    int label;
    int break_label;
    int continue_label;
    Node *cases;
    Node *default_case;
    Node *unused_value;
    char *name;
};

struct Reloc {
    Reloc *next;
    int offset;
    Obj *target;
    long long addend;
};

struct Obj {
    Obj *next;
    char *name;
    char *label;
    Type *type;
    int is_local;
    int offset;
    int is_function;
    int is_definition;
    int is_static;
    int has_init;
    char *data;
    Reloc *relocs;
    Obj *locals;
    Obj *return_slot;
    Node *body;
    int stack_size;
    Obj **refs;  // who this one uses, generate() walks it from serp_start and drops the rest
    int ref_count;
    int ref_capacity;
    int reachable;
    char *code;
    int code_length;
};

struct Buffer {
    char *data;
    int length;
    int capacity;
};

extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;
extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;
extern Type *ty_float;
extern Type *ty_double;

extern Obj *globals;
extern Buffer global_asm;
extern int unique_number;
extern int generated_functions;
extern char *include_paths[8];
extern int include_path_count;

void fatal(char *format, ...);
void error_at(Token *tok, char *format, ...);
void warn_at(Token *tok, char *format, ...);
char *copy_text(char *text, int length);
char *format_text(char *format, ...);
void buffer_add(Buffer *buffer, char *text, int length);
void buffer_printf(Buffer *buffer, char *format, ...);
char *read_file(char *path, int *length);

int equal(Token *tok, char *text);
Token *skip(Token *tok, char *text);
Token *tokenize_file(char *path);
Token *tokenize_text(char *file, char *text);
void mark_keywords(Token *tok);
Token *preprocess(Token *tok);
void define_macro(char *name, char *body);
void save_preprocessor(void);
void reset_preprocessor(void);

int is_integer(Type *type);
int is_struct(Type *type);
int is_flonum(Type *type);
int is_numeric(Type *type);
int is_pointer_like(Type *type);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int length);
Type *func_type(Type *ret);
Type *copy_type(Type *type);
Type *new_struct_type(int kind);
void add_type(Node *node);
Node *fold_cast(Node *expr, Type *type);
int align_to(int value, int align);

void parse_unit(Token *tok, int unit);
void generate(Buffer *out);

#endif
