/*
 * compiler.c - AST to bytecode, plus meaning checks (names, types, calls)
 * compiler.c - AST를 바이트코드로 변환하고 의미 검사(이름, 타입, 호출)를 한다
 *
 * Two passes / 두 단계:
 *   1. Declare every function, so calls work before or after a definition.
 *      모든 함수를 먼저 선언하여 정의 앞에서든 뒤에서든 호출할 수 있게 한다.
 *   2. Generate code for each function body.
 *      각 함수 본문의 코드를 생성한다.
 *
 * Every instruction records the source line it came from; the editor uses
 * this to highlight the line that is running.
 * 모든 명령은 자신이 나온 소스 줄을 기록한다. 에디터는 이것으로 실행 중인
 * 줄을 강조한다.
 */
#include "compiler.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SCOPE_VARS 256
#define MAX_LOOPS      32
#define MAX_JUMPS      64

typedef struct {
    char name[VM_NAME_MAX];
    int  slot;
    int  depth;
    int  line;
    int  debug;    /* index into Program.locals, or -1 / 디버그 정보 번호 */
} ScopeVar;

/* Jumps waiting for a loop's break/continue target.
 * 반복문의 break/continue 목적지를 기다리는 점프 목록. */
typedef struct {
    int breaks[MAX_JUMPS];
    int nbreaks;
    int conts[MAX_JUMPS];
    int nconts;
} LoopCtx;

typedef struct {
    Program       *prog;
    CompileResult *res;
    jmp_buf        fail;

    int            func;        /* current function index / 현재 함수 번호 */
    const Node    *fnode;

    ScopeVar       vars[MAX_SCOPE_VARS];
    int            nvars;
    int            depth;

    LoopCtx        loops[MAX_LOOPS];
    int            nloops;

    int            statements;
} Compiler;

/* ---- errors / 오류 ------------------------------------------------------ */

static void fail(Compiler *c, const Node *n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->res->message, sizeof c->res->message, fmt, ap);
    va_end(ap);
    c->res->ok = false;
    c->res->line = n ? n->line : 1;
    c->res->col = n ? n->col : 1;
    c->res->len = (n && n->len > 0) ? n->len : 1;
    longjmp(c->fail, 1);
}

/* Edit distance, used for "did you mean ...?" suggestions.
 * 편집 거리. "혹시 ...인가요?" 제안에 사용한다. */
static int edit_distance(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    int prev[VM_NAME_MAX + 1], cur[VM_NAME_MAX + 1];
    if (la > VM_NAME_MAX || lb > VM_NAME_MAX) return 99;
    for (int j = 0; j <= lb; ++j) prev[j] = j;
    for (int i = 1; i <= la; ++i) {
        cur[0] = i;
        for (int j = 1; j <= lb; ++j) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int best = prev[j] + 1;
            if (cur[j - 1] + 1 < best) best = cur[j - 1] + 1;
            if (prev[j - 1] + cost < best) best = prev[j - 1] + cost;
            cur[j] = best;
        }
        memcpy(prev, cur, sizeof(int) * (size_t)(lb + 1));
    }
    return prev[lb];
}

static void consider(const char *name, const char *cand, const char **best,
                     int *best_d)
{
    size_t n = strlen(name);
    int limit = n <= 2 ? 0 : (n <= 4 ? 1 : 2);
    int d = edit_distance(name, cand);
    if (d > 0 && d <= limit && d < *best_d) {
        *best_d = d;
        *best = cand;
    }
}

static const char *suggest(const Compiler *c, const char *name, bool vars,
                           bool funcs)
{
    const char *best = NULL;
    int best_d = 99;
    if (vars) {
        for (int i = 0; i < c->nvars; ++i)
            consider(name, c->vars[i].name, &best, &best_d);
        for (int i = 0; i < c->prog->array_count; ++i)
            consider(name, c->prog->arrays[i].name, &best, &best_d);
        for (int i = 0; i < c->prog->const_count; ++i)
            consider(name, c->prog->consts[i].name, &best, &best_d);
    }
    if (funcs) {
        for (int i = 0; i < c->prog->func_count; ++i)
            consider(name, c->prog->funcs[i].name, &best, &best_d);
        for (int i = 0; i < c->prog->native_count; ++i)
            consider(name, c->prog->natives[i].name, &best, &best_d);
    }
    return best;
}

