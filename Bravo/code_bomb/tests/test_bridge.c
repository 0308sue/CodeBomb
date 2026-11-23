/*
 * test_bridge.c - player C code -> compiler -> VM -> Sumin's array engine
 * test_bridge.c - 플레이어 C 코드 -> 컴파일러 -> VM -> 수민의 배열 엔진
 *
 * Every level of array_module_SOO is solved with real C written in the
 * editor's language, and the results are compared with Sumin's own sample
 * traces (same operation counts, same failures).
 * array_module_SOO의 모든 레벨을 에디터 언어로 쓴 실제 C로 풀고, 결과를
 * 수민의 샘플 기록과 비교한다 (같은 연산 횟수, 같은 실패).
 */
#include "array_bridge.h"
#include "bomb_modules.h"
#include "compiler.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

static Program *g_prog;
static VM *g_vm;
static ArrayTrace *g_trace;

/* Compile src for a level; on success record the trace.
 * 레벨에 맞춰 src를 컴파일하고, 성공하면 실행 기록을 만든다. */
static bool run_code(int level, const char *src, CompileResult *res)
{
    prog_init(g_prog);
    array_register_level(g_prog, &SOO_LEVELS[level]);
    if (!compile_program(src, (int)strlen(src), g_prog, res)) {
        printf("  compile error line %d: %s\n", res->line, res->message);
        return false;
    }
    array_record(g_trace, g_prog, &SOO_LEVELS[level], g_vm);
    return true;
}

static SooState solve(int level, const char *src)
{
    CompileResult r;
    SooState s;
    memset(&s, 0, sizeof s);
    if (!run_code(level, src, &r)) {
        s.status = SOO_FAILURE;
        return s;
    }
    return array_playback_all(g_trace);
}

static void test_all_levels(void)
{
    /* Expected memory operations: reads + writes of faulty cards.
     * 기대 연산 횟수: 읽기 + 결함 카드 쓰기. */
    const int expected_ops[SOO_LEVEL_COUNT] = { 2, 13, 13, 19, 8, 16 };
    for (int level = 0; level < SOO_LEVEL_COUNT; ++level) {
        SooState s = solve(level, array_example_solution(level));
        char what[320];
        snprintf(what, sizeof what, "level %d solved by player code "
                 "(status %d, ops %d, msg: %s)", level + 1, s.status,
                 s.operations, s.message);
        CHECK(s.status == SOO_SUCCESS && SooGoalReached(&s), what);
        snprintf(what, sizeof what, "level %d uses %d operations (got %d)",
                 level + 1, expected_ops[level], s.operations);
        CHECK(s.operations == expected_ops[level], what);
    }
}

static void test_failures(void)
{
    SooState s;
    CompileResult r;

    /* Sumin's "Find the bug" mission, written as real code.
     * 수민의 "Find the bug" 미션을 실제 코드로 작성. */
    run_code(1, "void run(void) {\n"
                "    for (int i = 0; i <= 8; ++i) {\n"
                "        if (cards[i] == 1)\n"
                "            cards[i] = 0;\n"
                "    }\n"
                "}\n", &r);
    s = array_playback_all(g_trace);
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "cards[8]"),
          "i <= 8 is stopped at cards[8] by Sumin's engine");
    CHECK(s.i == 8 && SooGoalReached(&s),
          "all cards repaired, yet the bounds error still fails (her rule)");
    CHECK(s.operations == 13, "same 13 operations as her bounds sample");
    CHECK(g_trace->engine_failed && g_trace->error_line == 3,
          "the program stops on the line that reads cards[8]");

    /* i < N - 1 misses the last card / 마지막 카드를 빠뜨림 */
    s = solve(1, "void run(void) {\n"
                 "    for (int i = 0; i < N - 1; ++i)\n"
                 "        if (cards[i] == 1) cards[i] = 0;\n"
                 "}\n");
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "Incomplete"),
          "missing the last card is reported as incomplete");

    /* Forward loop on the reverse level / 역방향 레벨에서 정방향 반복 */
    s = solve(2, array_example_solution(1));
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "Reverse"),
          "forward loop fails the reverse-route rule");

    /* Repeated scans exceed the level 6 budget, like her SOO_SLOW sample.
     * 반복 스캔은 수민의 SOO_SLOW 샘플처럼 레벨 6 예산을 넘는다. */
    s = solve(5, "void run(void) {\n"
                 "    for (int pass = 0; pass < 6; pass++)\n"
                 "        for (int i = 0; i < N; i++)\n"
                 "            if (cards[i]) cards[i] = 0;\n"
                 "}\n");
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "limit") &&
              s.operations == 20,
          "repeated scans stop at the operation limit of 20");

    /* Only 0 and 1 are valid card values / 카드 값은 0과 1만 가능 */
    s = solve(0, "void run(void) { cards[2] = 5; }\n");
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "0 (SAFE)"),
          "writing 5 is refused by the engine");

    /* VM errors appear in the log with a line number.
     * VM 오류는 줄 번호와 함께 로그에 나타난다. */
    run_code(1, "void run(void) {\n"
                "    int i;\n"
                "    cards[i] = 0;\n"
                "}\n", &r);
    s = array_playback_all(g_trace);
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "Line 3") &&
              strstr(s.message, "'i'"),
          "uninitialised index is reported on line 3");
    CHECK(array_trace_length(g_trace) == 1 && g_trace->count == 0,
          "the VM error is one extra playback step");

    s = solve(1, "void run(void) { while (1) { } }\n");
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "never ends"),
          "an endless loop is reported");

    /* Writing nothing is incomplete / 아무것도 쓰지 않으면 미완료 */
    s = solve(1, "void run(void) {\n    \n}\n");
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "Incomplete"),
          "empty run() is incomplete");
}

