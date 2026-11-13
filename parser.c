/*
 * parser.c — Recursive-descent parser (see parser.h for the grammar).
 */
#include "parser.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static Node *parse_decl(Parser *p);
static Node *parse_func_decl(Parser *p, bool is_static);
static Node *parse_class_decl(Parser *p);
static Node *parse_struct_decl(Parser *p);
static Node *parse_enum_decl(Parser *p);
static Node *parse_union_decl(Parser *p);
static Node *parse_typedef(Parser *p);
static Node *parse_var_decl(Parser *p, bool is_const, bool is_static);
static Node *parse_import(Parser *p);
static Node *parse_extern(Parser *p);
static Node *parse_block(Parser *p);
static Node *parse_stmt(Parser *p);
static Node *parse_if_stmt(Parser *p);
static Node *parse_c_if_stmt(Parser *p);
static Node *parse_while_stmt(Parser *p);
static Node *parse_for_stmt(Parser *p);
static Node *parse_do_while_stmt(Parser *p);
static Node *parse_switch_stmt(Parser *p);
static Node *parse_return_stmt(Parser *p);
static Node *parse_pathi_stmt(Parser *p);
static Node *parse_expr(Parser *p);
static Node *parse_assignment(Parser *p);
static Node *parse_ternary(Parser *p);
static Node *parse_binary_level(Parser *p, int level);
static Node *parse_unary(Parser *p);
static Node *parse_postfix(Parser *p);
static Node *parse_primary(Parser *p);
static TypeInfo *parse_type(Parser *p);

/* ─── Token helpers ────────────────────────────────────────────────────── */
static Token peek(Parser *p)    { return lexer_peek(p->lexer); }
static bool  check(Parser *p, TokenType t) { return peek(p).type == t; }

static Token advance(Parser *p) {
    p->consumed++;
    return lexer_next(p->lexer);
}

static bool match(Parser *p, TokenType t) {
    if (check(p, t)) { advance(p); return true; }
    return false;
}

static const char *lexeme(Parser *p, Token t) {
    return arena_strndup(p->arena, t.start, (size_t)t.length);
}