/* ---- emitting / 명령 생성 ----------------------------------------------- */

static int emit(Compiler *c, OpCode op, int32_t a, int32_t b, int line)
{
    int at = prog_emit(c->prog, op, a, b, line);
    if (at < 0) fail(c, c->fnode, "The program is too long");
    return at;
}

static void patch_here(Compiler *c, int at)
{
    prog_patch(c->prog, at, prog_here(c->prog));
}

/* ---- scopes / 범위 ------------------------------------------------------ */

static int lookup(const Compiler *c, const char *name)
{
    for (int i = c->nvars - 1; i >= 0; --i)
        if (strcmp(c->vars[i].name, name) == 0) return c->vars[i].slot;
    return -1;
}

/* Names the level already defines (arrays, constants, natives) can't be
 * reused for variables or parameters.
 * 레벨이 이미 정의한 이름(배열, 상수, 네이티브)은 변수나 매개변수 이름으로
 * 다시 쓸 수 없다. */
static void check_level_name(Compiler *c, const Node *n, const char *name)
{
    if (prog_find_array(c->prog, name) >= 0)
        fail(c, n, "'%s' is the level's array. Pick another name for your "
                   "variable", name);
    if (prog_find_const(c->prog, name) >= 0)
        fail(c, n, "'%s' is already defined by the level (like #define %s). "
                   "Pick another name", name, name);
    if (prog_find_native(c->prog, name) >= 0)
        fail(c, n, "'%s' is a built-in function of this level. Pick another "
                   "name", name);
}

static int declare(Compiler *c, const Node *n, const char *name)
{
    check_level_name(c, n, name);
    for (int i = c->nvars - 1; i >= 0 && c->vars[i].depth == c->depth; --i)
        if (strcmp(c->vars[i].name, name) == 0)
            fail(c, n, "'%s' is already declared in this block (on line %d)",
                 name, c->vars[i].line);
    if (c->nvars >= MAX_SCOPE_VARS) fail(c, n, "Too many variables");

    int slot = prog_add_local(c->prog, c->func, name);
    if (slot < 0) fail(c, n, "Too many variables");
    int debug = c->prog->local_count - 1;
    c->prog->locals[debug].scope_start = prog_here(c->prog);

    ScopeVar *v = &c->vars[c->nvars++];
    snprintf(v->name, sizeof v->name, "%s", name);
    v->slot = slot;
    v->depth = c->depth;
    v->line = n->line;
    v->debug = debug;
    return slot;
}

static void push_scope(Compiler *c) { c->depth++; }

/* Leaving a block: its variables stop being visible here.
 * 블록을 벗어남: 그 블록의 변수는 여기서부터 보이지 않는다. */
static void close_var(Compiler *c, const ScopeVar *v)
{
    if (v->debug >= 0) c->prog->locals[v->debug].scope_end = prog_here(c->prog);
}

static void pop_scope(Compiler *c)
{
    c->depth--;
    while (c->nvars > 0 && c->vars[c->nvars - 1].depth > c->depth)
        close_var(c, &c->vars[--c->nvars]);
}

static int var_slot(Compiler *c, const Node *n)
{
    int slot = lookup(c, n->name);
    if (slot >= 0) return slot;

    if (prog_find_function(c->prog, n->name) >= 0 ||
        prog_find_native(c->prog, n->name) >= 0)
        fail(c, n, "'%s' is a function. Call it with ( ), like %s()",
             n->name, n->name);
    if (strcmp(n->name, "NULL") == 0)
        fail(c, n, "NULL arrives with pointers in a later level");
    int arr = prog_find_array(c->prog, n->name);
    if (arr >= 0)
        fail(c, n, c->prog->arrays[arr].dims == 1
                       ? "'%s' is an array. Pick one element, like %s[i]"
                       : "'%s' is a 2D array. Pick one element, like %s[i][j]",
             n->name, n->name);
    const char *hint = suggest(c, n->name, true, false);
    if (hint)
        fail(c, n, "'%s' is not declared. Did you mean '%s'?", n->name, hint);
    fail(c, n, "'%s' is not declared. Declare it first, like: int %s = 0;",
         n->name, n->name);
    return -1;
}

