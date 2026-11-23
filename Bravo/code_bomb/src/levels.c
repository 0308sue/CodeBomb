/*
 * levels.c - level rules, natives and the headless test runner
 * levels.c - 레벨 규칙, 네이티브 함수, 헤드리스 테스트 실행기
 */
#include "level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Small deterministic RNG: the same seed gives the same inbox on every
 * platform (Windows and macOS see identical tests).
 * 작은 결정적 난수 생성기: 같은 시드는 모든 플랫폼에서 같은 입력함을
 * 만든다 (Windows와 macOS에서 테스트가 동일). */
static unsigned rng_next(unsigned *s)
{
    *s = *s * 1103515245u + 12345u;
    return (*s >> 16) & 0x7fffu;
}

void level_format_items(const int32_t *items, int count, char *buf, size_t n)
{
    size_t used = 0;
    buf[0] = '\0';
    used += (size_t)snprintf(buf, n, "[");
    for (int i = 0; i < count && used < n; ++i)
        used += (size_t)snprintf(buf + used, n - used, "%s%d",
                                 i ? ", " : "", (int)items[i]);
    if (used < n) snprintf(buf + used, n - used, "]");
}

void level_run_init(LevelRun *run, const LevelDef *def, unsigned seed,
                    bool headless)
{
    memset(run, 0, sizeof *run);
    run->def = def;
    run->seed = seed;
    run->headless = headless;
    def->generate(run, seed);
    def->solve(run);
}

bool level_check_finished(const LevelRun *run, char *msg, size_t n)
{
    if (run->outbox_len < run->expected_len) {
        snprintf(msg, n, "The program finished, but the outbox has only %d "
                         "of %d parcels.", run->outbox_len, run->expected_len);
        return false;
    }
    if (run->side_len < run->side_expected_len) {
        snprintf(msg, n, "The program finished, but only %d of %d parcels "
                         "reached the %s.", run->side_len,
                 run->side_expected_len, run->def->side_name);
        return false;
    }
    if (run->def->side_name && run->side_expected_len > 0)
        snprintf(msg, n, "Done: %d parcels delivered, %d sent to the %s.",
                 run->outbox_len, run->side_len, run->def->side_name);
    else
        snprintf(msg, n, "Outbox matches: %d of %d parcels delivered.",
                 run->outbox_len, run->expected_len);
    return true;
}

bool level_has_helper(const Program *prog)
{
    for (int f = 0; f < prog->func_count; ++f) {
        const Function *fn = &prog->funcs[f];
        if (f == prog->entry_func || !fn->returns_value || fn->nparams < 1)
            continue;
        for (int pc = 0; pc < prog->code_len; ++pc)
            if (prog->code[pc].op == OP_CALL && prog->code[pc].a == f &&
                prog_func_at(prog, pc) != f)
                return true;
    }
    return false;
}

/* Run the program against several seeded inboxes without animation.
 * 애니메이션 없이 여러 시드의 입력함으로 프로그램을 실행한다. */
void level_submit(const LevelDef *def, const Program *prog, int tests,
                  SubmitResult *out)
{
    memset(out, 0, sizeof *out);
    if (def->needs_helper && !level_has_helper(prog)) {
        out->failed_test = 1;
        snprintf(out->message, sizeof out->message,
                 "The parcels may be right, but this level is about writing "
                 "your own function. Write int press(int n) that returns the "
                 "pressed number, and call it from run().");
        return;
    }
    VM *vm = malloc(sizeof *vm);
    if (!vm) {
        snprintf(out->message, sizeof out->message, "Out of memory");
        return;
    }

    double total_steps = 0;
    for (int t = 0; t < tests; ++t) {
        LevelRun run;
        level_run_init(&run, def, (unsigned)(t + 1), true);
        vm_init(vm, prog, &run);
        while (vm->status == VM_PAUSED || vm->status == VM_YIELDED) {
            vm_resume(vm);
            vm_run(vm, 1 << 16);
        }

        char inputs[128];
        level_format_items(run.inbox, run.inbox_len, inputs, sizeof inputs);

        if (vm->status == VM_ERROR) {
            out->failed_test = t + 1;
            snprintf(out->message, sizeof out->message,
                     "Test %d of %d failed with inbox %s. Line %d: %s",
                     t + 1, tests, inputs, vm->error_line, vm->error);
            free(vm);
            return;
        }
        char why[160];
        if (!level_check_finished(&run, why, sizeof why)) {
            out->failed_test = t + 1;
            snprintf(out->message, sizeof out->message,
                     "Test %d of %d failed with inbox %s. %s",
                     t + 1, tests, inputs, why);
            free(vm);
            return;
        }
        total_steps += (double)vm->steps;
        out->tests_run = t + 1;
    }

    out->passed = true;
    out->avg_steps = tests > 0 ? total_steps / tests : 0.0;
    snprintf(out->message, sizeof out->message,
             "All %d tests passed!", tests);
    free(vm);
}

/* ========================================================================
 * Shared natives: take(), put(n), has_input()
 * 공용 네이티브: take(), put(n), has_input()
 * ====================================================================== */

