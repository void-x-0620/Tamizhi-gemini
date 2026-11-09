/*
 * sema.h — Semantic analysis.
 *
 * One walk over the AST that
 *   • builds a scoped symbol table (hash buckets per scope, arena-allocated),
 *   • resolves identifiers, types, classes, structs, enums and unions,
 *   • annotates every expression node with its type,
 *   • reports type errors, const violations, and memory-safety errors.
 */
#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

typedef enum {
    SYM_VAR, SYM_FUNC, SYM_STRUCT, SYM_CLASS, SYM_ENUM, SYM_UNION
} SymKind;

typedef struct Symbol {
    const char    *name;
    SymKind        kind;
    TypeInfo      *type;     /* variable type / function return type / enum type */
    Param         *params;   /* SYM_FUNC */
    Field         *fields;   /* SYM_STRUCT, SYM_CLASS, SYM_UNION */
    NodeList      *methods;  /* SYM_CLASS, SYM_STRUCT */
    EnumVal       *enum_vals;/* SYM_ENUM */
    const char    *base_name;/* SYM_CLASS (inheritance) */
    const char    *cname;    /* SYM_FUNC from a C binding, else NULL */
    long long      int_val;   /* for enum constants */
    bool           is_const;
    bool           is_proto;  /* SYM_FUNC: declared as forward prototype without body */
    bool           has_body;  /* SYM_FUNC: defined with a body */
    struct Symbol *next;
} Symbol;

#define SCOPE_BUCKETS 64

typedef struct Scope {
    Symbol       *buckets[SCOPE_BUCKETS];
    struct Scope *parent;
} Scope;

typedef struct {
    Scope      *scope;
    Arena      *arena;
    const char *filename;
    TypeInfo   *current_ret;   /* return type of the function being checked */
    TypeInfo   *current_class; /* type of enclosing class/struct (for `this`) */
    bool        has_main;
} Sema;

void sema_init(Sema *s, Arena *arena, const char *filename);
void sema_analyse(Sema *s, Node *program);

#endif /* SEMA_H */
