/*
 * test_compiler.c - lexer, parser and compiler tests
 * test_compiler.c - 렉서, 파서, 컴파일러 테스트
 */
#include "compiler.h"
#include "lexer.h"
#include "level.h"
#include "programs.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

static int32_t g_out[64];
static int g_out_count;
static int g_ticks;

static NativeResult nat_report(VM *vm, const int32_t *args, int32_t *ret,
                               void *user)
{
    (void)vm; (void)ret; (void)user;
    if (g_out_count < 64) g_out[g_out_count++] = args[0];
    return NATIVE_OK;
}

static NativeResult nat_tick(VM *vm, const int32_t *args, int32_t *ret,
                             void *user)
{
    (void)vm; (void)args; (void)user;
    g_ticks++;
    *ret = 1;
    return NATIVE_OK;
}

static bool compile_test(Program *p, const char *src, CompileResult *r)
{
    prog_init(p);
    prog_add_native(p, "report", 1, false, nat_report);
    prog_add_native(p, "tick", 0, true, nat_tick);
    return compile_program(src, (int)strlen(src), p, r);
}

/* ---- lexer / 렉서 ------------------------------------------------------- */
static void test_lexer(void)
{
    const char *src = "int x = 0x1F; // hi\nx += 'a';";
    Lexer lx;
    lexer_init(&lx, src, (int)strlen(src), true);
    TokenKind want[] = { TK_INT, TK_IDENT, TK_ASSIGN, TK_NUMBER, TK_SEMI,
                         TK_COMMENT, TK_IDENT, TK_PLUS_ASSIGN, TK_NUMBER,
                         TK_SEMI, TK_EOF };
    bool ok = true;
    int hex = 0, chr = 0, line2 = 0;
    for (size_t i = 0; i < sizeof want / sizeof want[0]; ++i) {
        Token t = lexer_next(&lx);
        if (t.kind != want[i]) ok = false;
        if (i == 3) hex = t.value;
        if (i == 8) chr = t.value;
        if (i == 6) line2 = t.line;
    }
    CHECK(ok, "lexer produces the expected token kinds");
    CHECK(hex == 31, "0x1F == 31");
    CHECK(chr == 'a', "'a' == 97");
    CHECK(line2 == 2, "lexer counts lines");

    /* The lexer must never get stuck on bad input.
     * 렉서는 잘못된 입력에서도 절대 멈추면 안 된다. */
    const char *junk = "@@ \x80\xff 12abc /* open";
    lexer_init(&lx, junk, (int)strlen(junk), true);
    int guard = 0;
    while (lexer_next(&lx).kind != TK_EOF && guard < 100) guard++;
    CHECK(guard < 100, "lexer always reaches EOF");
}

/* ---- examples match the hand-built bytecode -------------------------------
 * ---- 예제가 손으로 만든 바이트코드와 일치 */
static void test_examples(Program *a, Program *b)
{
    for (int i = 0; i < demo_count(); ++i) {
        const DemoProgram *d = demo_get(i);
        demo_build(d, &LEVEL_MAILROOM, a);
        prog_init(b);
        LEVEL_MAILROOM.register_natives(b);
        CompileResult r;
        bool ok = compile_program(d->source, (int)strlen(d->source), b, &r);
        bool same = ok && a->code_len == b->code_len;
        for (int k = 0; same && k < a->code_len; ++k)
            same = memcmp(&a->code[k], &b->code[k], sizeof(Instr)) == 0;
        char what[96];
        snprintf(what, sizeof what, "example '%s' compiles to the reference",
                 d->name);
        CHECK(same, what);
        CHECK(ok && r.statements == d->statements, "statement count");
    }

    /* Compiled programs through the real level rules.
     * 컴파일한 프로그램을 실제 레벨 규칙으로 실행. */
    SubmitResult s;
    const char *solution = demo_get(0)->source;
    prog_init(b);
    LEVEL_MAILROOM.register_natives(b);
    CompileResult r;
    compile_program(solution, (int)strlen(solution), b, &r);
    level_submit(&LEVEL_MAILROOM, b, 5, &s);
    CHECK(s.passed, "compiled solution passes level 1");

    const char *bad = demo_get(2)->source;
    prog_init(b);
    LEVEL_MAILROOM.register_natives(b);
    compile_program(bad, (int)strlen(bad), b, &r);
    level_submit(&LEVEL_MAILROOM, b, 5, &s);
    CHECK(!s.passed && strstr(s.message, "Line 5"),
          "compiled 'too many' fails on line 5");
}