static NativeResult nat_take(VM *vm, const int32_t *args, int32_t *ret,
                             void *user)
{
    (void)args;
    LevelRun *run = user;
    if (run->inbox_pos >= run->inbox_len) {
        vm_error(vm, "take(): the inbox is empty. There are no parcels "
                     "left to take.");
        return NATIVE_ERROR;
    }
    *ret = run->inbox[run->inbox_pos++];
    run->event.type = LEVEL_EVENT_TAKE;
    run->event.value = *ret;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

/* Deliver v to the outbox (side = false) or to the level's second output
 * (side = true). Each parcel is checked at once, like HRM does. If it is
 * the parcel the other output wants next, the message says so.
 * v를 출력함(side = false) 또는 레벨의 두 번째 출력(side = true)으로
 * 보낸다. HRM처럼 소포마다 바로 검사한다. 다른 출력이 다음에 원하는
 * 소포라면 메시지가 그렇게 알려 준다. */
static NativeResult deliver(VM *vm, LevelRun *run, bool side, const char *fn,
                            int32_t v)
{
    int32_t *items = side ? run->side : run->outbox;
    int *len = side ? &run->side_len : &run->outbox_len;
    const int32_t *want = side ? run->side_expected : run->expected;
    int want_len = side ? run->side_expected_len : run->expected_len;
    const char *place = side ? run->def->side_name : "outbox";

    if (*len >= want_len) {
        if (side)
            vm_error(vm, "%s(%d): nothing more should go to the %s. It "
                         "needs %d parcels.", fn, (int)v, place, want_len);
        else
            vm_error(vm, "%s(%d): the outbox is already full. It only needs "
                         "%d parcels.", fn, (int)v, want_len);
        return NATIVE_ERROR;
    }
    if (v != want[*len]) {
        /* does the other output want this parcel next?
           다른 출력이 다음에 이 소포를 원하는가? */
        bool other = false;
        if (run->def->side_name) {
            if (side)
                other = run->outbox_len < run->expected_len &&
                        run->expected[run->outbox_len] == v;
            else
                other = run->side_len < run->side_expected_len &&
                        run->side_expected[run->side_len] == v;
        }
        if (other)
            vm_error(vm, "%s(%d): this parcel belongs on the %s, not the %s.",
                     fn, (int)v, side ? "outbox" : run->def->side_name, place);
        else
            vm_error(vm, "%s(%d): wrong parcel. The %s expected %d next.", fn,
                     (int)v, place, (int)want[*len]);
        return NATIVE_ERROR;
    }
    items[(*len)++] = v;
    return NATIVE_OK;
}

static NativeResult nat_put(VM *vm, const int32_t *args, int32_t *ret,
                            void *user)
{
    (void)ret;
    LevelRun *run = user;
    if (deliver(vm, run, false, "put", args[0]) != NATIVE_OK)
        return NATIVE_ERROR;
    run->event.type = LEVEL_EVENT_PUT;
    run->event.value = args[0];
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

static NativeResult nat_has_input(VM *vm, const int32_t *args, int32_t *ret,
                                  void *user)
{
    (void)vm;
    (void)args;
    LevelRun *run = user;
    *ret = run->inbox_pos < run->inbox_len;
    return NATIVE_OK;
}

/* ========================================================================
 * Level 1: Mailroom - statements and function calls
 * 레벨 1: 우편실 - 문장과 함수 호출
 * ====================================================================== */

static void mailroom_natives(Program *p)
{
    prog_add_native(p, "take", 0, true, nat_take);
    prog_add_native(p, "put", 1, false, nat_put);
    prog_add_native(p, "has_input", 0, true, nat_has_input);
}

static void mailroom_generate(LevelRun *run, unsigned seed)
{
    unsigned s = seed * 2654435761u + 977u;
    run->inbox_len = 3;
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 99);
}

static void mailroom_solve(LevelRun *run)
{
    memcpy(run->expected, run->inbox, sizeof(int32_t) * (size_t)run->inbox_len);
    run->expected_len = run->inbox_len;
}

const LevelDef LEVEL_MAILROOM = {
    .number = 1,
    .title = "Mailroom",
    .concept = "Statements & function calls",
    .brief = "Parcels arrive on the INBOX belt. Deliver every parcel, in "
             "the same order, to the OUTBOX belt. take() picks up the next "
             "parcel and gives back its number; put(n) places a parcel "
             "stamped n on the outbox. Every statement ends with ;",
    .goal = "Deliver every parcel from the INBOX to the OUTBOX, in order.",
    .tools = "int take(void);   void put(int n);",
    .reference = "void run(void) {\n"
                 "    put(take());\n"
                 "    put(take());\n"
                 "    put(take());\n"
                 "}\n",
    .starter = "// Deliver every parcel from the INBOX to the OUTBOX.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .register_natives = mailroom_natives,
    .generate = mailroom_generate,
    .solve = mailroom_solve,
    .par_size = 3,
    .par_speed = 7,
};

/* ========================================================================
 * Levels 2-9 share take(), put(n) and has_input(); only the parcels and
 * the rule for the outbox change.
 * 레벨 2-9는 take(), put(n), has_input()을 함께 쓰고, 소포와 출력함 규칙만
 * 다르다.
 * ====================================================================== */

#define TOOLS_LOOP "int take(void);   void put(int n);   int has_input(void);"

static unsigned level_seed(unsigned seed, unsigned salt)
{
    return seed * 2654435761u + salt;
}

/* ---- Level 2: Night Shift - while loops / 레벨 2: 야간 근무 - while ---- */

static void night_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 4111u);
    run->inbox_len = 2 + (int)(rng_next(&s) % 6);          /* 2-7 parcels */
    if (seed == 0) run->inbox_len = 5;      /* on screen: not 3 / 화면: 3 아님 */
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 99);
}