/* Report a syntax error unless we're already recovering from one. */
static void syntax_error(Parser *p, Token at, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
static void syntax_error(Parser *p, Token at, const char *fmt, ...) {
    if (p->panic) return;
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    report_error(p->lexer->filename, at.line, at.col, "%s", msg);
    p->panic = true;
}

static Token expect(Parser *p, TokenType t) {
    Token tok = peek(p);
    if (tok.type == t) {
        if (t == TOK_SEMICOLON || t == TOK_RBRACE) p->panic = false;
        return advance(p);
    }
    syntax_error(p, tok, "expected %s but found %s",
                 token_type_name(t), token_type_name(tok.type));
    return tok;
}

/* Skip to the next statement/declaration boundary. */
static void synchronise(Parser *p) {
    p->panic = false;
    while (!check(p, TOK_EOF)) {
        TokenType t = peek(p).type;
        if (t == TOK_SEMICOLON) { advance(p); return; }
        if (t == TOK_RBRACE || t == TOK_SEYAL || t == TOK_KATTAMAIPU ||
            t == TOK_CLASS || t == TOK_ENUM || t == TOK_UNION ||
            t == TOK_VELI || t == TOK_SERKU) return;
        advance(p);
    }
}

/* ─── Entry point ──────────────────────────────────────────────────────── */
void parser_init(Parser *p, Lexer *L, Arena *arena) {
    p->lexer = L;
    p->arena = arena;
    p->panic = false;
    p->loop_depth = 0;
    p->func_depth = 0;
    p->consumed = 0;
    p->binding = false;
    p->saw_header = false;
    p->typedefs = NULL;
    p->inc_dirs = NULL;
    p->inc_count = 0;
    p->imports = arena_alloc(arena, sizeof(ImportSet));
    memset(p->imports, 0, sizeof *p->imports);
    char real[PATH_MAX];                       /* never import the main file again */
    if (L->filename && realpath(L->filename, real))
        p->imports->paths[p->imports->count++] = arena_strdup(arena, real);
}

void parser_set_include_dirs(Parser *p, const char **dirs, size_t count) {
    p->inc_dirs = dirs;
    p->inc_count = count;
}

Node *parse_program(Parser *p) {
    Node *root = node_new(p->arena, NK_PROGRAM, 1, 1);
    NodeListBuilder b = {0};

    while (!check(p, TOK_EOF)) {
        if (p->panic) {
            synchronise(p);
            if (check(p, TOK_RBRACE)) advance(p);    /* stray '}' at top level */
            continue;
        }
        size_t before = p->consumed;
        Node *d = parse_decl(p);
        if (d && d->kind == NK_PROGRAM) {            /* result of 'serku' */
            for (NodeList *it = d->program.decls; it; it = it->next)
                nlb_push(p->arena, &b, it->node);
        } else if (d) {
            nlb_push(p->arena, &b, d);
        }
        if (p->consumed == before && !check(p, TOK_EOF)) advance(p);
    }
    root->program.decls = b.head;
    return root;
}

/* ─── Declarations ─────────────────────────────────────────────────────── */
static Node *parse_decl(Parser *p) {
    Token tok = peek(p);
    if (p->binding && tok.type != TOK_VELI && tok.type != TOK_SERKU) {
        syntax_error(p, tok, "a binding file may only contain 'veli' and "
                             "'serku' declarations, found %s",
                     token_type_name(tok.type));
        return NULL;
    }
    switch (tok.type) {
        case TOK_SERKU:      return parse_import(p);
        case TOK_VELI:       return parse_extern(p);
        case TOK_SEYAL:      return parse_func_decl(p, false);
        case TOK_KATTAMAIPU: return parse_struct_decl(p);
        case TOK_CLASS:      return parse_class_decl(p);
        case TOK_ENUM:       return parse_enum_decl(p);
        case TOK_UNION:      return parse_union_decl(p);
        case TOK_TYPEDEF:    return parse_typedef(p);
        case TOK_STATIC: {
            advance(p);
            if (check(p, TOK_SEYAL)) {
                return parse_func_decl(p, true);
            }
            bool is_c = match(p, TOK_CONST);
            Node *n = parse_var_decl(p, is_c, true);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_CONST: {
            advance(p);
            bool is_s = match(p, TOK_STATIC);
            Node *n = parse_var_decl(p, true, is_s);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_EDU: {
            Node *n = parse_var_decl(p, false, false);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_SEMICOLON:
            advance(p);
            return NULL;
        default:
            syntax_error(p, tok,
                "unexpected %s at top level; only 'seyal', 'kattamaipu', "
                "'class', 'enum', 'union', 'edu', 'serku' and 'veli' declarations are allowed here",
                token_type_name(tok.type));
            return NULL;
    }
}

/* ─── C bindings: serku (import) and veli (extern) ──────────────────────── */
static void dir_of(const char *file, char *buf, size_t cap) {
    const char *slash = strrchr(file, '/');
    size_t n = slash ? (size_t)(slash - file) : 0;
    if (n >= cap) n = cap - 1;
    memcpy(buf, file, n);
    buf[n] = '\0';
}

static char *try_open(Parser *p, const char *path, char *out, size_t cap) {
    if (!path || !path[0]) return NULL;
    int n = snprintf(out, cap, "%s", path);
    if (n > 0 && (size_t)n < cap) {
        char *src = read_source_file(out, p->arena);
        if (src) return src;
    }
    return NULL;
}

static char *open_relative(Parser *p, const char *name, char *out, size_t cap) {
    if (!name || !name[0]) return NULL;
    char path_buf[PATH_MAX * 2];
    char *src = NULL;

    if (name[0] == '/') {
        return try_open(p, name, out, cap);
    }

    /* 1. File-relative */
    char dir[PATH_MAX];
    dir_of(p->lexer->filename, dir, sizeof dir);
    if (dir[0]) {
        snprintf(path_buf, sizeof path_buf, "%s/%s", dir, name);
        if ((src = try_open(p, path_buf, out, cap))) return src;
    }

    /* 2. Current working directory */
    if ((src = try_open(p, name, out, cap))) return src;

    /* 3. User -I include directories */
    for (size_t i = 0; i < p->inc_count; i++) {
        if (p->inc_dirs[i] && p->inc_dirs[i][0]) {
            snprintf(path_buf, sizeof path_buf, "%s/%s", p->inc_dirs[i], name);
            if ((src = try_open(p, path_buf, out, cap))) return src;
        }
    }

    /* 4. Project-local include/ directories */
    snprintf(path_buf, sizeof path_buf, "include/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;
    snprintf(path_buf, sizeof path_buf, "include/tamizhi/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;

    /* 5. User-local directories (~/.local/share/...) */
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(path_buf, sizeof path_buf, "%s/.local/share/include/%s", home, name);
        if ((src = try_open(p, path_buf, out, cap))) return src;
        snprintf(path_buf, sizeof path_buf, "%s/.local/share/include/tamizhi/%s", home, name);
        if ((src = try_open(p, path_buf, out, cap))) return src;
        snprintf(path_buf, sizeof path_buf, "%s/.local/share/tamizhi/include/%s", home, name);
        if ((src = try_open(p, path_buf, out, cap))) return src;
    }

    /* 6. System directories (/usr/...) */
    snprintf(path_buf, sizeof path_buf, "/usr/local/include/tamizhi/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;
    snprintf(path_buf, sizeof path_buf, "/usr/local/include/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;
    snprintf(path_buf, sizeof path_buf, "/usr/include/tamizhi/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;
    snprintf(path_buf, sizeof path_buf, "/usr/include/%s", name);
    if ((src = try_open(p, path_buf, out, cap))) return src;

    /* 7. Environment TAMIZHI_INCLUDE_PATH (colon-separated) */
    const char *env_inc = getenv("TAMIZHI_INCLUDE_PATH");
    if (env_inc && env_inc[0]) {
        char *copy = strdup(env_inc);
        if (copy) {
            char *saveptr = NULL;
            char *token = strtok_r(copy, ":", &saveptr);
            while (token) {
                if (token[0]) {
                    snprintf(path_buf, sizeof path_buf, "%s/%s", token, name);
                    src = try_open(p, path_buf, out, cap);
                    if (src) {
                        free(copy);
                        return src;
                    }
                }
                token = strtok_r(NULL, ":", &saveptr);
            }
            free(copy);
        }
    }

    return NULL;
}

static Node *parse_import(Parser *p) {
    Token kw   = expect(p, TOK_SERKU);
    Token path = peek(p);
    if (path.type != TOK_STRING_LIT) {
        syntax_error(p, path, "expected a file name in quotes after 'serku' "
                              "but found %s", token_type_name(path.type));
        return NULL;
    }
    advance(p);
    expect(p, TOK_SEMICOLON);
    if (p->panic) return NULL;

    const char *file = p->lexer->filename;
    char cand[PATH_MAX];
    char *src = open_relative(p, path.val.str_val, cand, sizeof cand);
    if (!src) {
        report_error(file, kw.line, kw.col, "cannot open '%s': %s",
                     path.val.str_val, strerror(errno));
        return NULL;
    }

    ImportSet *imp = p->imports;
    char real[PATH_MAX];
    const char *key = realpath(cand, real) ? real : cand;
    for (int i = 0; i < imp->count; i++)
        if (!strcmp(imp->paths[i], key)) return NULL;     /* include once */
    if (imp->count >= MAX_IMPORTS) {
        report_error(file, kw.line, kw.col, "too many imported files (max %d)",
                     MAX_IMPORTS);
        return NULL;
    }
    if (imp->depth >= MAX_IMPORT_DEPTH) {
        report_error(file, kw.line, kw.col, "imports are nested too deeply");
        return NULL;
    }
    imp->paths[imp->count++] = arena_strdup(p->arena, key);

    Lexer sub;
    lexer_init(&sub, src, arena_strdup(p->arena, cand), p->arena);
    Parser sp;
    parser_init(&sp, &sub, p->arena);
    sp.imports = imp;                       /* share the include-once set */
    sp.typedefs = p->typedefs;
    sp.inc_dirs = p->inc_dirs;
    sp.inc_count = p->inc_count;
    const char *ext = strrchr(cand, '.');
    sp.binding = (ext && !strcmp(ext, ".tzb"));
    imp->depth++;
    Node *prog = parse_program(&sp);
    imp->depth--;
    p->typedefs = sp.typedefs;
    return prog;
}

static const char *header_spec(Parser *p, const char *name) {
    size_t len = strlen(name);
    char *spec;
    if (name[0] == '<') {
        spec = arena_strdup(p->arena, name);
    } else {
        char cand[PATH_MAX], real[PATH_MAX];
        char dir[PATH_MAX];
        dir_of(p->lexer->filename, dir, sizeof dir);
        int n = (name[0] == '/' || !dir[0]) ? snprintf(cand, sizeof cand, "%s", name)
                                            : snprintf(cand, sizeof cand, "%s/%s", dir, name);
        if (n > 0 && (size_t)n < sizeof cand && realpath(cand, real) &&
            !strpbrk(real, "\"\\\n")) {
            spec = arena_alloc(p->arena, strlen(real) + 3);
            sprintf(spec, "\"%s\"", real);
        } else if (strpbrk(name, "\"\\\n>")) {
            spec = arena_strdup(p->arena, "<invalid.h>");
        } else {
            spec = arena_alloc(p->arena, len + 3);
            sprintf(spec, "<%s>", name);
        }
    }
    return spec;
}

static void check_c_type(Parser *p, TypeInfo *t, Token at, bool allow_void) {
    if (t == &ty_unknown) return;
    if (t->kind == TY_ARRAY) {
        report_error(p->lexer->filename, at.line, at.col,
                     "a C binding can only use int, float, string and bool%s",
                     allow_void ? " (or no return type)" : "");
    }
}

static Node *parse_extern(Parser *p) {
    Token kw = expect(p, TOK_VELI);

    if (check(p, TOK_STRING_LIT)) {
        Token h = advance(p);
        expect(p, TOK_SEMICOLON);
        if (!h.val.str_val[0]) {
            report_error(p->lexer->filename, h.line, h.col, "empty header name");
            return NULL;
        }
        Node *n = node_new(p->arena, NK_EXTERN_HEADER, kw.line, kw.col);
        n->extern_header.spec = header_spec(p, h.val.str_val);
        p->saw_header = true;
        return n;
    }

    if (check(p, TOK_KATTAMAIPU)) {
        Node *n = parse_struct_decl(p);
        if (n) {
            n->struct_decl.is_extern = true;
            n->struct_decl.has_header = p->saw_header;
        }
        return n;
    }

    if (check(p, TOK_ENUM)) {
        Node *n = parse_enum_decl(p);
        if (n) {
            n->enum_decl.is_extern = true;
            n->enum_decl.has_header = p->saw_header;
        }
        return n;
    }

    if (check(p, TOK_UNION)) {
        Node *n = parse_union_decl(p);
        if (n) {
            n->union_decl.is_extern = true;
            n->union_decl.has_header = p->saw_header;
        }
        return n;
    }

    if (check(p, TOK_TYPEDEF)) {
        Node *n = parse_typedef(p);
        if (n) {
            n->typedef_decl.is_extern = true;
            n->typedef_decl.has_header = p->saw_header;
        }
        return n;
    }

    expect(p, TOK_SEYAL);
    Token name  = expect(p, TOK_IDENT);
    Token cname = name;
    if (match(p, TOK_EQ)) cname = expect(p, TOK_IDENT);
    expect(p, TOK_LPAREN);

    Param *head = NULL, *tail = NULL;
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Token pn = expect(p, TOK_IDENT);
        expect(p, TOK_COLON);
        Token tt = peek(p);
        TypeInfo *pt = parse_type(p);
        check_c_type(p, pt, tt, false);
        Param *par = param_new(p->arena, lexeme(p, pn), pt);
        if (tail) tail->next = par; else head = par;
        tail = par;
        if (p->panic || !match(p, TOK_COMMA)) break;
    }
    expect(p, TOK_RPAREN);

    TypeInfo *ret = NULL;
    if (match(p, TOK_COLON)) {
        Token rt = peek(p);
        ret = parse_type(p);
        check_c_type(p, ret, rt, true);
    }
    expect(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_EXTERN_FUNC, kw.line, kw.col);
    n->extern_func.name       = lexeme(p, name);
    n->extern_func.cname      = lexeme(p, cname);
    n->extern_func.params     = head;
    n->extern_func.ret_type   = ret;
    n->extern_func.has_header = p->saw_header;
    n->extern_func.file       = p->lexer->filename;
    return n;
}

/* ─── Functions, Classes, Structs, Enums, Unions ───────────────────────── */
static Node *parse_func_decl(Parser *p, bool is_static) {
    Token kw   = expect(p, TOK_SEYAL);
    Token name = expect(p, TOK_IDENT);
    expect(p, TOK_LPAREN);

    Param *head = NULL, *tail = NULL;
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Token pn = expect(p, TOK_IDENT);
        TypeInfo *pt = NULL;
        if (match(p, TOK_COLON)) pt = parse_type(p);
        Node *def_val = NULL;
        if (match(p, TOK_EQ)) def_val = parse_expr(p);
        Param *par = param_new(p->arena, lexeme(p, pn), pt);
        par->default_val = def_val;
        if (tail) tail->next = par; else head = par;
        tail = par;
        if (p->panic || !match(p, TOK_COMMA)) break;
    }
    expect(p, TOK_RPAREN);

    TypeInfo *ret = NULL;
    if (match(p, TOK_COLON)) ret = parse_type(p);

    Node *body = NULL;
    if (match(p, TOK_SEMICOLON)) {
        body = NULL;
    } else {
        p->func_depth++;
        body = parse_block(p);
        p->func_depth--;
        match(p, TOK_SEMICOLON);
    }

    Node *n = node_new(p->arena, NK_FUNC_DECL, kw.line, kw.col);
    n->func_decl.name       = lexeme(p, name);
    n->func_decl.class_name = NULL;
    n->func_decl.params     = head;
    n->func_decl.ret_type   = ret;
    n->func_decl.body       = body;
    n->func_decl.is_static  = is_static;
    return n;
}

static Node *parse_class_decl(Parser *p) {
    Token kw   = expect(p, TOK_CLASS);
    Token name = expect(p, TOK_IDENT);
    const char *base_name = NULL;
    if (match(p, TOK_COLON)) {
        Token bn = expect(p, TOK_IDENT);
        base_name = lexeme(p, bn);
    }
    expect(p, TOK_LBRACE);

    Field *head = NULL, *tail = NULL;
    NodeListBuilder mb = {0};
    const char *cname = lexeme(p, name);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
        if (check(p, TOK_SEYAL)) {
            Node *m = parse_func_decl(p, false);
            m->func_decl.class_name = cname;
            nlb_push(p->arena, &mb, m);
        } else {
            Token fn = expect(p, TOK_IDENT);
            expect(p, TOK_COLON);
            TypeInfo *ft = parse_type(p);
            expect(p, TOK_SEMICOLON);
            Field *f = field_new(p->arena, lexeme(p, fn), ft);
            if (tail) tail->next = f; else head = f;
            tail = f;
        }
    }
    expect(p, TOK_RBRACE);
    match(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_CLASS_DECL, kw.line, kw.col);
    n->class_decl.name      = cname;
    n->class_decl.base_name = base_name;
    n->class_decl.fields    = head;
    n->class_decl.methods   = mb.head;
    return n;
}

static bool is_field_name_colon(Parser *p) {
    if (!check(p, TOK_IDENT)) return false;
    Lexer saved = *p->lexer;
    lexer_next(p->lexer);
    Token next = lexer_peek(p->lexer);
    *p->lexer = saved;
    return next.type == TOK_COLON;
}

static Node *parse_struct_decl(Parser *p) {
    Token kw   = expect(p, TOK_KATTAMAIPU);
    Token name = expect(p, TOK_IDENT);
    if (match(p, TOK_SEMICOLON)) {
        Node *n = node_new(p->arena, NK_STRUCT_DECL, kw.line, kw.col);
        n->struct_decl.name    = lexeme(p, name);
        n->struct_decl.fields  = NULL;
        n->struct_decl.methods = NULL;
        return n;
    }
    expect(p, TOK_LBRACE);

    Field *head = NULL, *tail = NULL;
    NodeListBuilder mb = {0};
    const char *sname = lexeme(p, name);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
        if (check(p, TOK_SEYAL)) {
            Node *m = parse_func_decl(p, false);
            m->func_decl.class_name = sname;
            nlb_push(p->arena, &mb, m);
        } else if (is_field_name_colon(p)) {
            Token fn = expect(p, TOK_IDENT);
            expect(p, TOK_COLON);
            TypeInfo *ft = parse_type(p);
            expect(p, TOK_SEMICOLON);
            Field *f = field_new(p->arena, lexeme(p, fn), ft);
            if (tail) tail->next = f; else head = f;
            tail = f;
        } else {
            TypeInfo *ft = parse_type(p);
            Token fn = expect(p, TOK_IDENT);
            expect(p, TOK_SEMICOLON);
            Field *f = field_new(p->arena, lexeme(p, fn), ft);
            if (tail) tail->next = f; else head = f;
            tail = f;
        }
    }
    expect(p, TOK_RBRACE);
    match(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_STRUCT_DECL, kw.line, kw.col);
    n->struct_decl.name    = sname;
    n->struct_decl.fields  = head;
    n->struct_decl.methods = mb.head;
    return n;
}

static Node *parse_enum_decl(Parser *p) {
    Token kw   = expect(p, TOK_ENUM);
    Token name = expect(p, TOK_IDENT);
    expect(p, TOK_LBRACE);

    EnumVal *head = NULL, *tail = NULL;
    long long cur_val = 0;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
        Token en = expect(p, TOK_IDENT);
        bool has_val = false;
        long long val = cur_val;
        if (match(p, TOK_EQ)) {
            Token vtok = peek(p);
            int sign = 1;
            if (match(p, TOK_MINUS)) sign = -1;
            if (check(p, TOK_INT_LIT)) {
                Token num = advance(p);
                val = sign * num.val.int_val;
                has_val = true;
                cur_val = val;
            } else {
                syntax_error(p, vtok, "enum value must be an integer literal");
            }
        }
        EnumVal *ev = enum_val_new(p->arena, lexeme(p, en), val, has_val);
        if (tail) tail->next = ev; else head = ev;
        tail = ev;
        cur_val++;
        if (!match(p, TOK_COMMA)) break;
        if (check(p, TOK_RBRACE)) break;
    }
    expect(p, TOK_RBRACE);
    match(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_ENUM_DECL, kw.line, kw.col);
    n->enum_decl.name   = lexeme(p, name);
    n->enum_decl.values = head;
    return n;
}

static Node *parse_union_decl(Parser *p) {
    Token kw   = expect(p, TOK_UNION);
    Token name = expect(p, TOK_IDENT);
    if (match(p, TOK_SEMICOLON)) {
        Node *n = node_new(p->arena, NK_UNION_DECL, kw.line, kw.col);
        n->union_decl.name   = lexeme(p, name);
        n->union_decl.fields = NULL;
        return n;
    }
    expect(p, TOK_LBRACE);

    Field *head = NULL, *tail = NULL;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
        if (is_field_name_colon(p)) {
            Token fn = expect(p, TOK_IDENT);
            expect(p, TOK_COLON);
            TypeInfo *ft = parse_type(p);
            expect(p, TOK_SEMICOLON);
            Field *f = field_new(p->arena, lexeme(p, fn), ft);
            if (tail) tail->next = f; else head = f;
            tail = f;
        } else {
            TypeInfo *ft = parse_type(p);
            Token fn = expect(p, TOK_IDENT);
            expect(p, TOK_SEMICOLON);
            Field *f = field_new(p->arena, lexeme(p, fn), ft);
            if (tail) tail->next = f; else head = f;
            tail = f;
        }
    }
    expect(p, TOK_RBRACE);
    match(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_UNION_DECL, kw.line, kw.col);
    n->union_decl.name   = lexeme(p, name);
    n->union_decl.fields = head;
    return n;
}

static Node *parse_typedef(Parser *p) {
    Token kw = expect(p, TOK_TYPEDEF);
    TypeInfo *target = parse_type(p);
    Token name = expect(p, TOK_IDENT);
    expect(p, TOK_SEMICOLON);

    const char *nstr = lexeme(p, name);
    TypedefEntry *te = arena_alloc(p->arena, sizeof(TypedefEntry));
    te->name = nstr;
    te->type = target;
    te->next = p->typedefs;
    p->typedefs = te;

    Node *n = node_new(p->arena, NK_TYPEDEF, kw.line, kw.col);
    n->typedef_decl.name = nstr;
    n->typedef_decl.type = target;
    return n;
}

static Node *parse_var_decl(Parser *p, bool is_const, bool is_static) {
    Token kw = peek(p);
    if (match(p, TOK_EDU)) {
        if (match(p, TOK_CONST))  is_const  = true;
        if (match(p, TOK_STATIC)) is_static = true;
    }
    Token name = expect(p, TOK_IDENT);

    TypeInfo *vt = NULL;
    if (match(p, TOK_COLON)) vt = parse_type(p);

    Node *init = NULL;
    if (match(p, TOK_EQ)) init = parse_expr(p);

    Node *n = node_new(p->arena, NK_VAR_DECL, kw.line, kw.col);
    n->var_decl.name      = lexeme(p, name);
    n->var_decl.var_type  = vt;
    n->var_decl.init      = init;
    n->var_decl.is_const  = is_const;
    n->var_decl.is_static = is_static;
    return n;
}

/* ─── Type Parsing ─────────────────────────────────────────────────────── */
static TypeInfo *lookup_typedef(Parser *p, const char *name) {
    for (TypedefEntry *e = p->typedefs; e; e = e->next)
        if (!strcmp(e->name, name)) return e->type;
    return NULL;
}

static bool is_type_ahead(Parser *p) {
    if (check(p, TOK_CONST)) return true;
    if (!check(p, TOK_IDENT)) return false;
    Token t = peek(p);
    int len = t.length;
    const char *s = t.start;
    if (lookup_typedef(p, lexeme(p, t))) return true;
    if (len == 3 && !memcmp(s, "int", 3)) return true;
    if (len == 5 && !memcmp(s, "float", 5)) return true;
    if (len == 6 && !memcmp(s, "double", 6)) return true;
    if (len == 4 && !memcmp(s, "char", 4)) return true;
    if (len == 5 && !memcmp(s, "short", 5)) return true;
    if (len == 4 && !memcmp(s, "long", 4)) return true;
    if (len == 4 && !memcmp(s, "bool", 4)) return true;
    if (len == 4 && !memcmp(s, "void", 4)) return true;
    if (len == 6 && !memcmp(s, "string", 6)) return true;
    if (len == 8 && !memcmp(s, "unsigned", 8)) return true;
    if (len == 6 && !memcmp(s, "signed", 6)) return true;
    if (len == 5 && !memcmp(s, "uchar", 5)) return true;
    if (len == 6 && !memcmp(s, "ushort", 6)) return true;
    if (len == 4 && !memcmp(s, "uint", 4)) return true;
    if (len == 5 && !memcmp(s, "ulong", 5)) return true;
    if (len >= 4 && (!memcmp(s, "int8", 4) || !memcmp(s, "uint", 4) ||
                     !memcmp(s, "size_t", 6) || !memcmp(s, "intptr", 6))) return true;
    if (s[0] == 'c') {
        const char *cs = s + 1;
        int clen = len - 1;
        if (clen == 3 && !memcmp(cs, "int", 3)) return true;
        if (clen == 5 && !memcmp(cs, "float", 5)) return true;
        if (clen == 6 && !memcmp(cs, "double", 6)) return true;
        if (clen == 6 && !memcmp(cs, "douple", 6)) return true;
        if (clen == 4 && !memcmp(cs, "char", 4)) return true;
        if (clen == 5 && !memcmp(cs, "short", 5)) return true;
        if (clen == 4 && !memcmp(cs, "long", 4)) return true;
        if (clen == 8 && !memcmp(cs, "longlong", 8)) return true;
        if (clen == 10 && !memcmp(cs, "longdouble", 10)) return true;
        if (clen == 4 && !memcmp(cs, "bool", 4)) return true;
        if (clen == 4 && !memcmp(cs, "void", 4)) return true;
        if (clen == 5 && !memcmp(cs, "uchar", 5)) return true;
        if (clen == 6 && !memcmp(cs, "ushort", 6)) return true;
        if (clen == 4 && !memcmp(cs, "uint", 4)) return true;
        if (clen == 5 && !memcmp(cs, "ulong", 5)) return true;
        if (clen == 4 && !memcmp(cs, "size", 4)) return true;
        if (clen == 6 && !memcmp(cs, "size_t", 6)) return true;
        if (clen == 5 && !memcmp(cs, "ssize", 5)) return true;
        if (clen == 7 && !memcmp(cs, "ssize_t", 7)) return true;
        if (clen == 6 && !memcmp(cs, "intptr", 6)) return true;
        if (clen == 8 && !memcmp(cs, "intptr_t", 8)) return true;
        if (clen == 7 && !memcmp(cs, "uintptr", 7)) return true;
        if (clen == 9 && !memcmp(cs, "uintptr_t", 9)) return true;
        if (clen == 7 && !memcmp(cs, "ptrdiff", 7)) return true;
        if (clen == 9 && !memcmp(cs, "ptrdiff_t", 9)) return true;
    }
    return false;
}

static TypeInfo *parse_type(Parser *p) {
    bool is_c = match(p, TOK_CONST);
    Token tok = peek(p);
    if (tok.type != TOK_IDENT) {
        syntax_error(p, tok, "expected a type name but found %s",
                     token_type_name(tok.type));
        return &ty_unknown;
    }
    advance(p);

    TypeInfo *base = NULL;
    int len = tok.length;
    const char *s = tok.start;

    TypeInfo *td = lookup_typedef(p, lexeme(p, tok));
    if (td) {
        base = td;
    } else if (len == 3 && !memcmp(s, "int", 3))       base = &ty_int;
    else if   (len == 5 && !memcmp(s, "float", 5))     base = &ty_float;
    else if   (len == 6 && !memcmp(s, "double", 6))    base = &ty_double;
    else if   (len == 4 && !memcmp(s, "char", 4))      base = &ty_char;
    else if   (len == 5 && !memcmp(s, "short", 5))     base = &ty_short;
    else if   (len == 4 && !memcmp(s, "long", 4)) {
        if (check(p, TOK_IDENT) && peek(p).length == 4 && !memcmp(peek(p).start, "long", 4)) {
            advance(p);
            base = &ty_long;
        } else if (check(p, TOK_IDENT) && peek(p).length == 6 && !memcmp(peek(p).start, "double", 6)) {
            advance(p);
            base = &ty_long_double;
        } else {
            base = &ty_long;
        }
    }
    else if (len == 4 && !memcmp(s, "bool", 4))        base = &ty_bool;
    else if (len == 4 && !memcmp(s, "void", 4))        base = &ty_void;
    else if (len == 6 && !memcmp(s, "string", 6))      base = &ty_string;
    else if (len == 8 && !memcmp(s, "unsigned", 8)) {
        if (check(p, TOK_IDENT)) {
            Token sub = advance(p);
            if (sub.length == 4 && !memcmp(sub.start, "char", 4)) base = &ty_uchar;
            else if (sub.length == 5 && !memcmp(sub.start, "short", 5)) base = &ty_ushort;
            else if (sub.length == 3 && !memcmp(sub.start, "int", 3)) base = &ty_uint;
            else if (sub.length == 4 && !memcmp(sub.start, "long", 4)) {
                if (check(p, TOK_IDENT) && peek(p).length == 4 && !memcmp(peek(p).start, "long", 4)) advance(p);
                base = &ty_ulong;
            } else base = &ty_uint;
        } else {
            base = &ty_uint;
        }
    }
    else if (len == 6 && !memcmp(s, "signed", 6)) {
        if (check(p, TOK_IDENT)) {
            Token sub = advance(p);
            if (sub.length == 4 && !memcmp(sub.start, "char", 4)) base = &ty_char;
            else if (sub.length == 5 && !memcmp(sub.start, "short", 5)) base = &ty_short;
            else if (sub.length == 3 && !memcmp(sub.start, "int", 3)) base = &ty_int;
            else if (sub.length == 4 && !memcmp(sub.start, "long", 4)) {
                if (check(p, TOK_IDENT) && peek(p).length == 4 && !memcmp(peek(p).start, "long", 4)) advance(p);
                base = &ty_long;
            } else base = &ty_int;
        } else {
            base = &ty_int;
        }
    }
    else if (len == 5 && !memcmp(s, "uchar", 5))       base = &ty_uchar;
    else if (len == 6 && !memcmp(s, "ushort", 6))      base = &ty_ushort;
    else if (len == 4 && !memcmp(s, "uint", 4))        base = &ty_uint;
    else if (len == 5 && !memcmp(s, "ulong", 5))       base = &ty_ulong;
    else if (len == 4 && !memcmp(s, "int8", 4))        base = &ty_int8;
    else if (len == 6 && !memcmp(s, "int8_t", 6))      base = &ty_int8;
    else if (len == 5 && !memcmp(s, "int16", 5))       base = &ty_int16;
    else if (len == 7 && !memcmp(s, "int16_t", 7))     base = &ty_int16;
    else if (len == 5 && !memcmp(s, "int32", 5))       base = &ty_int32;
    else if (len == 7 && !memcmp(s, "int32_t", 7))     base = &ty_int32;
    else if (len == 5 && !memcmp(s, "int64", 5))       base = &ty_int64;
    else if (len == 7 && !memcmp(s, "int64_t", 7))     base = &ty_int64;
    else if (len == 5 && !memcmp(s, "uint8", 5))       base = &ty_uint8;
    else if (len == 7 && !memcmp(s, "uint8_t", 7))     base = &ty_uint8;
    else if (len == 6 && !memcmp(s, "uint16", 6))      base = &ty_uint16;
    else if (len == 8 && !memcmp(s, "uint16_t", 8))    base = &ty_uint16;
    else if (len == 6 && !memcmp(s, "uint32", 6))      base = &ty_uint32;
    else if (len == 8 && !memcmp(s, "uint32_t", 8))    base = &ty_uint32;
    else if (len == 6 && !memcmp(s, "uint64", 6))      base = &ty_uint64;
    else if (len == 8 && !memcmp(s, "uint64_t", 8))    base = &ty_uint64;
    else if (len == 6 && !memcmp(s, "size_t", 6))      base = &ty_size_t;
    else if (len == 7 && !memcmp(s, "ssize_t", 7))     base = &ty_ssize_t;
    else if (len == 8 && !memcmp(s, "intptr_t", 8))    base = &ty_intptr;
    else if (len == 9 && !memcmp(s, "uintptr_t", 9))   base = &ty_uintptr;
    else if (len == 9 && !memcmp(s, "ptrdiff_t", 9))   base = &ty_ptrdiff;
    else if (s[0] == 'c') {
        const char *cs = s + 1;
        int clen = len - 1;
        if (clen == 3 && !memcmp(cs, "int", 3))            base = &ty_c_int;
        else if (clen == 5 && !memcmp(cs, "float", 5))     base = &ty_c_float;
        else if (clen == 6 && !memcmp(cs, "double", 6))    base = &ty_c_double;
        else if (clen == 6 && !memcmp(cs, "douple", 6))    base = &ty_c_double;
        else if (clen == 4 && !memcmp(cs, "char", 4))      base = &ty_c_char;
        else if (clen == 5 && !memcmp(cs, "short", 5))     base = &ty_c_short;
        else if (clen == 4 && !memcmp(cs, "long", 4)) {
            if (check(p, TOK_IDENT) && peek(p).length == 4 && !memcmp(peek(p).start, "long", 4)) {
                advance(p);
                base = &ty_c_longlong;
            } else if (check(p, TOK_IDENT) && peek(p).length == 6 && !memcmp(peek(p).start, "double", 6)) {
                advance(p);
                base = &ty_long_double;
            } else {
                base = &ty_c_long;
            }
        }
        else if (clen == 8 && !memcmp(cs, "longlong", 8))  base = &ty_c_longlong;
        else if (clen == 10 && !memcmp(cs, "longdouble", 10)) base = &ty_long_double;
        else if (clen == 4 && !memcmp(cs, "bool", 4))      base = &ty_c_bool;
        else if (clen == 4 && !memcmp(cs, "void", 4))      base = &ty_void;
        else if (clen == 5 && !memcmp(cs, "uchar", 5))     base = &ty_c_uchar;
        else if (clen == 6 && !memcmp(cs, "ushort", 6))    base = &ty_c_ushort;
        else if (clen == 4 && !memcmp(cs, "uint", 4))      base = &ty_c_uint;
        else if (clen == 5 && !memcmp(cs, "ulong", 5))     base = &ty_c_ulong;
        else if (clen == 4 && !memcmp(cs, "size", 4))      base = &ty_size_t;
        else if (clen == 6 && !memcmp(cs, "size_t", 6))    base = &ty_size_t;
        else if (clen == 5 && !memcmp(cs, "ssize", 5))     base = &ty_ssize_t;
        else if (clen == 7 && !memcmp(cs, "ssize_t", 7))   base = &ty_ssize_t;
        else if (clen == 6 && !memcmp(cs, "intptr", 6))    base = &ty_intptr;
        else if (clen == 8 && !memcmp(cs, "intptr_t", 8))  base = &ty_intptr;
        else if (clen == 7 && !memcmp(cs, "uintptr", 7))   base = &ty_uintptr;
        else if (clen == 9 && !memcmp(cs, "uintptr_t", 9)) base = &ty_uintptr;
        else if (clen == 7 && !memcmp(cs, "ptrdiff", 7))   base = &ty_ptrdiff;
        else if (clen == 9 && !memcmp(cs, "ptrdiff_t", 9)) base = &ty_ptrdiff;
        else {
            base = type_new(p->arena, TY_STRUCT);
            base->struct_name = lexeme(p, tok);
        }
    }
    else {
        base = type_new(p->arena, TY_STRUCT);
        base->struct_name = lexeme(p, tok);
    }

    if (match(p, TOK_CONST)) is_c = true;

    while (check(p, TOK_STAR) || check(p, TOK_LBRACKET)) {
        if (match(p, TOK_STAR)) {
            base = type_pointer(p->arena, base);
            if (match(p, TOK_CONST)) base->is_const = true;
        } else {
            advance(p);
            int size = -1;
            if (check(p, TOK_INT_LIT)) {
                Token n = advance(p);
                if (n.val.int_val <= 0 || n.val.int_val > (1 << 28)) {
                    syntax_error(p, n, "array size must be between 1 and %d", 1 << 28);
                } else {
                    size = (int)n.val.int_val;
                }
            }
            expect(p, TOK_RBRACKET);
            base = type_array(p->arena, base, size);
        }
    }
    if (is_c && !base->is_const) {
        TypeInfo *cp = type_new(p->arena, base->kind);
        *cp = *base;
        cp->is_const = true;
        base = cp;
    }
    return base;
}

/* ─── Statements ───────────────────────────────────────────────────────── */
static Node *parse_block(Parser *p) {
    Token lb = expect(p, TOK_LBRACE);
    Node *blk = node_new(p->arena, NK_BLOCK, lb.line, lb.col);
    NodeListBuilder b = {0};

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (p->panic) {
            synchronise(p);
            if (check(p, TOK_SEYAL) || check(p, TOK_KATTAMAIPU) ||
                check(p, TOK_CLASS) || check(p, TOK_ENUM) || check(p, TOK_UNION) ||
                check(p, TOK_VELI) || check(p, TOK_SERKU)) break;
            continue;
        }
        size_t before = p->consumed;
        Node *s = parse_stmt(p);
        if (s) nlb_push(p->arena, &b, s);
        if (p->consumed == before && !check(p, TOK_RBRACE) &&
            !check(p, TOK_EOF))
            advance(p);
    }
    expect(p, TOK_RBRACE);
    blk->block.stmts = b.head;
    return blk;
}

static Node *parse_stmt(Parser *p) {
    Token tok = peek(p);
    switch (tok.type) {
        case TOK_STATIC: {
            advance(p);
            bool is_c = match(p, TOK_CONST);
            Node *n = parse_var_decl(p, is_c, true);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_CONST: {
            advance(p);
            bool is_s = match(p, TOK_STATIC);
            Node *n = parse_var_decl(p, true, is_s);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_EDU: {
            Node *n = parse_var_decl(p, false, false);
            expect(p, TOK_SEMICOLON);
            return n;
        }
        case TOK_LBRACE: return parse_block(p);
        case TOK_AT:     return parse_if_stmt(p);
        case TOK_IF:     return parse_c_if_stmt(p);
        case TOK_SUTRI:  return parse_while_stmt(p);
        case TOK_FOR:    return parse_for_stmt(p);
        case TOK_DO:     return parse_do_while_stmt(p);
        case TOK_SWITCH: return parse_switch_stmt(p);
        case TOK_KUDU:   return parse_return_stmt(p);
        case TOK_PATHI:  return parse_pathi_stmt(p);
        case TOK_DELETE: {
            Token kw = advance(p);
            Node *e = parse_expr(p);
            expect(p, TOK_SEMICOLON);
            Node *n = node_new(p->arena, NK_DELETE, kw.line, kw.col);
            n->delete_stmt.expr = e;
            return n;
        }
        case TOK_DEFER: {
            Token kw = advance(p);
            Node *stmt = parse_stmt(p);
            Node *n = node_new(p->arena, NK_DEFER, kw.line, kw.col);
            n->defer_stmt.stmt = stmt;
            return n;
        }
        case TOK_UDAI:
        case TOK_THODAR: {
            Token t = advance(p);
            bool brk = t.type == TOK_UDAI;
            if (p->loop_depth == 0)
                report_error(p->lexer->filename, t.line, t.col,
                             "'%s' (%s) used outside of a loop",
                             brk ? "udai" : "thodar",
                             brk ? "break" : "continue");
            expect(p, TOK_SEMICOLON);
            return node_new(p->arena, brk ? NK_BREAK : NK_CONTINUE,
                            t.line, t.col);
        }
        default: {
            Node *e = parse_expr(p);
            expect(p, TOK_SEMICOLON);
            Node *n = node_new(p->arena, NK_EXPR_STMT, tok.line, tok.col);
            n->expr_stmt.expr = e;
            return n;
        }
    }
}

static IfBranch *parse_if_arm(Parser *p) {
    expect(p, TOK_LPAREN);
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN);
    expect(p, TOK_AGA);
    expect(p, TOK_IRUNDHA);
    Node *body = parse_block(p);

    IfBranch *br = arena_alloc(p->arena, sizeof(IfBranch));
    br->cond = cond; br->body = body; br->next = NULL;
    return br;
}

static Node *parse_if_stmt(Parser *p) {
    Token at = expect(p, TOK_AT);
    Node *n = node_new(p->arena, NK_IF, at.line, at.col);

    IfBranch *head = parse_if_arm(p), *tail = head;
    while (check(p, TOK_CARET)) {
        advance(p);
        IfBranch *br = parse_if_arm(p);
        tail->next = br; tail = br;
    }
    if (match(p, TOK_ILLANA)) {
        IfBranch *br = arena_alloc(p->arena, sizeof(IfBranch));
        br->cond = NULL; br->body = parse_block(p); br->next = NULL;
        tail->next = br;
    }
    n->if_stmt.branches = head;
    return n;
}

static Node *parse_c_if_stmt(Parser *p) {
    Token kw = expect(p, TOK_IF);
    Node *n = node_new(p->arena, NK_IF, kw.line, kw.col);
    expect(p, TOK_LPAREN);
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN);
    Node *body = parse_block(p);

    IfBranch *head = arena_alloc(p->arena, sizeof(IfBranch));
    head->cond = cond; head->body = body; head->next = NULL;
    IfBranch *tail = head;

    while (match(p, TOK_ELSE)) {
        if (match(p, TOK_IF)) {
            expect(p, TOK_LPAREN);
            Node *ec = parse_expr(p);
            expect(p, TOK_RPAREN);
            Node *eb = parse_block(p);
            IfBranch *br = arena_alloc(p->arena, sizeof(IfBranch));
            br->cond = ec; br->body = eb; br->next = NULL;
            tail->next = br; tail = br;
        } else {
            Node *eb = parse_block(p);
            IfBranch *br = arena_alloc(p->arena, sizeof(IfBranch));
            br->cond = NULL; br->body = eb; br->next = NULL;
            tail->next = br;
            break;
        }
    }
    n->if_stmt.branches = head;
    return n;
}

static Node *parse_while_stmt(Parser *p) {
    Token kw = expect(p, TOK_SUTRI);
    expect(p, TOK_LPAREN);
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN);

    p->loop_depth++;
    Node *body = parse_block(p);
    p->loop_depth--;

    Node *n = node_new(p->arena, NK_WHILE, kw.line, kw.col);
    n->while_stmt.cond = cond;
    n->while_stmt.body = body;
    return n;
}

