/*
 * ast.c — AST construction helpers and pathi format analysis.
 */
#include "ast.h"

TypeInfo ty_void        = { TY_VOID,        NULL, NULL, -1, false, false };
TypeInfo ty_int         = { TY_INT,         NULL, NULL, -1, false, false };
TypeInfo ty_float       = { TY_FLOAT,       NULL, NULL, -1, false, false };
TypeInfo ty_string      = { TY_STRING,      NULL, NULL, -1, false, false };
TypeInfo ty_bool        = { TY_BOOL,        NULL, NULL, -1, false, false };
TypeInfo ty_unknown     = { TY_UNKNOWN,     NULL, NULL, -1, false, false };

TypeInfo ty_char        = { TY_CHAR,        NULL, NULL, -1, false, false };
TypeInfo ty_short       = { TY_SHORT,       NULL, NULL, -1, false, false };
TypeInfo ty_long        = { TY_LONG,        NULL, NULL, -1, false, false };
TypeInfo ty_double      = { TY_DOUBLE,      NULL, NULL, -1, false, false };
TypeInfo ty_long_double = { TY_LONG_DOUBLE, NULL, NULL, -1, false, false };

TypeInfo ty_int8        = { TY_INT8,        NULL, NULL, -1, false, false };
TypeInfo ty_int16       = { TY_INT16,       NULL, NULL, -1, false, false };
TypeInfo ty_int32       = { TY_INT32,       NULL, NULL, -1, false, false };
TypeInfo ty_int64       = { TY_INT64,       NULL, NULL, -1, false, false };

TypeInfo ty_uchar       = { TY_UCHAR,       NULL, NULL, -1, false, false };
TypeInfo ty_ushort      = { TY_USHORT,      NULL, NULL, -1, false, false };
TypeInfo ty_uint        = { TY_UINT,        NULL, NULL, -1, false, false };
TypeInfo ty_ulong       = { TY_ULONG,       NULL, NULL, -1, false, false };

TypeInfo ty_uint8       = { TY_UINT8,       NULL, NULL, -1, false, false };
TypeInfo ty_uint16      = { TY_UINT16,      NULL, NULL, -1, false, false };
TypeInfo ty_uint32      = { TY_UINT32,      NULL, NULL, -1, false, false };
TypeInfo ty_uint64      = { TY_UINT64,      NULL, NULL, -1, false, false };

TypeInfo ty_size_t      = { TY_SIZE_T,      NULL, NULL, -1, false, false };
TypeInfo ty_ssize_t     = { TY_SSIZE_T,     NULL, NULL, -1, false, false };
TypeInfo ty_intptr      = { TY_INTPTR,      NULL, NULL, -1, false, false };
TypeInfo ty_uintptr     = { TY_UINTPTR,     NULL, NULL, -1, false, false };
TypeInfo ty_ptrdiff     = { TY_PTRDIFF,     NULL, NULL, -1, false, false };

Node *node_new(Arena *a, NodeKind kind, int line, int col) {
    Node *n = arena_alloc(a, sizeof(Node));
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->col  = col;
    return n;
}

void nlb_push(Arena *a, NodeListBuilder *b, Node *n) {
    NodeList *it = arena_alloc(a, sizeof(NodeList));
    it->node = n;
    it->next = NULL;
    if (b->tail) b->tail->next = it; else b->head = it;
    b->tail = it;
}

Param *param_new(Arena *a, const char *name, TypeInfo *type) {
    Param *p = arena_alloc(a, sizeof(Param));
    p->name = name; p->type = type; p->default_val = NULL; p->next = NULL;
    return p;
}

Field *field_new(Arena *a, const char *name, TypeInfo *type) {
    Field *f = arena_alloc(a, sizeof(Field));
    f->name = name; f->type = type; f->next = NULL;
    return f;
}

