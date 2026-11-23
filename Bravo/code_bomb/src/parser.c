/*
 * parser.c - recursive-descent parser for the Code Foundry C subset
 * parser.c - Code Foundry C 부분 집합을 위한 재귀 하강 파서
 *
 * Errors jump straight back to parse_program() with longjmp, which keeps
 * every parse function free of error-checking boilerplate.
 * 오류가 나면 longjmp로 parse_program()까지 바로 돌아간다. 덕분에 각 파싱
 * 함수에 오류 검사 코드를 반복해서 쓰지 않아도 된다.
 */
#include "parser.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 200

typedef struct {
    Lexer          lx;
    const char    *src;
    Token          cur;
    Token          prev;
    NodePool      *pool;
    CompileResult *res;
    jmp_buf        fail;
    int            depth;
} Parser;

/* ---- error reporting / 오류 보고 ---------------------------------------- */

static void jump(Parser *p, int line, int col, int len)
{
    p->res->ok = false;
    p->res->line = line;
    p->res->col = col;
    p->res->len = len > 0 ? len : 1;
    longjmp(p->fail, 1);
}

static void fail_at(Parser *p, int line, int col, int len, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->res->message, sizeof p->res->message, fmt, ap);
    va_end(ap);
    jump(p, line, col, len);
}

static void fail_tok(Parser *p, Token t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->res->message, sizeof p->res->message, fmt, ap);
    va_end(ap);
    jump(p, t.line, t.col, t.len);
}

/* Position just after the previous token: where a missing ';' or ')'
 * belongs. 이전 토큰 바로 뒤 위치: 빠진 ';'나 ')'가 있어야 할 곳. */
static void fail_after_prev(Parser *p, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->res->message, sizeof p->res->message, fmt, ap);
    va_end(ap);
    Token t = p->prev.len > 0 ? p->prev : p->cur;
    jump(p, t.line, t.col + t.len, 1);
}

static void describe(const Parser *p, Token t, char *buf, size_t n)
{
    const char *s = p->src + t.start;
    int shown = t.len > 24 ? 24 : t.len;
    switch (t.kind) {
    case TK_IDENT:
    case TK_RESERVED:
    case TK_UNSUPPORTED:
        snprintf(buf, n, "'%.*s'", shown, s);
        break;
    case TK_NUMBER:
        snprintf(buf, n, "the number %.*s", shown, s);
        break;
    default:
        snprintf(buf, n, "%s", token_kind_text(t.kind));
        break;
    }
}

/* Called when the current token is not what the grammar needs. Valid C
 * that is simply not unlocked yet gets an encouraging explanation.
 * 현재 토큰이 문법에 맞지 않을 때 호출된다. 아직 열리지 않았을 뿐인
 * 올바른 C 문법에는 격려하는 설명을 보여 준다. */
static void unexpected(Parser *p, const char *expected)
{
    Token t = p->cur;
    const char *s = p->src + t.start;
    switch (t.kind) {
    case TK_RESERVED:
        fail_tok(p, t, "'%.*s' is a C keyword that is not available yet in "
                       "this level", t.len, s);
        break;
    case TK_HASH:
        fail_tok(p, t, "Lines starting with # (like #include) are not needed "
                       "here: the level already provides its functions");
        break;
    case TK_STRING:
        fail_tok(p, t, "Text strings in \"double quotes\" arrive in a later "
                       "level");
        break;
    case TK_UNSUPPORTED:
        if (s[0] == '&' && t.len == 1)
            fail_tok(p, t, "The address operator & arrives with pointers in "
                           "a later level");
        if (s[0] == '.' || (s[0] == '-' && t.len == 2))
            fail_tok(p, t, "Structs (. and ->) arrive in a later level");
        if (s[0] == '?' || s[0] == ':')
            fail_tok(p, t, "The ?: operator is not available yet. Use "
                           "if/else instead");
        fail_tok(p, t, "The '%.*s' operator is not available yet in this "
                       "level", t.len, s);
        break;
    default: {
        char d[64];
        describe(p, t, d, sizeof d);
        fail_tok(p, t, "Expected %s, but found %s", expected, d);
        break;
    }
    }
}

