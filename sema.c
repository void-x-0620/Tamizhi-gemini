/*
 * sema.c — Semantic analysis (see sema.h).
 */
#include "sema.h"

/* ─── Scopes ───────────────────────────────────────────────────────────── */
static unsigned sym_hash(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h % SCOPE_BUCKETS;
}

static void scope_push(Sema *s) {
    Scope *sc = arena_alloc(s->arena, sizeof(Scope));
    memset(sc->buckets, 0, sizeof sc->buckets);
    sc->parent = s->scope;
    s->scope = sc;
}

static void scope_pop(Sema *s) { s->scope = s->scope->parent; }

static Symbol *lookup_in(Scope *sc, const char *name) {
    for (Symbol *y = sc->buckets[sym_hash(name)]; y; y = y->next)
        if (!strcmp(y->name, name)) return y;
    return NULL;
}

static Symbol *lookup(Sema *s, const char *name) {
    for (Scope *sc = s->scope; sc; sc = sc->parent) {
        Symbol *y = lookup_in(sc, name);
        if (y) return y;
    }
    if (!strcmp(name, "str_len")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_len";
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_str_len";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "str_sub")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_sub";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_str_sub";
        Param *p1 = param_new(s->arena, "s", &ty_string);
        Param *p2 = param_new(s->arena, "start", &ty_int);
        Param *p3 = param_new(s->arena, "len", &ty_int);
        p1->next = p2; p2->next = p3;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "str_find")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_find";
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_str_find";
        Param *p1 = param_new(s->arena, "haystack", &ty_string);
        Param *p2 = param_new(s->arena, "needle", &ty_string);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "file_read")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "file_read";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_file_read";
        y->params = param_new(s->arena, "path", &ty_string);
        return y;
    }
    if (!strcmp(name, "file_write")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "file_write";
        y->kind = SYM_FUNC;
        y->type = &ty_bool;
        y->cname = "tz_file_write";
        Param *p1 = param_new(s->arena, "path", &ty_string);
        Param *p2 = param_new(s->arena, "content", &ty_string);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "file_exists")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "file_exists";
        y->kind = SYM_FUNC;
        y->type = &ty_bool;
        y->cname = "tz_file_exists";
        y->params = param_new(s->arena, "path", &ty_string);
        return y;
    }
    if (!strcmp(name, "math_sqrt")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_sqrt";
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_math_sqrt";
        y->params = param_new(s->arena, "x", &ty_float);
        return y;
    }
    if (!strcmp(name, "math_pow")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_pow";
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_math_pow";
        Param *p1 = param_new(s->arena, "x", &ty_float);
        Param *p2 = param_new(s->arena, "y", &ty_float);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "math_abs")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_abs";
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_math_abs";
        y->params = param_new(s->arena, "x", &ty_int);
        return y;
    }
    if (!strcmp(name, "math_min")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_min";
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_math_min";
        Param *p1 = param_new(s->arena, "a", &ty_int);
        Param *p2 = param_new(s->arena, "b", &ty_int);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "math_max")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_max";
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_math_max";
        Param *p1 = param_new(s->arena, "a", &ty_int);
        Param *p2 = param_new(s->arena, "b", &ty_int);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "math_floor")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_floor";
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_math_floor";
        y->params = param_new(s->arena, "x", &ty_float);
        return y;
    }
    if (!strcmp(name, "math_ceil")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_ceil";
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_math_ceil";
        y->params = param_new(s->arena, "x", &ty_float);
        return y;
    }
    if (!strcmp(name, "math_round")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "math_round";
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_math_round";
        y->params = param_new(s->arena, "x", &ty_float);
        return y;
    }
    if (!strcmp(name, "str_contains")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_contains";
        y->kind = SYM_FUNC;
        y->type = &ty_bool;
        y->cname = "tz_str_contains";
        Param *p1 = param_new(s->arena, "s", &ty_string);
        Param *p2 = param_new(s->arena, "sub", &ty_string);
        p1->next = p2;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "str_upper")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_upper";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_str_upper";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "str_lower")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_lower";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_str_lower";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "str_trim")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_trim";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_str_trim";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "str_replace")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "str_replace";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_str_replace";
        Param *p1 = param_new(s->arena, "s", &ty_string);
        Param *p2 = param_new(s->arena, "old_str", &ty_string);
        Param *p3 = param_new(s->arena, "new_str", &ty_string);
        p1->next = p2; p2->next = p3;
        y->params = p1;
        return y;
    }
    if (!strcmp(name, "str_to_int") || !strcmp(name, "en")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_str_to_int";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "str_to_float") || !strcmp(name, "pulli_en")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_float;
        y->cname = "tz_str_to_float";
        y->params = param_new(s->arena, "s", &ty_string);
        return y;
    }
    if (!strcmp(name, "int_to_str") || !strcmp(name, "ezhuthu")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_to_str_int";
        y->params = param_new(s->arena, "v", &ty_int);
        return y;
    }
    if (!strcmp(name, "float_to_str")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "float_to_str";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_to_str_float";
        y->params = param_new(s->arena, "v", &ty_float);
        return y;
    }
    if (!strcmp(name, "char_to_str")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = "char_to_str";
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_to_str_char";
        y->params = param_new(s->arena, "v", &ty_char);
        return y;
    }
    if (!strcmp(name, "sys_time") || !strcmp(name, "neram")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_sys_time";
        y->params = NULL;
        return y;
    }
    if (!strcmp(name, "sys_getenv") || !strcmp(name, "soozhal")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_string;
        y->cname = "tz_sys_getenv";
        y->params = param_new(s->arena, "name", &ty_string);
        return y;
    }
    if (!strcmp(name, "sys_system") || !strcmp(name, "kattalai")) {
        Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
        memset(y, 0, sizeof *y);
        y->name = name;
        y->kind = SYM_FUNC;
        y->type = &ty_int;
        y->cname = "tz_sys_system";
        y->params = param_new(s->arena, "cmd", &ty_string);
        return y;
    }
    return NULL;
}

static Symbol *declare(Sema *s, const char *name, SymKind kind, TypeInfo *type,
                       int line, int col) {
    if (lookup_in(s->scope, name)) {
        report_error(s->filename, line, col,
                     "duplicate declaration of '%s'", name);
        return NULL;
    }
    Symbol *y = arena_alloc(s->arena, sizeof(Symbol));
    memset(y, 0, sizeof *y);
    y->name = name;
    y->kind = kind;
    y->type = type;
    unsigned h = sym_hash(name);
    y->next = s->scope->buckets[h];
    s->scope->buckets[h] = y;
    return y;
}

/* ─── Type helpers ─────────────────────────────────────────────────────── */
static bool is_unknown(TypeInfo *t) { return !t || t->kind == TY_UNKNOWN; }

static bool is_integral(TypeInfo *t) {
    if (!t) return false;
    switch (t->kind) {
        case TY_INT: case TY_BOOL: case TY_CHAR: case TY_SHORT: case TY_LONG:
        case TY_INT8: case TY_INT16: case TY_INT32: case TY_INT64:
        case TY_UCHAR: case TY_USHORT: case TY_UINT: case TY_ULONG:
        case TY_UINT8: case TY_UINT16: case TY_UINT32: case TY_UINT64:
        case TY_SIZE_T: case TY_SSIZE_T: case TY_INTPTR: case TY_UINTPTR: case TY_PTRDIFF:
        case TY_ENUM: case TY_UNKNOWN:
            return true;
        default:
            return false;
    }
}