static Node *parse_for_stmt(Parser *p) {
    Token kw = expect(p, TOK_FOR);
    expect(p, TOK_LPAREN);

    Lexer saved_lexer = *p->lexer;
    size_t saved_consumed = p->consumed;
    bool saved_panic = p->panic;

    bool is_foreach = false;
    const char *var_name = NULL;
    TypeInfo *var_type = NULL;

    bool has_edu = match(p, TOK_EDU);
    (void)has_edu;
    if (check(p, TOK_IDENT)) {
        Token id = advance(p);
        if (match(p, TOK_COLON)) {
            var_type = parse_type(p);
        }
        if (match(p, TOK_IN)) {
            is_foreach = true;
            var_name = arena_strndup(p->arena, id.start, (size_t)id.length);
        }
    }

    if (is_foreach) {
        Node *arr = parse_expr(p);
        expect(p, TOK_RPAREN);
        p->loop_depth++;
        Node *body = parse_block(p);
        p->loop_depth--;
        Node *n = node_new(p->arena, NK_FOREACH, kw.line, kw.col);
        n->foreach_stmt.var_name = var_name;
        n->foreach_stmt.var_type = var_type ? var_type : &ty_unknown;
        n->foreach_stmt.array = arr;
        n->foreach_stmt.body = body;
        return n;
    }

    *p->lexer = saved_lexer;
    p->consumed = saved_consumed;
    p->panic = saved_panic;

    Node *init = NULL;
    if (check(p, TOK_EDU) || check(p, TOK_CONST) || check(p, TOK_STATIC)) {
        bool is_s = match(p, TOK_STATIC);
        bool is_c = match(p, TOK_CONST);
        if (!is_s) is_s = match(p, TOK_STATIC);
        init = parse_var_decl(p, is_c, is_s);
        expect(p, TOK_SEMICOLON);
    } else if (!check(p, TOK_SEMICOLON)) {
        init = parse_expr(p);
        expect(p, TOK_SEMICOLON);
    } else {
        expect(p, TOK_SEMICOLON);
    }

    Node *cond = NULL;
    if (!check(p, TOK_SEMICOLON)) cond = parse_expr(p);
    expect(p, TOK_SEMICOLON);

    Node *step = NULL;
    if (!check(p, TOK_RPAREN)) step = parse_expr(p);
    expect(p, TOK_RPAREN);

    p->loop_depth++;
    Node *body = parse_block(p);
    p->loop_depth--;

    Node *n = node_new(p->arena, NK_FOR, kw.line, kw.col);
    n->for_stmt.init = init;
    n->for_stmt.cond = cond;
    n->for_stmt.step = step;
    n->for_stmt.body = body;
    return n;
}