/* ---- expressions / 식 --------------------------------------------------- */

static void expr(Compiler *c, const Node *n, bool want);

/* Push row and column for cards[i] / grid[i][j] and return the array.
 * 1D arrays use row 0, matching Sumin's event format (row=0, col=index).
 * cards[i] / grid[i][j]의 행과 열을 push 하고 배열 정보를 반환한다.
 * 1D 배열은 수민의 이벤트 형식(row=0, col=인덱스)에 맞춰 행 0을 쓴다. */
static const ProgArray *element_address(Compiler *c, const Node *n)
{
    int depth = 0;
    const Node *base = n;
    while (base->kind == N_INDEX) {
        base = base->a;
        depth++;
    }
    if (base->kind != N_IDENT)
        fail(c, n, "Only an array name can be followed by [ ]");

    int idx = prog_find_array(c->prog, base->name);
    if (idx < 0) {
        if (lookup(c, base->name) >= 0)
            fail(c, base, "'%s' is a variable, not an array", base->name);
        if (c->prog->array_count == 0)
            fail(c, base, "There are no arrays in this level, so '%s[...]' "
                          "can't be used", base->name);
        const char *hint = suggest(c, base->name, true, false);
        if (hint)
            fail(c, base, "There is no array called '%s'. Did you mean '%s'?",
                 base->name, hint);
        fail(c, base, "There is no array called '%s'. This level's array is "
                      "'%s'", base->name, c->prog->arrays[0].name);
    }
    const ProgArray *arr = &c->prog->arrays[idx];
    if (depth != arr->dims) {
        if (arr->dims == 1)
            fail(c, n, "'%s' is a 1D array: use one index, like %s[i]",
                 arr->name, arr->name);
        fail(c, n, "'%s' is a 2D array: use two indices, like %s[i][j]",
             arr->name, arr->name);
    }

    if (arr->dims == 1) {
        emit(c, OP_PUSH, 0, 0, n->line);              /* row 0 / 행 0 */
        expr(c, n->b, true);                          /* col / 열     */
    } else {
        expr(c, n->a->b, true);                       /* row / 행     */
        expr(c, n->b, true);                          /* col / 열     */
    }
    return arr;
}

static OpCode arith_op(TokenKind k)
{
    switch (k) {
    case TK_PLUS:  case TK_PLUS_ASSIGN:    return OP_ADD;
    case TK_MINUS: case TK_MINUS_ASSIGN:   return OP_SUB;
    case TK_STAR:  case TK_STAR_ASSIGN:    return OP_MUL;
    case TK_SLASH: case TK_SLASH_ASSIGN:   return OP_DIV;
    case TK_PERCENT: case TK_PERCENT_ASSIGN: return OP_MOD;
    case TK_EQ: return OP_EQ;
    case TK_NE: return OP_NE;
    case TK_LT: return OP_LT;
    case TK_LE: return OP_LE;
    case TK_GT: return OP_GT;
    case TK_GE: return OP_GE;
    default:    return OP_NOP;
    }
}

