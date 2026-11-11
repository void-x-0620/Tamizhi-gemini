/*
 * lexer.c — Single-pass hand-written lexer.
 *
 *  • One linear scan, one pointer; no regex, no backtracking.
 *  • Tokens point into the source buffer (zero copies for identifiers).
 *  • Only string literals allocate (exactly sized, from the arena).
 *  • Keywords are matched by a first-character switch + memcmp.
 *  • '#' starts a comment that runs to end of line.
 */
#include "lexer.h"
#include <errno.h>

/* ─── Keyword recognition ──────────────────────────────────────────────── */
static bool kw_eq(const char *s, int len, const char *kw) {
    return (int)strlen(kw) == len && memcmp(s, kw, (size_t)len) == 0;
}

static const struct { const char *word; TokenType type; } KEYWORDS[] = {
    /* Tanglish keywords */
    { "aga",        TOK_AGA        },
    { "alavu",      TOK_SIZEOF     },
    { "azhi",       TOK_DELETE     },
    { "edu",        TOK_EDU        },
    { "enumaripu",  TOK_ENUM       },
    { "illana",     TOK_ILLANA     },
    { "irundha",    TOK_IRUNDHA    },
    { "kattamaipu", TOK_KATTAMAIPU },
    { "kudu",       TOK_KUDU       },
    { "maradha",    TOK_CONST      },
    { "maru",       TOK_NULL       },
    { "mathu",      TOK_CAST       },
    { "moolam",     TOK_SUPER      },
    { "muthal",     TOK_DEFAULT    },
    { "nilai",      TOK_CASE       },
    { "nilaiyana",  TOK_STATIC     },
    { "ondrippu",   TOK_UNION      },
    { "padigal",    TOK_FOR        },
    { "pathi",      TOK_PATHI      },
    { "pinbu",      TOK_DEFER      },
    { "pudhiya",    TOK_NEW        },
    { "sei",        TOK_DO         },
    { "serku",      TOK_SERKU      },
    { "seyal",      TOK_SEYAL      },
    { "sutri",      TOK_SUTRI      },
    { "suya",       TOK_THIS       },
    { "thai",       TOK_SUPER      },
    { "therivu",    TOK_SWITCH     },
    { "thodar",     TOK_THODAR     },
    { "udai",       TOK_UDAI       },
    { "ul",         TOK_IN         },
    { "vaguppu",    TOK_CLASS      },
    { "veli",       TOK_VELI       },

    /* C / English keywords */
    { "break",      TOK_UDAI       },
    { "case",       TOK_CASE       },
    { "cast",       TOK_CAST       },
    { "class",      TOK_CLASS      },
    { "const",      TOK_CONST      },
    { "continue",   TOK_THODAR     },
    { "default",    TOK_DEFAULT    },
    { "defer",      TOK_DEFER      },
    { "delete",     TOK_DELETE     },
    { "do",         TOK_DO         },
    { "else",       TOK_ELSE       },
    { "enum",       TOK_ENUM       },
    { "extern",     TOK_VELI       },
    { "fn",         TOK_SEYAL      },
    { "for",        TOK_FOR        },
    { "func",       TOK_SEYAL      },
    { "function",   TOK_SEYAL      },
    { "if",         TOK_IF         },
    { "import",     TOK_SERKU      },
    { "in",         TOK_IN         },
    { "let",        TOK_EDU        },
    { "new",        TOK_NEW        },
    { "null",       TOK_NULL       },
    { "print",      TOK_PATHI      },
    { "println",    TOK_PATHI      },
    { "return",     TOK_KUDU       },
    { "sizeof",     TOK_SIZEOF     },
    { "static",     TOK_STATIC     },
    { "struct",     TOK_KATTAMAIPU },
    { "super",      TOK_SUPER      },
    { "switch",     TOK_SWITCH     },
    { "this",       TOK_THIS       },
    { "typedef",    TOK_TYPEDEF    },
    { "union",      TOK_UNION      },
    { "var",        TOK_EDU        },
    { "while",      TOK_SUTRI      },
};