static Node *parse_do_while_stmt(Parser *p) {
    Token kw = expect(p, TOK_DO);
    p->loop_depth++;
    Node *body = parse_block(p);
    p->loop_depth--;

    if (!match(p, TOK_SUTRI)) expect(p, TOK_SUTRI);
    expect(p, TOK_LPAREN);
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN);
    expect(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_DO_WHILE, kw.line, kw.col);
    n->do_while_stmt.body = body;
    n->do_while_stmt.cond = cond;
    return n;
}

static Node *parse_switch_stmt(Parser *p) {
    Token kw = expect(p, TOK_SWITCH);
    expect(p, TOK_LPAREN);
    Node *expr = parse_expr(p);
    expect(p, TOK_RPAREN);
    expect(p, TOK_LBRACE);

    NodeListBuilder cases = {0};
    p->loop_depth++;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
        if (check(p, TOK_CASE)) {
            Token ctok = advance(p);
            Node *val = parse_expr(p);
            expect(p, TOK_COLON);
            NodeListBuilder stmts = {0};
            while (!check(p, TOK_CASE) && !check(p, TOK_DEFAULT) &&
                   !check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
                Node *s = parse_stmt(p);
                if (s) nlb_push(p->arena, &stmts, s);
            }
            Node *cnode = node_new(p->arena, NK_CASE, ctok.line, ctok.col);
            cnode->case_stmt.value = val;
            cnode->case_stmt.stmts = stmts.head;
            nlb_push(p->arena, &cases, cnode);
        } else if (check(p, TOK_DEFAULT)) {
            Token dtok = advance(p);
            expect(p, TOK_COLON);
            NodeListBuilder stmts = {0};
            while (!check(p, TOK_CASE) && !check(p, TOK_DEFAULT) &&
                   !check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->panic) {
                Node *s = parse_stmt(p);
                if (s) nlb_push(p->arena, &stmts, s);
            }
            Node *cnode = node_new(p->arena, NK_DEFAULT, dtok.line, dtok.col);
            cnode->case_stmt.value = NULL;
            cnode->case_stmt.stmts = stmts.head;
            nlb_push(p->arena, &cases, cnode);
        } else {
            syntax_error(p, peek(p), "expected 'case' or 'default' inside switch");
            break;
        }
    }
    p->loop_depth--;
    expect(p, TOK_RBRACE);

    Node *n = node_new(p->arena, NK_SWITCH, kw.line, kw.col);
    n->switch_stmt.expr = expr;
    n->switch_stmt.cases = cases.head;
    return n;
}