EnumVal *enum_val_new(Arena *a, const char *name, long long val, bool has_val) {
    EnumVal *ev = arena_alloc(a, sizeof(EnumVal));
    ev->name = name;
    ev->value = val;
    ev->has_value = has_val;
    ev->next = NULL;
    return ev;
}

TypeInfo *type_new(Arena *a, TypeKind kind) {
    TypeInfo *t = arena_alloc(a, sizeof(TypeInfo));
    t->kind = kind; t->elem_type = NULL; t->struct_name = NULL;
    t->array_size = -1;
    t->is_const = false;
    t->is_static = false;
    return t;
}

TypeInfo *type_array(Arena *a, TypeInfo *elem, int size) {
    TypeInfo *t = type_new(a, TY_ARRAY);
    t->elem_type = elem;
    t->array_size = size;
    return t;
}

TypeInfo *type_pointer(Arena *a, TypeInfo *elem) {
    TypeInfo *t = type_new(a, TY_POINTER);
    t->elem_type = elem;
    return t;
}

const char *binop_name(BinOp op) {
    switch (op) {
        case BOP_ADD: return "+";  case BOP_SUB: return "-";
        case BOP_MUL: return "*";  case BOP_DIV: return "/";
        case BOP_MOD: return "%";  case BOP_EQ:  return "==";
        case BOP_NE:  return "!="; case BOP_LT:  return "<";
        case BOP_LE:  return "<="; case BOP_GT:  return ">";
        case BOP_GE:  return ">="; case BOP_AND: return "&&";
        case BOP_OR:  return "||";
        case BOP_BIT_AND: return "&";
        case BOP_BIT_OR:  return "|";
        case BOP_BIT_XOR: return "^";
        case BOP_SHL:     return "<<";
        case BOP_SHR:     return ">>";
    }
    return "?";
}

const char *unaryop_name(UnaryOp op) {
    switch (op) {
        case UOP_NEG:     return "-";
        case UOP_NOT:     return "!";
        case UOP_BIT_NOT: return "~";
        case UOP_ADDR_OF: return "&";
        case UOP_DEREF:   return "*";
        case UOP_PRE_INC: return "++";
        case UOP_PRE_DEC: return "--";
    }
    return "?";
}

/* ─── Format strings ───────────────────────────────────────────────────── */
char *fmt_rewrite(Arena *a, const char *fmt, char *kinds, int max_kinds,
                  int *count, const char **err) {
    size_t n = strlen(fmt);
    char  *out = arena_alloc(a, n * 3 + 1);   /* a spec grows by at most 2x */
    size_t o = 0;
    int    k = 0;
    *err = NULL;

    for (const char *p = fmt; *p; ) {
        if (*p != '%') { out[o++] = *p++; continue; }
        out[o++] = *p++;
        if (*p == '%') { out[o++] = *p++; continue; }

        while (*p && strchr("-+ #0", *p)) out[o++] = *p++;     /* flags     */
        while (isdigit((unsigned char)*p)) out[o++] = *p++;    /* width     */
        if (*p == '.') {                                       /* precision */
            out[o++] = *p++;
            while (isdigit((unsigned char)*p)) out[o++] = *p++;
        }
        while (*p && strchr("hlLqjzt", *p)) p++;               /* dropped   */

        char c = *p;
        if (!c) { *err = "incomplete conversion at end of format string"; return NULL; }
        p++;

        char kind;
        switch (c) {
            case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
                kind = 'i'; out[o++] = 'l'; out[o++] = 'l'; out[o++] = c; break;
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                kind = 'f'; out[o++] = c; break;
            case 's': kind = 's'; out[o++] = c; break;
            case 'c': kind = 'c'; out[o++] = c; break;
            default:
                *err = (c == 'n') ? "%n conversions are not allowed"
                                  : "unsupported conversion specifier";
                return NULL;
        }
        if (k >= max_kinds) { *err = "too many conversions in format string"; return NULL; }
        kinds[k++] = kind;
    }
    out[o] = '\0';
    *count = k;
    return out;
}