static TokenType keyword_lookup(const char *s, int len) {
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++) {
        const char *w = KEYWORDS[i].word;
        if (w[0] == s[0] && kw_eq(s, len, w)) return KEYWORDS[i].type;
    }
    return TOK_IDENT;
}

/* ─── Character helpers ────────────────────────────────────────────────── */
static inline bool at_end(Lexer *L)   { return *L->current == '\0'; }
static inline char peek_ch(Lexer *L)  { return *L->current; }
static inline char peek_ch2(Lexer *L) { return L->current[0] ? L->current[1] : '\0'; }

static char advance_ch(Lexer *L) {
    char c = *L->current++;
    if (c == '\n') { L->line++; L->col = 1; } else { L->col++; }
    return c;
}

static void skip_trivia(Lexer *L) {
    for (;;) {
        char c = peek_ch(L);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance_ch(L);
        } else if (c == '#') {
            while (!at_end(L) && peek_ch(L) != '\n') advance_ch(L);
        } else {
            break;
        }
    }
}

static Token make_token(Lexer *L, TokenType type, const char *start,
                        int line, int col) {
    Token t;
    memset(&t, 0, sizeof t);
    t.type   = type;
    t.start  = start;
    t.length = (int)(L->current - start);
    t.line   = line;
    t.col    = col;
    return t;
}

/* String literal. Pass 1 finds the closing quote so the unescaped result can
 * be allocated at its exact maximum size; pass 2 decodes the escapes. */
static Token lex_string(Lexer *L, const char *start, int line, int col) {
    const char *p = L->current;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        p++;
    }
    if (*p != '"') {
        report_error(L->filename, line, col, "unterminated string literal");
        while (!at_end(L)) advance_ch(L);
        return make_token(L, TOK_ERROR, start, line, col);
    }

    char *buf = arena_alloc(L->arena, (size_t)(p - L->current) + 1);
    size_t n = 0;
    while (peek_ch(L) != '"') {
        int ecol = L->col;
        char c = advance_ch(L);
        if (c == '\\') {
            char e = advance_ch(L);
            switch (e) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"':  c = '"';  break;
                default:
                    report_error(L->filename, L->line, ecol,
                                 "unknown escape sequence '\\%c'", e);
                    c = e;
                    break;
            }
        }
        buf[n++] = c;
    }
    advance_ch(L);                                   /* closing quote */
    buf[n] = '\0';

    Token t = make_token(L, TOK_STRING_LIT, start, line, col);
    t.val.str_val = buf;
    return t;
}

static Token lex_char(Lexer *L, const char *start, int line, int col) {
    if (at_end(L)) {
        report_error(L->filename, line, col, "unterminated character literal");
        return make_token(L, TOK_ERROR, start, line, col);
    }
    char ch = advance_ch(L);
    if (ch == '\'') {
        report_error(L->filename, line, col, "empty character literal");
        return make_token(L, TOK_ERROR, start, line, col);
    }
    if (ch == '\\') {
        if (at_end(L)) {
            report_error(L->filename, line, col, "unterminated character literal");
            return make_token(L, TOK_ERROR, start, line, col);
        }
        char e = advance_ch(L);
        switch (e) {
            case 'n':  ch = '\n'; break;
            case 't':  ch = '\t'; break;
            case 'r':  ch = '\r'; break;
            case '0':  ch = '\0'; break;
            case '\\': ch = '\\'; break;
            case '\'': ch = '\''; break;
            case '"':  ch = '"';  break;
            default:
                report_error(L->filename, line, col,
                             "unknown escape sequence '\\%c'", e);
        }
    }
    if (peek_ch(L) != '\'') {
        report_error(L->filename, line, col, "multi-character or unterminated character literal");
        while (!at_end(L) && peek_ch(L) != '\'' && peek_ch(L) != '\n') advance_ch(L);
        if (peek_ch(L) == '\'') advance_ch(L);
        return make_token(L, TOK_ERROR, start, line, col);
    }
    advance_ch(L); /* closing quote */
    Token t = make_token(L, TOK_CHAR_LIT, start, line, col);
    t.val.int_val = (unsigned char)ch;
    return t;
}