static Node *parse_return_stmt(Parser *p) {
    Token kw = expect(p, TOK_KUDU);
    Node *val = NULL;
    if (!check(p, TOK_SEMICOLON)) val = parse_expr(p);
    expect(p, TOK_SEMICOLON);
    Node *n = node_new(p->arena, NK_RETURN, kw.line, kw.col);
    n->ret.value = val;
    return n;
}

static Node *parse_pathi_stmt(Parser *p) {
    Token kw = expect(p, TOK_PATHI);
    expect(p, TOK_LPAREN);
    Node *fmt = parse_expr(p);
    NodeListBuilder b = {0};
    while (match(p, TOK_COMMA))
        nlb_push(p->arena, &b, parse_expr(p));
    expect(p, TOK_RPAREN);
    expect(p, TOK_SEMICOLON);

    Node *n = node_new(p->arena, NK_PATHI, kw.line, kw.col);
    n->pathi.fmt  = fmt;
    n->pathi.args = b.head;
    return n;
}

/* ─── Expressions ──────────────────────────────────────────────────────── */
static Node *parse_expr(Parser *p) { return parse_assignment(p); }

static bool is_compound_assign(TokenType t, BinOp *op) {
    switch (t) {
        case TOK_PLUS_EQ:    *op = BOP_ADD;     return true;
        case TOK_MINUS_EQ:   *op = BOP_SUB;     return true;
        case TOK_STAR_EQ:    *op = BOP_MUL;     return true;
        case TOK_SLASH_EQ:   *op = BOP_DIV;     return true;
        case TOK_PERCENT_EQ: *op = BOP_MOD;     return true;
        case TOK_AMP_EQ:     *op = BOP_BIT_AND; return true;
        case TOK_PIPE_EQ:    *op = BOP_BIT_OR;  return true;
        case TOK_CARET_EQ:   *op = BOP_BIT_XOR; return true;
        case TOK_SHL_EQ:     *op = BOP_SHL;     return true;
        case TOK_SHR_EQ:     *op = BOP_SHR;     return true;
        default: return false;
    }
}