static const LevelExample NIGHT_EXAMPLES[] = {
    { "Three again", "Level 1's answer: only works for exactly 3 parcels",
      "void run(void) {\n"
      "    put(take());\n"
      "    put(take());\n"
      "    put(take());\n"
      "}\n" },
    { "Forever", "Bug: while (1) never stops, so take() runs dry",
      "void run(void) {\n"
      "    while (1) {\n"
      "        put(take());\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_NIGHT_SHIFT = {
    .number = 2,
    .title = "Night Shift",
    .concept = "while loops",
    .brief = "Tonight nobody knows how many parcels will arrive: it could "
             "be two, it could be seven. Deliver all of them, in order. "
             "has_input() tells you whether any parcels are left.",
    .goal = "Deliver every parcel, however many arrive tonight.",
    .tools = TOOLS_LOOP,
    .starter = "// Deliver every parcel, however many there are.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = NIGHT_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        put(take());\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = night_generate,
    .solve = mailroom_solve,
    .par_size = 2,
    .par_speed = 28,
};

/* ---- Level 4: Weigh Station - variables / 레벨 4: 계량소 - 변수 ------- */

static void weigh_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 8209u);
    int pairs = 1 + (int)(rng_next(&s) % 3);               /* 1-3 pairs  */
    if (seed == 0) pairs = 3;
    run->inbox_len = pairs * 2;
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 50);
}

static void weigh_solve(LevelRun *run)
{
    run->expected_len = run->inbox_len / 2;
    for (int i = 0; i < run->expected_len; ++i)
        run->expected[i] = run->inbox[2 * i] + run->inbox[2 * i + 1];
}

static const LevelExample WEIGH_EXAMPLES[] = {
    { "Running total", "Bug: total is never set back to 0 for the next pair",
      "void run(void) {\n"
      "    int total = 0;\n"
      "    while (has_input()) {\n"
      "        total = total + take() + take();\n"
      "        put(total);\n"
      "    }\n"
      "}\n" },
    { "One take", "Bug: adds the same parcel to itself",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int a = take();\n"
      "        put(a + a);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_WEIGH_STATION = {
    .number = 4,
    .title = "Weigh Station",
    .concept = "Variables & arithmetic",
    .brief = "Parcels arrive in pairs. For each pair, deliver one new parcel "
             "stamped with the sum of the two. The robot can only hold one "
             "parcel, but your variables can remember numbers.",
    .goal = "Parcels come in pairs. Deliver one parcel with each pair's sum.",
    .tools = TOOLS_LOOP,
    .starter = "// For each pair of parcels, deliver their sum.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = WEIGH_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int a = take();\n"
                 "        int b = take();\n"
                 "        put(a + b);\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = weigh_generate,
    .solve = weigh_solve,
    .par_size = 4,
    .par_speed = 23,
};

/* ---- Level 6: Head Count - counters / 레벨 6: 인원 확인 - 카운터 ------ */

static void count_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 15013u);
    int groups = 2 + (int)(rng_next(&s) % 2);              /* 2-3 groups */
    int n = 0;
    for (int g = 0; g < groups; ++g) {
        int size = (int)(rng_next(&s) % 3);                /* 0-2 parcels */
        if (seed == 0) size = g == 0 ? 2 : g == 1 ? 0 : 1;
        for (int k = 0; k < size; ++k)
            run->inbox[n++] = (int32_t)(1 + rng_next(&s) % 99);
        run->inbox[n++] = 0;                               /* end marker */
    }
    run->inbox_len = n;
}

static void count_solve(LevelRun *run)
{
    int count = 0;
    run->expected_len = 0;
    for (int i = 0; i < run->inbox_len; ++i) {
        if (run->inbox[i] != 0) {
            count++;
            continue;
        }
        run->expected[run->expected_len++] = count;
        count = 0;
    }
}

static const LevelExample COUNT_EXAMPLES[] = {
    { "Counts the zero", "Bug: the 0 end marker is counted as a parcel",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int count = 0;\n"
      "        int box = 1;\n"
      "        while (box != 0) {\n"
      "            box = take();\n"
      "            count++;\n"
      "        }\n"
      "        put(count);\n"
      "    }\n"
      "}\n" },
    { "Never resets", "Bug: count starts at 0 only once, not per group",
      "void run(void) {\n"
      "    int count = 0;\n"
      "    while (has_input()) {\n"
      "        while (take() != 0)\n"
      "            count++;\n"
      "        put(count);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_HEAD_COUNT = {
    .number = 6,
    .title = "Head Count",
    .concept = "Counters & sentinel loops",
    .brief = "Parcels arrive in groups, and every group ends with a parcel "
             "stamped 0. For each group, deliver how many parcels it had. "
             "The 0 is only a marker: don't count it. A group can be empty.",
    .goal = "Count the parcels before each 0, then deliver the count.",
    .tools = TOOLS_LOOP,
    .starter = "// Count the parcels before each 0 and deliver the count.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = COUNT_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int count = 0;\n"
                 "        while (take() != 0) {\n"
                 "            count++;\n"
                 "        }\n"
                 "        put(count);\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = count_generate,
    .solve = count_solve,
    .par_size = 5,
    .par_speed = 59,
};

/* ---- Level 7: Biggest Box - if / 레벨 7: 가장 큰 상자 - if ------------ */

static void biggest_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 20011u);
    int groups = 1 + (int)(rng_next(&s) % 3);              /* 1-3 groups */
    if (seed == 0) groups = 2;
    run->inbox_len = groups * 3;
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 99);
}