/* ---- token stream / 토큰 흐름 ------------------------------------------- */

static void advance(Parser *p)
{
    p->prev = p->cur;
    p->cur = lexer_next(&p->lx);
    if (p->cur.kind == TK_ERROR) fail_tok(p, p->cur, "%s", p->cur.error);
}

/* Look one token past the current one without consuming anything.
 * 아무것도 소비하지 않고 현재 다음 토큰을 미리 본다. */
static Token peek_next(const Parser *p)
{
    Lexer copy = p->lx;
    return lexer_next(&copy);
}

static bool check(const Parser *p, TokenKind k) { return p->cur.kind == k; }

static void expect(Parser *p, TokenKind k, const char *what)
{
    if (check(p, k)) advance(p);
    else unexpected(p, what);
}

static bool is_special(TokenKind k)
{
    return k == TK_RESERVED || k == TK_UNSUPPORTED || k == TK_STRING ||
           k == TK_HASH;
}

static void expect_semi(Parser *p)
{
    if (check(p, TK_SEMI)) {
        advance(p);
        return;
    }
    if (is_special(p->cur.kind)) unexpected(p, "';'");
    fail_after_prev(p, "Missing ';' at the end of this statement");
}

static void expect_close(Parser *p, TokenKind k, Token open)
{
    if (check(p, k)) {
        advance(p);
        return;
    }
    if (is_special(p->cur.kind)) unexpected(p, token_kind_text(k));
    fail_after_prev(p, "Missing %s to close the %s on line %d",
                    token_kind_text(k), token_kind_text(open.kind), open.line);
}

/* ---- nodes / 노드 ------------------------------------------------------- */

static Node *new_node(Parser *p, NodeKind kind, Token at)
{
    if (p->pool->count >= p->pool->cap)
        fail_tok(p, at, "The program is too long");
    Node *n = &p->pool->nodes[p->pool->count++];
    memset(n, 0, sizeof *n);
    n->kind = kind;
    n->line = at.line;
    n->col = at.col;
    n->len = at.len;
    n->end_line = at.line;
    return n;
}

static void copy_name(Parser *p, Node *n, Token t)
{
    if (t.len >= VM_NAME_MAX)
        fail_tok(p, t, "Names can be at most %d characters long",
                 VM_NAME_MAX - 1);
    memcpy(n->name, p->src + t.start, (size_t)t.len);
    n->name[t.len] = '\0';
}

static void enter(Parser *p)
{
    if (++p->depth > MAX_DEPTH)
        fail_tok(p, p->cur, "The code is nested too deeply");
}

static void leave(Parser *p) { p->depth--; }

/* ---- expressions / 식 --------------------------------------------------- */

static Node *parse_expr(Parser *p);
static Node *parse_unary(Parser *p);

static Node *parse_primary(Parser *p)
{
    Token t = p->cur;

    if (check(p, TK_NUMBER)) {
        Node *n = new_node(p, N_NUMBER, t);
        n->value = t.value;
        advance(p);
        return n;
    }

    if (check(p, TK_IDENT)) {
        advance(p);
        if (check(p, TK_LPAREN)) {
            /* function call / 함수 호출 */
            Node *n = new_node(p, N_CALL, t);
            copy_name(p, n, t);
            Token open = p->cur;
            advance(p);
            Node head, *tail = &head;
            head.next = NULL;
            if (!check(p, TK_RPAREN)) {
                for (;;) {
                    tail->next = parse_expr(p);
                    tail = tail->next;
                    if (!check(p, TK_COMMA)) break;
                    advance(p);
                }
            }
            expect_close(p, TK_RPAREN, open);
            n->a = head.next;
            return n;
        }
        Node *n = new_node(p, N_IDENT, t);
        copy_name(p, n, t);
        return n;
    }

    if (check(p, TK_LPAREN)) {
        Token open = t;
        advance(p);
        Node *e = parse_expr(p);
        expect_close(p, TK_RPAREN, open);
        return e;
    }

    unexpected(p, "a value or an expression");
    return NULL;
}

