/*
 * lexer.h — Tokens and lexer interface for Tamizhi.
 *
 * Keywords (Tanglish & English):
 *   edu / let / var, seyal / fn / func / function, aga irundha, illana / else,
 *   sutri / while, udai / break, thodar / continue, kudu / return, pathi / print,
 *   kattamaipu / struct, vaguppu / class, enumaripu / enum, ondrippu / union,
 *   maradha / const, nilaiyana / static, padigal / for, sei / do,
 *   therivu / switch, nilai / case, muthal / default, suya / this, pudhiya / new,
 *   serku / import, veli / extern, typedef
 */
#ifndef LEXER_H
#define LEXER_H

#include "tamizhi.h"

typedef enum {
    TOK_INT_LIT, TOK_FLOAT_LIT, TOK_STRING_LIT, TOK_CHAR_LIT, TOK_IDENT,

    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET, TOK_SEMICOLON, TOK_COMMA,
    TOK_DOT, TOK_COLON, TOK_AT, TOK_CARET,

    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,

    TOK_BANG, TOK_BANG_EQ, TOK_EQ, TOK_EQ_EQ,
    TOK_LT, TOK_LT_EQ, TOK_GT, TOK_GT_EQ,
    TOK_AMP_AMP, TOK_PIPE_PIPE,

    /* Extended operators */
    TOK_ARROW, TOK_PLUS_PLUS, TOK_MINUS_MINUS,
    TOK_AMP, TOK_PIPE, TOK_TILDE, TOK_QUESTION,
    TOK_SHL, TOK_SHR,
    TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ, TOK_PERCENT_EQ,
    TOK_AMP_EQ, TOK_PIPE_EQ, TOK_CARET_EQ, TOK_SHL_EQ, TOK_SHR_EQ,

    /* Core Tanglish keywords */
    TOK_EDU, TOK_SEYAL, TOK_AGA, TOK_IRUNDHA, TOK_ILLANA, TOK_SUTRI,
    TOK_UDAI, TOK_THODAR, TOK_KUDU, TOK_PATHI, TOK_KATTAMAIPU,
    TOK_SERKU, TOK_VELI,

    /* OOP, C types, qualifiers and control flow */
    TOK_CLASS, TOK_THIS, TOK_SUPER, TOK_NEW,
    TOK_ENUM, TOK_UNION, TOK_CONST, TOK_STATIC, TOK_TYPEDEF,
    TOK_FOR, TOK_DO, TOK_SWITCH, TOK_CASE, TOK_DEFAULT,
    TOK_IF, TOK_ELSE,
    TOK_NULL, TOK_SIZEOF, TOK_CAST, TOK_DELETE,
    TOK_DEFER, TOK_IN,

    TOK_EOF, TOK_ERROR,
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;    /* points into the source buffer (not terminated) */
    int         length;
    int         line;
    int         col;
    union {
        long long   int_val;
        double      float_val;
        const char *str_val;      /* arena-owned, NUL-terminated, unescaped */
    } val;
} Token;

typedef struct {
    const char *source;
    const char *current;
    int         line;
    int         col;
    const char *filename;
    Token       peeked;
    bool        has_peeked;
    Arena      *arena;
} Lexer;

void        lexer_init(Lexer *L, const char *source, const char *filename,
                       Arena *arena);
Token       lexer_next(Lexer *L);
Token       lexer_peek(Lexer *L);
const char *token_type_name(TokenType t);

#endif /* LEXER_H */