static void biggest_solve(LevelRun *run)
{
    run->expected_len = run->inbox_len / 3;
    for (int g = 0; g < run->expected_len; ++g) {
        int32_t best = run->inbox[3 * g];
        for (int k = 1; k < 3; ++k)
            if (run->inbox[3 * g + k] > best) best = run->inbox[3 * g + k];
        run->expected[g] = best;
    }
}

static const LevelExample BIGGEST_EXAMPLES[] = {
    { "First wins", "Bug: delivers the first parcel without comparing",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int best = take();\n"
      "        take();\n"
      "        take();\n"
      "        put(best);\n"
      "    }\n"
      "}\n" },
    { "Best never resets", "Bug: a big parcel from an earlier group wins",
      "void run(void) {\n"
      "    int best = 0;\n"
      "    while (has_input()) {\n"
      "        for (int i = 0; i < 3; i++) {\n"
      "            int box = take();\n"
      "            if (box > best)\n"
      "                best = box;\n"
      "        }\n"
      "        put(best);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_BIGGEST_BOX = {
    .number = 7,
    .title = "Biggest Box",
    .concept = "if & comparison",
    .brief = "Parcels arrive in groups of three. For each group, deliver "
             "the parcel with the biggest number. Two parcels can be equal.",
    .goal = "Parcels come in groups of 3. Deliver the biggest of each group.",
    .tools = TOOLS_LOOP,
    .starter = "// For each group of 3 parcels, deliver the biggest.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = BIGGEST_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int best = take();\n"
                 "        for (int i = 0; i < 2; i++) {\n"
                 "            int box = take();\n"
                 "            if (box > best)\n"
                 "                best = box;\n"
                 "        }\n"
                 "        put(best);\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = biggest_generate,
    .solve = biggest_solve,
    .par_size = 7,
    .par_speed = 92,
};

/* ---- Level 9: Countdown - for loops / 레벨 9: 카운트다운 - for -------- */

static void countdown_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 30011u);
    run->inbox_len = 1 + (int)(rng_next(&s) % 2);          /* 1-2 parcels */
    if (seed == 0) run->inbox_len = 2;
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 4);   /* 1-4 */
    if (seed == 0) {
        run->inbox[0] = 3;
        run->inbox[1] = 2;
    }
}

static void countdown_solve(LevelRun *run)
{
    run->expected_len = 0;
    for (int i = 0; i < run->inbox_len; ++i)
        for (int32_t v = run->inbox[i]; v > 0; --v)
            run->expected[run->expected_len++] = v;
}

