/*
 * codegen.c — AST -> C99 (see codegen.h for the optimisation list).
 */
#include "codegen.h"
#include <limits.h>

static void gen_stmt(CodeGen *cg, Node *n);
static void gen_expr(CodeGen *cg, Node *n);
static void gen_to_string(CodeGen *cg, Node *n);

/* ─── Output helpers ──────────────────────────────────────────────────── */
static void emit(CodeGen *cg, const char *s) { strbuf_append(&cg->out, s); }
static void emit_n(CodeGen *cg, const char *s, size_t n) {
    strbuf_append_n(&cg->out, s, n);
}
#define emitf(cg, ...) strbuf_appendf(&(cg)->out, __VA_ARGS__)

static void indent(CodeGen *cg) {
    for (int i = 0; i < cg->indent; i++) emit(cg, "    ");
}

/* All user identifiers get a tz_ prefix, except the entry point. */
static void emit_ident(CodeGen *cg, const char *name) {
    if (!strcmp(name, "main")) emit(cg, "main");
    else if (!strcmp(name, "this") || !strcmp(name, "suya")) emit(cg, "tz_this");
    else emitf(cg, "tz_%s", name);
}

/* Emit a C string literal. */
static void emit_c_string(CodeGen *cg, const char *s) {
    emit(cg, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '\n': emit(cg, "\\n");  break;
            case '\t': emit(cg, "\\t");  break;
            case '\r': emit(cg, "\\r");  break;
            case '\\': emit(cg, "\\\\"); break;
            case '"':  emit(cg, "\\\""); break;
            case '?':  emit(cg, "\\?");  break;
            default:
                if (*p < 0x20 || *p == 0x7f) emitf(cg, "\\%03o", *p);
                else emit_n(cg, (const char *)p, 1);
        }
    }
    emit(cg, "\"");
}

/* ─── Program search helpers ──────────────────────────────────────────── */
static Node *find_decl(CodeGen *cg, const char *name) {
    if (!cg->program) return NULL;
    for (NodeList *it = cg->program->program.decls; it; it = it->next) {
        Node *d = it->node;
        if (d->kind == NK_CLASS_DECL && !strcmp(d->class_decl.name, name)) return d;
        if (d->kind == NK_STRUCT_DECL && !strcmp(d->struct_decl.name, name)) return d;
        if (d->kind == NK_UNION_DECL && !strcmp(d->union_decl.name, name)) return d;
        if (d->kind == NK_ENUM_DECL && !strcmp(d->enum_decl.name, name)) return d;
    }
    return NULL;
}

static bool is_class_or_struct(CodeGen *cg, const char *name) {
    Node *d = find_decl(cg, name);
    return d && (d->kind == NK_CLASS_DECL || d->kind == NK_STRUCT_DECL);
}

static const char *find_ctor(CodeGen *cg, const char *name) {
    Node *d = find_decl(cg, name);
    if (!d) return NULL;
    NodeList *methods = (d->kind == NK_CLASS_DECL) ? d->class_decl.methods
                                                   : d->struct_decl.methods;
    for (NodeList *m = methods; m; m = m->next) {
        const char *mn = m->node->func_decl.name;
        if (!strcmp(mn, "uruvakku") || !strcmp(mn, "init") ||
            !strcmp(mn, "constructor") || !strcmp(mn, name))
            return mn;
    }
    return NULL;
}

static const char *find_method_owner(CodeGen *cg, const char *class_name, const char *method_name) {
    Node *d = find_decl(cg, class_name);
    while (d) {
        NodeList *methods = (d->kind == NK_CLASS_DECL) ? d->class_decl.methods
                                                       : d->struct_decl.methods;
        for (NodeList *m = methods; m; m = m->next) {
            if (!strcmp(m->node->func_decl.name, method_name))
                return (d->kind == NK_CLASS_DECL) ? d->class_decl.name : d->struct_decl.name;
        }
        if (d->kind == NK_CLASS_DECL && d->class_decl.base_name)
            d = find_decl(cg, d->class_decl.base_name);
        else
            break;
    }
    return NULL;
}

/* ─── Constant folding and peepholes ──────────────────────────────────── */
static bool is_int_class(Node *n) {
    return n->kind == NK_INT_LIT || n->kind == NK_BOOL_LIT;
}
static bool is_num_lit(Node *n) {
    return is_int_class(n) || n->kind == NK_FLOAT_LIT;
}
static long long ival(Node *n) {
    return n->kind == NK_BOOL_LIT ? (n->bool_lit.value ? 1 : 0)
                                  : n->int_lit.value;
}
static double fval(Node *n) {
    return n->kind == NK_FLOAT_LIT ? n->float_lit.value : (double)ival(n);
}

static bool is_pure(Node *n) {
    switch (n->kind) {
        case NK_INT_LIT: case NK_FLOAT_LIT: case NK_STRING_LIT:
        case NK_BOOL_LIT: case NK_IDENT: case NK_THIS:
            return true;
        case NK_BINARY: return is_pure(n->binary.left) && is_pure(n->binary.right);
        case NK_UNARY:  return is_pure(n->unary.operand);
        case NK_INDEX:  return is_pure(n->index.array) && is_pure(n->index.index);
        case NK_MEMBER: return is_pure(n->member.object);
        case NK_ARROW:  return is_pure(n->arrow.object);
        default:        return false;
    }
}

static Node *mk_int(CodeGen *cg, Node *at, long long v) {
    Node *n = node_new(cg->arena, NK_INT_LIT, at->line, at->col);
    n->int_lit.value = v;
    n->type = &ty_int;
    return n;
}
static Node *mk_bool(CodeGen *cg, Node *at, bool v) {
    Node *n = node_new(cg->arena, NK_BOOL_LIT, at->line, at->col);
    n->bool_lit.value = v;
    n->type = &ty_bool;
    return n;
}
static Node *mk_float(CodeGen *cg, Node *at, double v) {
    Node *n = node_new(cg->arena, NK_FLOAT_LIT, at->line, at->col);
    n->float_lit.value = v;
    n->type = &ty_float;
    return n;
}

static Node *fold_binary(CodeGen *cg, Node *n) {
    Node *L = n->binary.left, *R = n->binary.right;
    BinOp op = n->binary.op;

    if (op == BOP_AND && is_num_lit(L) && fval(L) == 0.0) return mk_bool(cg, n, false);
    if (op == BOP_OR  && is_num_lit(L) && fval(L) != 0.0) return mk_bool(cg, n, true);

    if (!is_num_lit(L) || !is_num_lit(R)) return NULL;

    if (is_int_class(L) && is_int_class(R)) {
        long long a = ival(L), b = ival(R), r;
        switch (op) {
            case BOP_ADD: if (__builtin_add_overflow(a, b, &r)) return NULL;
                          return mk_int(cg, n, r);
            case BOP_SUB: if (__builtin_sub_overflow(a, b, &r)) return NULL;
                          return mk_int(cg, n, r);
            case BOP_MUL: if (__builtin_mul_overflow(a, b, &r)) return NULL;
                          return mk_int(cg, n, r);
            case BOP_DIV: if (b == 0 || (a == LLONG_MIN && b == -1)) return NULL;
                          return mk_int(cg, n, a / b);
            case BOP_MOD: if (b == 0 || (a == LLONG_MIN && b == -1)) return NULL;
                          return mk_int(cg, n, a % b);
            case BOP_EQ:  return mk_bool(cg, n, a == b);
            case BOP_NE:  return mk_bool(cg, n, a != b);
            case BOP_LT:  return mk_bool(cg, n, a <  b);
            case BOP_LE:  return mk_bool(cg, n, a <= b);
            case BOP_GT:  return mk_bool(cg, n, a >  b);
            case BOP_GE:  return mk_bool(cg, n, a >= b);
            case BOP_AND: return mk_bool(cg, n, a && b);
            case BOP_OR:  return mk_bool(cg, n, a || b);
            case BOP_BIT_AND: return mk_int(cg, n, a & b);
            case BOP_BIT_OR:  return mk_int(cg, n, a | b);
            case BOP_BIT_XOR: return mk_int(cg, n, a ^ b);
            case BOP_SHL:     return mk_int(cg, n, a << b);
            case BOP_SHR:     return mk_int(cg, n, a >> b);
        }
        return NULL;
    }

    double a = fval(L), b = fval(R);
    switch (op) {
        case BOP_ADD: return mk_float(cg, n, a + b);
        case BOP_SUB: return mk_float(cg, n, a - b);
        case BOP_MUL: return mk_float(cg, n, a * b);
        case BOP_DIV: return b == 0.0 ? NULL : mk_float(cg, n, a / b);
        case BOP_EQ:  return mk_bool(cg, n, a == b);
        case BOP_NE:  return mk_bool(cg, n, a != b);
        case BOP_LT:  return mk_bool(cg, n, a <  b);
        case BOP_LE:  return mk_bool(cg, n, a <= b);
        case BOP_GT:  return mk_bool(cg, n, a >  b);
        case BOP_GE:  return mk_bool(cg, n, a >= b);
        case BOP_AND: return mk_bool(cg, n, a != 0.0 && b != 0.0);
        case BOP_OR:  return mk_bool(cg, n, a != 0.0 || b != 0.0);
        default:      return NULL;
    }
}