/* ---- language semantics / 언어 의미 ------------------------------------- */
static void test_semantics(Program *p, VM *vm)
{
    const char *src =
        "/* helpers / 도우미 */\n"
        "int add(int a, int b) { return a + b; }\n"
        "int fact(int n) {\n"
        "    if (n <= 1) return 1;\n"
        "    return n * fact(n - 1);\n"
        "}\n"
        "void run(void) {\n"
        "    report(add(2, 40));            // 42\n"
        "    report(fact(5));               // 120\n"
        "    int sum = 0;\n"
        "    for (int i = 1; i <= 10; i++) sum += i;\n"
        "    report(sum);                   // 55\n"
        "    int i = 0;\n"
        "    while (1) {\n"
        "        i++;\n"
        "        if (i == 3) continue;\n"
        "        if (i > 5) break;\n"
        "        sum -= i;\n"
        "    }\n"
        "    report(sum);                   // 55-1-2-4-5 = 43\n"
        "    int x = 5;\n"
        "    int y = x++;\n"
        "    int z = ++x;\n"
        "    report(y); report(z); report(x);   // 5 7 7\n"
        "    report(0x1F + 010 + 'A');      // 31+8+65 = 104\n"
        "    report(-7 / 2); report(-7 % 2);    // -3 -1\n"
        "    report(1 < 2 && 2 < 3);        // 1\n"
        "    report(!(1 || 0));             // 0\n"
        "    { int x = 100; report(x); }    // shadowing: 100\n"
        "    report(x);                     // 7\n"
        "    int a; int b;\n"
        "    a = b = 4;\n"
        "    report(a + b);                 // 8\n"
        "    x *= 3; report(x);             // 21\n"
        "    x /= 2; report(x);             // 10\n"
        "    x %= 4; report(x);             // 2\n"
        "    if (0 && tick()) report(-1);\n"
        "    if (1 || tick()) report(-2);   // -2, no tick\n"
        "    if (x == 2) report(1); else report(0);\n"
        "    for (;;) { break; }\n"
        "    tick();                        // value discarded\n"
        "}\n";

    int32_t want[] = { 42, 120, 55, 43, 5, 7, 7, 104, -3, -1, 1, 0, 100, 7,
                       8, 21, 10, 2, -2, 1 };
    int nwant = (int)(sizeof want / sizeof want[0]);

    CompileResult r;
    bool ok = compile_test(p, src, &r);
    if (!ok) printf("  compile error line %d: %s\n", r.line, r.message);
    CHECK(ok, "semantics program compiles");
    if (!ok) return;

    g_out_count = 0;
    g_ticks = 0;
    vm_init(vm, p, NULL);
    while (vm->status == VM_PAUSED) vm_run(vm, 10000);
    if (vm->status == VM_ERROR)
        printf("  runtime error line %d: %s\n", vm->error_line, vm->error);
    CHECK(vm->status == VM_HALTED, "semantics program halts");
    bool match = g_out_count == nwant;
    for (int i = 0; match && i < nwant; ++i) match = g_out[i] == want[i];
    if (!match) {
        printf("  got:");
        for (int i = 0; i < g_out_count; ++i) printf(" %d", (int)g_out[i]);
        printf("\n");
    }
    CHECK(match, "all reported values are correct");
    CHECK(g_ticks == 1, "&& and || short-circuit");
}

/* ---- error messages / 오류 메시지 --------------------------------------- */
typedef struct {
    const char *src;
    const char *needle;
    int         line;
} ErrCase;

static void test_errors(Program *p)
{
    static const ErrCase cases[] = {
        { "void run(void) {\n    report(1)\n}", "Missing ';'", 2 },
        { "void run(void) {\n    report(1);\n", "Missing '}'", 1 },
        { "void run(void) { int parcel = 1; report(parcle); }",
          "Did you mean 'parcel'", 1 },
        { "void run(void) { reprot(1); }", "Did you mean 'report'", 1 },
        { "void run(void) { int v = report(1); }", "is void", 1 },
        { "void run(void) { report(1, 2); }", "needs 1 argument", 1 },
        { "void run(void) {\n  break;\n}", "inside a loop", 2 },
        { "#include <stdio.h>\nvoid run(void) {}", "#include", 1 },
        { "int main(void) { return 0; }", "not main()", 1 },
        { "void go(void) {}", "no run()", 1 },
        { "int run(void) { return 1; }", "void run(void)", 1 },
        { "void run(void) { char c; }", "'char'", 1 },
        { "void run(void) { int a[3]; }", "your own arrays", 1 },
        { "void run(void) { int x = 1.5; }", "float", 1 },
        { "void run(void) { int x = 09; }", "octal", 1 },
        { "void run(void) { int x = 1; int x = 2; }", "already declared", 1 },
        { "void run(void) { if (1) int x = 1; }", "inside { }", 1 },
        { "report(1);", "inside a function", 1 },
        { "run(void) { }", "return type", 1 },
        { "void run(void) { 5 = 3; }", "must be a variable", 1 },
        { "void run(void) { int x; x(); }", "variable, not a function", 1 },
        { "void run(void) { return 5; }", "is void", 1 },
        { "int f(void) { return; }\nvoid run(void) {}", "must give back", 1 },
        { "void run(void);", "prototypes", 1 },
        { "int g;\nvoid run(void) {}", "inside a function", 1 },
        { "void run(void) { int *p; }", "Pointers", 1 },
        { "void run(void) { report(\"hi\"); }", "strings", 1 },
        { "void run(void) { else {} }", "no matching 'if'", 1 },
        { "void run(void) {}\n}", "no matching '{'", 2 },
        { "void run(void) { report(99999999999); }", "too big", 1 },
        { "void run(void) { /* never closed", "never closed", 1 },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        CompileResult r;
        bool ok = compile_test(p, cases[i].src, &r);
        char what[400];
        snprintf(what, sizeof what, "error for \"%s\" mentions \"%s\" on line "
                 "%d (got line %d: %s)", cases[i].src, cases[i].needle,
                 cases[i].line, r.line, r.message);
        CHECK(!ok && strstr(r.message, cases[i].needle) &&
                  r.line == cases[i].line,
              what);
        CHECK(p->broken, "failed compile marks the program broken");
    }
}

void run_compiler_tests(void)
{
    Program *a = malloc(sizeof *a);
    Program *b = malloc(sizeof *b);
    VM *vm = malloc(sizeof *vm);
    if (!a || !b || !vm) return;
    test_lexer();
    test_examples(a, b);
    test_semantics(a, vm);
    test_errors(a);
    free(a);
    free(b);
    free(vm);
}