static const LevelExample COUNTDOWN_EXAMPLES[] = {
    { "Off by one", "Bug: i >= 0 also delivers a parcel stamped 0",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int n = take();\n"
      "        for (int i = n; i >= 0; i--)\n"
      "            put(i);\n"
      "    }\n"
      "}\n" },
    { "Counts up", "Bug: delivers 1, 2, 3 instead of 3, 2, 1",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int n = take();\n"
      "        for (int i = 1; i <= n; i++)\n"
      "            put(i);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_COUNTDOWN = {
    .number = 9,
    .title = "Countdown",
    .concept = "for loops",
    .brief = "Each parcel is a countdown. For a parcel stamped n, deliver "
             "parcels n, n-1, n-2 and so on, down to 1.",
    .goal = "For each parcel n, deliver n, n-1, ... down to 1.",
    .tools = TOOLS_LOOP,
    .starter = "// For each parcel n, deliver n, n-1, ..., 1.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = COUNTDOWN_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int n = take();\n"
                 "        for (int i = n; i > 0; i--) {\n"
                 "            put(i);\n"
                 "        }\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = countdown_generate,
    .solve = countdown_solve,
    .par_size = 4,
    .par_speed = 61,
};

/* ========================================================================
 * Phase 3 machines: the shredder, the ODD belt and the stamp press
 * 3단계 기계: 파쇄기, ODD 벨트, 스탬프 프레스
 * ====================================================================== */

static NativeResult nat_shred(VM *vm, const int32_t *args, int32_t *ret,
                              void *user)
{
    (void)ret;
    LevelRun *run = user;
    int32_t v = args[0];
    if (v >= 0) {
        vm_error(vm, "shred(%d): that parcel isn't damaged. Only parcels "
                     "below 0 go to the shredder; %d goes to put().",
                 (int)v, (int)v);
        return NATIVE_ERROR;
    }
    if (deliver(vm, run, true, "shred", v) != NATIVE_OK) return NATIVE_ERROR;
    run->event.type = LEVEL_EVENT_SHRED;
    run->event.value = v;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

/* put(n) for levels with a shredder: a damaged parcel gets a clear message.
 * 파쇄기가 있는 레벨의 put(n): 손상된 소포에는 분명한 메시지를 준다. */
static NativeResult nat_put_checked(VM *vm, const int32_t *args, int32_t *ret,
                                    void *user)
{
    if (args[0] < 0) {
        vm_error(vm, "put(%d): that parcel is damaged (below 0). Damaged "
                     "parcels go to shred().", (int)args[0]);
        return NATIVE_ERROR;
    }
    return nat_put(vm, args, ret, user);
}

static NativeResult nat_put_even(VM *vm, const int32_t *args, int32_t *ret,
                                 void *user)
{
    (void)ret;
    LevelRun *run = user;
    int32_t v = args[0];
    if (v % 2 != 0) {
        vm_error(vm, "put_even(%d): %d is odd (%d %% 2 is 1), so it goes on "
                     "the ODD belt with put_odd().", (int)v, (int)v, (int)v);
        return NATIVE_ERROR;
    }
    if (deliver(vm, run, false, "put_even", v) != NATIVE_OK)
        return NATIVE_ERROR;
    run->event.type = LEVEL_EVENT_PUT;
    run->event.value = v;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

static NativeResult nat_put_odd(VM *vm, const int32_t *args, int32_t *ret,
                                void *user)
{
    (void)ret;
    LevelRun *run = user;
    int32_t v = args[0];
    if (v % 2 == 0) {
        vm_error(vm, "put_odd(%d): %d is even (%d %% 2 is 0), so it goes on "
                     "the EVEN belt with put_even().", (int)v, (int)v, (int)v);
        return NATIVE_ERROR;
    }
    if (deliver(vm, run, true, "put_odd", v) != NATIVE_OK) return NATIVE_ERROR;
    run->event.type = LEVEL_EVENT_PUT_ODD;
    run->event.value = v;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

/* ---- the Back Shelf: int shelf[8] (levels 10-11) / 뒤 선반 ------------- */

/* The compiler turns shelf[i] into these natives, like the bomb's cards[i],
 * so every access is checked. The row argument is always 0 (1D array).
 * 컴파일러는 폭탄의 cards[i]처럼 shelf[i]를 이 네이티브로 바꾸므로 모든
 * 접근이 검사된다. row 인자는 항상 0이다 (1차원 배열). */
static bool shelf_index(VM *vm, int32_t i)
{
    if (i < 0 || i >= LEVEL_SHELF_SIZE) {
        vm_error(vm, "shelf[%d] is outside the shelf. Its slots are shelf[0] "
                     "to shelf[%d] (SHELF_SIZE is %d).", (int)i,
                 LEVEL_SHELF_SIZE - 1, LEVEL_SHELF_SIZE);
        return false;
    }
    return true;
}

static NativeResult nat_shelf_get(VM *vm, const int32_t *args, int32_t *ret,
                                  void *user)
{
    LevelRun *run = user;
    int32_t i = args[1];
    if (!shelf_index(vm, i)) return NATIVE_ERROR;
    if (!run->shelf_used[i]) {
        vm_error(vm, "shelf[%d] is empty: nothing was stored there yet. In C "
                     "an unset slot holds garbage, so never read it.", (int)i);
        return NATIVE_ERROR;
    }
    *ret = run->shelf[i];
    run->event.type = LEVEL_EVENT_SHELF_GET;
    run->event.value = *ret;
    run->event.index = (int)i;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

static NativeResult nat_shelf_set(VM *vm, const int32_t *args, int32_t *ret,
                                  void *user)
{
    LevelRun *run = user;
    int32_t i = args[1];
    if (!shelf_index(vm, i)) return NATIVE_ERROR;
    run->shelf[i] = args[2];
    run->shelf_used[i] = true;
    *ret = args[2];
    run->event.type = LEVEL_EVENT_SHELF_SET;
    run->event.value = args[2];
    run->event.index = (int)i;
    return run->headless ? NATIVE_OK : NATIVE_YIELD;
}

static void add_shelf(Program *p)
{
    int rd = prog_add_native(p, "$shelf_get", 2, true, nat_shelf_get);
    int wr = prog_add_native(p, "$shelf_set", 3, true, nat_shelf_set);
    prog_add_array(p, "shelf", 1, rd, wr);
    prog_add_const(p, "SHELF_SIZE", LEVEL_SHELF_SIZE);
}

/* ---- Level 3: Damaged Goods - if / else / 레벨 3: 파손품 - if / else --- */

static void damaged_natives(Program *p)
{
    prog_add_native(p, "take", 0, true, nat_take);
    prog_add_native(p, "put", 1, false, nat_put_checked);
    prog_add_native(p, "shred", 1, false, nat_shred);
    prog_add_native(p, "has_input", 0, true, nat_has_input);
}

static void damaged_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 3301u);
    run->inbox_len = 3 + (int)(rng_next(&s) % 5);           /* 3-7 parcels */
    bool good = false, bad = false;
    for (int i = 0; i < run->inbox_len; ++i) {
        unsigned r = rng_next(&s) % 10;
        int32_t v = r < 4 ? -(int32_t)(1 + rng_next(&s) % 50)    /* damaged */
                  : r == 4 ? 0                                  /* edge: 0 */
                           : (int32_t)(1 + rng_next(&s) % 99);
        run->inbox[i] = v;
        if (v < 0) bad = true; else good = true;
    }
    /* always at least one of each / 항상 하나씩은 있다 */
    if (!bad) run->inbox[rng_next(&s) % (unsigned)run->inbox_len] = -7;
    if (!good) run->inbox[0] = 12;
    if (seed == 0) {
        static const int32_t shown[5] = { 12, -5, 30, -18, 7 };
        run->inbox_len = 5;
        memcpy(run->inbox, shown, sizeof shown);
    }
}

/* good parcels to the outbox, damaged ones to the shredder
   좋은 소포는 출력함, 손상된 것은 파쇄기 */
static void damaged_solve(LevelRun *run)
{
    run->expected_len = run->side_expected_len = 0;
    for (int i = 0; i < run->inbox_len; ++i) {
        int32_t v = run->inbox[i];
        if (v < 0) run->side_expected[run->side_expected_len++] = v;
        else run->expected[run->expected_len++] = v;
    }
}

static const LevelExample DAMAGED_EXAMPLES[] = {
    { "No shredder", "Bug: delivers every parcel, damaged or not",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        put(take());\n"
      "    }\n"
      "}\n" },
    { "Zero is damaged?", "Bug: <= 0 also shreds a parcel stamped 0",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        if (box <= 0)\n"
      "            shred(box);\n"
      "        else\n"
      "            put(box);\n"
      "    }\n"
      "}\n" },
    { "Take twice", "Bug: the test takes one parcel, the action another",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        if (take() < 0)\n"
      "            shred(take());\n"
      "        else\n"
      "            put(take());\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_DAMAGED_GOODS = {
    .number = 3,
    .title = "Damaged Goods",
    .concept = "if / else",
    .brief = "Some parcels were crushed in the van: their numbers are below "
             "0. Send damaged parcels to the shredder with shred(n) and "
             "deliver the rest. 0 is not below 0, so it is a good parcel.",
    .goal = "Parcels below 0 go to shred(n). Deliver the others in order.",
    .tools = "int take();  put(int n);  shred(int n);  int has_input();",
    .starter = "// Parcels below 0 are damaged: shred them.\n"
               "// Deliver every other parcel.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = DAMAGED_EXAMPLES,
    .example_count = 3,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int box = take();\n"
                 "        if (box < 0)\n"
                 "            shred(box);\n"
                 "        else\n"
                 "            put(box);\n"
                 "    }\n"
                 "}\n",
    .register_natives = damaged_natives,
    .generate = damaged_generate,
    .solve = damaged_solve,
    .side_name = "shredder",
    .par_size = 5,
    .par_speed = 54,
};

/* ---- Level 5: Two Belts - % and == / 레벨 5: 두 벨트 - %와 == --------- */

static void belts_natives(Program *p)
{
    prog_add_native(p, "take", 0, true, nat_take);
    prog_add_native(p, "put_even", 1, false, nat_put_even);
    prog_add_native(p, "put_odd", 1, false, nat_put_odd);
    prog_add_native(p, "has_input", 0, true, nat_has_input);
}

static void belts_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 5503u);
    run->inbox_len = 3 + (int)(rng_next(&s) % 6);           /* 3-8 parcels */
    int evens = 0;
    for (int i = 0; i < run->inbox_len; ++i) {
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 99);
        evens += run->inbox[i] % 2 == 0;
    }
    /* both belts get work / 두 벨트 모두 일한다 */
    if (evens == 0) run->inbox[0] = 2 * (int32_t)(1 + rng_next(&s) % 49);
    if (evens == run->inbox_len) run->inbox[run->inbox_len - 1] |= 1;
    if (seed == 0) {
        static const int32_t shown[5] = { 4, 7, 10, 3, 8 };
        run->inbox_len = 5;
        memcpy(run->inbox, shown, sizeof shown);
    }
}

static void belts_solve(LevelRun *run)
{
    run->expected_len = run->side_expected_len = 0;
    for (int i = 0; i < run->inbox_len; ++i) {
        int32_t v = run->inbox[i];
        if (v % 2 == 0) run->expected[run->expected_len++] = v;
        else run->side_expected[run->side_expected_len++] = v;
    }
}

static const LevelExample BELTS_EXAMPLES[] = {
    { "Divide, not remainder", "Bug: box / 2 is half the number, not the rest",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        if (box / 2 == 0)\n"
      "            put_even(box);\n"
      "        else\n"
      "            put_odd(box);\n"
      "    }\n"
      "}\n" },
    { "Swapped belts", "Bug: even and odd go to the wrong belts",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        if (box % 2 == 0)\n"
      "            put_odd(box);\n"
      "        else\n"
      "            put_even(box);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_TWO_BELTS = {
    .number = 5,
    .title = "Two Belts",
    .concept = "% (remainder) and ==",
    .brief = "A second belt was installed. Even parcels go on the EVEN belt "
             "with put_even(n), odd parcels on the ODD belt with put_odd(n). "
             "n % 2 is the remainder after dividing by 2: 0 for even numbers, "
             "1 for odd ones.",
    .goal = "Even parcels to put_even(n), odd parcels to put_odd(n).",
    .tools = "int take();  put_even(int n);  put_odd(int n);  int has_input();",
    .starter = "// Even parcels go to put_even, odd ones to put_odd.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = BELTS_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int box = take();\n"
                 "        if (box % 2 == 0)\n"
                 "            put_even(box);\n"
                 "        else\n"
                 "            put_odd(box);\n"
                 "    }\n"
                 "}\n",
    .register_natives = belts_natives,
    .generate = belts_generate,
    .solve = belts_solve,
    .side_name = "ODD belt",
    .par_size = 5,
    .par_speed = 84,
};

/* ---- Level 8: Stamp Press - functions / 레벨 8: 스탬프 프레스 - 함수 --- */

static void press_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 8807u);
    run->inbox_len = 2 + (int)(rng_next(&s) % 4);           /* 2-5 parcels */
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 9);    /* 1-9 */
    if (seed == 0) {
        static const int32_t shown[3] = { 2, 5, 1 };
        run->inbox_len = 3;
        memcpy(run->inbox, shown, sizeof shown);
    }
}

