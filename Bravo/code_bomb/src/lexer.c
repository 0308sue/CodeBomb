/*
 * lexer.c - C tokenizer for Code Foundry
 * lexer.c - Code Foundry용 C 토크나이저
 */
#include "lexer.h"

#include <limits.h>
#include <string.h>

static const struct {
    const char *word;
    TokenKind   kind;
} KEYWORDS[] = {
    { "int", TK_INT },       { "void", TK_VOID },   { "if", TK_IF },
    { "else", TK_ELSE },     { "while", TK_WHILE }, { "for", TK_FOR },
    { "return", TK_RETURN }, { "break", TK_BREAK }, { "continue", TK_CONTINUE },

    /* Real C keywords (and common stdbool names) that unlock later.
     * 나중에 열리는 실제 C 키워드 (그리고 자주 쓰는 stdbool 이름). */
    { "auto", TK_RESERVED },   { "case", TK_RESERVED },
    { "char", TK_RESERVED },   { "const", TK_RESERVED },
    { "default", TK_RESERVED }, { "do", TK_RESERVED },
    { "double", TK_RESERVED }, { "enum", TK_RESERVED },
    { "extern", TK_RESERVED }, { "float", TK_RESERVED },
    { "goto", TK_RESERVED },   { "inline", TK_RESERVED },
    { "long", TK_RESERVED },   { "register", TK_RESERVED },
    { "restrict", TK_RESERVED }, { "short", TK_RESERVED },
    { "signed", TK_RESERVED }, { "sizeof", TK_RESERVED },
    { "static", TK_RESERVED }, { "struct", TK_RESERVED },
    { "switch", TK_RESERVED }, { "typedef", TK_RESERVED },
    { "union", TK_RESERVED },  { "unsigned", TK_RESERVED },
    { "volatile", TK_RESERVED }, { "_Bool", TK_RESERVED },
    { "bool", TK_RESERVED },   { "true", TK_RESERVED },
    { "false", TK_RESERVED },
};

static const char *const KIND_TEXT[TK_COUNT] = {
    [TK_EOF] = "the end of the code",
    [TK_ERROR] = "an invalid character",
    [TK_COMMENT] = "a comment",
    [TK_IDENT] = "a name",
    [TK_NUMBER] = "a number",
    [TK_STRING] = "a string",
    [TK_HASH] = "'#'",
    [TK_INT] = "'int'",
    [TK_VOID] = "'void'",
    [TK_IF] = "'if'",
    [TK_ELSE] = "'else'",
    [TK_WHILE] = "'while'",
    [TK_FOR] = "'for'",
    [TK_RETURN] = "'return'",
    [TK_BREAK] = "'break'",
    [TK_CONTINUE] = "'continue'",
    [TK_RESERVED] = "a keyword",
    [TK_LPAREN] = "'('",
    [TK_RPAREN] = "')'",
    [TK_LBRACE] = "'{'",
    [TK_RBRACE] = "'}'",
    [TK_SEMI] = "';'",
    [TK_COMMA] = "','",
    [TK_LBRACKET] = "'['",
    [TK_RBRACKET] = "']'",
    [TK_PLUS] = "'+'",
    [TK_MINUS] = "'-'",
    [TK_STAR] = "'*'",
    [TK_SLASH] = "'/'",
    [TK_PERCENT] = "'%'",
    [TK_ASSIGN] = "'='",
    [TK_PLUS_ASSIGN] = "'+='",
    [TK_MINUS_ASSIGN] = "'-='",
    [TK_STAR_ASSIGN] = "'*='",
    [TK_SLASH_ASSIGN] = "'/='",
    [TK_PERCENT_ASSIGN] = "'%='",
    [TK_INC] = "'++'",
    [TK_DEC] = "'--'",
    [TK_EQ] = "'=='",
    [TK_NE] = "'!='",
    [TK_LT] = "'<'",
    [TK_LE] = "'<='",
    [TK_GT] = "'>'",
    [TK_GE] = "'>='",
    [TK_NOT] = "'!'",
    [TK_AND_AND] = "'&&'",
    [TK_OR_OR] = "'||'",
    [TK_UNSUPPORTED] = "an operator",
};

const char *token_kind_text(TokenKind kind)
{
    if (kind < 0 || kind >= TK_COUNT || !KIND_TEXT[kind]) return "a token";
    return KIND_TEXT[kind];
}

void lexer_init(Lexer *lx, const char *src, int len, bool keep_comments)
{
    lx->src = src;
    lx->len = len;
    lx->pos = 0;
    lx->line = 1;
    lx->col = 1;
    lx->keep_comments = keep_comments;
}