static Token lex_number(Lexer *L, const char *start, int line, int col) {
    /* start[0] is the first digit, which was already consumed before lex_number was called. */
    char first = start[0];
    if (first == '0') {
        if (peek_ch(L) == 'x' || peek_ch(L) == 'X') {
            advance_ch(L); /* consume 'x' */
            int digits = 0;
            while (isxdigit((unsigned char)peek_ch(L))) {
                advance_ch(L);
                digits++;
            }
            if (digits == 0) {
                report_error(L->filename, line, col, "expected hexadecimal digits after '0x'");
            }
            Token t = make_token(L, TOK_INT_LIT, start, line, col);
            t.val.int_val = strtoll(start, NULL, 16);
            return t;
        }
        if (peek_ch(L) == 'b' || peek_ch(L) == 'B') {
            advance_ch(L); /* consume 'b' */
            int digits = 0;
            long long val = 0;
            while (peek_ch(L) == '0' || peek_ch(L) == '1') {
                val = (val << 1) | (advance_ch(L) - '0');
                digits++;
            }
            if (digits == 0) {
                report_error(L->filename, line, col, "expected binary digits after '0b'");
            }
            Token t = make_token(L, TOK_INT_LIT, start, line, col);
            t.val.int_val = val;
            return t;
        }
        if (peek_ch(L) == 'o' || peek_ch(L) == 'O') {
            advance_ch(L); /* consume 'o' */
            int digits = 0;
            while (peek_ch(L) >= '0' && peek_ch(L) <= '7') {
                advance_ch(L);
                digits++;
            }
            if (digits == 0) {
                report_error(L->filename, line, col, "expected octal digits after '0o'");
            }
            Token t = make_token(L, TOK_INT_LIT, start, line, col);
            t.val.int_val = strtoll(start + 2, NULL, 8);
            return t;
        }
    }

    while (isdigit((unsigned char)peek_ch(L))) advance_ch(L);
    bool is_float = false;
    if (peek_ch(L) == '.' && isdigit((unsigned char)peek_ch2(L))) {
        is_float = true;
        advance_ch(L);
        while (isdigit((unsigned char)peek_ch(L))) advance_ch(L);
    }
    if (peek_ch(L) == 'e' || peek_ch(L) == 'E') {
        is_float = true;
        advance_ch(L);
        if (peek_ch(L) == '+' || peek_ch(L) == '-') advance_ch(L);
        while (isdigit((unsigned char)peek_ch(L))) advance_ch(L);
    }
    Token t = make_token(L, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT,
                         start, line, col);
    if (is_float) {
        t.val.float_val = strtod(start, NULL);
    } else {
        errno = 0;
        t.val.int_val = strtoll(start, NULL, 10);
        if (errno == ERANGE) {
            report_error(L->filename, line, col,
                         "integer literal is too large");
            t.type = TOK_ERROR;
        }
    }
    return t;
}

/* ─── Public API ───────────────────────────────────────────────────────── */
void lexer_init(Lexer *L, const char *source, const char *filename,
                Arena *arena) {
    L->source = L->current = source;
    L->line = 1;
    L->col = 1;
    L->filename = filename;
    L->has_peeked = false;
    L->arena = arena;
}

Token lexer_next(Lexer *L) {
    if (L->has_peeked) {
        L->has_peeked = false;
        return L->peeked;
    }

    skip_trivia(L);
    const char *start = L->current;
    int line = L->line, col = L->col;
    if (at_end(L)) return make_token(L, TOK_EOF, start, line, col);

    char c = advance_ch(L);

    if (isdigit((unsigned char)c)) return lex_number(L, start, line, col);
    if (c == 'f' && peek_ch(L) == '"') {
        advance_ch(L);
        Token t = lex_string(L, start, line, col);
        t.type = TOK_FSTRING_LIT;
        return t;
    }
    if (c == '"')                  return lex_string(L, start, line, col);
    if (c == '\'')                 return lex_char(L, start, line, col);

    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek_ch(L)) || peek_ch(L) == '_')
            advance_ch(L);
        int len = (int)(L->current - start);
        return make_token(L, keyword_lookup(start, len), start, line, col);
    }