/* Something that can be assigned to: a variable or an array element.
 * 대입할 수 있는 대상: 변수 또는 배열 원소. */
static bool is_lvalue(const Node *n)
{
    return n->kind == N_IDENT || n->kind == N_INDEX;
}

static Node *parse_postfix(Parser *p)
{
    Node *n = parse_primary(p);

    /* a[i] and a[i][j] / 배열 접근 a[i], a[i][j] */
    while (check(p, TK_LBRACKET)) {
        Token open = p->cur;
        if (!is_lvalue(n))
            fail_tok(p, open, "Only an array name can be followed by [ ]");
        advance(p);
        if (check(p, TK_RBRACKET))
            fail_tok(p, p->cur, "Put an index inside the brackets, like "
                                "cards[0]");
        Node *m = new_node(p, N_INDEX, open);
        m->a = n;
        m->b = parse_expr(p);
        expect_close(p, TK_RBRACKET, open);
        m->line = n->line;
        m->col = n->col;
        /* Underline the whole "cards[i]" when reporting errors.
         * 오류를 보고할 때 "cards[i]" 전체에 밑줄을 긋는다. */
        m->len = p->prev.col + p->prev.len - n->col;
        if (p->prev.line != n->line || m->len < 1) m->len = n->len;
        n = m;
    }

    while (check(p, TK_INC) || check(p, TK_DEC)) {
        Token op = p->cur;
        if (!is_lvalue(n))
            fail_tok(p, op, "%s only works on a variable",
                     token_kind_text(op.kind));
        Node *m = new_node(p, N_INCDEC, op);
        m->op = op.kind;
        m->prefix = false;
        m->a = n;
        m->line = n->line;
        m->col = n->col;
        n = m;
        advance(p);
    }
    return n;
}

static Node *parse_unary(Parser *p)
{
    Token t = p->cur;
    enter(p);
    Node *result;

    if (check(p, TK_MINUS) || check(p, TK_PLUS) || check(p, TK_NOT)) {
        advance(p);
        Node *n = new_node(p, N_UNARY, t);
        n->op = t.kind;
        n->a = parse_unary(p);
        result = n;
    } else if (check(p, TK_INC) || check(p, TK_DEC)) {
        advance(p);
        Node *operand = parse_unary(p);
        if (!is_lvalue(operand))
            fail_tok(p, t, "%s only works on a variable",
                     token_kind_text(t.kind));
        Node *n = new_node(p, N_INCDEC, t);
        n->op = t.kind;
        n->prefix = true;
        n->a = operand;
        result = n;
    } else if (check(p, TK_STAR)) {
        fail_tok(p, t, "Pointers (*p) arrive in a later level");
        result = NULL;
    } else {
        result = parse_postfix(p);
    }
    leave(p);
    return result;
}

/* Binary operator precedence; 0 = not a binary operator.
 * 이항 연산자 우선순위. 0 = 이항 연산자가 아님. */
static int precedence(TokenKind k)
{
    switch (k) {
    case TK_OR_OR:   return 1;
    case TK_AND_AND: return 2;
    case TK_EQ: case TK_NE: return 3;
    case TK_LT: case TK_LE: case TK_GT: case TK_GE: return 4;
    case TK_PLUS: case TK_MINUS: return 5;
    case TK_STAR: case TK_SLASH: case TK_PERCENT: return 6;
    default: return 0;
    }
}

/* Precedence climbing: all binary operators are left-associative.
 * 우선순위 등반 방식: 모든 이항 연산자는 왼쪽 결합이다. */