static bool lit_is_int(Node *n, long long v) {
    return n->kind == NK_INT_LIT && n->int_lit.value == v;
}

static Node *peephole(Node *n) {
    Node *L = n->binary.left, *R = n->binary.right;
    switch (n->binary.op) {
        case BOP_ADD:
            if (lit_is_int(R, 0)) return L;
            if (lit_is_int(L, 0)) return R;
            break;
        case BOP_SUB:
            if (lit_is_int(R, 0)) return L;
            break;
        case BOP_MUL:
            if (lit_is_int(R, 1)) return L;
            if (lit_is_int(L, 1)) return R;
            if (lit_is_int(R, 0) && is_pure(L) && L->type &&
                L->type->kind == TY_INT) return R;
            if (lit_is_int(L, 0) && is_pure(R) && R->type &&
                R->type->kind == TY_INT) return L;
            break;
        case BOP_DIV:
            if (lit_is_int(R, 1)) return L;
            break;
        default: break;
    }
    return n;
}

static Node *simplify(CodeGen *cg, Node *n) {
    if (!n) return NULL;
    switch (n->kind) {
        case NK_BINARY: {
            n->binary.left  = simplify(cg, n->binary.left);
            n->binary.right = simplify(cg, n->binary.right);
            Node *f = fold_binary(cg, n);
            return f ? f : peephole(n);
        }
        case NK_UNARY: {
            Node *o = n->unary.operand = simplify(cg, n->unary.operand);
            if (n->unary.op == UOP_NOT) {
                if (is_num_lit(o)) return mk_bool(cg, n, fval(o) == 0.0);
                if (o->kind == NK_UNARY && o->unary.op == UOP_NOT &&
                    o->unary.operand->type &&
                    o->unary.operand->type->kind == TY_BOOL)
                    return o->unary.operand;
            } else if (n->unary.op == UOP_BIT_NOT) {
                if (o->kind == NK_INT_LIT) return mk_int(cg, n, ~o->int_lit.value);
            } else if (n->unary.op == UOP_NEG) {
                if (o->kind == NK_INT_LIT && o->int_lit.value != LLONG_MIN)
                    return mk_int(cg, n, -o->int_lit.value);
                if (o->kind == NK_FLOAT_LIT)
                    return mk_float(cg, n, -o->float_lit.value);
            }
            return n;
        }
        case NK_ASSIGN:
            n->assign.value  = simplify(cg, n->assign.value);
            n->assign.target = simplify(cg, n->assign.target);
            return n;
        case NK_COMPOUND_ASSIGN:
            n->compound_assign.value  = simplify(cg, n->compound_assign.value);
            n->compound_assign.target = simplify(cg, n->compound_assign.target);
            return n;
        case NK_CALL:
            for (NodeList *a = n->call.args; a; a = a->next)
                a->node = simplify(cg, a->node);
            return n;
        case NK_METHOD_CALL:
            n->method_call.object = simplify(cg, n->method_call.object);
            for (NodeList *a = n->method_call.args; a; a = a->next)
                a->node = simplify(cg, a->node);
            return n;
        case NK_INDEX:
            n->index.array = simplify(cg, n->index.array);
            n->index.index = simplify(cg, n->index.index);
            return n;
        case NK_MEMBER:
            n->member.object = simplify(cg, n->member.object);
            return n;
        case NK_ARROW:
            n->arrow.object = simplify(cg, n->arrow.object);
            return n;
        case NK_TERNARY:
            n->ternary.cond      = simplify(cg, n->ternary.cond);
            n->ternary.then_expr = simplify(cg, n->ternary.then_expr);
            n->ternary.else_expr = simplify(cg, n->ternary.else_expr);
            return n;
        case NK_NULL_COALESCE:
            n->binary.left  = simplify(cg, n->binary.left);
            n->binary.right = simplify(cg, n->binary.right);
            return n;
        case NK_RANGE:
            n->range.start = simplify(cg, n->range.start);
            n->range.end   = simplify(cg, n->range.end);
            return n;
        case NK_STRING_INTERP:
            for (NodeList *pt = n->string_interp.parts; pt; pt = pt->next)
                pt->node = simplify(cg, pt->node);
            return n;
        default:
            return n;
    }
}

/* ─── Types and declarations ──────────────────────────────────────────── */
static void gen_base_type(CodeGen *cg, TypeInfo *t) {
    if (!t) { emit(cg, "long long"); return; }
    if (t->is_const) emit(cg, "const ");
    switch (t->kind) {
        case TY_INT:         emit(cg, "long long");          break;
        case TY_FLOAT:       emit(cg, "double");             break;
        case TY_DOUBLE:      emit(cg, "double");             break;
        case TY_LONG_DOUBLE: emit(cg, "long double");        break;
        case TY_CHAR:        emit(cg, "char");               break;
        case TY_SHORT:       emit(cg, "short");              break;
        case TY_LONG:        emit(cg, "long long");          break;
        case TY_INT8:        emit(cg, "int8_t");             break;
        case TY_INT16:       emit(cg, "int16_t");            break;
        case TY_INT32:       emit(cg, "int32_t");            break;
        case TY_INT64:       emit(cg, "int64_t");            break;
        case TY_UCHAR:       emit(cg, "unsigned char");      break;
        case TY_USHORT:      emit(cg, "unsigned short");     break;
        case TY_UINT:        emit(cg, "unsigned int");       break;
        case TY_ULONG:       emit(cg, "unsigned long long"); break;
        case TY_UINT8:       emit(cg, "uint8_t");            break;
        case TY_UINT16:      emit(cg, "uint16_t");           break;
        case TY_UINT32:      emit(cg, "uint32_t");           break;
        case TY_UINT64:      emit(cg, "uint64_t");           break;
        case TY_SIZE_T:      emit(cg, "size_t");             break;
        case TY_SSIZE_T:     emit(cg, "ssize_t");            break;
        case TY_INTPTR:      emit(cg, "intptr_t");           break;
        case TY_UINTPTR:     emit(cg, "uintptr_t");          break;
        case TY_PTRDIFF:     emit(cg, "ptrdiff_t");          break;
        case TY_STRING:      emit(cg, "const char *");       break;
        case TY_BOOL:        emit(cg, "int");                break;
        case TY_VOID:        emit(cg, "void");               break;
        case TY_POINTER:
            gen_base_type(cg, t->elem_type);
            emit(cg, " *");
            break;
        case TY_STRUCT:
        case TY_CLASS:
        case TY_UNION:
        case TY_ENUM:        emit_ident(cg, t->struct_name); break;
        case TY_ARRAY:       gen_base_type(cg, t->elem_type); break;
        default:             emit(cg, "long long");          break;
    }
}

static void gen_decl(CodeGen *cg, TypeInfo *t, const char *name,
                     bool as_param) {
    if (t && t->is_static) emit(cg, "static ");
    gen_base_type(cg, t);
    emit(cg, " ");
    if (t && t->kind == TY_ARRAY) {
        if (!as_param && t->array_size > 0) {
            emit_ident(cg, name);
            emitf(cg, "[%d]", t->array_size);
        } else {
            emit(cg, "*");
            emit_ident(cg, name);
        }
    } else {
        emit_ident(cg, name);
    }
}

static void gen_zero_init(CodeGen *cg, TypeInfo *t) {
    switch (t ? t->kind : TY_UNKNOWN) {
        case TY_FLOAT:
        case TY_DOUBLE:
        case TY_LONG_DOUBLE: emit(cg, "0.0"); break;
        case TY_STRING:      emit(cg, "\"\""); break;
        case TY_POINTER:     emit(cg, "NULL"); break;
        case TY_ARRAY:
        case TY_STRUCT:
        case TY_CLASS:
        case TY_UNION:       emit(cg, "{0}"); break;
        default:             emit(cg, "0");   break;
    }
}

/* ─── Expressions ─────────────────────────────────────────────────────── */
static void gen_float(CodeGen *cg, double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.17g", v);
    emit(cg, buf);
    if (!strpbrk(buf, ".eEnN")) emit(cg, ".0");
}

