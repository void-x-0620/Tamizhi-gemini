/*
 * ast.h — Abstract syntax tree for Tamizhi.
 *
 * Tagged-union nodes, all arena-allocated. Lists are singly linked and built
 * with a tail pointer (NodeListBuilder) so appends are O(1).
 */
#ifndef AST_H
#define AST_H

#include "tamizhi.h"

/* ─── Types ────────────────────────────────────────────────────────────── */
typedef enum {
    TY_VOID, TY_INT, TY_FLOAT, TY_STRING, TY_BOOL,
    /* C types */
    TY_CHAR, TY_SHORT, TY_LONG,
    TY_INT8, TY_INT16, TY_INT32, TY_INT64,
    TY_UCHAR, TY_USHORT, TY_UINT, TY_ULONG,
    TY_UINT8, TY_UINT16, TY_UINT32, TY_UINT64,
    TY_SIZE_T, TY_SSIZE_T, TY_INTPTR, TY_UINTPTR, TY_PTRDIFF,
    TY_DOUBLE, TY_LONG_DOUBLE,
    /* Compound & pointer types */
    TY_ARRAY, TY_POINTER,
    TY_STRUCT, TY_CLASS, TY_UNION, TY_ENUM,
    TY_UNKNOWN,
} TypeKind;

typedef struct TypeInfo TypeInfo;
struct TypeInfo {
    TypeKind    kind;
    TypeInfo   *elem_type;     /* TY_ARRAY, TY_POINTER */
    const char *struct_name;   /* TY_STRUCT, TY_CLASS, TY_UNION, TY_ENUM */
    int         array_size;    /* TY_ARRAY: element count, -1 = unsized */
    bool        is_const;
    bool        is_static;
};

/* Shared singletons for built-in types (never freed, never mutated). */
extern TypeInfo ty_void, ty_int, ty_float, ty_string, ty_bool, ty_unknown;
extern TypeInfo ty_char, ty_short, ty_long, ty_double, ty_long_double;
extern TypeInfo ty_int8, ty_int16, ty_int32, ty_int64;
extern TypeInfo ty_uchar, ty_ushort, ty_uint, ty_ulong;
extern TypeInfo ty_uint8, ty_uint16, ty_uint32, ty_uint64;
extern TypeInfo ty_size_t, ty_ssize_t, ty_intptr, ty_uintptr, ty_ptrdiff;

/* ─── Node kinds ───────────────────────────────────────────────────────── */
typedef enum {
    NK_PROGRAM, NK_FUNC_DECL, NK_VAR_DECL, NK_STRUCT_DECL, NK_BLOCK,
    NK_IF, NK_WHILE, NK_RETURN, NK_BREAK, NK_CONTINUE, NK_EXPR_STMT, NK_PATHI,

    /* OOP, C options & control flow */
    NK_CLASS_DECL, NK_ENUM_DECL, NK_UNION_DECL,
    NK_FOR, NK_FOREACH, NK_DO_WHILE, NK_SWITCH, NK_CASE, NK_DEFAULT,
    NK_TYPEDEF,
    NK_DELETE,
    NK_DEFER,

    NK_INT_LIT, NK_FLOAT_LIT, NK_STRING_LIT, NK_BOOL_LIT, NK_CHAR_LIT, NK_NULL_LIT, NK_IDENT,
    NK_THIS, NK_SUPER, NK_NEW,
    NK_ASSIGN, NK_COMPOUND_ASSIGN,
    NK_POST_INC, NK_POST_DEC,
    NK_BINARY, NK_UNARY, NK_TERNARY,
    NK_NULL_COALESCE, NK_RANGE, NK_STRING_INTERP,
    NK_CALL, NK_METHOD_CALL, NK_INDEX, NK_MEMBER, NK_ARROW,
    NK_ARRAY_INIT,
    NK_SIZEOF, NK_CAST,
    NK_EXTERN_FUNC, NK_EXTERN_HEADER,      /* C bindings (veli) */
} NodeKind;

typedef enum {
    BOP_ADD, BOP_SUB, BOP_MUL, BOP_DIV, BOP_MOD,
    BOP_EQ, BOP_NE, BOP_LT, BOP_LE, BOP_GT, BOP_GE,
    BOP_AND, BOP_OR,
    /* Bitwise operators */
    BOP_BIT_AND, BOP_BIT_OR, BOP_BIT_XOR, BOP_SHL, BOP_SHR,
} BinOp;

typedef enum {
    UOP_NEG, UOP_NOT,
    UOP_BIT_NOT, UOP_ADDR_OF, UOP_DEREF,
    UOP_PRE_INC, UOP_PRE_DEC,
} UnaryOp;

typedef struct Node Node;