#define TOK(t) return make_token(L, (t), start, line, col)
#define TWO(next, yes, no) \
    if (peek_ch(L) == (next)) { advance_ch(L); TOK(yes); } TOK(no)

    switch (c) {
        case '(': TOK(TOK_LPAREN);    case ')': TOK(TOK_RPAREN);
        case '{': TOK(TOK_LBRACE);    case '}': TOK(TOK_RBRACE);
        case '[': TOK(TOK_LBRACKET);  case ']': TOK(TOK_RBRACKET);
        case ';': TOK(TOK_SEMICOLON); case ',': TOK(TOK_COMMA);
        case '.':
            if (peek_ch(L) == '.') {
                advance_ch(L);
                if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_DOT_DOT_EQ); }
                TOK(TOK_DOT_DOT);
            }
            TOK(TOK_DOT);
        case ':': TOK(TOK_COLON);
        case '@': TOK(TOK_AT);
        case '?':
            if (peek_ch(L) == '?') { advance_ch(L); TOK(TOK_QUESTION_QUESTION); }
            TOK(TOK_QUESTION);
        case '~': TOK(TOK_TILDE);
        case '+':
            if (peek_ch(L) == '+') { advance_ch(L); TOK(TOK_PLUS_PLUS); }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_PLUS_EQ); }
            TOK(TOK_PLUS);
        case '-':
            if (peek_ch(L) == '>') { advance_ch(L); TOK(TOK_ARROW); }
            if (peek_ch(L) == '-') { advance_ch(L); TOK(TOK_MINUS_MINUS); }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_MINUS_EQ); }
            TOK(TOK_MINUS);
        case '*':
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_STAR_EQ); }
            TOK(TOK_STAR);
        case '/':
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_SLASH_EQ); }
            TOK(TOK_SLASH);
        case '%':
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_PERCENT_EQ); }
            TOK(TOK_PERCENT);
        case '^':
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_CARET_EQ); }
            TOK(TOK_CARET);
        case '!': TWO('=', TOK_BANG_EQ, TOK_BANG);
        case '=': TWO('=', TOK_EQ_EQ,   TOK_EQ);
        case '<':
            if (peek_ch(L) == '<') {
                advance_ch(L);
                if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_SHL_EQ); }
                TOK(TOK_SHL);
            }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_LT_EQ); }
            TOK(TOK_LT);
        case '>':
            if (peek_ch(L) == '>') {
                advance_ch(L);
                if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_SHR_EQ); }
                TOK(TOK_SHR);
            }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_GT_EQ); }
            TOK(TOK_GT);
        case '&':
            if (peek_ch(L) == '&') { advance_ch(L); TOK(TOK_AMP_AMP); }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_AMP_EQ); }
            TOK(TOK_AMP);
        case '|':
            if (peek_ch(L) == '|') { advance_ch(L); TOK(TOK_PIPE_PIPE); }
            if (peek_ch(L) == '=') { advance_ch(L); TOK(TOK_PIPE_EQ); }
            TOK(TOK_PIPE);
        default: break;
    }
#undef TWO
#undef TOK

    if (isprint((unsigned char)c))
        report_error(L->filename, line, col, "unexpected character '%c'", c);
    else
        report_error(L->filename, line, col,
                     "unexpected byte 0x%02x", (unsigned char)c);
    return make_token(L, TOK_ERROR, start, line, col);
}