static bool is_string_type(Node *n) {
    return n->type && n->type->kind == TY_STRING;
}

static void gen_expr(CodeGen *cg, Node *n) {
    if (!n) return;
    switch (n->kind) {
        case NK_INT_LIT:
            if (n->int_lit.value == LLONG_MIN) emit(cg, "(-9223372036854775807LL-1)");
            else emitf(cg, "%lld", n->int_lit.value);
            break;
        case NK_FLOAT_LIT:  gen_float(cg, n->float_lit.value); break;
        case NK_STRING_LIT: emit_c_string(cg, n->string_lit.value); break;
        case NK_BOOL_LIT:   emit(cg, n->bool_lit.value ? "1" : "0"); break;
        case NK_CHAR_LIT:   emitf(cg, "'\\x%02x'", (unsigned char)n->char_lit.value); break;
        case NK_NULL_LIT:   emit(cg, "NULL"); break;
        case NK_THIS:
        case NK_SUPER:      emit(cg, "tz_this"); break;
        case NK_SIZEOF:
            emit(cg, "sizeof(");
            if (n->sizeof_expr.target_type) gen_base_type(cg, n->sizeof_expr.target_type);
            else gen_expr(cg, n->sizeof_expr.expr);
            emit(cg, ")");
            break;
        case NK_CAST:
            emit(cg, "((");
            gen_base_type(cg, n->cast_expr.target_type);
            emit(cg, ")(");
            gen_expr(cg, n->cast_expr.expr);
            emit(cg, "))");
            break;
        case NK_IDENT:      emit_ident(cg, n->ident.name); break;

        case NK_NEW: {
            cg->uses_heap = true;
            const char *cn = n->new_expr.class_name;
            emitf(cg, "({ tz_%s *_p = (tz_%s *)tz_alloc(sizeof(tz_%s)); memset(_p, 0, sizeof(tz_%s)); ",
                  cn, cn, cn, cn);
            const char *ctor = find_ctor(cg, cn);
            if (ctor) {
                emitf(cg, "tz_%s_%s(_p", cn, ctor);
                for (NodeList *a = n->new_expr.args; a; a = a->next) {
                    emit(cg, ", ");
                    gen_expr(cg, a->node);
                }
                emit(cg, "); ");
            }
            emit(cg, "_p; })");
            break;
        }

        case NK_ASSIGN:
            emit(cg, "(");
            gen_expr(cg, n->assign.target);
            emit(cg, " = ");
            gen_expr(cg, n->assign.value);
            emit(cg, ")");
            break;

        case NK_COMPOUND_ASSIGN:
            if (is_string_type(n->compound_assign.target) && n->compound_assign.op == BOP_ADD) {
                cg->uses_heap = true;
                emit(cg, "(");
                gen_expr(cg, n->compound_assign.target);
                emit(cg, " = tz_concat(");
                gen_expr(cg, n->compound_assign.target);
                emit(cg, ", ");
                gen_expr(cg, n->compound_assign.value);
                emit(cg, "))");
                break;
            }
            emit(cg, "(");
            gen_expr(cg, n->compound_assign.target);
            emitf(cg, " %s= ", binop_name(n->compound_assign.op));
            gen_expr(cg, n->compound_assign.value);
            emit(cg, ")");
            break;

        case NK_POST_INC:
            emit(cg, "(");
            gen_expr(cg, n->inc_dec.target);
            emit(cg, "++)");
            break;

        case NK_POST_DEC:
            emit(cg, "(");
            gen_expr(cg, n->inc_dec.target);
            emit(cg, "--)");
            break;

        case NK_TERNARY:
            emit(cg, "(");
            gen_expr(cg, n->ternary.cond);
            emit(cg, " ? ");
            gen_expr(cg, n->ternary.then_expr);
            emit(cg, " : ");
            gen_expr(cg, n->ternary.else_expr);
            emit(cg, ")");
            break;

        case NK_BINARY: {
            Node *L = n->binary.left, *R = n->binary.right;
            if (is_string_type(L) && is_string_type(R) && n->binary.op == BOP_ADD) {
                cg->uses_heap = true;
                emit(cg, "tz_concat(");
                gen_expr(cg, L); emit(cg, ", "); gen_expr(cg, R);
                emit(cg, ")");
                break;
            }
            if (is_string_type(L) && is_string_type(R)) {
                cg->uses_heap = true;
                emit(cg, "(tz_str_cmp(");
                gen_expr(cg, L); emit(cg, ", "); gen_expr(cg, R);
                emitf(cg, ") %s 0)", binop_name(n->binary.op));
                break;
            }
            emit(cg, "(");
            gen_expr(cg, L);
            emitf(cg, " %s ", binop_name(n->binary.op));
            gen_expr(cg, R);
            emit(cg, ")");
            break;
        }

        case NK_UNARY:
            if (n->unary.op == UOP_ADDR_OF) {
                emit(cg, "(&(");
                gen_expr(cg, n->unary.operand);
                emit(cg, "))");
            } else if (n->unary.op == UOP_DEREF) {
                emit(cg, "(*(");
                gen_expr(cg, n->unary.operand);
                emit(cg, "))");
            } else if (n->unary.op == UOP_PRE_INC) {
                emit(cg, "(++(");
                gen_expr(cg, n->unary.operand);
                emit(cg, "))");
            } else if (n->unary.op == UOP_PRE_DEC) {
                emit(cg, "(--(");
                gen_expr(cg, n->unary.operand);
                emit(cg, "))");
            } else {
                emitf(cg, "%s(", unaryop_name(n->unary.op));
                gen_expr(cg, n->unary.operand);
                emit(cg, ")");
            }
            break;

        case NK_CALL:
            if (is_class_or_struct(cg, n->call.name)) {
                emitf(cg, "({ tz_%s _tmp = {0}; ", n->call.name);
                const char *ctor = find_ctor(cg, n->call.name);
                if (ctor) {
                    emitf(cg, "tz_%s_%s(&_tmp", n->call.name, ctor);
                    for (NodeList *a = n->call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, "); ");
                }
                emit(cg, "_tmp; })");
            } else {
                if (n->call.cname) {
                    if (!strncmp(n->call.cname, "tz_str_", 7) ||
                        !strncmp(n->call.cname, "tz_to_str_", 10) ||
                        !strncmp(n->call.cname, "tz_file_", 8)) cg->uses_heap = true;
                    emit(cg, n->call.cname);
                } else {
                    emit_ident(cg, n->call.name);
                }
                emit(cg, "(");
                for (NodeList *a = n->call.args; a; a = a->next) {
                    gen_expr(cg, a->node);
                    if (a->next) emit(cg, ", ");
                }
                emit(cg, ")");
            }
            break;

        case NK_METHOD_CALL: {
            TypeInfo *ot = n->method_call.object->type;
            if (ot && ot->kind == TY_ARRAY) {
                if (!strcmp(n->method_call.method, "length") || !strcmp(n->method_call.method, "alavu")) {
                    if (ot->array_size > 0) {
                        emitf(cg, "%d", ot->array_size);
                    } else {
                        emit(cg, "(int)(sizeof(");
                        gen_expr(cg, n->method_call.object);
                        emit(cg, ") / sizeof((");
                        gen_expr(cg, n->method_call.object);
                        emit(cg, ")[0]))");
                    }
                    break;
                }
            }
            if (ot && ot->kind == TY_STRING) {
                cg->uses_heap = true;
                if (!strcmp(n->method_call.method, "length") || !strcmp(n->method_call.method, "alavu")) {
                    emit(cg, "tz_str_len(");
                    gen_expr(cg, n->method_call.object);
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "sub") || !strcmp(n->method_call.method, "substring")) {
                    emit(cg, "tz_str_sub(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "find")) {
                    emit(cg, "tz_str_find(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "contains") || !strcmp(n->method_call.method, "adangiullathu")) {
                    emit(cg, "tz_str_contains(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "starts_with") || !strcmp(n->method_call.method, "thodangugiradhu")) {
                    emit(cg, "tz_str_starts_with(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "ends_with") || !strcmp(n->method_call.method, "mudigiradhu")) {
                    emit(cg, "tz_str_ends_with(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "to_uppercase") || !strcmp(n->method_call.method, "periya_ezhuthu") ||
                    !strcmp(n->method_call.method, "upper")) {
                    emit(cg, "tz_str_upper(");
                    gen_expr(cg, n->method_call.object);
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "to_lowercase") || !strcmp(n->method_call.method, "siriya_ezhuthu") ||
                    !strcmp(n->method_call.method, "lower")) {
                    emit(cg, "tz_str_lower(");
                    gen_expr(cg, n->method_call.object);
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "trim") || !strcmp(n->method_call.method, "vetru")) {
                    emit(cg, "tz_str_trim(");
                    gen_expr(cg, n->method_call.object);
                    emit(cg, ")");
                    break;
                }
                if (!strcmp(n->method_call.method, "replace") || !strcmp(n->method_call.method, "matru")) {
                    emit(cg, "tz_str_replace(");
                    gen_expr(cg, n->method_call.object);
                    for (NodeList *a = n->method_call.args; a; a = a->next) {
                        emit(cg, ", ");
                        gen_expr(cg, a->node);
                    }
                    emit(cg, ")");
                    break;
                }
            }
            const char *cname = ot ? ot->struct_name : NULL;
            if (ot && ot->kind == TY_POINTER && ot->elem_type)
                cname = ot->elem_type->struct_name;
            if (!cname) cname = "Object";

            const char *owner = find_method_owner(cg, cname, n->method_call.method);
            if (!owner) owner = cname;

            emitf(cg, "tz_%s_%s(", owner, n->method_call.method);
            bool is_ptr = n->method_call.is_arrow || (ot && ot->kind == TY_POINTER);
            if (n->method_call.object->kind == NK_SUPER || strcmp(owner, cname) != 0) {
                emitf(cg, "(tz_%s *)", owner);
            }
            if (is_ptr) {
                gen_expr(cg, n->method_call.object);
            } else {
                emit(cg, "&(");
                gen_expr(cg, n->method_call.object);
                emit(cg, ")");
            }
            for (NodeList *a = n->method_call.args; a; a = a->next) {
                emit(cg, ", ");
                gen_expr(cg, a->node);
            }
            emit(cg, ")");
            break;
        }

        case NK_INDEX:
            gen_expr(cg, n->index.array);
            emit(cg, "[");
            gen_expr(cg, n->index.index);
            emit(cg, "]");
            break;

        case NK_MEMBER:
            gen_expr(cg, n->member.object);
            if (n->member.object->type && n->member.object->type->kind == TY_POINTER)
                emit(cg, "->");
            else
                emit(cg, ".");
            emit_ident(cg, n->member.field);
            break;

        case NK_ARROW:
            gen_expr(cg, n->arrow.object);
            emit(cg, "->");
            emit_ident(cg, n->arrow.field);
            break;

        case NK_NULL_COALESCE: {
            emit(cg, "({ __typeof__(");
            gen_expr(cg, n->binary.left);
            emit(cg, ") _nc = (");
            gen_expr(cg, n->binary.left);
            emit(cg, "); _nc != NULL ? _nc : (");
            gen_expr(cg, n->binary.right);
            emit(cg, "); })");
            break;
        }

        case NK_STRING_INTERP: {
            cg->uses_heap = true;
            NodeList *pt = n->string_interp.parts;
            if (!pt) {
                emit(cg, "\"\"");
                break;
            }
            if (!pt->next) {
                gen_to_string(cg, pt->node);
                break;
            }
            int count = 0;
            for (NodeList *cur = pt; cur; cur = cur->next) count++;
            for (int i = 0; i < count - 1; i++) emit(cg, "tz_concat(");
            gen_to_string(cg, pt->node);
            for (NodeList *cur = pt->next; cur; cur = cur->next) {
                emit(cg, ", ");
                gen_to_string(cg, cur->node);
                emit(cg, ")");
            }
            break;
        }

        default:
            emit(cg, "0 /* unsupported */");
            break;
    }
}

static void gen_to_string(CodeGen *cg, Node *n) {
    TypeInfo *t = n->type;
    if (t && t->kind == TY_STRING) {
        gen_expr(cg, n);
    } else if (t && (t->kind == TY_FLOAT || t->kind == TY_DOUBLE)) {
        emit(cg, "tz_to_str_float(");
        gen_expr(cg, n);
        emit(cg, ")");
    } else if (t && t->kind == TY_BOOL) {
        emit(cg, "tz_to_str_bool(");
        gen_expr(cg, n);
        emit(cg, ")");
    } else if (t && t->kind == TY_CHAR) {
        emit(cg, "tz_to_str_char(");
        gen_expr(cg, n);
        emit(cg, ")");
    } else {
        emit(cg, "tz_to_str_int(");
        gen_expr(cg, n);
        emit(cg, ")");
    }
}

static void gen_expr_stmt(CodeGen *cg, Node *e) {
    if (e->kind == NK_ASSIGN) {
        gen_expr(cg, e->assign.target);
        emit(cg, " = ");
        gen_expr(cg, e->assign.value);
    } else if (e->kind == NK_COMPOUND_ASSIGN) {
        if (is_string_type(e->compound_assign.target) && e->compound_assign.op == BOP_ADD) {
            cg->uses_heap = true;
            gen_expr(cg, e->compound_assign.target);
            emit(cg, " = tz_concat(");
            gen_expr(cg, e->compound_assign.target);
            emit(cg, ", ");
            gen_expr(cg, e->compound_assign.value);
            emit(cg, ")");
            return;
        }
        gen_expr(cg, e->compound_assign.target);
        emitf(cg, " %s= ", binop_name(e->compound_assign.op));
        gen_expr(cg, e->compound_assign.value);
    } else {
        gen_expr(cg, e);
    }
}

/* ─── Statements ──────────────────────────────────────────────────────── */
static bool terminates(Node *n) {
    return n->kind == NK_RETURN || n->kind == NK_BREAK ||
           n->kind == NK_CONTINUE;
}

static void gen_block(CodeGen *cg, Node *blk) {
    emit(cg, "{\n");
    cg->indent++;
    DeferFrame frame;
    frame.count = 0;
    frame.prev = cg->defer_frame;
    cg->defer_frame = &frame;

    bool term = false;
    for (NodeList *it = blk->block.stmts; it; it = it->next) {
        gen_stmt(cg, it->node);
        if (terminates(it->node)) { term = true; break; }
    }
    if (!term) {
        for (int i = frame.count - 1; i >= 0; i--) {
            gen_stmt(cg, frame.defers[i]);
        }
    }
    cg->defer_frame = frame.prev;
    cg->indent--;
    indent(cg);
    emit(cg, "}");
}

static void gen_var_decl(CodeGen *cg, Node *n) {
    TypeInfo *vt = n->var_decl.var_type ? n->var_decl.var_type : n->type;
    Node *init = n->var_decl.init;
    if (n->type && n->type->kind == TY_ARRAY) vt = n->type;

    indent(cg);
    if (n->var_decl.is_static) emit(cg, "static ");
    if (n->var_decl.is_const)  emit(cg, "const ");
    gen_decl(cg, vt, n->var_decl.name, false);
    emit(cg, " = ");
    if (init && init->kind == NK_ARRAY_INIT) {
        emit(cg, "{");
        for (NodeList *e = init->array_init.elements; e; e = e->next) {
            e->node = simplify(cg, e->node);
            gen_expr(cg, e->node);
            if (e->next) emit(cg, ", ");
        }
        emit(cg, "}");
    } else if (init) {
        init = n->var_decl.init = simplify(cg, init);
        if (vt && (vt->kind == TY_FLOAT || vt->kind == TY_DOUBLE) && init->type &&
            init->type->kind != TY_FLOAT && init->type->kind != TY_DOUBLE) {
            emit(cg, "(double)");
        }
        gen_expr(cg, init);
    } else {
        gen_zero_init(cg, vt);
    }
    emit(cg, ";\n");
}

static void gen_pathi(CodeGen *cg, Node *n) {
    char kinds[64];
    int count = 0;
    const char *err;
    char *fmt = fmt_rewrite(cg->arena, n->pathi.fmt->string_lit.value, kinds,
                            (int)sizeof kinds, &count, &err);

    indent(cg);
    emit(cg, "printf(");
    emit_c_string(cg, fmt ? fmt : "");
    int i = 0;
    for (NodeList *a = n->pathi.args; a; a = a->next, i++) {
        a->node = simplify(cg, a->node);
        emit(cg, ", ");
        switch (i < count ? kinds[i] : 's') {
            case 'i': emit(cg, "(long long)"); break;
            case 'f': emit(cg, "(double)");    break;
            case 'c': emit(cg, "(int)");       break;
            default: break;
        }
        gen_expr(cg, a->node);
    }
    emit(cg, ");\n");
}

static bool const_cond(Node *c, bool *val) {
    if (!is_num_lit(c)) return false;
    *val = fval(c) != 0.0;
    return true;
}

static void gen_if(CodeGen *cg, Node *n) {
    bool emitted = false;
    for (IfBranch *br = n->if_stmt.branches; br; br = br->next) {
        bool truth = true, constant = true;
        if (br->cond) {
            br->cond = simplify(cg, br->cond);
            constant = const_cond(br->cond, &truth);
        }
        if (constant && !truth) continue;

        if (!br->cond || constant) {
            if (emitted) emit(cg, " else ");
            else         indent(cg);
            gen_block(cg, br->body);
            emitted = true;
            break;
        }
        if (emitted) emit(cg, " else if (");
        else { indent(cg); emit(cg, "if ("); }
        gen_expr(cg, br->cond);
        emit(cg, ") ");
        gen_block(cg, br->body);
        emitted = true;
    }
    if (emitted) emit(cg, "\n");
}

static void gen_stmt(CodeGen *cg, Node *n) {
    if (!n) return;
    switch (n->kind) {
        case NK_VAR_DECL: gen_var_decl(cg, n); break;
        case NK_BLOCK:    indent(cg); gen_block(cg, n); emit(cg, "\n"); break;
        case NK_IF:       gen_if(cg, n); break;

        case NK_WHILE: {
            n->while_stmt.cond = simplify(cg, n->while_stmt.cond);
            bool truth;
            if (const_cond(n->while_stmt.cond, &truth) && !truth) break;
            indent(cg);
            emit(cg, "while (");
            gen_expr(cg, n->while_stmt.cond);
            emit(cg, ") ");
            gen_block(cg, n->while_stmt.body);
            emit(cg, "\n");
            break;
        }

        case NK_FOR: {
            indent(cg);
            emit(cg, "for (");
            if (n->for_stmt.init) {
                if (n->for_stmt.init->kind == NK_VAR_DECL) {
                    TypeInfo *vt = n->for_stmt.init->var_decl.var_type ?
                                   n->for_stmt.init->var_decl.var_type : n->for_stmt.init->type;
                    gen_decl(cg, vt, n->for_stmt.init->var_decl.name, false);
                    if (n->for_stmt.init->var_decl.init) {
                        emit(cg, " = ");
                        gen_expr(cg, n->for_stmt.init->var_decl.init);
                    }
                } else {
                    gen_expr_stmt(cg, n->for_stmt.init);
                }
            }
            emit(cg, "; ");
            if (n->for_stmt.cond) gen_expr(cg, n->for_stmt.cond);
            emit(cg, "; ");
            if (n->for_stmt.step) gen_expr_stmt(cg, n->for_stmt.step);
            emit(cg, ") ");
            gen_block(cg, n->for_stmt.body);
            emit(cg, "\n");
            break;
        }

        case NK_DO_WHILE:
            indent(cg);
            emit(cg, "do ");
            gen_block(cg, n->do_while_stmt.body);
            emit(cg, " while (");
            gen_expr(cg, n->do_while_stmt.cond);
            emit(cg, ");\n");
            break;

        case NK_SWITCH: {
            indent(cg);
            emit(cg, "switch (");
            gen_expr(cg, n->switch_stmt.expr);
            emit(cg, ") {\n");
            cg->indent++;
            for (NodeList *c = n->switch_stmt.cases; c; c = c->next) {
                Node *cn = c->node;
                indent(cg);
                if (cn->kind == NK_CASE && cn->case_stmt.value) {
                    emit(cg, "case ");
                    gen_expr(cg, cn->case_stmt.value);
                    emit(cg, ": {\n");
                } else {
                    emit(cg, "default: {\n");
                }
                cg->indent++;
                for (NodeList *cs = cn->case_stmt.stmts; cs; cs = cs->next)
                    gen_stmt(cg, cs->node);
                cg->indent--;
                indent(cg);
                emit(cg, "}\n");
            }
            cg->indent--;
            indent(cg);
            emit(cg, "}\n");
            break;
        }

        case NK_RETURN: {
            bool has_defers = false;
            for (DeferFrame *f = cg->defer_frame; f; f = f->prev) {
                if (f->count > 0) { has_defers = true; break; }
            }
            if (has_defers) {
                if (n->ret.value) {
                    n->ret.value = simplify(cg, n->ret.value);
                    int ret_id = cg->unique_id++;
                    indent(cg);
                    char ret_var[32];
                    snprintf(ret_var, sizeof(ret_var), "ret_%d", ret_id);
                    gen_decl(cg, n->ret.value->type, ret_var, false);
                    emit(cg, " = ");
                    gen_expr(cg, n->ret.value);
                    emit(cg, ";\n");

                    for (DeferFrame *f = cg->defer_frame; f; f = f->prev) {
                        for (int i = f->count - 1; i >= 0; i--) {
                            gen_stmt(cg, f->defers[i]);
                        }
                    }
                    indent(cg);
                    emitf(cg, "return tz_ret_%d;\n", ret_id);
                } else {
                    for (DeferFrame *f = cg->defer_frame; f; f = f->prev) {
                        for (int i = f->count - 1; i >= 0; i--) {
                            gen_stmt(cg, f->defers[i]);
                        }
                    }
                    indent(cg);
                    emit(cg, "return;\n");
                }
            } else {
                indent(cg);
                emit(cg, "return");
                if (n->ret.value) {
                    n->ret.value = simplify(cg, n->ret.value);
                    emit(cg, " ");
                    gen_expr(cg, n->ret.value);
                }
                emit(cg, ";\n");
            }
            break;
        }

        case NK_DEFER:
            if (cg->defer_frame && cg->defer_frame->count < 64) {
                cg->defer_frame->defers[cg->defer_frame->count++] = n->defer_stmt.stmt;
            }
            break;

        case NK_FOREACH: {
            indent(cg);
            emit(cg, "{\n");
            cg->indent++;
            int loop_id = cg->unique_id++;
            TypeInfo *arr_type = n->foreach_stmt.array->type;
            if (n->foreach_stmt.array->kind == NK_RANGE) {
                Node *rng = n->foreach_stmt.array;
                indent(cg);
                emitf(cg, "long long __tz_start_%d = (long long)(", loop_id);
                gen_expr(cg, rng->range.start);
                emit(cg, ");\n");
                indent(cg);
                emitf(cg, "long long __tz_end_%d = (long long)(", loop_id);
                gen_expr(cg, rng->range.end);
                emit(cg, ");\n");
                indent(cg);
                emit(cg, "for (");
                gen_decl(cg, n->foreach_stmt.var_type, n->foreach_stmt.var_name, false);
                emitf(cg, " = __tz_start_%d; ", loop_id);
                emit_ident(cg, n->foreach_stmt.var_name);
                emitf(cg, " %s __tz_end_%d; ", rng->range.inclusive ? "<=" : "<", loop_id);
                emit_ident(cg, n->foreach_stmt.var_name);
                emit(cg, "++) {\n");
                cg->indent++;
                if (n->foreach_stmt.body->kind == NK_BLOCK) {
                    for (NodeList *it = n->foreach_stmt.body->block.stmts; it; it = it->next) {
                        gen_stmt(cg, it->node);
                        if (terminates(it->node)) break;
                    }
                } else {
                    gen_stmt(cg, n->foreach_stmt.body);
                }
                cg->indent--;
                indent(cg);
                emit(cg, "}\n");
            } else if (arr_type && arr_type->kind == TY_STRING) {
                indent(cg);
                emitf(cg, "const char *__tz_arr_%d = ", loop_id);
                gen_expr(cg, n->foreach_stmt.array);
                emit(cg, ";\n");
                indent(cg);
                emitf(cg, "int __tz_len_%d = (int)strlen(__tz_arr_%d);\n", loop_id, loop_id);
                indent(cg);
                emitf(cg, "for (int __tz_i_%d = 0; __tz_i_%d < __tz_len_%d; __tz_i_%d++) {\n",
                      loop_id, loop_id, loop_id, loop_id);
                cg->indent++;
                indent(cg);
                emitf(cg, "char tz_%s = __tz_arr_%d[__tz_i_%d];\n",
                      n->foreach_stmt.var_name, loop_id, loop_id);
                if (n->foreach_stmt.body->kind == NK_BLOCK) {
                    for (NodeList *it = n->foreach_stmt.body->block.stmts; it; it = it->next) {
                        gen_stmt(cg, it->node);
                        if (terminates(it->node)) break;
                    }
                } else {
                    gen_stmt(cg, n->foreach_stmt.body);
                }
                cg->indent--;
                indent(cg);
                emit(cg, "}\n");
            } else {
                int arr_len = (arr_type && arr_type->array_size > 0) ? arr_type->array_size : 0;
                indent(cg);
                emitf(cg, "int __tz_len_%d = %d;\n", loop_id, arr_len);
                indent(cg);
                emitf(cg, "for (int __tz_i_%d = 0; __tz_i_%d < __tz_len_%d; __tz_i_%d++) {\n",
                      loop_id, loop_id, loop_id, loop_id);
                cg->indent++;
                indent(cg);
                gen_decl(cg, n->foreach_stmt.var_type, n->foreach_stmt.var_name, false);
                emit(cg, " = ");
                gen_expr(cg, n->foreach_stmt.array);
                emitf(cg, "[__tz_i_%d];\n", loop_id);
                if (n->foreach_stmt.body->kind == NK_BLOCK) {
                    for (NodeList *it = n->foreach_stmt.body->block.stmts; it; it = it->next) {
                        gen_stmt(cg, it->node);
                        if (terminates(it->node)) break;
                    }
                } else {
                    gen_stmt(cg, n->foreach_stmt.body);
                }
                cg->indent--;
                indent(cg);
                emit(cg, "}\n");
            }
            cg->indent--;
            indent(cg);
            emit(cg, "}\n");
            break;
        }

        case NK_BREAK:    indent(cg); emit(cg, "break;\n");    break;
        case NK_CONTINUE: indent(cg); emit(cg, "continue;\n"); break;
        case NK_PATHI:    gen_pathi(cg, n); break;
        case NK_DELETE:
            cg->uses_heap = true;
            indent(cg);
            emit(cg, "tz_free((void *)(");
            gen_expr(cg, n->delete_stmt.expr);
            emit(cg, "));\n");
            break;

        case NK_EXPR_STMT:
            n->expr_stmt.expr = simplify(cg, n->expr_stmt.expr);
            indent(cg);
            gen_expr_stmt(cg, n->expr_stmt.expr);
            emit(cg, ";\n");
            break;

        default:
            break;
    }
}

/* ─── Top level ───────────────────────────────────────────────────────── */
static void gen_func_header(CodeGen *cg, Node *f) {
    if (f->func_decl.is_static) emit(cg, "static ");
    bool is_main = !strcmp(f->func_decl.name, "main");
    if (is_main) emit(cg, "int");
    else         gen_base_type(cg, f->func_decl.ret_type ? f->func_decl.ret_type : &ty_void);
    emit(cg, " ");
    if (f->func_decl.class_name) {
        emitf(cg, "tz_%s_%s", f->func_decl.class_name, f->func_decl.name);
    } else {
        emit_ident(cg, f->func_decl.name);
    }
    emit(cg, "(");
    if (f->func_decl.class_name) {
        emitf(cg, "tz_%s *tz_this", f->func_decl.class_name);
        if (f->func_decl.params) emit(cg, ", ");
    }
    for (Param *p = f->func_decl.params; p; p = p->next) {
        gen_decl(cg, p->type, p->name, true);
        if (p->next) emit(cg, ", ");
    }
    if (!f->func_decl.class_name && !f->func_decl.params) emit(cg, "void");
    emit(cg, ")");
}

static void gen_enum(CodeGen *cg, Node *n) {
    emit(cg, "typedef enum {\n");
    cg->indent++;
    for (EnumVal *v = n->enum_decl.values; v; v = v->next) {
        indent(cg);
        emit_ident(cg, v->name);
        if (v->has_value) emitf(cg, " = %lld", v->value);
        if (v->next) emit(cg, ",");
        emit(cg, "\n");
    }
    cg->indent--;
    emit(cg, "} ");
    emit_ident(cg, n->enum_decl.name);
    emit(cg, ";\n\n");
}

static void gen_union(CodeGen *cg, Node *n) {
    emit(cg, "typedef union {\n");
    cg->indent++;
    for (Field *f = n->union_decl.fields; f; f = f->next) {
        indent(cg);
        gen_decl(cg, f->type, f->name, false);
        emit(cg, ";\n");
    }
    cg->indent--;
    emit(cg, "} ");
    emit_ident(cg, n->union_decl.name);
    emit(cg, ";\n\n");
}

static void gen_typedef(CodeGen *cg, Node *n) {
    emit(cg, "typedef ");
    gen_base_type(cg, n->typedef_decl.type);
    emit(cg, " ");
    emit_ident(cg, n->typedef_decl.name);
    emit(cg, ";\n\n");
}

static void gen_struct(CodeGen *cg, Node *n) {
    emit(cg, "typedef struct {\n");
    cg->indent++;
    for (Field *f = n->struct_decl.fields; f; f = f->next) {
        indent(cg);
        gen_decl(cg, f->type, f->name, false);
        emit(cg, ";\n");
    }
    cg->indent--;
    emit(cg, "} ");
    emit_ident(cg, n->struct_decl.name);
    emit(cg, ";\n\n");
}

static void gen_class(CodeGen *cg, Node *n) {
    emit(cg, "typedef struct {\n");
    cg->indent++;
    if (n->class_decl.base_name) {
        Node *base = find_decl(cg, n->class_decl.base_name);
        if (base) {
            Field *bf = (base->kind == NK_CLASS_DECL) ? base->class_decl.fields
                                                      : base->struct_decl.fields;
            for (Field *f = bf; f; f = f->next) {
                indent(cg);
                gen_decl(cg, f->type, f->name, false);
                emit(cg, ";\n");
            }
        }
    }
    for (Field *f = n->class_decl.fields; f; f = f->next) {
        indent(cg);
        gen_decl(cg, f->type, f->name, false);
        emit(cg, ";\n");
    }
    cg->indent--;
    emit(cg, "} ");
    emit_ident(cg, n->class_decl.name);
    emit(cg, ";\n\n");
}

static const char *field_struct_dep(Field *f) {
    TypeInfo *t = f->type;
    while (t && (t->kind == TY_ARRAY || t->kind == TY_POINTER)) t = t->elem_type;
    return (t && (t->kind == TY_STRUCT || t->kind == TY_CLASS)) ? t->struct_name : NULL;
}

static void gen_aggregates_ordered(CodeGen *cg, Node *prog) {
    int total = 0;
    for (NodeList *it = prog->program.decls; it; it = it->next) {
        NodeKind k = it->node->kind;
        if (k == NK_STRUCT_DECL || k == NK_CLASS_DECL || k == NK_UNION_DECL ||
            k == NK_ENUM_DECL || k == NK_TYPEDEF)
            total++;
    }
    if (!total) return;

    /* First emit all enums and typedefs */
    for (NodeList *it = prog->program.decls; it; it = it->next) {
        if (it->node->kind == NK_ENUM_DECL) gen_enum(cg, it->node);
        else if (it->node->kind == NK_TYPEDEF) gen_typedef(cg, it->node);
        else if (it->node->kind == NK_UNION_DECL) gen_union(cg, it->node);
    }

    /* Collect structs and classes for dependency ordering */
    int count = 0;
    for (NodeList *it = prog->program.decls; it; it = it->next)
        if (it->node->kind == NK_STRUCT_DECL || it->node->kind == NK_CLASS_DECL) count++;
    if (!count) return;

    bool *done  = arena_alloc(cg->arena, (size_t)count * sizeof(bool));
    Node **decl = arena_alloc(cg->arena, (size_t)count * sizeof(Node *));
    int k = 0;
    for (NodeList *it = prog->program.decls; it; it = it->next)
        if (it->node->kind == NK_STRUCT_DECL || it->node->kind == NK_CLASS_DECL) {
            decl[k] = it->node; done[k++] = false;
        }

    int emitted = 0;
    for (int pass = 0; pass < count && emitted < count; pass++) {
        for (int i = 0; i < count; i++) {
            if (done[i]) continue;
            bool ready = true;
            const char *my_name = (decl[i]->kind == NK_CLASS_DECL) ? decl[i]->class_decl.name
                                                                   : decl[i]->struct_decl.name;
            if (decl[i]->kind == NK_CLASS_DECL && decl[i]->class_decl.base_name) {
                const char *base = decl[i]->class_decl.base_name;
                for (int j = 0; j < count; j++) {
                    const char *jname = (decl[j]->kind == NK_CLASS_DECL) ? decl[j]->class_decl.name
                                                                         : decl[j]->struct_decl.name;
                    if (!done[j] && !strcmp(jname, base)) ready = false;
                }
            }
            Field *fl = (decl[i]->kind == NK_CLASS_DECL) ? decl[i]->class_decl.fields
                                                         : decl[i]->struct_decl.fields;
            for (Field *f = fl; f && ready; f = f->next) {
                if (f->type && f->type->kind == TY_POINTER) continue; /* pointers don't need ordering */
                const char *dep = field_struct_dep(f);
                if (!dep || !strcmp(dep, my_name)) continue;
                for (int j = 0; j < count; j++) {
                    const char *jname = (decl[j]->kind == NK_CLASS_DECL) ? decl[j]->class_decl.name
                                                                         : decl[j]->struct_decl.name;
                    if (!done[j] && !strcmp(jname, dep)) ready = false;
                }
            }
            if (ready) {
                if (decl[i]->kind == NK_CLASS_DECL) gen_class(cg, decl[i]);
                else gen_struct(cg, decl[i]);
                done[i] = true; emitted++;
            }
        }
    }
    for (int i = 0; i < count; i++) {
        if (!done[i]) {
            if (decl[i]->kind == NK_CLASS_DECL) gen_class(cg, decl[i]);
            else gen_struct(cg, decl[i]);
        }
    }
}

void codegen_init(CodeGen *cg, Arena *arena, const char *filename) {
    strbuf_init(&cg->out);
    cg->indent = 0;
    cg->arena = arena;
    cg->filename = filename;
    cg->gc = false;
    cg->uses_heap = false;
    cg->program = NULL;
    cg->defer_frame = NULL;
    cg->unique_id = 0;
}

static const char RT_COMMON[] =
    "static int tz_str_len(const char *s) {\n"
    "    return s ? (int)strlen(s) : 0;\n"
    "}\n"
    "static int tz_str_cmp(const char *a, const char *b) {\n"
    "    if (a == b) return 0;\n"
    "    if (!a) return -1;\n"
    "    if (!b) return 1;\n"
    "    return strcmp(a, b);\n"
    "}\n"
    "static const char *tz_str_sub(const char *s, int start, int len) {\n"
    "    if (!s) return \"\";\n"
    "    int slen = (int)strlen(s);\n"
    "    if (start < 0) start = 0;\n"
    "    if (start >= slen || len <= 0) return \"\";\n"
    "    if (start + len > slen) len = slen - start;\n"
    "    char *r = (char *)tz_alloc((size_t)len + 1);\n"
    "    memcpy(r, s + start, (size_t)len);\n"
    "    r[len] = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static int tz_str_find(const char *haystack, const char *needle) {\n"
    "    if (!haystack || !needle) return -1;\n"
    "    const char *p = strstr(haystack, needle);\n"
    "    return p ? (int)(p - haystack) : -1;\n"
    "}\n"
    "static bool tz_str_contains(const char *s, const char *sub) {\n"
    "    if (!s || !sub) return false;\n"
    "    return strstr(s, sub) != NULL;\n"
    "}\n"
    "static bool tz_str_starts_with(const char *s, const char *prefix) {\n"
    "    if (!s || !prefix) return false;\n"
    "    size_t slen = strlen(s), plen = strlen(prefix);\n"
    "    if (plen > slen) return false;\n"
    "    return strncmp(s, prefix, plen) == 0;\n"
    "}\n"
    "static bool tz_str_ends_with(const char *s, const char *suffix) {\n"
    "    if (!s || !suffix) return false;\n"
    "    size_t slen = strlen(s), xlen = strlen(suffix);\n"
    "    if (xlen > slen) return false;\n"
    "    return strcmp(s + (slen - xlen), suffix) == 0;\n"
    "}\n"
    "static const char *tz_str_upper(const char *s) {\n"
    "    if (!s) return \"\";\n"
    "    size_t len = strlen(s);\n"
    "    char *r = (char *)tz_alloc(len + 1);\n"
    "    for (size_t i = 0; i < len; i++) r[i] = (char)toupper((unsigned char)s[i]);\n"
    "    r[len] = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_str_lower(const char *s) {\n"
    "    if (!s) return \"\";\n"
    "    size_t len = strlen(s);\n"
    "    char *r = (char *)tz_alloc(len + 1);\n"
    "    for (size_t i = 0; i < len; i++) r[i] = (char)tolower((unsigned char)s[i]);\n"
    "    r[len] = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_str_trim(const char *s) {\n"
    "    if (!s) return \"\";\n"
    "    while (*s && isspace((unsigned char)*s)) s++;\n"
    "    if (!*s) return \"\";\n"
    "    const char *end = s + strlen(s) - 1;\n"
    "    while (end > s && isspace((unsigned char)*end)) end--;\n"
    "    size_t len = (size_t)(end - s + 1);\n"
    "    char *r = (char *)tz_alloc(len + 1);\n"
    "    memcpy(r, s, len);\n"
    "    r[len] = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_str_replace(const char *s, const char *old_str, const char *new_str) {\n"
    "    if (!s || !old_str || !new_str || !*old_str) return s ? s : \"\";\n"
    "    size_t old_len = strlen(old_str), new_len = strlen(new_str);\n"
    "    int count = 0;\n"
    "    const char *p = s;\n"
    "    while ((p = strstr(p, old_str)) != NULL) { count++; p += old_len; }\n"
    "    if (count == 0) return s;\n"
    "    size_t new_total = strlen(s) + (size_t)count * (new_len > old_len ? (new_len - old_len) : 0);\n"
    "    char *r = (char *)tz_alloc(new_total + 1);\n"
    "    char *dst = r;\n"
    "    p = s;\n"
    "    while (*p) {\n"
    "        if (strncmp(p, old_str, old_len) == 0) {\n"
    "            memcpy(dst, new_str, new_len);\n"
    "            dst += new_len;\n"
    "            p += old_len;\n"
    "        } else {\n"
    "            *dst++ = *p++;\n"
    "        }\n"
    "    }\n"
    "    *dst = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static long long tz_str_to_int(const char *s) {\n"
    "    return s ? atoll(s) : 0;\n"
    "}\n"
    "static double tz_str_to_float(const char *s) {\n"
    "    return s ? atof(s) : 0.0;\n"
    "}\n"
    "static const char *tz_concat(const char *a, const char *b) {\n"
    "    if (!a) a = \"null\";\n"
    "    if (!b) b = \"null\";\n"
    "    size_t la = strlen(a), lb = strlen(b);\n"
    "    char *r = (char *)tz_alloc(la + lb + 1);\n"
    "    memcpy(r, a, la);\n"
    "    memcpy(r + la, b, lb + 1);\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_to_str_int(long long v) {\n"
    "    char buf[64];\n"
    "    snprintf(buf, sizeof(buf), \"%lld\", v);\n"
    "    size_t len = strlen(buf);\n"
    "    char *r = (char *)tz_alloc(len + 1);\n"
    "    memcpy(r, buf, len + 1);\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_to_str_float(double v) {\n"
    "    char buf[64];\n"
    "    snprintf(buf, sizeof(buf), \"%g\", v);\n"
    "    size_t len = strlen(buf);\n"
    "    char *r = (char *)tz_alloc(len + 1);\n"
    "    memcpy(r, buf, len + 1);\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_to_str_bool(bool v) {\n"
    "    return v ? \"true\" : \"false\";\n"
    "}\n"
    "static const char *tz_to_str_char(char v) {\n"
    "    char *r = (char *)tz_alloc(2);\n"
    "    r[0] = v;\n"
    "    r[1] = '\\0';\n"
    "    return r;\n"
    "}\n"
    "static const char *tz_file_read(const char *path) {\n"
    "    if (!path) return \"\";\n"
    "    FILE *f = fopen(path, \"rb\");\n"
    "    if (!f) return \"\";\n"
    "    fseek(f, 0, SEEK_END);\n"
    "    long sz = ftell(f);\n"
    "    fseek(f, 0, SEEK_SET);\n"
    "    if (sz < 0) { fclose(f); return \"\"; }\n"
    "    char *buf = (char *)tz_alloc((size_t)sz + 1);\n"
    "    size_t read_bytes = fread(buf, 1, (size_t)sz, f);\n"
    "    buf[read_bytes] = '\\0';\n"
    "    fclose(f);\n"
    "    return buf;\n"
    "}\n\n";

static const char RT_GC[] =
    "/* --- runtime: heap via the Boehm collector (compile with --no-gc to use malloc) --- */\n"
    "#include <stdlib.h>\n"
    "#include <stddef.h>\n"
    "extern void *GC_malloc_atomic(size_t);\n"
    "extern void GC_free(void *);\n"
    "extern void GC_init(void);\n"
    "static void tz_gc_boot(void) __attribute__((constructor(101)));\n"
    "static void tz_gc_boot(void) { GC_init(); }\n"
    "static void *tz_alloc(size_t n) {\n"
    "    void *p = GC_malloc_atomic(n);\n"
    "    if (!p) { fputs(\"tamizhi: out of memory\\n\", stderr); exit(1); }\n"
    "    return p;\n"
    "}\n"
    "static void tz_free(void *p) { if (p) GC_free(p); }\n";

static const char RT_MALLOC[] =
    "/* --- runtime: heap via malloc --- */\n"
    "#include <stdlib.h>\n"
    "static void *tz_alloc(size_t n) {\n"
    "    void *p = malloc(n);\n"
    "    if (!p) { fputs(\"tamizhi: out of memory\\n\", stderr); exit(1); }\n"
    "    return p;\n"
    "}\n"
    "static void tz_free(void *p) { if (p) free(p); }\n";

static void insert_at(CodeGen *cg, size_t pos, const char *a, const char *b) {
    StrBuf nb;
    strbuf_init(&nb);
    strbuf_append_n(&nb, cg->out.data, pos);
    strbuf_append(&nb, a);
    strbuf_append(&nb, b);
    strbuf_append_n(&nb, cg->out.data + pos, cg->out.len - pos);
    strbuf_free(&cg->out);
    cg->out = nb;
}

static void gen_extern_proto(CodeGen *cg, Node *d) {
    gen_base_type(cg, d->extern_func.ret_type ? d->extern_func.ret_type : &ty_void);
    emitf(cg, " %s(", d->extern_func.cname);
    for (Param *p = d->extern_func.params; p; p = p->next) {
        gen_base_type(cg, p->type);
        if (p->next) emit(cg, ", ");
    }
    if (!d->extern_func.params) emit(cg, "void");
    emit(cg, ");\n");
}

static bool is_c_const(Node *n) {
    switch (n->kind) {
        case NK_INT_LIT: case NK_FLOAT_LIT: case NK_BOOL_LIT: case NK_STRING_LIT:
        case NK_CHAR_LIT: case NK_NULL_LIT:
            return true;
        case NK_UNARY:
            return is_c_const(n->unary.operand);
        case NK_BINARY:
            if (is_string_type(n->binary.left) || is_string_type(n->binary.right))
                return false;
            return is_c_const(n->binary.left) && is_c_const(n->binary.right);
        default:
            return false;
    }
}

static bool needs_startup_init(CodeGen *cg, Node *d) {
    Node *init = d->var_decl.init;
    if (!init || init->kind == NK_ARRAY_INIT) return false;
    init = d->var_decl.init = simplify(cg, init);
    return !is_c_const(init);
}

void codegen_generate(CodeGen *cg, Node *prog) {
    cg->program = prog;
    emit(cg, "/* Generated by Tamizhi " TAMIZHI_VERSION " */\n"
             "#include <stdio.h>\n"
             "#include <stdlib.h>\n"
             "#include <string.h>\n"
             "#include <stdint.h>\n"
             "#include <stddef.h>\n"
             "#include <stdbool.h>\n"
             "#include <time.h>\n"
             "#include <ctype.h>\n"
             "#include <math.h>\n\n"
             "static inline double tz_math_sqrt(double x) { return sqrt(x); }\n"
             "static inline double tz_math_pow(double x, double y) { return pow(x, y); }\n"
             "static inline double tz_math_floor(double x) { return floor(x); }\n"
             "static inline double tz_math_ceil(double x) { return ceil(x); }\n"
             "static inline double tz_math_round(double x) { return round(x); }\n"
             "static inline long long tz_math_abs(long long x) { return x < 0 ? -x : x; }\n"
             "static inline long long tz_math_min(long long a, long long b) { return a < b ? a : b; }\n"
             "static inline long long tz_math_max(long long a, long long b) { return a > b ? a : b; }\n"
             "static inline long long tz_sys_time(void) { return (long long)time(NULL); }\n"
             "static inline const char *tz_sys_getenv(const char *name) {\n"
             "    if (!name) return \"\";\n"
             "    const char *v = getenv(name);\n"
             "    return v ? v : \"\";\n"
             "}\n"
             "static inline int tz_sys_system(const char *cmd) {\n"
             "    return cmd ? system(cmd) : 0;\n"
             "}\n"
             "static inline bool tz_file_exists(const char *path) {\n"
             "    if (!path) return false;\n"
             "    FILE *f = fopen(path, \"rb\");\n"
             "    if (f) { fclose(f); return true; }\n"
             "    return false;\n"
             "}\n"
             "static inline bool tz_file_write(const char *path, const char *content) {\n"
             "    if (!path) return false;\n"
             "    FILE *f = fopen(path, \"wb\");\n"
             "    if (!f) return false;\n"
             "    if (content) fputs(content, f);\n"
             "    fclose(f);\n"
             "    return true;\n"
             "}\n\n");
    for (NodeList *it = prog->program.decls; it; it = it->next)
        if (it->node->kind == NK_EXTERN_HEADER)
            emitf(cg, "#include %s\n", it->node->extern_header.spec);
    emit(cg, "\n");
    size_t rt_pos = cg->out.len;

    gen_aggregates_ordered(cg, prog);

    bool any_proto = false, any_startup = false;
    for (NodeList *it = prog->program.decls; it; it = it->next) {
        if (it->node->kind == NK_EXTERN_FUNC && !it->node->extern_func.has_header) {
            gen_extern_proto(cg, it->node);
            any_proto = true;
        }
    }
    if (any_proto) emit(cg, "\n");

    /* Prototypes for standalone functions and class/struct methods */
    for (NodeList *it = prog->program.decls; it; it = it->next) {
        Node *d = it->node;
        if (d->kind == NK_FUNC_DECL) {
            gen_func_header(cg, d);
            emit(cg, ";\n");
        } else if (d->kind == NK_CLASS_DECL) {
            for (NodeList *m = d->class_decl.methods; m; m = m->next) {
                gen_func_header(cg, m->node);
                emit(cg, ";\n");
            }
        } else if (d->kind == NK_STRUCT_DECL) {
            for (NodeList *m = d->struct_decl.methods; m; m = m->next) {
                gen_func_header(cg, m->node);
                emit(cg, ";\n");
            }
        }
    }
    emit(cg, "\n");

    /* Global variables and function definitions */
    for (NodeList *it = prog->program.decls; it; it = it->next) {
        Node *d = it->node;
        if (d->kind == NK_VAR_DECL) {
            if (needs_startup_init(cg, d)) {
                Node *saved = d->var_decl.init;
                d->var_decl.init = NULL;
                gen_var_decl(cg, d);
                d->var_decl.init = saved;
                any_startup = true;
            } else {
                gen_var_decl(cg, d);
            }
        } else if (d->kind == NK_FUNC_DECL) {
            if (!d->func_decl.body) continue;
            emit(cg, "\n");
            gen_func_header(cg, d);
            emit(cg, " ");
            gen_block(cg, d->func_decl.body);
            emit(cg, "\n");
        } else if (d->kind == NK_CLASS_DECL) {
            for (NodeList *m = d->class_decl.methods; m; m = m->next) {
                if (!m->node->func_decl.body) continue;
                emit(cg, "\n");
                gen_func_header(cg, m->node);
                emit(cg, " ");
                gen_block(cg, m->node->func_decl.body);
                emit(cg, "\n");
            }
        } else if (d->kind == NK_STRUCT_DECL) {
            for (NodeList *m = d->struct_decl.methods; m; m = m->next) {
                if (!m->node->func_decl.body) continue;
                emit(cg, "\n");
                gen_func_header(cg, m->node);
                emit(cg, " ");
                gen_block(cg, m->node->func_decl.body);
                emit(cg, "\n");
            }
        }
    }

    if (any_startup) {
        emit(cg, "\n/* globals whose initialiser is not a C constant */\n"
                 "static void tz_init_globals(void) __attribute__((constructor(102)));\n"
                 "static void tz_init_globals(void) {\n");
        for (NodeList *it = prog->program.decls; it; it = it->next) {
            Node *d = it->node;
            if (d->kind != NK_VAR_DECL || !needs_startup_init(cg, d)) continue;
            TypeInfo *vt = d->var_decl.var_type ? d->var_decl.var_type : d->type;
            emit(cg, "    ");
            emit_ident(cg, d->var_decl.name);
            emit(cg, " = ");
            if (vt && (vt->kind == TY_FLOAT || vt->kind == TY_DOUBLE) && d->var_decl.init->type &&
                d->var_decl.init->type->kind != TY_FLOAT && d->var_decl.init->type->kind != TY_DOUBLE)
                emit(cg, "(double)");
            gen_expr(cg, d->var_decl.init);
            emit(cg, ";\n");
        }
        emit(cg, "}\n");
    }

    if (cg->uses_heap) {
        StrBuf rt;
        strbuf_init(&rt);
        strbuf_append(&rt, cg->gc ? RT_GC : RT_MALLOC);
        strbuf_append(&rt, "\n");
        strbuf_append(&rt, RT_COMMON);
        insert_at(cg, rt_pos, rt.data, "");
        strbuf_free(&rt);
    }
}

void codegen_write(CodeGen *cg, FILE *fp) {
    fwrite(cg->out.data, 1, cg->out.len, fp);
}

void codegen_free(CodeGen *cg) { strbuf_free(&cg->out); }