static bool is_numeric(TypeInfo *t) {
    if (!t) return false;
    if (is_integral(t)) return true;
    switch (t->kind) {
        case TY_FLOAT: case TY_DOUBLE: case TY_LONG_DOUBLE:
            return true;
        default:
            return false;
    }
}

static const char *type_str(Sema *s, TypeInfo *t) {
    if (!t) return "unknown";
    switch (t->kind) {
        case TY_VOID:        return "void";
        case TY_INT:         return "int";
        case TY_FLOAT:       return "float";
        case TY_DOUBLE:      return "double";
        case TY_LONG_DOUBLE: return "long double";
        case TY_CHAR:        return "char";
        case TY_SHORT:       return "short";
        case TY_LONG:        return "long";
        case TY_INT8:        return "int8_t";
        case TY_INT16:       return "int16_t";
        case TY_INT32:       return "int32_t";
        case TY_INT64:       return "int64_t";
        case TY_UCHAR:       return "unsigned char";
        case TY_USHORT:      return "unsigned short";
        case TY_UINT:        return "unsigned int";
        case TY_ULONG:       return "unsigned long";
        case TY_UINT8:       return "uint8_t";
        case TY_UINT16:      return "uint16_t";
        case TY_UINT32:      return "uint32_t";
        case TY_UINT64:      return "uint64_t";
        case TY_SIZE_T:      return "size_t";
        case TY_SSIZE_T:     return "ssize_t";
        case TY_INTPTR:      return "intptr_t";
        case TY_UINTPTR:     return "uintptr_t";
        case TY_PTRDIFF:     return "ptrdiff_t";
        case TY_STRING:      return "string";
        case TY_BOOL:        return "bool";
        case TY_STRUCT:
        case TY_CLASS:
        case TY_UNION:
        case TY_ENUM:        return t->struct_name ? t->struct_name : "struct";
        case TY_UNKNOWN:     return "unknown";
        case TY_POINTER: {
            const char *elem = type_str(s, t->elem_type);
            size_t n = strlen(elem) + 3;
            char *buf = arena_alloc(s->arena, n);
            snprintf(buf, n, "%s*", elem);
            return buf;
        }
        case TY_ARRAY: {
            const char *e = type_str(s, t->elem_type);
            size_t n = strlen(e) + 16;
            char *buf = arena_alloc(s->arena, n);
            if (t->array_size > 0) snprintf(buf, n, "%s[%d]", e, t->array_size);
            else                   snprintf(buf, n, "%s[]", e);
            return buf;
        }
    }
    return "unknown";
}

static bool is_subclass(Sema *s, const char *sub, const char *super) {
    if (!sub || !super) return false;
    if (!strcmp(sub, super)) return true;
    Symbol *sym = lookup(s, sub);
    while (sym && sym->base_name) {
        if (!strcmp(sym->base_name, super)) return true;
        sym = lookup(s, sym->base_name);
    }
    return false;
}

static bool compatible(Sema *s, TypeInfo *dst, TypeInfo *src) {
    if (is_unknown(dst) || is_unknown(src)) return true;
    if (dst->kind == TY_POINTER && src->kind == TY_POINTER) {
        if (dst->elem_type->kind == TY_VOID || src->elem_type->kind == TY_VOID)
            return true;
        if (dst->elem_type->kind == TY_CLASS && src->elem_type->kind == TY_CLASS) {
            return is_subclass(s, src->elem_type->struct_name, dst->elem_type->struct_name);
        }
        return compatible(s, dst->elem_type, src->elem_type);
    }
    if (dst->kind == TY_STRING && src->kind == TY_POINTER && src->elem_type->kind == TY_VOID)
        return true;
    if (src->kind == TY_STRING && dst->kind == TY_POINTER && dst->elem_type->kind == TY_VOID)
        return true;
    if (dst->kind == TY_POINTER && is_integral(src)) return true; /* NULL / 0 */
    if (dst->kind == TY_ARRAY || src->kind == TY_ARRAY) {
        if (dst->kind != src->kind) return false;
        TypeInfo *de = dst->elem_type, *se = src->elem_type;
        if (is_unknown(de) || is_unknown(se)) return true;
        if (de->kind != se->kind) return false;
        if (de->kind == TY_STRUCT || de->kind == TY_CLASS)
            return !strcmp(de->struct_name, se->struct_name);
        return true;
    }
    if (dst->kind == src->kind) {
        if (dst->kind == TY_STRUCT || dst->kind == TY_CLASS ||
            dst->kind == TY_UNION  || dst->kind == TY_ENUM)
            return !strcmp(dst->struct_name, src->struct_name);
        return true;
    }
    if ((dst->kind == TY_CLASS && src->kind == TY_CLASS) ||
        (dst->kind == TY_STRUCT && src->kind == TY_CLASS)) {
        return is_subclass(s, src->struct_name, dst->struct_name);
    }
    if (is_numeric(dst) && is_numeric(src)) return true;
    return false;
}

static void resolve_type(Sema *s, TypeInfo *t, int line, int col) {
    while (t && (t->kind == TY_ARRAY || t->kind == TY_POINTER))
        t = t->elem_type;
    if (!t) return;
    if (t->kind == TY_STRUCT || t->kind == TY_CLASS ||
        t->kind == TY_UNION  || t->kind == TY_ENUM) {
        Symbol *y = lookup(s, t->struct_name);
        if (!y || (y->kind != SYM_STRUCT && y->kind != SYM_CLASS &&
                   y->kind != SYM_UNION  && y->kind != SYM_ENUM)) {
            report_error(s->filename, line, col, "unknown type '%s'",
                         t->struct_name);
        } else {
            if (y->kind == SYM_CLASS)  t->kind = TY_CLASS;
            if (y->kind == SYM_UNION)  t->kind = TY_UNION;
            if (y->kind == SYM_ENUM)   t->kind = TY_ENUM;
        }
    }
}

static Field *find_field_in(Sema *s, Symbol *sym, const char *fname) {
    for (Field *f = sym->fields; f; f = f->next)
        if (!strcmp(f->name, fname)) return f;
    if (sym->base_name) {
        Symbol *base = lookup(s, sym->base_name);
        if (base) return find_field_in(s, base, fname);
    }
    return NULL;
}

static Node *find_method_in(Sema *s, Symbol *sym, const char *mname) {
    for (NodeList *m = sym->methods; m; m = m->next)
        if (!strcmp(m->node->func_decl.name, mname)) return m->node;
    if (sym->base_name) {
        Symbol *base = lookup(s, sym->base_name);
        if (base) return find_method_in(s, base, mname);
    }
    return NULL;
}

/* ─── Expressions ──────────────────────────────────────────────────────── */
static TypeInfo *expr(Sema *s, Node *n);
static void      stmt(Sema *s, Node *n);

static void cond_check(Sema *s, Node *cond, const char *what) {
    TypeInfo *t = expr(s, cond);
    if (!is_numeric(t) && t->kind != TY_POINTER)
        report_error(s->filename, cond->line, cond->col,
                     "%s must be a number, bool or pointer, not %s",
                     what, type_str(s, t));
}

