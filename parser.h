/*
 * parser.h — Recursive-descent parser for Tamizhi.
 *
 * Supports:
 *   - Object-Oriented Programming: classes (vaguppu/class), structs (kattamaipu/struct)
 *     with methods, constructors (uruvakku/init), inheritance (: Base), this/suya, new/pudhiya
 *   - C types: char, short, int, long, float, double, unsigned types, exact-width types
 *     (int32_t, uint64_t, etc.), pointers (T*), arrays (T[N])
 *   - C features: enum (enumaripu/enum), union (ondrippu/union), typedef
 *   - Storage qualifiers: const (maradha), static (nilaiyana)
 *   - Operators: bitwise (&, |, ^, ~, <<, >>), compound assignment (+=, -=, etc.),
 *     increment/decrement (++, --), ternary (? :), pointer operators (*, &, ->)
 *   - Control flow: if/else, while, for (padigal/for), do-while (sei/do),
 *     switch/case/default (therivu/nilai/muthal)
 *   - C bindings (veli) and module imports (serku)
 */
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

/* Files already pulled in by 'serku' (each file is included at most once). */
#define MAX_IMPORTS 64
#define MAX_IMPORT_DEPTH 16
typedef struct {
    const char *paths[MAX_IMPORTS];
    int         count;
    int         depth;
} ImportSet;

typedef struct TypedefEntry {
    const char          *name;
    TypeInfo            *type;
    struct TypedefEntry *next;
} TypedefEntry;

typedef struct {
    Lexer        *lexer;
    Arena        *arena;
    bool          panic;
    int           loop_depth;
    int           func_depth;
    size_t        consumed;     /* tokens consumed so far; used to prove progress */
    bool          binding;      /* parsing a binding file: only veli/serku allowed */
    bool          saw_header;   /* a 'veli "x.h"' came before: prototypes trusted */
    ImportSet    *imports;      /* shared by the main file and everything it imports */
    TypedefEntry *typedefs;
    const char  **inc_dirs;
    size_t        inc_count;
} Parser;

void  parser_init(Parser *p, Lexer *L, Arena *arena);
void  parser_set_include_dirs(Parser *p, const char **dirs, size_t count);
Node *parse_program(Parser *p);

#endif /* PARSER_H */