static Node *parse_binary(Parser *p, int min_prec)
{
    enter(p);
    Node *lhs = parse_unary(p);
    for (;;) {
        int prec = precedence(p->cur.kind);
        if (prec == 0 || prec < min_prec) break;
        Token op = p->cur;
        advance(p);
        Node *rhs = parse_binary(p, prec + 1);
        Node *n = new_node(p, N_BINARY, op);
        n->op = op.kind;
        n->a = lhs;
        n->b = rhs;
        lhs = n;
    }
    leave(p);
    return lhs;
}

static bool is_assign_op(TokenKind k)
{
    return k == TK_ASSIGN || k == TK_PLUS_ASSIGN || k == TK_MINUS_ASSIGN ||
           k == TK_STAR_ASSIGN || k == TK_SLASH_ASSIGN ||
           k == TK_PERCENT_ASSIGN;
}

/* Assignment is right-associative: a = b = 3.
 * 대입은 오른쪽 결합이다: a = b = 3. */
static Node *parse_expr(Parser *p)
{
    enter(p);
    Node *lhs = parse_binary(p, 1);
    if (is_assign_op(p->cur.kind)) {
        Token op = p->cur;
        if (!is_lvalue(lhs))
            fail_tok(p, op, "The left side of %s must be a variable",
                     token_kind_text(op.kind));
        advance(p);
        Node *n = new_node(p, N_ASSIGN, op);
        n->op = op.kind;
        n->a = lhs;
        n->b = parse_expr(p);
        n->line = lhs->line;
        n->col = lhs->col;
        lhs = n;
    }
    leave(p);
    return lhs;
}

/* ---- statements / 문장 -------------------------------------------------- */

static Node *parse_statement(Parser *p);

static Node *parse_block(Parser *p)
{
    Token open = p->cur;
    Node *blk = new_node(p, N_BLOCK, open);
    advance(p);                                   /* '{' */
    Node head, *tail = &head;
    head.next = NULL;
    while (!check(p, TK_RBRACE)) {
        if (check(p, TK_EOF))
            fail_at(p, open.line, open.col, 1,
                    "Missing '}' to close the '{' on line %d", open.line);
        tail->next = parse_statement(p);
        while (tail->next) tail = tail->next;     /* a decl may be a chain
                                                     선언은 여러 노드일 수 있음 */
    }
    blk->end_line = p->cur.line;
    advance(p);                                   /* '}' */
    blk->a = head.next;
    return blk;
}

/* int a, b = 2, c;  ->  a chain of N_DECL nodes / N_DECL 노드 목록 */
static Node *parse_declaration(Parser *p)
{
    advance(p);                                   /* 'int' */
    Node head, *tail = &head;
    head.next = NULL;
    for (;;) {
        if (check(p, TK_STAR))
            fail_tok(p, p->cur, "Pointers (int *p) arrive in a later level");
        if (!check(p, TK_IDENT)) unexpected(p, "a variable name");
        Token name = p->cur;
        Node *d = new_node(p, N_DECL, name);
        copy_name(p, d, name);
        advance(p);
        if (check(p, TK_LPAREN))
            fail_tok(p, name, "Functions can't be written inside another "
                              "function. Is a '}' missing above?");
        if (check(p, TK_LBRACKET))
            fail_tok(p, name, "Declaring your own arrays is not available "
                              "yet. Use the array this level gives you");
        if (check(p, TK_ASSIGN)) {
            advance(p);
            d->a = parse_expr(p);
        }
        tail->next = d;
        tail = d;
        if (!check(p, TK_COMMA)) break;
        advance(p);
    }
    expect_semi(p);
    for (Node *d = head.next; d; d = d->next) d->end_line = p->prev.line;
    return head.next;
}

/* Body of if/while/for. C forbids a bare declaration there.
 * if/while/for의 본문. C에서는 여기에 선언만 단독으로 올 수 없다. */
static Node *parse_body(Parser *p)
{
    if (check(p, TK_INT))
        fail_tok(p, p->cur, "A declaration can't be the whole body of "
                            "if/while/for. Put it inside { }");
    return parse_statement(p);
}