static void check_pathi(Sema *s, Node *n) {
    Node *fmt = n->pathi.fmt;
    expr(s, fmt);
    if (fmt->kind != NK_STRING_LIT) {
        report_error(s->filename, fmt->line, fmt->col,
                     "the first argument of 'pathi' must be a string literal");
        for (NodeList *a = n->pathi.args; a; a = a->next) expr(s, a->node);
        return;
    }

    char kinds[64];
    int  count = 0;
    const char *err;
    char *rewritten = fmt_rewrite(s->arena, fmt->string_lit.value, kinds,
                                  (int)sizeof kinds, &count, &err);
    (void)rewritten;
    if (err) {
        report_error(s->filename, fmt->line, fmt->col, "bad format string: %s", err);
        for (NodeList *a = n->pathi.args; a; a = a->next) expr(s, a->node);
        return;
    }

    int i = 0;
    for (NodeList *a = n->pathi.args; a; a = a->next, i++) {
        TypeInfo *t = expr(s, a->node);
        if (i >= count) continue;
        bool ok = true;
        switch (kinds[i]) {
            case 'i': case 'c': ok = is_integral(t);                 break;
            case 'f': ok = is_unknown(t) || t->kind == TY_FLOAT || t->kind == TY_DOUBLE || t->kind == TY_LONG_DOUBLE;            break;
            case 's': ok = is_unknown(t) || t->kind == TY_STRING ||
                           (t->kind == TY_POINTER && t->elem_type->kind == TY_CHAR); break;
        }
        if (!ok)
            report_error(s->filename, a->node->line, a->node->col,
                         "format conversion #%d expects %s but the argument "
                         "is %s", i + 1,
                         kinds[i] == 'f' ? "a float" :
                         kinds[i] == 's' ? "a string" : "an integer",
                         type_str(s, t));
    }
    if (i != count)
        report_error(s->filename, n->line, n->col,
                     "format string has %d conversion(s) but %d argument(s) "
                     "were given", count, i);
}