static void call(Compiler *c, const Node *n, bool want)
{
    int argc = 0;
    for (const Node *a = n->a; a; a = a->next) argc++;

    const char *name = n->name;
    int nparams;
    bool returns;
    OpCode op;
    int index = prog_find_function(c->prog, name);
    if (index >= 0) {
        nparams = c->prog->funcs[index].nparams;
        returns = c->prog->funcs[index].returns_value;
        op = OP_CALL;
    } else {
        index = prog_find_native(c->prog, name);
        if (index < 0) {
            if (lookup(c, name) >= 0)
                fail(c, n, "'%s' is a variable, not a function", name);
            if (prog_find_array(c->prog, name) >= 0)
                fail(c, n, "'%s' is an array, not a function. Use [ ] to pick "
                           "an element, like %s[0]", name, name);
            const char *hint = suggest(c, name, false, true);
            if (hint)
                fail(c, n, "There is no function called '%s'. Did you mean "
                           "'%s'?", name, hint);
            fail(c, n, "There is no function called '%s'", name);
        }
        nparams = c->prog->natives[index].argc;
        returns = c->prog->natives[index].returns_value;
        op = OP_CALL_NATIVE;
    }

    if (argc != nparams)
        fail(c, n, "%s() needs %d argument%s, but %d %s given", name,
             nparams, nparams == 1 ? "" : "s", argc,
             argc == 1 ? "was" : "were");
    if (want && !returns)
        fail(c, n, "%s() is void: it does not give back a value, so it can't "
                   "be used inside an expression", name);

    for (const Node *a = n->a; a; a = a->next) expr(c, a, true);
    emit(c, op, index, argc, n->line);
    if (!want && returns) emit(c, OP_POP, 0, 0, n->line);
}

/* Compile an expression. want = the caller needs its value on the stack.
 * 식을 컴파일한다. want = 호출자가 스택 위의 값을 필요로 하는가. */