/* press(n) = 3n + 1, applied twice / press(n) = 3n + 1을 두 번 */
static void press_solve(LevelRun *run)
{
    run->expected_len = run->inbox_len;
    for (int i = 0; i < run->inbox_len; ++i) {
        int32_t once = 3 * run->inbox[i] + 1;
        run->expected[i] = 3 * once + 1;
    }
}

static const LevelExample PRESS_EXAMPLES[] = {
    { "Pressed once", "Bug: each parcel needs two trips through the press",
      "int press(int n) {\n"
      "    return 3 * n + 1;\n"
      "}\n"
      "\n"
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        put(press(take()));\n"
      "    }\n"
      "}\n" },
    { "Changes a copy", "Bug: n is a copy, so the caller's box never changes",
      "void press(int n) {\n"
      "    n = 3 * n + 1;\n"
      "}\n"
      "\n"
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        press(box);\n"
      "        press(box);\n"
      "        put(box);\n"
      "    }\n"
      "}\n" },
    { "No function", "Right numbers, but Submit wants your own press()",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        box = 3 * box + 1;\n"
      "        box = 3 * box + 1;\n"
      "        put(box);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_STAMP_PRESS = {
    .number = 8,
    .title = "Stamp Press",
    .concept = "Functions, parameters, return",
    .brief = "The stamp press turns a parcel stamped n into 3 * n + 1. Every "
             "parcel goes through the press twice. Write the press once as "
             "your own function, int press(int n), and call it from run().",
    .goal = "Write int press(int n) (3*n + 1). Deliver press(press(n)).",
    .tools = "int take();  put(int n);  int has_input();  + your int press(int n)",
    .starter = "// The stamp press turns n into 3 * n + 1.\n"
               "int press(int n) {\n"
               "    return n;   // fix me\n"
               "}\n"
               "\n"
               "// Every parcel goes through the press twice.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = PRESS_EXAMPLES,
    .example_count = 3,
    .reference = "int press(int n) {\n"
                 "    return 3 * n + 1;\n"
                 "}\n"
                 "\n"
                 "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        put(press(press(take())));\n"
                 "    }\n"
                 "}\n",
    .register_natives = mailroom_natives,
    .generate = press_generate,
    .solve = press_solve,
    .needs_helper = true,
    .par_size = 3,
    .par_speed = 76,
};

/* ---- Level 10: Back Shelf - arrays / 레벨 10: 뒤 선반 - 배열 ---------- */

static void shelf_natives(Program *p)
{
    mailroom_natives(p);
    add_shelf(p);
}

static void shelf_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 10007u);
    run->inbox_len = 2 + (int)(rng_next(&s) % 7);           /* 2-8 parcels */
    for (int i = 0; i < run->inbox_len; ++i)
        run->inbox[i] = (int32_t)(1 + rng_next(&s) % 99);
    if (seed == 0) {
        static const int32_t shown[5] = { 14, 3, 27, 8, 61 };
        run->inbox_len = 5;
        memcpy(run->inbox, shown, sizeof shown);
    }
}