typedef struct Param { const char *name; TypeInfo *type; Node *default_val; struct Param *next; } Param;
typedef struct Field { const char *name; TypeInfo *type; struct Field *next; } Field;
typedef struct EnumVal {
    const char     *name;
    long long       value;
    bool            has_value;
    struct EnumVal *next;
} EnumVal;

/* One arm of an if / else-if / else chain (cond == NULL for the else arm). */
typedef struct IfBranch {
    Node            *cond;
    Node            *body;
    struct IfBranch *next;
} IfBranch;

typedef struct NodeList { Node *node; struct NodeList *next; } NodeList;
typedef struct { NodeList *head, *tail; } NodeListBuilder;

struct Node {
    NodeKind  kind;
    int       line, col;
    TypeInfo *type;                     /* set by semantic analysis */

    union {
        struct { NodeList *decls; } program;
        struct { const char *name; const char *class_name; Param *params; TypeInfo *ret_type;
                 Node *body; bool is_static; } func_decl;
        struct { const char *name; TypeInfo *var_type; Node *init;
                 bool is_const; bool is_static; } var_decl;
        struct { const char *name; Field *fields; NodeList *methods; } struct_decl;
        struct { const char *name; const char *base_name; Field *fields;
                 NodeList *methods; } class_decl;
        struct { const char *name; EnumVal *values; } enum_decl;
        struct { const char *name; Field *fields; } union_decl;
        struct { const char *name; TypeInfo *type; } typedef_decl;
        struct { NodeList *stmts; } block;
        struct { IfBranch *branches; } if_stmt;
        struct { Node *cond; Node *body; } while_stmt;
        struct { Node *init; Node *cond; Node *step; Node *body; } for_stmt;
        struct { const char *var_name; TypeInfo *var_type; Node *array; Node *body; } foreach_stmt;
        struct { Node *body; Node *cond; } do_while_stmt;
        struct { Node *expr; NodeList *cases; } switch_stmt;
        struct { Node *value; NodeList *stmts; } case_stmt;
        struct { Node *value; } ret;
        struct { Node *expr; } expr_stmt;
        struct { Node *expr; } delete_stmt;
        struct { Node *stmt; } defer_stmt;
        struct { Node *fmt; NodeList *args; } pathi;

        struct { long long value; } int_lit;
        struct { double value; } float_lit;
        struct { const char *value; } string_lit;
        struct { bool value; } bool_lit;
        struct { char value; } char_lit;
        struct { const char *name; } ident;
        struct { const char *class_name; NodeList *args; } new_expr;
        struct { Node *target; Node *value; } assign;
        struct { BinOp op; Node *target; Node *value; } compound_assign;
        struct { Node *target; } inc_dec;
        struct { BinOp op; Node *left; Node *right; } binary;
        struct { UnaryOp op; Node *operand; } unary;
        struct { Node *cond; Node *then_expr; Node *else_expr; } ternary;
        struct { Node *start; Node *end; bool inclusive; } range;
        struct { NodeList *parts; } string_interp;
        struct { TypeInfo *target_type; Node *expr; } sizeof_expr;
        struct { TypeInfo *target_type; Node *expr; } cast_expr;
        struct { const char *name; NodeList *args;
                 const char *cname; } call;     /* cname: set for C bindings */
        struct { Node *object; const char *method; NodeList *args;
                 bool is_arrow; } method_call;
        struct { Node *array; Node *index; } index;
        struct { Node *object; const char *field; } member;
        struct { Node *object; const char *field; } arrow;
        struct { NodeList *elements; } array_init;

        /* veli seyal name [= cname](params) [: type];  — a C function */
        struct { const char *name;     /* name used in Tamizhi code        */
                 const char *cname;    /* name of the C function           */
                 Param *params; TypeInfo *ret_type;
                 bool has_header;      /* a header declared its prototype  */
                 const char *file;     /* binding file, for diagnostics    */
        } extern_func;
        /* veli "header.h";  — spec is ready to paste after #include */
        struct { const char *spec; } extern_header;
    };
};

/* ─── Construction helpers ─────────────────────────────────────────────── */
Node     *node_new(Arena *a, NodeKind kind, int line, int col);
void      nlb_push(Arena *a, NodeListBuilder *b, Node *n);
Param    *param_new(Arena *a, const char *name, TypeInfo *type);
Field    *field_new(Arena *a, const char *name, TypeInfo *type);
EnumVal  *enum_val_new(Arena *a, const char *name, long long val, bool has_val);
TypeInfo *type_new(Arena *a, TypeKind kind);
TypeInfo *type_array(Arena *a, TypeInfo *elem, int size);
TypeInfo *type_pointer(Arena *a, TypeInfo *elem);

const char *binop_name(BinOp op);
const char *unaryop_name(UnaryOp op);

/* ─── pathi format-string analysis ─────────────────────────────────────── */
char *fmt_rewrite(Arena *a, const char *fmt, char *kinds, int max_kinds,
                  int *count, const char **err);

#endif /* AST_H */