Token lexer_peek(Lexer *L) {
    if (!L->has_peeked) {
        L->peeked = lexer_next(L);
        L->has_peeked = true;
    }
    return L->peeked;
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_INT_LIT:     return "integer literal";
        case TOK_FLOAT_LIT:   return "float literal";
        case TOK_STRING_LIT:  return "string literal";
        case TOK_CHAR_LIT:    return "character literal";
        case TOK_FSTRING_LIT: return "interpolated string literal";
        case TOK_IDENT:       return "identifier";
        case TOK_LPAREN:      return "'('";
        case TOK_RPAREN:      return "')'";
        case TOK_LBRACE:      return "'{'";
        case TOK_RBRACE:      return "'}'";
        case TOK_LBRACKET:    return "'['";
        case TOK_RBRACKET:    return "']'";
        case TOK_SEMICOLON:   return "';'";
        case TOK_COMMA:       return "','";
        case TOK_DOT:         return "'.'";
        case TOK_DOT_DOT:     return "'..'";
        case TOK_DOT_DOT_EQ:  return "'..='";
        case TOK_COLON:       return "':'";
        case TOK_AT:          return "'@'";
        case TOK_CARET:       return "'^'";
        case TOK_PLUS:        return "'+'";
        case TOK_MINUS:       return "'-'";
        case TOK_STAR:        return "'*'";
        case TOK_SLASH:       return "'/'";
        case TOK_PERCENT:     return "'%'";
        case TOK_BANG:        return "'!'";
        case TOK_BANG_EQ:     return "'!='";
        case TOK_EQ:          return "'='";
        case TOK_EQ_EQ:       return "'=='";
        case TOK_LT:          return "'<'";
        case TOK_LT_EQ:       return "'<='";
        case TOK_GT:          return "'>'";
        case TOK_GT_EQ:       return "'>='";
        case TOK_AMP_AMP:     return "'&&'";
        case TOK_PIPE_PIPE:   return "'||'";
        case TOK_ARROW:       return "'->'";
        case TOK_PLUS_PLUS:   return "'++'";
        case TOK_MINUS_MINUS: return "'--'";
        case TOK_AMP:         return "'&'";
        case TOK_PIPE:        return "'|'";
        case TOK_TILDE:       return "'~'";
        case TOK_QUESTION:    return "'?'";
        case TOK_QUESTION_QUESTION: return "'?\\?'";
        case TOK_SHL:         return "'<<'";
        case TOK_SHR:         return "'>>'";
        case TOK_PLUS_EQ:     return "'+='";
        case TOK_MINUS_EQ:    return "'-='";
        case TOK_STAR_EQ:     return "'*='";
        case TOK_SLASH_EQ:    return "'/='";
        case TOK_PERCENT_EQ:  return "'%='";
        case TOK_AMP_EQ:      return "'&='";
        case TOK_PIPE_EQ:     return "'|='";
        case TOK_CARET_EQ:    return "'^='";
        case TOK_SHL_EQ:      return "'<<='";
        case TOK_SHR_EQ:      return "'>>='";
        case TOK_EDU:         return "'edu'";
        case TOK_SEYAL:       return "'seyal'";
        case TOK_AGA:         return "'aga'";
        case TOK_IRUNDHA:     return "'irundha'";
        case TOK_ILLANA:      return "'illana'";
        case TOK_SUTRI:       return "'sutri'";
        case TOK_UDAI:        return "'udai'";
        case TOK_THODAR:      return "'thodar'";
        case TOK_KUDU:        return "'kudu'";
        case TOK_PATHI:       return "'pathi'";
        case TOK_KATTAMAIPU:  return "'kattamaipu'";
        case TOK_SERKU:       return "'serku'";
        case TOK_VELI:        return "'veli'";
        case TOK_CLASS:       return "'class'";
        case TOK_THIS:        return "'this'";
        case TOK_SUPER:       return "'super'";
        case TOK_NEW:         return "'new'";
        case TOK_ENUM:        return "'enum'";
        case TOK_UNION:       return "'union'";
        case TOK_CONST:       return "'const'";
        case TOK_STATIC:      return "'static'";
        case TOK_TYPEDEF:     return "'typedef'";
        case TOK_FOR:         return "'for'";
        case TOK_DO:          return "'do'";
        case TOK_SWITCH:      return "'switch'";
        case TOK_CASE:        return "'case'";
        case TOK_DEFAULT:     return "'default'";
        case TOK_IF:          return "'if'";
        case TOK_ELSE:        return "'else'";
        case TOK_NULL:        return "'null'";
        case TOK_SIZEOF:      return "'sizeof'";
        case TOK_CAST:        return "'cast'";
        case TOK_DELETE:      return "'delete'";
        case TOK_DEFER:       return "'defer'";
        case TOK_IN:          return "'in'";
        case TOK_EOF:         return "end of file";
        case TOK_ERROR:       return "invalid token";
    }
    return "<unknown>";
}