static void shelf_solve(LevelRun *run)
{
    run->expected_len = run->inbox_len;
    for (int i = 0; i < run->inbox_len; ++i)
        run->expected[i] = run->inbox[run->inbox_len - 1 - i];
}

static const LevelExample SHELF_EXAMPLES[] = {
    { "Same order", "Bug: the second loop counts up, so nothing is reversed",
      "void run(void) {\n"
      "    int n = 0;\n"
      "    while (has_input()) {\n"
      "        shelf[n] = take();\n"
      "        n++;\n"
      "    }\n"
      "    for (int i = 0; i < n; i++)\n"
      "        put(shelf[i]);\n"
      "}\n" },
    { "One past the end", "Bug: starts at shelf[n], one past the last parcel",
      "void run(void) {\n"
      "    int n = 0;\n"
      "    while (has_input()) {\n"
      "        shelf[n] = take();\n"
      "        n++;\n"
      "    }\n"
      "    for (int i = n; i >= 0; i--)\n"
      "        put(shelf[i]);\n"
      "}\n" },
    { "Forgot n++", "Bug: every parcel lands in shelf[0]",
      "void run(void) {\n"
      "    int n = 0;\n"
      "    while (has_input()) {\n"
      "        shelf[n] = take();\n"
      "    }\n"
      "    put(shelf[0]);\n"
      "}\n" },
};

const LevelDef LEVEL_BACK_SHELF = {
    .number = 10,
    .title = "Back Shelf",
    .concept = "Arrays & indices",
    .brief = "Deliver tonight's parcels in reverse order: the last one in "
             "goes out first. The back shelf is an array, int shelf[8], with "
             "slots shelf[0] to shelf[7]. Store the parcels, count them, then "
             "deliver from the last slot back to the first.",
    .goal = "Deliver the parcels in reverse order. Use int shelf[8].",
    .tools = "int take();  put(int n);  int has_input();  int shelf[8];",
    .starter = "// Deliver the parcels in reverse order.\n"
               "// shelf[0] ... shelf[7] can hold one parcel each.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = SHELF_EXAMPLES,
    .example_count = 3,
    .reference = "void run(void) {\n"
                 "    int n = 0;\n"
                 "    while (has_input()) {\n"
                 "        shelf[n] = take();\n"
                 "        n++;\n"
                 "    }\n"
                 "    for (int i = n - 1; i >= 0; i--) {\n"
                 "        put(shelf[i]);\n"
                 "    }\n"
                 "}\n",
    .register_natives = shelf_natives,
    .generate = shelf_generate,
    .solve = shelf_solve,
    .has_shelf = true,
    .par_size = 6,
    .par_speed = 148,
};

/* ---- Level 11: Holiday Rush - everything / 레벨 11: 연말 대란 - 종합 -- */

static void rush_natives(Program *p)
{
    damaged_natives(p);
    add_shelf(p);
}

