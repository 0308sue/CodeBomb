/*
 * lexer.h - turns C source text into tokens
 * lexer.h - C 소스 텍스트를 토큰으로 나눈다
 *
 * Used by the compiler and by the editor's syntax highlighting, so it never
 * gets stuck: every call consumes at least one character until TK_EOF.
 * 컴파일러와 에디터의 문법 강조 양쪽에서 사용하므로 절대 멈추지 않는다:
 * TK_EOF가 나올 때까지 매 호출마다 최소 한 글자를 소비한다.
 *
 * Valid C that the game does not support yet is still recognised
 * (TK_RESERVED, TK_UNSUPPORTED, TK_STRING, TK_HASH) so the parser can say
 * "this arrives in a later level" instead of a confusing syntax error.
 * 게임이 아직 지원하지 않는 올바른 C 문법도 인식한다 (TK_RESERVED 등).
 * 그래서 파서가 헷갈리는 문법 오류 대신 "나중 레벨에서 배운다"고 안내할 수 있다.
 */
#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TK_EOF,
    TK_ERROR,       /* bad input, see Token.error   / 잘못된 입력          */
    TK_COMMENT,     /* only when keep_comments      / keep_comments일 때만 */
    TK_IDENT,
    TK_NUMBER,      /* integer or character literal / 정수 또는 문자 리터럴 */
    TK_STRING,      /* "text" (not supported yet)   / 문자열 (아직 미지원)  */
    TK_HASH,        /* #include ... (whole line)    / # 전처리 줄 전체      */

    /* keywords / 키워드 */
    TK_INT, TK_VOID, TK_IF, TK_ELSE, TK_WHILE, TK_FOR,
    TK_RETURN, TK_BREAK, TK_CONTINUE,
    TK_RESERVED,    /* C keyword not available yet  / 아직 미지원 C 키워드  */

    /* punctuation / 구두점 */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_SEMI, TK_COMMA,
    TK_LBRACKET, TK_RBRACKET,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_ASSIGN, TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_STAR_ASSIGN,
    TK_SLASH_ASSIGN, TK_PERCENT_ASSIGN,
    TK_INC, TK_DEC,
    TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_NOT, TK_AND_AND, TK_OR_OR,
    TK_UNSUPPORTED, /* C operator not available yet  / 아직 미지원 C 연산자 */

    TK_COUNT
} TokenKind;

typedef struct {
    TokenKind   kind;
    int         start;     /* byte offset in source / 소스 안의 바이트 위치 */
    int         len;
    int         line;      /* 1-based / 1부터 시작 */
    int         col;       /* 1-based / 1부터 시작 */
    int32_t     value;     /* TK_NUMBER value / TK_NUMBER의 값 */
    const char *error;     /* TK_ERROR message / TK_ERROR의 메시지 */
} Token;

typedef struct {
    const char *src;
    int         len;
    int         pos;
    int         line;
    int         col;
    bool        keep_comments;
} Lexer;

void        lexer_init(Lexer *lx, const char *src, int len, bool keep_comments);
Token       lexer_next(Lexer *lx);
const char *token_kind_text(TokenKind kind);   /* e.g. "';'" / 예: "';'" */

#endif