static bool is_valid_lvalue(Node *n) {
    if (n->kind == NK_IDENT || n->kind == NK_INDEX ||
        n->kind == NK_MEMBER || n->kind == NK_ARROW) return true;
    if (n->kind == NK_UNARY && n->unary.op == UOP_DEREF) return true;
    return false;
}

static Node *parse_assignment(Parser *p) {
    Node *left = parse_ternary(p);
    if (check(p, TOK_EQ)) {
        Token eq = advance(p);
        Node *val = parse_assignment(p);
        if (!is_valid_lvalue(left))
            syntax_error(p, eq, "invalid assignment target");
        Node *n = node_new(p->arena, NK_ASSIGN, eq.line, eq.col);
        n->assign.target = left;
        n->assign.value  = val;
        return n;
    }
    BinOp cop;
    if (is_compound_assign(peek(p).type, &cop)) {
        Token op_tok = advance(p);
        Node *val = parse_assignment(p);
        if (!is_valid_lvalue(left))
            syntax_error(p, op_tok, "invalid assignment target");
        Node *n = node_new(p->arena, NK_COMPOUND_ASSIGN, op_tok.line, op_tok.col);
        n->compound_assign.op     = cop;
        n->compound_assign.target = left;
        n->compound_assign.value  = val;
        return n;
    }
    return left;
}