static TypeInfo *expr(Sema *s, Node *n) {
    if (!n) return &ty_unknown;
    switch (n->kind) {
        case NK_INT_LIT:    return n->type = &ty_int;
        case NK_FLOAT_LIT:  return n->type = &ty_float;
        case NK_STRING_LIT: return n->type = &ty_string;
        case NK_BOOL_LIT:   return n->type = &ty_bool;
        case NK_CHAR_LIT:   return n->type = &ty_char;
        case NK_NULL_LIT:   return n->type = type_pointer(s->arena, &ty_void);

        case NK_SUPER: {
            if (!s->current_class) {
                report_error(s->filename, n->line, n->col,
                             "'super' used outside of a class method");
                return n->type = &ty_unknown;
            }
            Symbol *csym = lookup(s, s->current_class->struct_name);
            if (!csym || !csym->base_name) {
                report_error(s->filename, n->line, n->col,
                             "class '%s' has no base class",
                             s->current_class->struct_name ? s->current_class->struct_name : "<anon>");
                return n->type = &ty_unknown;
            }
            TypeInfo *bt = type_new(s->arena, TY_CLASS);
            bt->struct_name = csym->base_name;
            return n->type = type_pointer(s->arena, bt);
        }

        case NK_SIZEOF: {
            if (n->sizeof_expr.target_type) {
                resolve_type(s, n->sizeof_expr.target_type, n->line, n->col);
            } else if (n->sizeof_expr.expr) {
                expr(s, n->sizeof_expr.expr);
            }
            return n->type = &ty_size_t;
        }

        case NK_CAST: {
            resolve_type(s, n->cast_expr.target_type, n->line, n->col);
            expr(s, n->cast_expr.expr);
            return n->type = n->cast_expr.target_type;
        }

        case NK_THIS: {
            if (!s->current_class) {
                report_error(s->filename, n->line, n->col,
                             "'this' used outside of a class method");
                return n->type = &ty_unknown;
            }
            return n->type = type_pointer(s->arena, s->current_class);
        }

        case NK_IDENT: {
            Symbol *y = lookup(s, n->ident.name);
            if (!y) {
                const char *best = NULL;
                int best_dist = 999;
                for (Scope *sc = s->scope; sc; sc = sc->parent) {
                    for (int i = 0; i < SCOPE_BUCKETS; i++) {
                        for (Symbol *cand = sc->buckets[i]; cand; cand = cand->next) {
                            int d = levenshtein_dist(n->ident.name, cand->name);
                            if (d < best_dist) { best_dist = d; best = cand->name; }
                        }
                    }
                }
                if (best && best_dist <= 2) {
                    report_error(s->filename, n->line, n->col,
                                 "undeclared identifier '%s'; did you mean '%s'?",
                                 n->ident.name, best);
                } else {
                    report_error(s->filename, n->line, n->col,
                                 "undeclared identifier '%s'", n->ident.name);
                }
                return n->type = &ty_unknown;
            }
            if (y->kind == SYM_ENUM) {
                return n->type = y->type;
            }
            if (y->kind != SYM_VAR) {
                report_error(s->filename, n->line, n->col,
                             "'%s' is not a variable", n->ident.name);
                return n->type = &ty_unknown;
            }
            return n->type = y->type;
        }

        case NK_NEW: {
            Symbol *y = lookup(s, n->new_expr.class_name);
            if (!y || (y->kind != SYM_CLASS && y->kind != SYM_STRUCT)) {
                report_error(s->filename, n->line, n->col,
                             "unknown class '%s' in 'new'", n->new_expr.class_name);
                for (NodeList *a = n->new_expr.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            for (NodeList *a = n->new_expr.args; a; a = a->next) expr(s, a->node);
            TypeInfo *ct = type_new(s->arena, y->kind == SYM_CLASS ? TY_CLASS : TY_STRUCT);
            ct->struct_name = y->name;
            return n->type = type_pointer(s->arena, ct);
        }

        case NK_ASSIGN: {
            TypeInfo *lt = expr(s, n->assign.target);
            TypeInfo *rt = expr(s, n->assign.value);

            if (n->assign.target->kind == NK_IDENT) {
                Symbol *sy = lookup(s, n->assign.target->ident.name);
                if (sy && sy->is_const) {
                    report_error(s->filename, n->line, n->col,
                                 "cannot assign to const variable '%s'", sy->name);
                }
            }
            if (n->assign.target->kind == NK_INDEX &&
                n->assign.target->index.array->type &&
                n->assign.target->index.array->type->kind == TY_STRING) {
                report_error(s->filename, n->line, n->col,
                             "cannot modify character in immutable string");
            }
            if (!is_unknown(lt) && lt->kind == TY_ARRAY)
                report_error(s->filename, n->line, n->col,
                             "arrays cannot be assigned as a whole");
            else if (!compatible(s, lt, rt))
                report_error(s->filename, n->line, n->col,
                             "cannot assign %s to %s",
                             type_str(s, rt), type_str(s, lt));
            return n->type = lt;
        }

        case NK_COMPOUND_ASSIGN: {
            TypeInfo *lt = expr(s, n->compound_assign.target);
            TypeInfo *rt = expr(s, n->compound_assign.value);
            if (n->compound_assign.target->kind == NK_IDENT) {
                Symbol *sy = lookup(s, n->compound_assign.target->ident.name);
                if (sy && sy->is_const)
                    report_error(s->filename, n->line, n->col,
                                 "cannot assign to const variable '%s'", sy->name);
            }
            if (lt->kind == TY_STRING) {
                if (n->compound_assign.op != BOP_ADD || rt->kind != TY_STRING) {
                    report_error(s->filename, n->line, n->col,
                                 "operator '%s=' cannot be applied to string",
                                 binop_name(n->compound_assign.op));
                }
            } else if (!compatible(s, lt, rt))
                report_error(s->filename, n->line, n->col,
                             "incompatible types in compound assignment: %s and %s",
                             type_str(s, lt), type_str(s, rt));
            return n->type = lt;
        }

        case NK_POST_INC:
        case NK_POST_DEC: {
            TypeInfo *t = expr(s, n->inc_dec.target);
            if (n->inc_dec.target->kind == NK_IDENT) {
                Symbol *sy = lookup(s, n->inc_dec.target->ident.name);
                if (sy && sy->is_const)
                    report_error(s->filename, n->line, n->col,
                                 "cannot modify const variable '%s'", sy->name);
            }
            if (!is_numeric(t) && t->kind != TY_POINTER)
                report_error(s->filename, n->line, n->col,
                             "increment/decrement requires a numeric or pointer type");
            return n->type = t;
        }

        case NK_TERNARY: {
            cond_check(s, n->ternary.cond, "ternary condition");
            TypeInfo *tt = expr(s, n->ternary.then_expr);
            TypeInfo *et = expr(s, n->ternary.else_expr);
            if (!compatible(s, tt, et) && !compatible(s, et, tt))
                report_error(s->filename, n->line, n->col,
                             "mismatched types in ternary expression: %s vs %s",
                             type_str(s, tt), type_str(s, et));
            return n->type = tt;
        }

        case NK_NULL_COALESCE: {
            TypeInfo *lt = expr(s, n->binary.left);
            TypeInfo *rt = expr(s, n->binary.right);
            if (!compatible(s, lt, rt) && !compatible(s, rt, lt))
                report_error(s->filename, n->line, n->col,
                             "mismatched types in null-coalescing '?\\?': %s vs %s",
                             type_str(s, lt), type_str(s, rt));
            return n->type = (lt->kind == TY_POINTER && lt->elem_type->kind == TY_VOID) ? rt : lt;
        }

        case NK_RANGE: {
            TypeInfo *st = expr(s, n->range.start);
            TypeInfo *et = expr(s, n->range.end);
            if (!is_integral(st) || !is_integral(et))
                report_error(s->filename, n->line, n->col,
                             "range bounds must be integers");
            return n->type = &ty_int;
        }

        case NK_STRING_INTERP: {
            for (NodeList *pt = n->string_interp.parts; pt; pt = pt->next) {
                expr(s, pt->node);
            }
            return n->type = &ty_string;
        }

        case NK_BINARY: {
            Node *L = n->binary.left, *R = n->binary.right;
            TypeInfo *lt = expr(s, L), *rt = expr(s, R);
            BinOp op = n->binary.op;
            bool cmp = op == BOP_EQ || op == BOP_NE || op == BOP_LT ||
                       op == BOP_LE || op == BOP_GT || op == BOP_GE;

            if (cmp && !is_unknown(lt) && !is_unknown(rt) &&
                lt->kind == TY_STRING && rt->kind == TY_STRING) {
                return n->type = &ty_bool;           /* lowered to strcmp */
            }
            if ((op == BOP_EQ || op == BOP_NE) &&
                (lt->kind == TY_POINTER || rt->kind == TY_POINTER)) {
                if (!compatible(s, lt, rt) && !compatible(s, rt, lt))
                    report_error(s->filename, n->line, n->col,
                                 "cannot compare pointer types %s and %s",
                                 type_str(s, lt), type_str(s, rt));
                return n->type = &ty_bool;
            }
            if (cmp && lt->kind == TY_POINTER && rt->kind == TY_POINTER) {
                return n->type = &ty_bool;
            }
            if (op == BOP_ADD && !is_unknown(lt) && !is_unknown(rt) &&
                lt->kind == TY_STRING && rt->kind == TY_STRING) {
                return n->type = &ty_string;         /* heap concat */
            }
            if (op == BOP_ADD && lt->kind == TY_POINTER && is_integral(rt))
                return n->type = lt;                 /* pointer + int */
            if (op == BOP_SUB && lt->kind == TY_POINTER && is_integral(rt))
                return n->type = lt;                 /* pointer - int */

            if (op == BOP_BIT_AND || op == BOP_BIT_OR || op == BOP_BIT_XOR ||
                op == BOP_SHL || op == BOP_SHR) {
                if (!is_integral(lt) || !is_integral(rt))
                    report_error(s->filename, n->line, n->col,
                                 "bitwise operator requires integer operands");
                return n->type = lt;
            }

            if (!is_numeric(lt) || !is_numeric(rt)) {
                report_error(s->filename, n->line, n->col,
                             "invalid operands to '%s': %s and %s",
                             binop_name(op), type_str(s, lt), type_str(s, rt));
                return n->type = (cmp || op == BOP_AND || op == BOP_OR)
                                 ? &ty_bool : &ty_unknown;
            }
            if (op == BOP_MOD && (!is_integral(lt) || !is_integral(rt)))
                report_error(s->filename, n->line, n->col,
                             "'%%' requires integer operands");
            if ((op == BOP_DIV || op == BOP_MOD) && R->kind == NK_INT_LIT &&
                R->int_lit.value == 0)
                report_error(s->filename, R->line, R->col,
                             "division by zero");
            if (cmp || op == BOP_AND || op == BOP_OR) return n->type = &ty_bool;
            if ((!is_unknown(lt) && (lt->kind == TY_FLOAT || lt->kind == TY_DOUBLE)) ||
                (!is_unknown(rt) && (rt->kind == TY_FLOAT || rt->kind == TY_DOUBLE)))
                return n->type = &ty_float;
            if (is_unknown(lt) || is_unknown(rt)) return n->type = &ty_unknown;
            return n->type = &ty_int;
        }

        case NK_UNARY: {
            UnaryOp uop = n->unary.op;
            if (uop == UOP_ADDR_OF) {
                TypeInfo *t = expr(s, n->unary.operand);
                return n->type = type_pointer(s->arena, t);
            }
            if (uop == UOP_DEREF) {
                TypeInfo *t = expr(s, n->unary.operand);
                if (is_unknown(t)) return n->type = &ty_unknown;
                if (t->kind != TY_POINTER) {
                    report_error(s->filename, n->line, n->col,
                                 "cannot dereference non-pointer type %s", type_str(s, t));
                    return n->type = &ty_unknown;
                }
                return n->type = t->elem_type;
            }
            if (uop == UOP_PRE_INC || uop == UOP_PRE_DEC) {
                TypeInfo *t = expr(s, n->unary.operand);
                if (n->unary.operand->kind == NK_IDENT) {
                    Symbol *sy = lookup(s, n->unary.operand->ident.name);
                    if (sy && sy->is_const)
                        report_error(s->filename, n->line, n->col,
                                     "cannot modify const variable '%s'", sy->name);
                }
                return n->type = t;
            }
            if (uop == UOP_BIT_NOT) {
                TypeInfo *t = expr(s, n->unary.operand);
                if (!is_integral(t))
                    report_error(s->filename, n->line, n->col,
                                 "bitwise NOT requires an integer operand");
                return n->type = t;
            }
            TypeInfo *t = expr(s, n->unary.operand);
            if (!is_numeric(t))
                report_error(s->filename, n->line, n->col,
                             "invalid operand to '%s': %s",
                             unaryop_name(n->unary.op), type_str(s, t));
            if (n->unary.op == UOP_NOT) return n->type = &ty_bool;
            if (!is_unknown(t) && t->kind == TY_BOOL) return n->type = &ty_int;
            return n->type = t;
        }

        case NK_CALL: {
            Symbol *y = lookup(s, n->call.name);
            if (!y) {
                const char *best = NULL;
                int best_dist = 999;
                for (Scope *sc = s->scope; sc; sc = sc->parent) {
                    for (int i = 0; i < SCOPE_BUCKETS; i++) {
                        for (Symbol *cand = sc->buckets[i]; cand; cand = cand->next) {
                            if (cand->kind == SYM_FUNC) {
                                int d = levenshtein_dist(n->call.name, cand->name);
                                if (d < best_dist) { best_dist = d; best = cand->name; }
                            }
                        }
                    }
                }
                if (best && best_dist <= 2) {
                    report_error(s->filename, n->line, n->col,
                                 "call to undeclared function '%s'; did you mean '%s'?",
                                 n->call.name, best);
                } else {
                    report_error(s->filename, n->line, n->col,
                                 "call to undeclared function '%s'", n->call.name);
                }
                for (NodeList *a = n->call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            if (y->kind == SYM_CLASS || y->kind == SYM_STRUCT) {
                for (NodeList *a = n->call.args; a; a = a->next) expr(s, a->node);
                TypeInfo *t = type_new(s->arena, y->kind == SYM_CLASS ? TY_CLASS : TY_STRUCT);
                t->struct_name = y->name;
                return n->type = t;
            }
            if (y->kind != SYM_FUNC) {
                report_error(s->filename, n->line, n->col,
                             "'%s' is not a function", n->call.name);
                for (NodeList *a = n->call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            Param *par = y->params;
            NodeList *arg = n->call.args;
            int idx = 1;
            for (; par && arg; par = par->next, arg = arg->next, idx++) {
                TypeInfo *at = expr(s, arg->node);
                if (par->type && !compatible(s, par->type, at))
                    report_error(s->filename, arg->node->line, arg->node->col,
                                 "argument %d of '%s': expected %s but got %s",
                                 idx, n->call.name,
                                 type_str(s, par->type), type_str(s, at));
            }
            while (par && par->default_val) {
                NodeList *new_arg = arena_alloc(s->arena, sizeof(NodeList));
                new_arg->node = par->default_val;
                new_arg->next = NULL;
                if (!n->call.args) {
                    n->call.args = new_arg;
                } else {
                    NodeList *last = n->call.args;
                    while (last->next) last = last->next;
                    last->next = new_arg;
                }
                TypeInfo *at = expr(s, new_arg->node);
                if (par->type && !compatible(s, par->type, at))
                    report_error(s->filename, n->line, n->col,
                                 "default argument for parameter '%s' of '%s': expected %s but got %s",
                                 par->name, n->call.name,
                                 type_str(s, par->type), type_str(s, at));
                par = par->next;
            }
            for (; arg; arg = arg->next) expr(s, arg->node);
            if (par || arg)
                report_error(s->filename, n->line, n->col,
                             "wrong number of arguments to '%s'", n->call.name);
            n->call.cname = y->cname;
            return n->type = y->type;
        }

        case NK_METHOD_CALL: {
            TypeInfo *ot = expr(s, n->method_call.object);
            if (is_unknown(ot)) {
                for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            if (ot->kind == TY_POINTER) ot = ot->elem_type;
            if (ot->kind == TY_ARRAY) {
                if (!strcmp(n->method_call.method, "length") || !strcmp(n->method_call.method, "alavu")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_int;
                }
            }
            if (ot->kind == TY_STRING) {
                if (!strcmp(n->method_call.method, "length") || !strcmp(n->method_call.method, "alavu")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_int;
                }
                if (!strcmp(n->method_call.method, "sub") || !strcmp(n->method_call.method, "substring")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_string;
                }
                if (!strcmp(n->method_call.method, "find")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_int;
                }
                if (!strcmp(n->method_call.method, "contains") || !strcmp(n->method_call.method, "adangiullathu")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_bool;
                }
                if (!strcmp(n->method_call.method, "starts_with") || !strcmp(n->method_call.method, "thodangugiradhu")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_bool;
                }
                if (!strcmp(n->method_call.method, "ends_with") || !strcmp(n->method_call.method, "mudigiradhu")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_bool;
                }
                if (!strcmp(n->method_call.method, "to_uppercase") || !strcmp(n->method_call.method, "periya_ezhuthu") ||
                    !strcmp(n->method_call.method, "upper")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_string;
                }
                if (!strcmp(n->method_call.method, "to_lowercase") || !strcmp(n->method_call.method, "siriya_ezhuthu") ||
                    !strcmp(n->method_call.method, "lower")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_string;
                }
                if (!strcmp(n->method_call.method, "trim") || !strcmp(n->method_call.method, "vetru")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_string;
                }
                if (!strcmp(n->method_call.method, "replace") || !strcmp(n->method_call.method, "matru")) {
                    for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                    return n->type = &ty_string;
                }
            }
            if (ot->kind != TY_CLASS && ot->kind != TY_STRUCT) {
                report_error(s->filename, n->line, n->col,
                             "cannot call method '%s' on non-class/struct type %s",
                             n->method_call.method, type_str(s, ot));
                for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            Symbol *csym = lookup(s, ot->struct_name);
            if (!csym) {
                report_error(s->filename, n->line, n->col,
                             "unknown class '%s'", ot->struct_name);
                for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            Node *m = find_method_in(s, csym, n->method_call.method);
            if (!m) {
                report_error(s->filename, n->line, n->col,
                             "type '%s' has no method '%s'",
                             ot->struct_name, n->method_call.method);
                for (NodeList *a = n->method_call.args; a; a = a->next) expr(s, a->node);
                return n->type = &ty_unknown;
            }
            Param *par = m->func_decl.params;
            NodeList *arg = n->method_call.args;
            int idx = 1;
            for (; par && arg; par = par->next, arg = arg->next, idx++) {
                TypeInfo *at = expr(s, arg->node);
                if (par->type && !compatible(s, par->type, at))
                    report_error(s->filename, arg->node->line, arg->node->col,
                                 "argument %d of '%s': expected %s but got %s",
                                 idx, n->method_call.method,
                                 type_str(s, par->type), type_str(s, at));
            }
            while (par && par->default_val) {
                NodeList *new_arg = arena_alloc(s->arena, sizeof(NodeList));
                new_arg->node = par->default_val;
                new_arg->next = NULL;
                if (!n->method_call.args) {
                    n->method_call.args = new_arg;
                } else {
                    NodeList *last = n->method_call.args;
                    while (last->next) last = last->next;
                    last->next = new_arg;
                }
                TypeInfo *at = expr(s, new_arg->node);
                if (par->type && !compatible(s, par->type, at))
                    report_error(s->filename, n->line, n->col,
                                 "default argument for parameter '%s' of '%s': expected %s but got %s",
                                 par->name, n->method_call.method,
                                 type_str(s, par->type), type_str(s, at));
                par = par->next;
            }
            for (; arg; arg = arg->next) expr(s, arg->node);
            if (par || arg)
                report_error(s->filename, n->line, n->col,
                             "wrong number of arguments to method '%s'",
                             n->method_call.method);
            return n->type = m->func_decl.ret_type ? m->func_decl.ret_type : &ty_void;
        }

        case NK_INDEX: {
            TypeInfo *at = expr(s, n->index.array);
            TypeInfo *it = expr(s, n->index.index);
            if (!is_integral(it))
                report_error(s->filename, n->index.index->line,
                             n->index.index->col,
                             "array index must be an integer, not %s",
                             type_str(s, it));
            if (is_unknown(at)) return n->type = &ty_unknown;
            if (at->kind == TY_STRING) return n->type = &ty_char;
            if (at->kind == TY_POINTER) return n->type = at->elem_type;
            if (at->kind != TY_ARRAY) {
                report_error(s->filename, n->line, n->col,
                             "cannot index a value of type %s", type_str(s, at));
                return n->type = &ty_unknown;
            }
            Node *ix = n->index.index;
            if (ix->kind == NK_INT_LIT) {
                long long v = ix->int_lit.value;
                if (v < 0 || (at->array_size > 0 && v >= at->array_size))
                    report_error(s->filename, ix->line, ix->col,
                                 "index %lld is out of bounds for %s",
                                 v, type_str(s, at));
            }
            return n->type = at->elem_type;
        }

        case NK_MEMBER:
        case NK_ARROW: {
            bool is_arrow = n->kind == NK_ARROW;
            Node *obj = is_arrow ? n->arrow.object : n->member.object;
            const char *field_name = is_arrow ? n->arrow.field : n->member.field;
            TypeInfo *ot = expr(s, obj);
            if (is_unknown(ot)) return n->type = &ty_unknown;

            if (is_arrow) {
                if (ot->kind != TY_POINTER) {
                    report_error(s->filename, n->line, n->col,
                                 "'->' applied to non-pointer type %s", type_str(s, ot));
                    return n->type = &ty_unknown;
                }
                ot = ot->elem_type;
            } else if (ot->kind == TY_POINTER) {
                /* Allow auto-deref for dot access */
                ot = ot->elem_type;
            }

            if (ot->kind == TY_ENUM) {
                Symbol *esym = lookup(s, ot->struct_name);
                if (esym) {
                    for (EnumVal *ev = esym->enum_vals; ev; ev = ev->next)
                        if (!strcmp(ev->name, field_name)) return n->type = &ty_int;
                }
            }

            if (ot->kind != TY_STRUCT && ot->kind != TY_CLASS && ot->kind != TY_UNION) {
                report_error(s->filename, n->line, n->col,
                             "'.' applied to non-struct type %s",
                             type_str(s, ot));
                return n->type = &ty_unknown;
            }
            Symbol *sd = lookup(s, ot->struct_name);
            if (!sd) return n->type = &ty_unknown;
            Field *f = find_field_in(s, sd, field_name);
            if (!f) {
                const char *best = NULL;
                int best_dist = 999;
                for (Field *it = sd->fields; it; it = it->next) {
                    int d = levenshtein_dist(field_name, it->name);
                    if (d < best_dist) { best_dist = d; best = it->name; }
                }
                if (best && best_dist <= 2) {
                    report_error(s->filename, n->line, n->col,
                                 "struct '%s' has no field '%s'; did you mean '%s'?",
                                 ot->struct_name, field_name, best);
                } else {
                    report_error(s->filename, n->line, n->col,
                                 "struct '%s' has no field '%s'",
                                 ot->struct_name, field_name);
                }
                return n->type = &ty_unknown;
            }
            return n->type = f->type;
        }

        case NK_ARRAY_INIT:
            report_error(s->filename, n->line, n->col,
                         "an array literal may only initialise an 'edu' declaration");
            for (NodeList *e = n->array_init.elements; e; e = e->next) expr(s, e->node);
            return n->type = &ty_unknown;

        default:
            return n->type = &ty_unknown;
    }
}

/* ─── Statements and declarations ──────────────────────────────────────── */
static void var_decl(Sema *s, Node *n) {
    TypeInfo   *vt   = n->var_decl.var_type;
    Node       *init = n->var_decl.init;
    const char *name = n->var_decl.name;

    if (vt) {
        resolve_type(s, vt, n->line, n->col);
        if (vt->kind == TY_VOID)
            report_error(s->filename, n->line, n->col,
                         "variable '%s' cannot have type void", name);
    }

    if (n->var_decl.is_const && !init) {
        report_error(s->filename, n->line, n->col,
                     "const variable '%s' must be initialised", name);
    }

    if (init && init->kind == NK_ARRAY_INIT) {
        TypeInfo *et = NULL;
        if (vt && vt->kind != TY_ARRAY && !is_unknown(vt))
            report_error(s->filename, n->line, n->col,
                         "cannot initialise %s with an array literal",
                         type_str(s, vt));
        else if (vt && vt->kind == TY_ARRAY)
            et = vt->elem_type;

        int count = 0;
        TypeInfo *first = NULL;
        for (NodeList *e = init->array_init.elements; e; e = e->next, count++) {
            TypeInfo *t = expr(s, e->node);
            if (!first) first = t;
            TypeInfo *want = et ? et : first;
            if (want && !compatible(s, want, t))
                report_error(s->filename, e->node->line, e->node->col,
                             et ? "array element of type %s does not match %s"
                                : "array elements must all have the same type (%s vs %s)",
                             type_str(s, t), type_str(s, want));
        }
        if (!vt) {
            vt = type_array(s->arena, first ? first : &ty_unknown, count);
        } else if (vt->kind == TY_ARRAY) {
            if (vt->array_size < 0) vt->array_size = count;
            else if (count > vt->array_size)
                report_error(s->filename, init->line, init->col,
                             "too many initialisers: %d for %s",
                             count, type_str(s, vt));
        }
        if (vt->kind == TY_ARRAY && vt->array_size <= 0)
            report_error(s->filename, n->line, n->col,
                         "an empty array literal needs an explicit size, e.g. int[8]");
        init->type = vt;
    } else if (init) {
        TypeInfo *it = expr(s, init);
        if (!vt) {
            vt = it;
            if (!is_unknown(vt) && vt->kind == TY_VOID)
                report_error(s->filename, n->line, n->col,
                             "cannot initialise '%s' from a void value", name);
        } else if (!compatible(s, vt, it)) {
            report_error(s->filename, n->line, n->col,
                         "cannot initialise '%s' (%s) with a value of type %s",
                         name, type_str(s, vt), type_str(s, it));
        }
        if (!is_unknown(vt) && vt->kind == TY_ARRAY)
            report_error(s->filename, n->line, n->col,
                         "arrays cannot be copied; initialise '%s' with an "
                         "array literal", name);
    } else {
        if (!vt)
            report_error(s->filename, n->line, n->col,
                         "cannot infer the type of '%s'; add a type or an initialiser", name);
        else if (vt->kind == TY_ARRAY && vt->array_size <= 0)
            report_error(s->filename, n->line, n->col,
                         "array '%s' needs a size or an initialiser", name);
    }

    n->type = vt ? vt : &ty_unknown;
    Symbol *sym = declare(s, name, SYM_VAR, n->type, n->line, n->col);
    if (sym) {
        sym->is_const = n->var_decl.is_const;
    }
}

static void func_decl(Sema *s, Node *n) {
    if (!n->func_decl.body) return;
    TypeInfo *rt = n->func_decl.ret_type ? n->func_decl.ret_type : &ty_void;
    resolve_type(s, rt, n->line, n->col);
    if (rt->kind == TY_ARRAY)
        report_error(s->filename, n->line, n->col,
                     "functions cannot return arrays");

    TypeInfo *saved = s->current_ret;
    s->current_ret = rt;
    scope_push(s);
    for (Param *p = n->func_decl.params; p; p = p->next) {
        if (p->type) resolve_type(s, p->type, n->line, n->col);
        declare(s, p->name, SYM_VAR, p->type ? p->type : &ty_unknown,
                n->line, n->col);
    }
    stmt(s, n->func_decl.body);
    scope_pop(s);
    s->current_ret = saved;
}

static void check_method_body(Sema *s, Node *m, TypeInfo *class_type) {
    if (!m->func_decl.body) return;
    TypeInfo *rt = m->func_decl.ret_type ? m->func_decl.ret_type : &ty_void;
    resolve_type(s, rt, m->line, m->col);

    TypeInfo *saved_ret = s->current_ret;
    TypeInfo *saved_class = s->current_class;
    s->current_ret = rt;
    s->current_class = class_type;

    scope_push(s);
    TypeInfo *ptr_type = type_pointer(s->arena, class_type);
    declare(s, "this", SYM_VAR, ptr_type, m->line, m->col);
    declare(s, "suya", SYM_VAR, ptr_type, m->line, m->col);

    for (Param *p = m->func_decl.params; p; p = p->next) {
        if (p->type) resolve_type(s, p->type, m->line, m->col);
        declare(s, p->name, SYM_VAR, p->type ? p->type : &ty_unknown,
                m->line, m->col);
    }
    stmt(s, m->func_decl.body);
    scope_pop(s);

    s->current_ret = saved_ret;
    s->current_class = saved_class;
}

static void struct_decl(Sema *s, Node *n) {
    for (Field *f = n->struct_decl.fields; f; f = f->next) {
        resolve_type(s, f->type, n->line, n->col);
        if (f->type->kind == TY_VOID)
            report_error(s->filename, n->line, n->col,
                         "field '%s' cannot have type void", f->name);
        if (f->type->kind == TY_ARRAY && f->type->array_size <= 0)
            report_error(s->filename, n->line, n->col,
                         "array field '%s' needs a size, e.g. int[4]", f->name);
        if (f->type->kind == TY_STRUCT &&
            !strcmp(f->type->struct_name, n->struct_decl.name))
            report_error(s->filename, n->line, n->col,
                         "struct '%s' cannot contain itself",
                         n->struct_decl.name);
        for (Field *g = n->struct_decl.fields; g != f; g = g->next)
            if (!strcmp(g->name, f->name))
                report_error(s->filename, n->line, n->col,
                             "duplicate field '%s' in struct '%s'",
                             f->name, n->struct_decl.name);
    }
    TypeInfo *st = type_new(s->arena, TY_STRUCT);
    st->struct_name = n->struct_decl.name;
    for (NodeList *m = n->struct_decl.methods; m; m = m->next)
        check_method_body(s, m->node, st);
}

static void class_decl(Sema *s, Node *n) {
    if (n->class_decl.base_name) {
        Symbol *base = lookup(s, n->class_decl.base_name);
        if (!base || (base->kind != SYM_CLASS && base->kind != SYM_STRUCT)) {
            report_error(s->filename, n->line, n->col,
                         "base class '%s' not found", n->class_decl.base_name);
        }
    }
    for (Field *f = n->class_decl.fields; f; f = f->next) {
        resolve_type(s, f->type, n->line, n->col);
        if (f->type->kind == TY_VOID)
            report_error(s->filename, n->line, n->col,
                         "field '%s' cannot have type void", f->name);
    }
    TypeInfo *ct = type_new(s->arena, TY_CLASS);
    ct->struct_name = n->class_decl.name;
    for (NodeList *m = n->class_decl.methods; m; m = m->next)
        check_method_body(s, m->node, ct);
}

static void union_decl(Sema *s, Node *n) {
    for (Field *f = n->union_decl.fields; f; f = f->next) {
        resolve_type(s, f->type, n->line, n->col);
        if (f->type->kind == TY_VOID)
            report_error(s->filename, n->line, n->col,
                         "union field '%s' cannot have type void", f->name);
    }
}

static void enum_decl(Sema *s, Node *n) {
    (void)s; (void)n;
}

static void stmt(Sema *s, Node *n) {
    if (!n) return;
    switch (n->kind) {
        case NK_PROGRAM:
            /* Hoist structs, classes, enums, unions, functions and externs */
            for (NodeList *it = n->program.decls; it; it = it->next) {
                Node *d = it->node;
                if (d->kind == NK_FUNC_DECL) {
                    TypeInfo *rt = d->func_decl.ret_type ? d->func_decl.ret_type : &ty_void;
                    Symbol *existing = lookup_in(s->scope, d->func_decl.name);
                    if (existing && existing->kind == SYM_FUNC) {
                        if (existing->cname) {
                            report_error(s->filename, d->line, d->col,
                                         "duplicate declaration of '%s'", d->func_decl.name);
                        } else if (existing->is_proto && d->func_decl.body) {
                            existing->is_proto = false;
                            existing->has_body = true;
                            existing->type = rt;
                            existing->params = d->func_decl.params;
                        } else if (!existing->is_proto && !d->func_decl.body) {
                            /* redundant forward declaration after definition */
                        } else {
                            report_error(s->filename, d->line, d->col,
                                         "duplicate declaration of '%s'", d->func_decl.name);
                        }
                    } else {
                        Symbol *y = declare(s, d->func_decl.name, SYM_FUNC, rt,
                                            d->line, d->col);
                        if (y) {
                            y->params = d->func_decl.params;
                            y->is_proto = (d->func_decl.body == NULL);
                            y->has_body = (d->func_decl.body != NULL);
                        }
                    }
                    if (!strcmp(d->func_decl.name, "main")) {
                        s->has_main = true;
                        if (rt->kind != TY_INT)
                            report_error(s->filename, d->line, d->col,
                                         "'main' must be declared to return int");
                        if (d->func_decl.params)
                            report_error(s->filename, d->line, d->col,
                                         "'main' must not take parameters");
                    }
                } else if (d->kind == NK_EXTERN_FUNC) {
                    const char *saved = s->filename;
                    s->filename = d->extern_func.file;
                    TypeInfo *rt = d->extern_func.ret_type ? d->extern_func.ret_type : &ty_void;
                    Symbol *y = declare(s, d->extern_func.name, SYM_FUNC, rt,
                                        d->line, d->col);
                    if (y) {
                        y->params = d->extern_func.params;
                        y->cname  = d->extern_func.cname;
                    }
                    s->filename = saved;
                } else if (d->kind == NK_STRUCT_DECL) {
                    Symbol *y = declare(s, d->struct_decl.name, SYM_STRUCT,
                                        &ty_unknown, d->line, d->col);
                    if (y) {
                        y->fields  = d->struct_decl.fields;
                        y->methods = d->struct_decl.methods;
                    }
                } else if (d->kind == NK_CLASS_DECL) {
                    Symbol *y = declare(s, d->class_decl.name, SYM_CLASS,
                                        &ty_unknown, d->line, d->col);
                    if (y) {
                        y->fields    = d->class_decl.fields;
                        y->methods   = d->class_decl.methods;
                        y->base_name = d->class_decl.base_name;
                    }
                } else if (d->kind == NK_UNION_DECL) {
                    Symbol *y = declare(s, d->union_decl.name, SYM_UNION,
                                        &ty_unknown, d->line, d->col);
                    if (y) y->fields = d->union_decl.fields;
                } else if (d->kind == NK_ENUM_DECL) {
                    TypeInfo *et = type_new(s->arena, TY_ENUM);
                    et->struct_name = d->enum_decl.name;
                    Symbol *y = declare(s, d->enum_decl.name, SYM_ENUM,
                                        et, d->line, d->col);
                    if (y) y->enum_vals = d->enum_decl.values;
                    for (EnumVal *ev = d->enum_decl.values; ev; ev = ev->next) {
                        Symbol *ey = declare(s, ev->name, SYM_VAR, &ty_int, d->line, d->col);
                        if (ey) {
                            ey->is_const = true;
                            ey->int_val  = ev->value;
                        }
                    }
                }
            }
            for (NodeList *it = n->program.decls; it; it = it->next)
                stmt(s, it->node);
            break;

        case NK_FUNC_DECL:   func_decl(s, n);   break;
        case NK_STRUCT_DECL: struct_decl(s, n); break;
        case NK_CLASS_DECL:  class_decl(s, n);  break;
        case NK_UNION_DECL:  union_decl(s, n);  break;
        case NK_ENUM_DECL:   enum_decl(s, n);   break;
        case NK_VAR_DECL:    var_decl(s, n);    break;
        case NK_DELETE: {
            TypeInfo *t = expr(s, n->delete_stmt.expr);
            if (!is_unknown(t) && t->kind != TY_POINTER) {
                report_error(s->filename, n->line, n->col,
                             "cannot delete non-pointer type %s", type_str(s, t));
            }
            break;
        }
        case NK_TYPEDEF:
        case NK_EXTERN_FUNC:
        case NK_EXTERN_HEADER: break;

        case NK_BLOCK:
            scope_push(s);
            for (NodeList *it = n->block.stmts; it; it = it->next)
                stmt(s, it->node);
            scope_pop(s);
            break;

        case NK_IF:
            for (IfBranch *b = n->if_stmt.branches; b; b = b->next) {
                if (b->cond) cond_check(s, b->cond, "an if condition");
                stmt(s, b->body);
            }
            break;

        case NK_WHILE:
            cond_check(s, n->while_stmt.cond, "a loop condition");
            stmt(s, n->while_stmt.body);
            break;

        case NK_FOR:
            scope_push(s);
            if (n->for_stmt.init) stmt(s, n->for_stmt.init);
            if (n->for_stmt.cond) cond_check(s, n->for_stmt.cond, "a for loop condition");
            if (n->for_stmt.step) expr(s, n->for_stmt.step);
            stmt(s, n->for_stmt.body);
            scope_pop(s);
            break;

        case NK_FOREACH: {
            TypeInfo *arr_type = expr(s, n->foreach_stmt.array);
            TypeInfo *elem_type = &ty_unknown;
            if (n->foreach_stmt.array->kind == NK_RANGE) {
                elem_type = &ty_int;
            } else if (arr_type->kind == TY_ARRAY || arr_type->kind == TY_POINTER) {
                elem_type = arr_type->elem_type;
            } else if (arr_type->kind == TY_STRING) {
                elem_type = &ty_char;
            } else if (!is_unknown(arr_type)) {
                report_error(s->filename, n->line, n->col,
                             "cannot iterate over non-array/string type %s", type_str(s, arr_type));
            }
            if (is_unknown(n->foreach_stmt.var_type)) {
                n->foreach_stmt.var_type = elem_type;
            } else if (!compatible(s, n->foreach_stmt.var_type, elem_type)) {
                report_error(s->filename, n->line, n->col,
                             "loop variable '%s' expected %s but array element is %s",
                             n->foreach_stmt.var_name,
                             type_str(s, n->foreach_stmt.var_type),
                             type_str(s, elem_type));
            }
            scope_push(s);
            declare(s, n->foreach_stmt.var_name, SYM_VAR, n->foreach_stmt.var_type, n->line, n->col);
            stmt(s, n->foreach_stmt.body);
            scope_pop(s);
            break;
        }

        case NK_DEFER:
            stmt(s, n->defer_stmt.stmt);
            break;

        case NK_DO_WHILE:
            stmt(s, n->do_while_stmt.body);
            cond_check(s, n->do_while_stmt.cond, "a do-while condition");
            break;

        case NK_SWITCH: {
            TypeInfo *st = expr(s, n->switch_stmt.expr);
            if (!is_integral(st))
                report_error(s->filename, n->line, n->col,
                             "switch expression must be an integer, not %s",
                             type_str(s, st));
            for (NodeList *c = n->switch_stmt.cases; c; c = c->next) {
                Node *cn = c->node;
                if (cn->kind == NK_CASE && cn->case_stmt.value)
                    expr(s, cn->case_stmt.value);
                for (NodeList *cs = cn->case_stmt.stmts; cs; cs = cs->next)
                    stmt(s, cs->node);
            }
            break;
        }

        case NK_RETURN: {
            TypeInfo *want = s->current_ret ? s->current_ret : &ty_void;
            if (n->ret.value) {
                TypeInfo *t = expr(s, n->ret.value);
                if (want->kind == TY_VOID)
                    report_error(s->filename, n->line, n->col,
                                 "a void function cannot return a value");
                else if (!compatible(s, want, t))
                    report_error(s->filename, n->line, n->col,
                                 "return type mismatch: expected %s but got %s",
                                 type_str(s, want), type_str(s, t));
            } else if (want->kind != TY_VOID) {
                report_error(s->filename, n->line, n->col,
                             "'kudu' needs a value of type %s",
                             type_str(s, want));
            }
            break;
        }

        case NK_BREAK:
        case NK_CONTINUE:
            break;

        case NK_EXPR_STMT: expr(s, n->expr_stmt.expr); break;
        case NK_PATHI:     check_pathi(s, n);          break;

        default: expr(s, n); break;
    }
}

/* ─── Public API ───────────────────────────────────────────────────────── */
void sema_init(Sema *s, Arena *arena, const char *filename) {
    s->arena = arena;
    s->filename = filename;
    s->current_ret = NULL;
    s->current_class = NULL;
    s->has_main = false;
    s->scope = NULL;
    scope_push(s);
}

void sema_analyse(Sema *s, Node *program) { stmt(s, program); }
