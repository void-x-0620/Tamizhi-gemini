/*
 * codegen.h — Tamizhi -> C99 back end.
 *
 * The compiler emits one self-contained C translation unit and lets the host
 * C compiler (cc/gcc/clang) do instruction selection, register allocation and
 * machine-level optimisation. That gives native-speed output with no
 * interpreter and no LLVM dependency.
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

typedef struct DeferFrame {
    Node              *defers[64];
    int                count;
    struct DeferFrame *prev;
} DeferFrame;

typedef struct {
    StrBuf      out;
    int         indent;
    Arena      *arena;
    const char *filename;
    bool        gc;          /* in:  use the Boehm collector for heap data     */
    bool        uses_heap;   /* out: the program allocates (needs -lgc if gc)  */
    Node       *program;
    DeferFrame *defer_frame;
    int         unique_id;
} CodeGen;

void codegen_init(CodeGen *cg, Arena *arena, const char *filename);
void codegen_generate(CodeGen *cg, Node *program);
void codegen_write(CodeGen *cg, FILE *fp);
void codegen_free(CodeGen *cg);

#endif /* CODEGEN_H */