static Node *parse_range(Parser *p) {
    Node *left = parse_binary_level(p, 0);
    if (check(p, TOK_DOT_DOT) || check(p, TOK_DOT_DOT_EQ)) {
        Token op = advance(p);
        bool incl = op.type == TOK_DOT_DOT_EQ;
        Node *right = parse_binary_level(p, 0);
        Node *rn = node_new(p->arena, NK_RANGE, op.line, op.col);
        rn->range.start = left;
        rn->range.end = right;
        rn->range.inclusive = incl;
        return rn;
    }
    return left;
}

static Node *parse_null_coalesce(Parser *p) {
    Node *n = parse_range(p);
    while (check(p, TOK_QUESTION_QUESTION)) {
        Token qtok = advance(p);
        Node *rhs = parse_range(p);
        Node *b = node_new(p->arena, NK_NULL_COALESCE, qtok.line, qtok.col);
        b->binary.left = n;
        b->binary.right = rhs;
        n = b;
    }
    return n;
}

static Node *parse_ternary(Parser *p) {
    Node *cond = parse_null_coalesce(p);
    if (match(p, TOK_QUESTION)) {
        Token qtok = peek(p);
        Node *then_e = parse_expr(p);
        expect(p, TOK_COLON);
        Node *else_e = parse_ternary(p);
        Node *n = node_new(p->arena, NK_TERNARY, qtok.line, qtok.col);
        n->ternary.cond      = cond;
        n->ternary.then_expr = then_e;
        n->ternary.else_expr = else_e;
        return n;
    }
    return cond;
}

/* Left-associative binary levels, loosest first. */
typedef struct { TokenType tok; BinOp op; } BinTok;
static const BinTok LV_OR[]      = { { TOK_PIPE_PIPE, BOP_OR      }, { TOK_EOF, 0 } };
static const BinTok LV_AND[]     = { { TOK_AMP_AMP,   BOP_AND     }, { TOK_EOF, 0 } };
static const BinTok LV_BIT_OR[]  = { { TOK_PIPE,      BOP_BIT_OR  }, { TOK_EOF, 0 } };
static const BinTok LV_BIT_XOR[] = { { TOK_CARET,     BOP_BIT_XOR }, { TOK_EOF, 0 } };
static const BinTok LV_BIT_AND[] = { { TOK_AMP,       BOP_BIT_AND }, { TOK_EOF, 0 } };
static const BinTok LV_EQ[]      = { { TOK_EQ_EQ, BOP_EQ }, { TOK_BANG_EQ, BOP_NE },
                                     { TOK_EOF, 0 } };
static const BinTok LV_CMP[]     = { { TOK_LT, BOP_LT }, { TOK_LT_EQ, BOP_LE },
                                     { TOK_GT, BOP_GT }, { TOK_GT_EQ, BOP_GE },
                                     { TOK_EOF, 0 } };
static const BinTok LV_SHIFT[]   = { { TOK_SHL, BOP_SHL }, { TOK_SHR, BOP_SHR },
                                     { TOK_EOF, 0 } };
static const BinTok LV_ADD[]     = { { TOK_PLUS, BOP_ADD }, { TOK_MINUS, BOP_SUB },
                                     { TOK_EOF, 0 } };
static const BinTok LV_MUL[]     = { { TOK_STAR, BOP_MUL }, { TOK_SLASH, BOP_DIV },
                                     { TOK_PERCENT, BOP_MOD }, { TOK_EOF, 0 } };

static const BinTok *const LEVELS[] = {
    LV_OR, LV_AND, LV_BIT_OR, LV_BIT_XOR, LV_BIT_AND,
    LV_EQ, LV_CMP, LV_SHIFT, LV_ADD, LV_MUL
};
#define NUM_LEVELS ((int)(sizeof LEVELS / sizeof LEVELS[0]))

static Node *parse_binary_level(Parser *p, int level) {
    if (level >= NUM_LEVELS) return parse_unary(p);

    Node *left = parse_binary_level(p, level + 1);
    for (;;) {
        TokenType t = peek(p).type;
        const BinTok *hit = NULL;
        for (const BinTok *e = LEVELS[level]; e->tok != TOK_EOF; e++)
            if (e->tok == t) { hit = e; break; }
        if (!hit) return left;

        Token op = advance(p);
        Node *right = parse_binary_level(p, level + 1);
        Node *n = node_new(p->arena, NK_BINARY, op.line, op.col);
        n->binary.op = hit->op;
        n->binary.left = left;
        n->binary.right = right;
        left = n;
    }
}

static Node *parse_unary(Parser *p) {
    if (check(p, TOK_MINUS) || check(p, TOK_BANG) || check(p, TOK_TILDE) ||
        check(p, TOK_AMP) || check(p, TOK_STAR) ||
        check(p, TOK_PLUS_PLUS) || check(p, TOK_MINUS_MINUS)) {
        Token op = advance(p);
        Node *operand = parse_unary(p);
        Node *n = node_new(p->arena, NK_UNARY, op.line, op.col);
        switch (op.type) {
            case TOK_MINUS:       n->unary.op = UOP_NEG;     break;
            case TOK_BANG:        n->unary.op = UOP_NOT;     break;
            case TOK_TILDE:       n->unary.op = UOP_BIT_NOT; break;
            case TOK_AMP:         n->unary.op = UOP_ADDR_OF; break;
            case TOK_STAR:        n->unary.op = UOP_DEREF;   break;
            case TOK_PLUS_PLUS:   n->unary.op = UOP_PRE_INC; break;
            case TOK_MINUS_MINUS: n->unary.op = UOP_PRE_DEC; break;
            default:              n->unary.op = UOP_NEG;     break;
        }
        n->unary.operand = operand;
        return n;
    }
    return parse_postfix(p);
}

static Node *parse_postfix(Parser *p) {
    Node *node = parse_primary(p);
    for (;;) {
        if (check(p, TOK_LBRACKET)) {
            Token lb = advance(p);
            Node *idx = parse_expr(p);
            expect(p, TOK_RBRACKET);
            Node *n = node_new(p->arena, NK_INDEX, lb.line, lb.col);
            n->index.array = node;
            n->index.index = idx;
            node = n;
        } else if (check(p, TOK_DOT)) {
            Token dot = advance(p);
            Token fn;
            if (peek(p).type == TOK_IDENT || (peek(p).type >= TOK_EDU && peek(p).type <= TOK_IN))
                fn = advance(p);
            else
                fn = expect(p, TOK_IDENT);
            if (check(p, TOK_LPAREN)) {
                advance(p);
                NodeListBuilder b = {0};
                if (!check(p, TOK_RPAREN)) {
                    nlb_push(p->arena, &b, parse_expr(p));
                    while (match(p, TOK_COMMA))
                        nlb_push(p->arena, &b, parse_expr(p));
                }
                expect(p, TOK_RPAREN);
                Node *n = node_new(p->arena, NK_METHOD_CALL, dot.line, dot.col);
                n->method_call.object   = node;
                n->method_call.method   = lexeme(p, fn);
                n->method_call.args     = b.head;
                n->method_call.is_arrow = false;
                node = n;
            } else {
                Node *n = node_new(p->arena, NK_MEMBER, dot.line, dot.col);
                n->member.object = node;
                n->member.field  = lexeme(p, fn);
                node = n;
            }
        } else if (check(p, TOK_ARROW)) {
            Token arr = advance(p);
            Token fn;
            if (peek(p).type == TOK_IDENT || (peek(p).type >= TOK_EDU && peek(p).type <= TOK_IN))
                fn = advance(p);
            else
                fn = expect(p, TOK_IDENT);
            if (check(p, TOK_LPAREN)) {
                advance(p);
                NodeListBuilder b = {0};
                if (!check(p, TOK_RPAREN)) {
                    nlb_push(p->arena, &b, parse_expr(p));
                    while (match(p, TOK_COMMA))
                        nlb_push(p->arena, &b, parse_expr(p));
                }
                expect(p, TOK_RPAREN);
                Node *n = node_new(p->arena, NK_METHOD_CALL, arr.line, arr.col);
                n->method_call.object   = node;
                n->method_call.method   = lexeme(p, fn);
                n->method_call.args     = b.head;
                n->method_call.is_arrow = true;
                node = n;
            } else {
                Node *n = node_new(p->arena, NK_ARROW, arr.line, arr.col);
                n->arrow.object = node;
                n->arrow.field  = lexeme(p, fn);
                node = n;
            }
        } else if (check(p, TOK_LPAREN)) {
            Token lp = advance(p);
            NodeListBuilder b = {0};
            if (!check(p, TOK_RPAREN)) {
                nlb_push(p->arena, &b, parse_expr(p));
                while (match(p, TOK_COMMA))
                    nlb_push(p->arena, &b, parse_expr(p));
            }
            expect(p, TOK_RPAREN);
            if (node->kind != NK_IDENT)
                syntax_error(p, lp, "only named functions can be called");
            Node *n = node_new(p->arena, NK_CALL, node->line, node->col);
            n->call.name = node->kind == NK_IDENT ? node->ident.name : "?";
            n->call.args = b.head;
            node = n;
        } else if (match(p, TOK_PLUS_PLUS)) {
            Node *n = node_new(p->arena, NK_POST_INC, node->line, node->col);
            n->inc_dec.target = node;
            node = n;
        } else if (match(p, TOK_MINUS_MINUS)) {
            Node *n = node_new(p->arena, NK_POST_DEC, node->line, node->col);
            n->inc_dec.target = node;
            node = n;
        } else {
            return node;
        }
    }
}