static void test_language_features(void)
{
    SooState s;

    /* Compound assignment and -- go through read + write.
     * 복합 대입과 --는 읽기 + 쓰기를 거친다. */
    s = solve(0, "void run(void) { cards[2] -= 1; }\n");
    CHECK(s.status == SOO_SUCCESS && s.operations == 2, "cards[2] -= 1");
    s = solve(0, "void run(void) { int old = cards[2]--; if (old != 1) "
                 "cards[0] = 1; }\n");
    CHECK(s.status == SOO_SUCCESS && s.operations == 2,
          "postfix -- gives back the old value");

    /* A helper function with a parameter named i.
     * 매개변수 이름이 i인 도우미 함수. */
    s = solve(1, "void fix(int i) {\n"
                 "    if (cards[i] == 1) cards[i] = 0;\n"
                 "}\n"
                 "void run(void) {\n"
                 "    for (int k = 0; k < N; k++) fix(k);\n"
                 "}\n");
    CHECK(s.status == SOO_SUCCESS, "helper function fixes the row");

    /* Two loops that both declare i: each event reports the right i.
     * i를 각각 선언한 두 반복문: 이벤트마다 올바른 i를 보고한다. */
    CompileResult r;
    run_code(1, "void run(void) {\n"
                "    int faults = 0;\n"
                "    for (int i = 0; i < N; i++) faults += cards[i];\n"
                "    for (int i = N - 1; i >= 0; i--)\n"
                "        if (cards[i]) cards[i] = 0;\n"
                "}\n", &r);
    bool ok = g_trace->finished;
    for (int k = 0; k < g_trace->count; ++k) {
        SooEvent e = g_trace->events[k];
        if ((e.kind == SOO_READ || e.kind == SOO_WRITE) && e.i != e.col)
            ok = false;
    }
    CHECK(ok, "events carry the i of the loop that is running");
    CHECK(array_playback_all(g_trace).status == SOO_SUCCESS,
          "two-loop solution succeeds");

    /* Nested loops send LOOP events with both i and j.
     * 중첩 반복은 i와 j를 모두 담은 LOOP 이벤트를 보낸다. */
    run_code(3, array_example_solution(3), &r);
    int loops = 0;
    bool ij_ok = true;
    for (int k = 0; k < g_trace->count; ++k) {
        SooEvent e = g_trace->events[k];
        if (e.kind == SOO_LOOP) loops++;
        if (e.kind == SOO_READ && (e.i != e.row || e.j != e.col)) ij_ok = false;
    }
    CHECK(loops == 12 && ij_ok, "12 LOOP events with matching i and j");
    CHECK(g_trace->lines[1] == 4, "the first READ maps to source line 4");

    /* Back = reset and replay, matching forward stepping (Sumin's design).
     * Back = 초기화 후 다시 재생, 앞으로 진행한 결과와 같음 (수민의 설계). */
    SooState fwd, back;
    int c1 = 0, c2 = 0;
    SooReset(&fwd, &SOO_LEVELS[3]);
    for (int k = 0; k < 10; ++k) array_playback_step(g_trace, &fwd, &c1);
    array_playback_to(g_trace, &back, &c2, 10);
    CHECK(c1 == c2 && memcmp(fwd.cells, back.cells, sizeof fwd.cells) == 0 &&
              fwd.operations == back.operations,
          "Back replays to the same state");
}

typedef struct {
    int         level;
    const char *src;
    const char *needle;
} ErrCase;