/* Character helpers. We avoid <ctype.h> so behaviour does not depend on
 * the platform's locale.
 * 문자 도우미. 플랫폼 로케일에 따라 동작이 달라지지 않도록 <ctype.h>를
 * 쓰지 않는다. */
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }
static int hex_value(char c)
{
    if (is_digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char peek(const Lexer *lx, int off)
{
    int p = lx->pos + off;
    return (p >= 0 && p < lx->len) ? lx->src[p] : '\0';
}

static void advance(Lexer *lx)
{
    if (lx->pos >= lx->len) return;
    if (lx->src[lx->pos] == '\n') {
        lx->line++;
        lx->col = 1;
    } else {
        lx->col++;
    }
    lx->pos++;
}

static Token finish(const Lexer *lx, Token t, TokenKind kind)
{
    t.kind = kind;
    t.len = lx->pos - t.start;
    return t;
}

static Token fail(const Lexer *lx, Token t, const char *msg)
{
    t = finish(lx, t, TK_ERROR);
    if (t.len == 0) t.len = 1;
    t.error = msg;
    return t;
}

static Token lex_number(Lexer *lx, Token t)
{
    int base = 10;
    int64_t v = 0;
    bool overflow = false;
    bool bad_octal = false;

    if (peek(lx, 0) == '0' && (peek(lx, 1) == 'x' || peek(lx, 1) == 'X') &&
        hex_value(peek(lx, 2)) >= 0) {
        base = 16;
        advance(lx);
        advance(lx);
    } else if (peek(lx, 0) == '0' && is_digit(peek(lx, 1))) {
        /* A leading 0 means octal in C: 010 == 8.
         * C에서 앞자리 0은 8진수를 뜻한다: 010 == 8. */
        base = 8;
    }

    for (;;) {
        char c = peek(lx, 0);
        int d = (base == 16) ? hex_value(c) : (is_digit(c) ? c - '0' : -1);
        if (d < 0) break;
        if (d >= base) bad_octal = true;
        if (!overflow) {
            v = v * base + d;
            if (v > INT32_MAX) overflow = true;
        }
        advance(lx);
    }

    if (peek(lx, 0) == '.') {
        advance(lx);
        while (is_digit(peek(lx, 0))) advance(lx);
        return fail(lx, t, "Decimal numbers like 1.5 need float or double, "
                           "which arrive in a later level");
    }
    if (is_alpha(peek(lx, 0))) {
        while (is_alnum(peek(lx, 0))) advance(lx);
        return fail(lx, t, "This number has letters stuck to it. Names "
                           "cannot start with a digit");
    }
    if (bad_octal)
        return fail(lx, t, "Numbers starting with 0 are octal in C, so they "
                           "can only use the digits 0 to 7");
    if (overflow)
        return fail(lx, t, "This number is too big for an int (the largest "
                           "is 2147483647)");

    t = finish(lx, t, TK_NUMBER);
    t.value = (int32_t)v;
    return t;
}

static Token lex_char(Lexer *lx, Token t)
{
    advance(lx);                          /* opening quote / 여는 따옴표 */
    int v = 0;
    char c = peek(lx, 0);
    if (c == '\\') {
        advance(lx);
        switch (peek(lx, 0)) {
        case 'n':  v = '\n'; break;
        case 't':  v = '\t'; break;
        case 'r':  v = '\r'; break;
        case '0':  v = 0;    break;
        case '\\': v = '\\'; break;
        case '\'': v = '\''; break;
        case '"':  v = '"';  break;
        default:
            while (lx->pos < lx->len && peek(lx, 0) != '\'' &&
                   peek(lx, 0) != '\n')
                advance(lx);
            if (peek(lx, 0) == '\'') advance(lx);
            return fail(lx, t, "Unknown escape sequence. Try \\n, \\t, \\0, "
                               "\\\\ or \\'");
        }
        advance(lx);
    } else if (c == '\'' || c == '\n' || c == '\0') {
        if (c == '\'') advance(lx);
        return fail(lx, t, "A character literal needs one character, "
                           "like 'a'");
    } else {
        v = (unsigned char)c;
        advance(lx);
    }
    if (peek(lx, 0) != '\'') {
        while (lx->pos < lx->len && peek(lx, 0) != '\'' && peek(lx, 0) != '\n')
            advance(lx);
        if (peek(lx, 0) == '\'') advance(lx);
        return fail(lx, t, "A character literal holds exactly one character, "
                           "like 'a'. Text in \"double quotes\" is a string");
    }
    advance(lx);
    t = finish(lx, t, TK_NUMBER);
    t.value = v;
    return t;
}

Token lexer_next(Lexer *lx)
{
    for (;;) {
        /* Skip whitespace / 공백 건너뛰기 */
        while (lx->pos < lx->len) {
            char c = peek(lx, 0);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r' &&
                c != '\f' && c != '\v')
                break;
            advance(lx);
        }

        Token t;
        memset(&t, 0, sizeof t);
        t.start = lx->pos;
        t.line = lx->line;
        t.col = lx->col;

        if (lx->pos >= lx->len) return finish(lx, t, TK_EOF);

        char c = peek(lx, 0);

        /* Comments / 주석 */
        if (c == '/' && peek(lx, 1) == '/') {
            while (lx->pos < lx->len && peek(lx, 0) != '\n') advance(lx);
            if (lx->keep_comments) return finish(lx, t, TK_COMMENT);
            continue;
        }
        if (c == '/' && peek(lx, 1) == '*') {
            advance(lx);
            advance(lx);
            bool closed = false;
            while (lx->pos < lx->len) {
                if (peek(lx, 0) == '*' && peek(lx, 1) == '/') {
                    advance(lx);
                    advance(lx);
                    closed = true;
                    break;
                }
                advance(lx);
            }
            if (lx->keep_comments) return finish(lx, t, TK_COMMENT);
            if (!closed) {
                Token e = fail(lx, t, "This /* comment is never closed with */");
                e.len = 2;
                return e;
            }
            continue;
        }

        /* Preprocessor line: keep the whole line as one token.
         * 전처리 줄: 줄 전체를 하나의 토큰으로 묶는다. */
        if (c == '#') {
            while (lx->pos < lx->len && peek(lx, 0) != '\n') advance(lx);
            return finish(lx, t, TK_HASH);
        }

        if (is_alpha(c)) {
            while (is_alnum(peek(lx, 0))) advance(lx);
            t = finish(lx, t, TK_IDENT);
            for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; ++i) {
                const char *w = KEYWORDS[i].word;
                if ((int)strlen(w) == t.len &&
                    memcmp(w, lx->src + t.start, (size_t)t.len) == 0) {
                    t.kind = KEYWORDS[i].kind;
                    break;
                }
            }
            return t;
        }

        if (is_digit(c)) return lex_number(lx, t);
        if (c == '\'') return lex_char(lx, t);

        if (c == '"') {
            advance(lx);
            while (lx->pos < lx->len && peek(lx, 0) != '"' &&
                   peek(lx, 0) != '\n') {
                if (peek(lx, 0) == '\\') advance(lx);
                advance(lx);
            }
            if (peek(lx, 0) != '"')
                return fail(lx, t, "This string is never closed with \"");
            advance(lx);
            return finish(lx, t, TK_STRING);
        }

        /* Operators and punctuation / 연산자와 구두점 */
        advance(lx);
        char n = peek(lx, 0);
        switch (c) {
        case '(': return finish(lx, t, TK_LPAREN);
        case ')': return finish(lx, t, TK_RPAREN);
        case '{': return finish(lx, t, TK_LBRACE);
        case '}': return finish(lx, t, TK_RBRACE);
        case ';': return finish(lx, t, TK_SEMI);
        case ',': return finish(lx, t, TK_COMMA);
        case '[': return finish(lx, t, TK_LBRACKET);
        case ']': return finish(lx, t, TK_RBRACKET);
        case '+':
            if (n == '+') { advance(lx); return finish(lx, t, TK_INC); }
            if (n == '=') { advance(lx); return finish(lx, t, TK_PLUS_ASSIGN); }
            return finish(lx, t, TK_PLUS);
        case '-':
            if (n == '-') { advance(lx); return finish(lx, t, TK_DEC); }
            if (n == '=') { advance(lx); return finish(lx, t, TK_MINUS_ASSIGN); }
            if (n == '>') { advance(lx); return finish(lx, t, TK_UNSUPPORTED); }
            return finish(lx, t, TK_MINUS);
        case '*':
            if (n == '=') { advance(lx); return finish(lx, t, TK_STAR_ASSIGN); }
            return finish(lx, t, TK_STAR);
        case '/':
            if (n == '=') { advance(lx); return finish(lx, t, TK_SLASH_ASSIGN); }
            return finish(lx, t, TK_SLASH);
        case '%':
            if (n == '=') {
                advance(lx);
                return finish(lx, t, TK_PERCENT_ASSIGN);
            }
            return finish(lx, t, TK_PERCENT);
        case '=':
            if (n == '=') { advance(lx); return finish(lx, t, TK_EQ); }
            return finish(lx, t, TK_ASSIGN);
        case '!':
            if (n == '=') { advance(lx); return finish(lx, t, TK_NE); }
            return finish(lx, t, TK_NOT);
        case '<':
            if (n == '=') { advance(lx); return finish(lx, t, TK_LE); }
            if (n == '<') {
                advance(lx);
                if (peek(lx, 0) == '=') advance(lx);
                return finish(lx, t, TK_UNSUPPORTED);
            }
            return finish(lx, t, TK_LT);
        case '>':
            if (n == '=') { advance(lx); return finish(lx, t, TK_GE); }
            if (n == '>') {
                advance(lx);
                if (peek(lx, 0) == '=') advance(lx);
                return finish(lx, t, TK_UNSUPPORTED);
            }
            return finish(lx, t, TK_GT);
        case '&':
            if (n == '&') { advance(lx); return finish(lx, t, TK_AND_AND); }
            if (n == '=') advance(lx);
            return finish(lx, t, TK_UNSUPPORTED);
        case '|':
            if (n == '|') { advance(lx); return finish(lx, t, TK_OR_OR); }
            if (n == '=') advance(lx);
            return finish(lx, t, TK_UNSUPPORTED);
        case '^':
            if (n == '=') advance(lx);
            return finish(lx, t, TK_UNSUPPORTED);
        case '.': case '?': case ':': case '~':
            return finish(lx, t, TK_UNSUPPORTED);
        default:
            return fail(lx, t, "This character is not part of C");
        }
    }
}