static Node *parse_statement(Parser *p)
{
    Token t = p->cur;
    Node *n = NULL;
    enter(p);

    switch (t.kind) {
    case TK_LBRACE:
        n = parse_block(p);
        break;

    case TK_IF: {
        n = new_node(p, N_IF, t);
        advance(p);
        Token open = p->cur;
        expect(p, TK_LPAREN, "'(' after 'if'");
        n->a = parse_expr(p);
        expect_close(p, TK_RPAREN, open);
        n->b = parse_body(p);
        n->end_line = n->b->end_line;
        if (check(p, TK_ELSE)) {
            advance(p);
            n->c = parse_body(p);
            n->end_line = n->c->end_line;
        }
        break;
    }

    case TK_WHILE: {
        n = new_node(p, N_WHILE, t);
        advance(p);
        Token open = p->cur;
        expect(p, TK_LPAREN, "'(' after 'while'");
        n->a = parse_expr(p);
        expect_close(p, TK_RPAREN, open);
        n->b = parse_body(p);
        n->end_line = n->b->end_line;
        break;
    }

    case TK_FOR: {
        n = new_node(p, N_FOR, t);
        advance(p);
        Token open = p->cur;
        expect(p, TK_LPAREN, "'(' after 'for'");
        if (check(p, TK_SEMI)) {
            advance(p);
        } else if (check(p, TK_INT)) {
            n->a = parse_declaration(p);
        } else {
            Node *e = new_node(p, N_EXPR_STMT, p->cur);
            e->a = parse_expr(p);
            n->a = e;
            expect_semi(p);
        }
        if (!check(p, TK_SEMI)) n->b = parse_expr(p);
        expect_semi(p);
        if (!check(p, TK_RPAREN)) n->c = parse_expr(p);
        expect_close(p, TK_RPAREN, open);
        n->d = parse_body(p);
        n->end_line = n->d->end_line;
        break;
    }

    case TK_RETURN:
        n = new_node(p, N_RETURN, t);
        advance(p);
        if (!check(p, TK_SEMI)) n->a = parse_expr(p);
        expect_semi(p);
        n->end_line = p->prev.line;
        break;

    case TK_BREAK:
    case TK_CONTINUE:
        n = new_node(p, t.kind == TK_BREAK ? N_BREAK : N_CONTINUE, t);
        advance(p);
        expect_semi(p);
        break;

    case TK_SEMI:
        n = new_node(p, N_EMPTY, t);
        advance(p);
        break;

    case TK_INT:
        n = parse_declaration(p);
        break;

    case TK_VOID:
        fail_tok(p, t, "Variables can't have the type 'void'. Use 'int'");
        break;

    case TK_ELSE:
        fail_tok(p, t, "This 'else' has no matching 'if'. Check the braces "
                       "before it");
        break;

    default:
        n = new_node(p, N_EXPR_STMT, t);
        n->a = parse_expr(p);
        expect_semi(p);
        n->end_line = p->prev.line;
        break;
    }
    leave(p);
    return n;
}

/* ---- functions / 함수 --------------------------------------------------- */

/* Is the current identifier the start of a function definition with no
 * return type, like  run(void) { ... } ?
 * 현재 이름이 반환형 없는 함수 정의의 시작인가? 예: run(void) { ... } */
static bool looks_like_untyped_function(const Parser *p)
{
    Lexer copy = p->lx;
    Token t = lexer_next(&copy);
    if (t.kind != TK_LPAREN) return false;
    int depth = 1;
    while (depth > 0) {
        t = lexer_next(&copy);
        if (t.kind == TK_EOF || t.kind == TK_ERROR) return false;
        if (t.kind == TK_LPAREN) depth++;
        if (t.kind == TK_RPAREN) depth--;
    }
    t = lexer_next(&copy);
    return t.kind == TK_LBRACE;
}