static void test_compile_errors(void)
{
    static const ErrCase cases[] = {
        { 1, "void run(void) { cards[1][2] = 0; }", "1D array" },
        { 3, "void run(void) { grid[1] = 0; }", "2D array" },
        { 1, "void run(void) { N = 3; }", "constant" },
        { 1, "void run(void) { int cards = 0; }", "level's array" },
        { 1, "void run(void) { int N = 0; }", "#define N" },
        { 1, "void run(void) { cards = 0; }", "Pick one element" },
        { 1, "void run(void) { card[0] = 0; }", "Did you mean 'cards'" },
        { 3, "void run(void) { cards[0] = 0; }", "array is 'grid'" },
        { 1, "void run(void) { cards(0); }", "not a function" },
        { 1, "void run(void) { int x = 0; x[0] = 1; }", "not an array" },
        { 1, "void run(void) { cards[] = 0; }", "index inside" },
        { 1, "void run(void) { int a[4]; }", "your own arrays" },
        { 1, "void run(void) { cards[0; }", "Missing ']'" },
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
        prog_init(g_prog);
        array_register_level(g_prog, &SOO_LEVELS[cases[k].level]);
        CompileResult r;
        bool ok = compile_program(cases[k].src, (int)strlen(cases[k].src),
                                  g_prog, &r);
        char what[320];
        snprintf(what, sizeof what, "\"%s\" -> mentions \"%s\" (got: %s)",
                 cases[k].src, cases[k].needle, r.message);
        CHECK(!ok && strstr(r.message, cases[k].needle), what);
    }

    /* Arrays are not available in the mailroom level.
     * 우편실 레벨에서는 배열을 쓸 수 없다. */
    prog_init(g_prog);
    CompileResult r;
    const char *src = "void run(void) { int x = box[0]; }";
    bool ok = compile_program(src, (int)strlen(src), g_prog, &r);
    CHECK(!ok && strstr(r.message, "no arrays in this level"),
          "levels without arrays explain why [ ] can't be used");
}

/* The cube bomb modules run through the same engine.
 * 큐브 폭탄 모듈도 같은 엔진으로 실행된다. */
static SooState solve_module(int module, const char *src)
{
    CompileResult r;
    SooState s;
    memset(&s, 0, sizeof s);
    prog_init(g_prog);
    array_register_level(g_prog, &CUBE_MODULES[module]);
    if (!compile_program(src, (int)strlen(src), g_prog, &r)) {
        printf("  compile error line %d: %s\n", r.line, r.message);
        s.status = SOO_FAILURE;
        return s;
    }
    array_record(g_trace, g_prog, &CUBE_MODULES[module], g_vm);
    return array_playback_all(g_trace);
}

static void test_cube_modules(void)
{
    for (int m = 0; m < CUBE_MODULE_COUNT; ++m) {
        const SooLevel *l = &CUBE_MODULES[m];
        bool differs = false;
        for (int k = 0; k < l->rows * l->cols; ++k)
            if (l->initial[k] != l->goal[k]) differs = true;
        char what[320];
        snprintf(what, sizeof what, "module %d (%s) does not start solved", m,
                 cube_module_name(m));
        CHECK(differs, what);
        SooState s = solve_module(m, cube_example_solution(m));
        snprintf(what, sizeof what, "module %d (%s) solved by its example "
                 "(status %d, ops %d/%d, msg: %s)", m, cube_module_name(m),
                 s.status, s.operations, l->budget, s.message);
        CHECK(s.status == SOO_SUCCESS, what);
    }

    /* Sumin's "repair faults" sweep is the wrong idea for Invert.
     * 수민의 "결함 고치기" 반복은 Invert에 맞지 않는 생각이다. */
    SooState s = solve_module(0, array_example_solution(1));
    CHECK(s.status == SOO_FAILURE && strstr(s.message, "Incomplete"),
          "a fault-repair sweep does not solve Invert");

    /* Mirroring the wrong way round changes the left half.
     * 반대로 복사하면 왼쪽 절반이 바뀐다. */
    s = solve_module(2, "void run(void) {\n"
                        "    for (int i = 0; i < N / 2; i++)\n"
                        "        cards[i] = cards[N - 1 - i];\n"
                        "}\n");
    CHECK(s.status == SOO_FAILURE, "mirroring right-to-left fails");

    /* Checkerboard with a read before every write still fits the budget.
     * 쓰기 전마다 읽어도 체커보드는 예산 안에 든다. */
    s = solve_module(3, "void run(void) {\n"
                        "    for (int i = 0; i < ROWS; i++)\n"
                        "        for (int j = 0; j < COLS; j++)\n"
                        "            if (grid[i][j] != (i + j) % 2)\n"
                        "                grid[i][j] = (i + j) % 2;\n"
                        "}\n");
    CHECK(s.status == SOO_SUCCESS, "read-then-write checkerboard succeeds");
}

void run_bridge_tests(void)
{
    g_prog = malloc(sizeof *g_prog);
    g_vm = malloc(sizeof *g_vm);
    g_trace = malloc(sizeof *g_trace);
    if (!g_prog || !g_vm || !g_trace) return;
    test_all_levels();
    test_failures();
    test_language_features();
    test_compile_errors();
    test_cube_modules();
    free(g_prog);
    free(g_vm);
    free(g_trace);
}