static Node *parse_fstring(Parser *p, Token tok) {
    Node *res = node_new(p->arena, NK_STRING_INTERP, tok.line, tok.col);
    NodeListBuilder b = {0};
    const char *s = tok.val.str_val;
    while (*s) {
        if (*s == '\\' && (s[1] == '{' || s[1] == '}')) {
            s++;
            char *lit = arena_alloc(p->arena, 2);
            lit[0] = *s++;
            lit[1] = '\0';
            Node *lit_node = node_new(p->arena, NK_STRING_LIT, tok.line, tok.col);
            lit_node->string_lit.value = lit;
            nlb_push(p->arena, &b, lit_node);
        } else if (*s == '{' && s[1] == '{') {
            s += 2;
            Node *lit_node = node_new(p->arena, NK_STRING_LIT, tok.line, tok.col);
            lit_node->string_lit.value = "{";
            nlb_push(p->arena, &b, lit_node);
        } else if (*s == '}' && s[1] == '}') {
            s += 2;
            Node *lit_node = node_new(p->arena, NK_STRING_LIT, tok.line, tok.col);
            lit_node->string_lit.value = "}";
            nlb_push(p->arena, &b, lit_node);
        } else if (*s == '{') {
            s++;
            const char *expr_start = s;
            int depth = 1;
            while (*s && depth > 0) {
                if (*s == '"') {
                    s++;
                    while (*s && *s != '"') {
                        if (*s == '\\' && s[1]) s++;
                        s++;
                    }
                    if (*s == '"') s++;
                    continue;
                }
                if (*s == '\'') {
                    s++;
                    while (*s && *s != '\'') {
                        if (*s == '\\' && s[1]) s++;
                        s++;
                    }
                    if (*s == '\'') s++;
                    continue;
                }
                if (*s == '{') depth++;
                else if (*s == '}') depth--;
                if (depth > 0) s++;
            }
            if (depth > 0) {
                syntax_error(p, tok, "unterminated '{' in interpolated string");
                break;
            }
            size_t expr_len = (size_t)(s - expr_start);
            char *expr_str = arena_strndup(p->arena, expr_start, expr_len);
            s++; /* skip '}' */

            Lexer sub_lex;
            lexer_init(&sub_lex, expr_str, p->lexer->filename, p->arena);
            sub_lex.line = tok.line;
            sub_lex.col = tok.col;
            Parser sub_p = *p;
            sub_p.lexer = &sub_lex;
            Node *expr_node = parse_expr(&sub_p);
            nlb_push(p->arena, &b, expr_node);
        } else {
            const char *lit_start = s;
            while (*s && *s != '{' && !(*s == '\\' && (s[1] == '{' || s[1] == '}')) &&
                   !(*s == '}' && s[1] == '}'))
                s++;
            size_t lit_len = (size_t)(s - lit_start);
            char *lit_str = arena_strndup(p->arena, lit_start, lit_len);
            Node *lit_node = node_new(p->arena, NK_STRING_LIT, tok.line, tok.col);
            lit_node->string_lit.value = lit_str;
            nlb_push(p->arena, &b, lit_node);
        }
    }
    res->string_interp.parts = b.head;
    return res;
}

static Node *parse_primary(Parser *p) {
    Token tok = peek(p);
    switch (tok.type) {
        case TOK_INT_LIT: {
            advance(p);
            Node *n = node_new(p->arena, NK_INT_LIT, tok.line, tok.col);
            n->int_lit.value = tok.val.int_val;
            return n;
        }
        case TOK_FLOAT_LIT: {
            advance(p);
            Node *n = node_new(p->arena, NK_FLOAT_LIT, tok.line, tok.col);
            n->float_lit.value = tok.val.float_val;
            return n;
        }
        case TOK_STRING_LIT: {
            advance(p);
            Node *n = node_new(p->arena, NK_STRING_LIT, tok.line, tok.col);
            n->string_lit.value = tok.val.str_val;
            return n;
        }
        case TOK_FSTRING_LIT: {
            advance(p);
            return parse_fstring(p, tok);
        }
        case TOK_CHAR_LIT: {
            advance(p);
            Node *n = node_new(p->arena, NK_CHAR_LIT, tok.line, tok.col);
            n->char_lit.value = (char)tok.val.int_val;
            return n;
        }
        case TOK_NULL: {
            advance(p);
            return node_new(p->arena, NK_NULL_LIT, tok.line, tok.col);
        }
        case TOK_SUPER: {
            advance(p);
            return node_new(p->arena, NK_SUPER, tok.line, tok.col);
        }
        case TOK_SIZEOF: {
            Token kw = advance(p);
            expect(p, TOK_LPAREN);
            Node *n = node_new(p->arena, NK_SIZEOF, kw.line, kw.col);
            if (is_type_ahead(p)) {
                n->sizeof_expr.target_type = parse_type(p);
                n->sizeof_expr.expr = NULL;
            } else {
                n->sizeof_expr.target_type = NULL;
                n->sizeof_expr.expr = parse_expr(p);
            }
            expect(p, TOK_RPAREN);
            return n;
        }
        case TOK_CAST: {
            Token kw = advance(p);
            expect(p, TOK_LT);
            TypeInfo *tgt = parse_type(p);
            expect(p, TOK_GT);
            expect(p, TOK_LPAREN);
            Node *e = parse_expr(p);
            expect(p, TOK_RPAREN);
            Node *n = node_new(p->arena, NK_CAST, kw.line, kw.col);
            n->cast_expr.target_type = tgt;
            n->cast_expr.expr = e;
            return n;
        }
        case TOK_THIS: {
            advance(p);
            Node *n = node_new(p->arena, NK_THIS, tok.line, tok.col);
            return n;
        }
        case TOK_NEW: {
            advance(p);
            Token cn = expect(p, TOK_IDENT);
            expect(p, TOK_LPAREN);
            NodeListBuilder b = {0};
            if (!check(p, TOK_RPAREN)) {
                nlb_push(p->arena, &b, parse_expr(p));
                while (match(p, TOK_COMMA))
                    nlb_push(p->arena, &b, parse_expr(p));
            }
            expect(p, TOK_RPAREN);
            Node *n = node_new(p->arena, NK_NEW, tok.line, tok.col);
            n->new_expr.class_name = lexeme(p, cn);
            n->new_expr.args       = b.head;
            return n;
        }
        case TOK_IDENT: {
            advance(p);
            bool is_true  = tok.length == 4 && !memcmp(tok.start, "true", 4);
            bool is_false = tok.length == 5 && !memcmp(tok.start, "false", 5);
            if (is_true || is_false) {
                Node *n = node_new(p->arena, NK_BOOL_LIT, tok.line, tok.col);
                n->bool_lit.value = is_true;
                return n;
            }
            Node *n = node_new(p->arena, NK_IDENT, tok.line, tok.col);
            n->ident.name = lexeme(p, tok);
            return n;
        }
        case TOK_LPAREN: {
            advance(p);
            if (is_type_ahead(p)) {
                TypeInfo *tgt = parse_type(p);
                if (match(p, TOK_RPAREN)) {
                    Node *e = parse_unary(p);
                    Node *n = node_new(p->arena, NK_CAST, tok.line, tok.col);
                    n->cast_expr.target_type = tgt;
                    n->cast_expr.expr = e;
                    return n;
                }
            }
            Node *inner = parse_expr(p);
            expect(p, TOK_RPAREN);
            return inner;
        }
        case TOK_LBRACKET: {
            advance(p);
            NodeListBuilder b = {0};
            if (!check(p, TOK_RBRACKET)) {
                nlb_push(p->arena, &b, parse_expr(p));
                while (match(p, TOK_COMMA))
                    nlb_push(p->arena, &b, parse_expr(p));
            }
            expect(p, TOK_RBRACKET);
            Node *n = node_new(p->arena, NK_ARRAY_INIT, tok.line, tok.col);
            n->array_init.elements = b.head;
            return n;
        }
        default: {
            if (tok.type == TOK_ERROR) {
                advance(p);
                p->panic = true;
            } else {
                syntax_error(p, tok, "expected an expression but found %s",
                             token_type_name(tok.type));
            }
            Node *dummy = node_new(p->arena, NK_INT_LIT, tok.line, tok.col);
            return dummy;
        }
    }
}