static void expr(Compiler *c, const Node *n, bool want)
{
    switch (n->kind) {
    case N_NUMBER:
        if (want) emit(c, OP_PUSH, n->value, 0, n->line);
        return;

    case N_IDENT: {
        if (lookup(c, n->name) < 0) {
            int k = prog_find_const(c->prog, n->name);
            if (k >= 0) {
                if (want) emit(c, OP_PUSH, c->prog->consts[k].value, 0, n->line);
                return;
            }
        }
        int slot = var_slot(c, n);
        if (want) emit(c, OP_LOAD_LOCAL, slot, 0, n->line);
        return;
    }

    case N_UNARY:
        expr(c, n->a, true);
        if (n->op == TK_MINUS) emit(c, OP_NEG, 0, 0, n->line);
        else if (n->op == TK_NOT) emit(c, OP_NOT, 0, 0, n->line);
        break;

    case N_BINARY:
        if (n->op == TK_AND_AND || n->op == TK_OR_OR) {
            /* Short-circuit: the right side runs only when needed.
             * 단락 평가: 필요할 때만 오른쪽을 실행한다. */
            bool is_and = n->op == TK_AND_AND;
            OpCode jump = is_and ? OP_JMP_FALSE : OP_JMP_TRUE;
            expr(c, n->a, true);
            int j1 = emit(c, jump, 0, 0, n->line);
            expr(c, n->b, true);
            int j2 = emit(c, jump, 0, 0, n->line);
            emit(c, OP_PUSH, is_and ? 1 : 0, 0, n->line);
            int j3 = emit(c, OP_JMP, 0, 0, n->line);
            patch_here(c, j1);
            patch_here(c, j2);
            emit(c, OP_PUSH, is_and ? 0 : 1, 0, n->line);
            patch_here(c, j3);
        } else {
            expr(c, n->a, true);
            expr(c, n->b, true);
            emit(c, arith_op(n->op), 0, 0, n->line);
        }
        break;

    case N_INDEX: {
        /* Read an element through the level: native(row, col) -> value.
         * 레벨을 통해 원소를 읽는다: native(row, col) -> 값. */
        const ProgArray *arr = element_address(c, n);
        emit(c, OP_CALL_NATIVE, arr->read_native, 2, n->line);
        if (!want) emit(c, OP_POP, 0, 0, n->line);
        return;
    }

    case N_ASSIGN: {
        if (n->a->kind == N_INDEX) {
            /* Write through the level: native(row, col, value) -> value.
             * 레벨을 통해 쓴다: native(row, col, value) -> 값. */
            const ProgArray *arr = element_address(c, n->a);
            if (n->op != TK_ASSIGN) {
                emit(c, OP_DUP2, 0, 0, n->line);
                emit(c, OP_CALL_NATIVE, arr->read_native, 2, n->line);
                expr(c, n->b, true);
                emit(c, arith_op(n->op), 0, 0, n->line);
            } else {
                expr(c, n->b, true);
            }
            emit(c, OP_CALL_NATIVE, arr->write_native, 3, n->line);
            if (!want) emit(c, OP_POP, 0, 0, n->line);
            return;
        }
        if (lookup(c, n->a->name) < 0 && prog_find_const(c->prog, n->a->name) >= 0)
            fail(c, n->a, "'%s' is a constant set by the level, so it can't "
                          "be changed", n->a->name);
        int slot = var_slot(c, n->a);
        if (n->op == TK_ASSIGN) {
            expr(c, n->b, true);
        } else {
            emit(c, OP_LOAD_LOCAL, slot, 0, n->line);
            expr(c, n->b, true);
            emit(c, arith_op(n->op), 0, 0, n->line);
        }
        if (want) emit(c, OP_DUP, 0, 0, n->line);
        emit(c, OP_STORE_LOCAL, slot, 0, n->line);
        return;
    }

    case N_INCDEC: {
        OpCode step_op = n->op == TK_INC ? OP_ADD : OP_SUB;
        if (n->a->kind == N_INDEX) {
            /* read, +/- 1, write. Postfix gives back the old value.
             * 읽고, 1을 더하거나 빼고, 쓴다. 후위형은 이전 값을 돌려준다. */
            const ProgArray *arr = element_address(c, n->a);
            emit(c, OP_DUP2, 0, 0, n->line);
            emit(c, OP_CALL_NATIVE, arr->read_native, 2, n->line);
            emit(c, OP_PUSH, 1, 0, n->line);
            emit(c, step_op, 0, 0, n->line);
            emit(c, OP_CALL_NATIVE, arr->write_native, 3, n->line);
            if (want && !n->prefix) {
                emit(c, OP_PUSH, 1, 0, n->line);
                emit(c, n->op == TK_INC ? OP_SUB : OP_ADD, 0, 0, n->line);
            }
            if (!want) emit(c, OP_POP, 0, 0, n->line);
            return;
        }
        if (lookup(c, n->a->name) < 0 && prog_find_const(c->prog, n->a->name) >= 0)
            fail(c, n->a, "'%s' is a constant set by the level, so it can't "
                          "be changed", n->a->name);
        int slot = var_slot(c, n->a);
        OpCode op = step_op;
        emit(c, OP_LOAD_LOCAL, slot, 0, n->line);
        if (want && !n->prefix) emit(c, OP_DUP, 0, 0, n->line); /* old value
                                                                   이전 값 */
        emit(c, OP_PUSH, 1, 0, n->line);
        emit(c, op, 0, 0, n->line);
        if (want && n->prefix) emit(c, OP_DUP, 0, 0, n->line);  /* new value
                                                                   새 값 */
        emit(c, OP_STORE_LOCAL, slot, 0, n->line);
        return;
    }

    case N_CALL:
        call(c, n, want);
        return;

    default:
        fail(c, n, "Internal error: unexpected expression");
    }

    /* Unary and binary results are discarded if unused.
     * 사용하지 않는 단항/이항 결과는 버린다. */
    if (!want) emit(c, OP_POP, 0, 0, n->line);
}

/* ---- statements / 문장 -------------------------------------------------- */

static void stmt(Compiler *c, const Node *n);

static void stmt_list(Compiler *c, const Node *n)
{
    for (; n; n = n->next) stmt(c, n);
}

static LoopCtx *push_loop(Compiler *c, const Node *n)
{
    if (c->nloops >= MAX_LOOPS) fail(c, n, "Loops are nested too deeply");
    LoopCtx *l = &c->loops[c->nloops++];
    memset(l, 0, sizeof *l);
    return l;
}