static Node *parse_function(Parser *p)
{
    Token t = p->cur;

    if (check(p, TK_RBRACE))
        fail_tok(p, t, "This '}' has no matching '{'");
    if (check(p, TK_IDENT)) {
        if (looks_like_untyped_function(p))
            fail_tok(p, t, "This function needs a return type, like "
                           "'void %.*s'", t.len, p->src + t.start);
        fail_tok(p, t, "Code must be inside a function, between the { } of "
                       "void run(void). Is a '}' closing it too early?");
    }
    if (!check(p, TK_INT) && !check(p, TK_VOID))
        unexpected(p, "a function like: void run(void) { ... }");

    bool returns = check(p, TK_INT);
    advance(p);
    if (!check(p, TK_IDENT)) unexpected(p, "a function name");
    Token name = p->cur;
    advance(p);

    if (check(p, TK_SEMI) || check(p, TK_ASSIGN) || check(p, TK_COMMA))
        fail_tok(p, name, "Variables must be declared inside a function for "
                          "now (global variables come later)");
    Token open = p->cur;
    expect(p, TK_LPAREN, "'(' after the function name");

    Node *fn = new_node(p, N_FUNC, name);
    copy_name(p, fn, name);
    fn->returns_value = returns;

    /* Parameters: (void), (), or (int a, int b)
     * 매개변수: (void), (), 또는 (int a, int b) */
    Node head, *tail = &head;
    head.next = NULL;
    if (check(p, TK_VOID) && peek_next(p).kind == TK_RPAREN) {
        advance(p);
    } else if (!check(p, TK_RPAREN)) {
        for (;;) {
            if (check(p, TK_VOID))
                fail_tok(p, p->cur, "A parameter can't have the type 'void'. "
                                    "Use 'int'");
            if (!check(p, TK_INT)) unexpected(p, "a parameter like 'int n'");
            advance(p);
            if (!check(p, TK_IDENT)) unexpected(p, "a parameter name");
            Node *param = new_node(p, N_PARAM, p->cur);
            copy_name(p, param, p->cur);
            advance(p);
            tail->next = param;
            tail = param;
            if (!check(p, TK_COMMA)) break;
            advance(p);
        }
    }
    expect_close(p, TK_RPAREN, open);
    fn->a = head.next;

    if (check(p, TK_SEMI))
        fail_tok(p, p->cur, "Function prototypes are not needed here: you can "
                            "call a function before or after it is written. "
                            "Remove the ';'");
    if (!check(p, TK_LBRACE)) unexpected(p, "'{' to start the function body");
    fn->b = parse_block(p);
    fn->end_line = fn->b->end_line;
    return fn;
}

/* ---- entry point / 진입점 ----------------------------------------------- */

bool parse_program(const char *src, int len, NodePool *pool, Node **out,
                   CompileResult *res)
{
    memset(res, 0, sizeof *res);
    *out = NULL;
    pool->count = 0;
    pool->cap = len + 64;                 /* nodes <= characters + slack
                                             노드 수 <= 글자 수 + 여유 */
    pool->nodes = malloc(sizeof(Node) * (size_t)pool->cap);
    if (!pool->nodes) {
        snprintf(res->message, sizeof res->message, "Out of memory");
        res->line = 1;
        res->col = 1;
        return false;
    }

    Parser p;
    memset(&p, 0, sizeof p);
    p.src = src;
    p.pool = pool;
    p.res = res;
    lexer_init(&p.lx, src, len, false);

    if (setjmp(p.fail)) return false;

    Node head, *tail = &head;
    head.next = NULL;
    advance(&p);
    while (!check(&p, TK_EOF)) {
        tail->next = parse_function(&p);
        tail = tail->next;
    }
    *out = head.next;
    res->ok = true;
    return true;
}

void node_pool_free(NodePool *pool)
{
    free(pool->nodes);
    pool->nodes = NULL;
    pool->count = pool->cap = 0;
}