static void rush_generate(LevelRun *run, unsigned seed)
{
    unsigned s = level_seed(seed, 11113u);
    int groups = 2 + (int)(rng_next(&s) % 2);               /* 2-3 groups */
    int n = 0;
    for (int g = 0; g < groups; ++g) {
        int left = 9 - n - (groups - g);    /* keep room for the 0s
                                               0이 들어갈 자리를 남긴다 */
        int size = 1 + (int)(rng_next(&s) % 3);             /* 1-3 parcels */
        if (size > left) size = left;
        for (int k = 0; k < size; ++k) {
            unsigned r = rng_next(&s) % 10;
            run->inbox[n++] = r < 3 ? -(int32_t)(1 + rng_next(&s) % 50)
                                    : (int32_t)(1 + rng_next(&s) % 99);
        }
        run->inbox[n++] = 0;                                /* end marker */
    }
    run->inbox_len = n;
    bool good = false;
    for (int i = 0; i < n; ++i) good = good || run->inbox[i] > 0;
    if (!good) run->inbox[0] = 15;      /* something to deliver / 배달할 것 */
    if (seed == 0) {
        static const int32_t shown[8] = { 5, -3, 9, 0, 22, 4, -8, 0 };
        run->inbox_len = 8;
        memcpy(run->inbox, shown, sizeof shown);
    }
}

/* each group: shred the damaged, deliver the good ones last-first
   묶음마다: 손상된 것은 파쇄, 좋은 것은 마지막 것부터 배달 */
static void rush_solve(LevelRun *run)
{
    int32_t group[LEVEL_MAX_ITEMS];
    int g = 0;
    run->expected_len = run->side_expected_len = 0;
    for (int i = 0; i < run->inbox_len; ++i) {
        int32_t v = run->inbox[i];
        if (v < 0) {
            run->side_expected[run->side_expected_len++] = v;
        } else if (v > 0) {
            group[g++] = v;
        } else {
            while (g > 0) run->expected[run->expected_len++] = group[--g];
        }
    }
}

static const LevelExample RUSH_EXAMPLES[] = {
    { "Stores the damaged", "Bug: damaged parcels end up on the shelf",
      "void run(void) {\n"
      "    while (has_input()) {\n"
      "        int n = 0;\n"
      "        int box = take();\n"
      "        while (box != 0) {\n"
      "            shelf[n] = box;\n"
      "            n++;\n"
      "            box = take();\n"
      "        }\n"
      "        for (int i = n - 1; i >= 0; i--)\n"
      "            put(shelf[i]);\n"
      "    }\n"
      "}\n" },
    { "Shelf never resets", "Bug: n keeps counting, so old groups go again",
      "void run(void) {\n"
      "    int n = 0;\n"
      "    while (has_input()) {\n"
      "        int box = take();\n"
      "        while (box != 0) {\n"
      "            if (box < 0)\n"
      "                shred(box);\n"
      "            else {\n"
      "                shelf[n] = box;\n"
      "                n++;\n"
      "            }\n"
      "            box = take();\n"
      "        }\n"
      "        for (int i = n - 1; i >= 0; i--)\n"
      "            put(shelf[i]);\n"
      "    }\n"
      "}\n" },
};

const LevelDef LEVEL_HOLIDAY_RUSH = {
    .number = 11,
    .title = "Holiday Rush",
    .concept = "Everything together",
    .brief = "The holiday rush: parcels arrive in groups that end with a 0. "
             "Shred every damaged parcel (below 0) as soon as it arrives. "
             "Keep the good ones on the shelf, and when the 0 arrives deliver "
             "that group's good parcels in reverse order.",
    .goal = "Per group (ends in 0): shred n < 0, deliver the rest reversed.",
    .tools = "take()  put(n)  shred(n)  has_input()  int shelf[8];",
    .starter = "// Groups end with a 0. Shred damaged parcels (below 0).\n"
               "// At each 0, deliver the group's good parcels, last first.\n"
               "void run(void) {\n"
               "    \n"
               "}\n",
    .examples = RUSH_EXAMPLES,
    .example_count = 2,
    .reference = "void run(void) {\n"
                 "    while (has_input()) {\n"
                 "        int n = 0;\n"
                 "        int box = take();\n"
                 "        while (box != 0) {\n"
                 "            if (box < 0) {\n"
                 "                shred(box);\n"
                 "            } else {\n"
                 "                shelf[n] = box;\n"
                 "                n++;\n"
                 "            }\n"
                 "            box = take();\n"
                 "        }\n"
                 "        for (int i = n - 1; i >= 0; i--) {\n"
                 "            put(shelf[i]);\n"
                 "        }\n"
                 "    }\n"
                 "}\n",
    .register_natives = rush_natives,
    .generate = rush_generate,
    .solve = rush_solve,
    .side_name = "shredder",
    .has_shelf = true,
    .par_size = 11,
    .par_speed = 169,
};

/* ---- the list / 목록 ---------------------------------------------------- */

const LevelDef *const LEVEL_LIST[LEVEL_SLOTS] = {
    &LEVEL_MAILROOM, &LEVEL_NIGHT_SHIFT, &LEVEL_DAMAGED_GOODS,
    &LEVEL_WEIGH_STATION, &LEVEL_TWO_BELTS, &LEVEL_HEAD_COUNT,
    &LEVEL_BIGGEST_BOX, &LEVEL_STAMP_PRESS, &LEVEL_COUNTDOWN,
    &LEVEL_BACK_SHELF, &LEVEL_HOLIDAY_RUSH
};

const char *const LEVEL_NAMES[LEVEL_SLOTS] = {
    "Mailroom", "Night Shift", "Damaged Goods", "Weigh Station", "Two Belts",
    "Head Count", "Biggest Box", "Stamp Press", "Countdown", "Back Shelf",
    "Holiday Rush"
};