static void pop_loop(Compiler *c, int break_target, int cont_target)
{
    LoopCtx *l = &c->loops[--c->nloops];
    for (int i = 0; i < l->nbreaks; ++i)
        prog_patch(c->prog, l->breaks[i], break_target);
    for (int i = 0; i < l->nconts; ++i)
        prog_patch(c->prog, l->conts[i], cont_target);
}

static void stmt(Compiler *c, const Node *n)
{
    switch (n->kind) {
    case N_BLOCK:
        push_scope(c);
        stmt_list(c, n->a);
        pop_scope(c);
        break;

    case N_EXPR_STMT:
        c->statements++;
        expr(c, n->a, false);
        break;

    case N_DECL: {
        /* The name is in scope from its own initialiser on, like C.
         * C처럼 이름은 자신의 초기화 식부터 유효하다. */
        c->statements++;
        int slot = declare(c, n, n->name);
        if (n->a) {
            expr(c, n->a, true);
            emit(c, OP_STORE_LOCAL, slot, 0, n->line);
        }
        break;
    }

    case N_IF: {
        c->statements++;
        expr(c, n->a, true);
        int jf = emit(c, OP_JMP_FALSE, 0, 0, n->line);
        stmt(c, n->b);
        if (n->c) {
            int jend = emit(c, OP_JMP, 0, 0, n->b->end_line);
            patch_here(c, jf);
            stmt(c, n->c);
            patch_here(c, jend);
        } else {
            patch_here(c, jf);
        }
        break;
    }

    case N_WHILE: {
        c->statements++;
        int top = prog_here(c->prog);
        expr(c, n->a, true);
        int jf = emit(c, OP_JMP_FALSE, 0, 0, n->line);
        push_loop(c, n);
        stmt(c, n->b);
        emit(c, OP_JMP, top, 0, n->b->end_line);
        pop_loop(c, prog_here(c->prog), top);
        patch_here(c, jf);
        break;
    }

    case N_FOR: {
        c->statements++;
        push_scope(c);
        if (n->a) {
            /* The init part is part of the for statement's size.
             * 초기화 부분은 for 문 크기에 포함된다. */
            int saved = c->statements;
            stmt_list(c, n->a);
            c->statements = saved;
        }
        int top = prog_here(c->prog);
        int jf = -1;
        if (n->b) {
            expr(c, n->b, true);
            jf = emit(c, OP_JMP_FALSE, 0, 0, n->line);
        }
        push_loop(c, n);
        stmt(c, n->d);
        int cont = prog_here(c->prog);
        if (n->c) expr(c, n->c, false);
        emit(c, OP_JMP, top, 0, n->d->end_line);
        pop_loop(c, prog_here(c->prog), cont);
        if (jf >= 0) patch_here(c, jf);
        pop_scope(c);
        break;
    }

    case N_RETURN:
        c->statements++;
        if (c->fnode->returns_value) {
            if (!n->a)
                fail(c, n, "%s() must give back an int: write return "
                           "followed by a value", c->fnode->name);
            expr(c, n->a, true);
            emit(c, OP_RET_VAL, 0, 0, n->line);
        } else {
            if (n->a)
                fail(c, n, "%s() is void, so 'return' can't give back a "
                           "value", c->fnode->name);
            emit(c, OP_RET, 0, 0, n->line);
        }
        break;

    case N_BREAK:
    case N_CONTINUE: {
        c->statements++;
        bool is_break = n->kind == N_BREAK;
        if (c->nloops == 0)
            fail(c, n, "'%s' can only be used inside a loop",
                 is_break ? "break" : "continue");
        LoopCtx *l = &c->loops[c->nloops - 1];
        int *count = is_break ? &l->nbreaks : &l->nconts;
        if (*count >= MAX_JUMPS) fail(c, n, "Too many break/continue");
        int at = emit(c, OP_JMP, 0, 0, n->line);
        if (is_break) l->breaks[l->nbreaks++] = at;
        else          l->conts[l->nconts++] = at;
        break;
    }

    case N_EMPTY:
        break;

    default:
        fail(c, n, "Internal error: unexpected statement");
    }
}

/* ---- functions / 함수 --------------------------------------------------- */

static void declare_functions(Compiler *c, const Node *funcs)
{
    for (const Node *fn = funcs; fn; fn = fn->next) {
        check_level_name(c, fn, fn->name);
        if (prog_find_function(c->prog, fn->name) >= 0)
            fail(c, fn, "There are two functions named '%s'", fn->name);
        int f = prog_begin_function(c->prog, fn->name, fn->returns_value);
        if (f < 0) fail(c, fn, "Too many functions");
        for (const Node *p = fn->a; p; p = p->next) {
            for (const Node *q = fn->a; q != p; q = q->next)
                if (strcmp(q->name, p->name) == 0)
                    fail(c, p, "Two parameters are named '%s'", p->name);
            check_level_name(c, p, p->name);
            if (prog_add_param(c->prog, f, p->name) < 0)
                fail(c, p, "Too many parameters");
        }
    }

    int run = prog_find_function(c->prog, "run");
    if (run < 0) {
        const Node *main_fn = NULL;
        for (const Node *fn = funcs; fn; fn = fn->next)
            if (strcmp(fn->name, "main") == 0) main_fn = fn;
        if (main_fn)
            fail(c, main_fn, "In this game the robot starts at run(), not "
                             "main(). Rename it to: void run(void)");
        fail(c, NULL, "There is no run() function. The robot starts by "
                      "calling: void run(void) { ... }");
    }
    const Function *rf = &c->prog->funcs[run];
    if (rf->returns_value || rf->nparams != 0) {
        const Node *fn = funcs;
        while (fn && strcmp(fn->name, "run") != 0) fn = fn->next;
        fail(c, fn, "run must be written exactly as: void run(void)");
    }
    c->prog->entry_func = run;
}

static void compile_function(Compiler *c, const Node *fn)
{
    int f = prog_find_function(c->prog, fn->name);
    c->func = f;
    c->fnode = fn;
    c->nvars = 0;
    c->depth = 0;
    c->nloops = 0;
    c->prog->funcs[f].entry = prog_here(c->prog);

    /* Parameters live in slots 0, 4, 8, ... and share the outermost scope
     * with the body's locals (redeclaring a parameter is an error in C).
     * 매개변수는 0, 4, 8... 칸에 있고 본문 지역 변수와 가장 바깥 범위를
     * 공유한다 (C에서 매개변수를 다시 선언하면 오류). */
    int slot = 0;
    for (const Node *p = fn->a; p; p = p->next, slot += 4) {
        ScopeVar *v = &c->vars[c->nvars++];
        snprintf(v->name, sizeof v->name, "%s", p->name);
        v->slot = slot;
        v->depth = 0;
        v->line = p->line;
        v->debug = -1;
    }

    stmt_list(c, fn->b->a);
    emit(c, OP_RET, 0, 0, fn->end_line);
    /* Body-level variables stay visible through the final RET.
     * 본문 최상위 변수는 마지막 RET까지 보인다. */
    while (c->nvars > 0) close_var(c, &c->vars[--c->nvars]);
}

bool compile_program(const char *src, int len, Program *prog,
                     CompileResult *res)
{
    NodePool pool = { 0 };
    Node *funcs = NULL;
    if (!parse_program(src, len, &pool, &funcs, res)) {
        node_pool_free(&pool);
        prog->broken = true;
        return false;
    }

    Compiler *c = calloc(1, sizeof *c);
    if (!c) {
        node_pool_free(&pool);
        snprintf(res->message, sizeof res->message, "Out of memory");
        res->ok = false;
        prog->broken = true;
        return false;
    }
    c->prog = prog;
    c->res = res;

    bool ok = false;
    if (setjmp(c->fail) == 0) {
        declare_functions(c, funcs);
        for (const Node *fn = funcs; fn; fn = fn->next) compile_function(c, fn);
        if (prog->broken) fail(c, NULL, "The program is too long");
        ok = true;
    }

    if (ok) {
        res->ok = true;
        res->statements = c->statements;
    } else {
        prog->broken = true;
    }
    free(c);
    node_pool_free(&pool);
    return ok;
}
