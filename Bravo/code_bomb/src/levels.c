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
    snprintf(msg, n, "Outbox matches: %d of %d parcels delivered.",
             run->outbox_len, run->expected_len);
    return true;
}

/* Run the program against several seeded inboxes without animation.
 * 애니메이션 없이 여러 시드의 입력함으로 프로그램을 실행한다. */
void level_submit(const LevelDef *def, const Program *prog, int tests,
                  SubmitResult *out)
{
    memset(out, 0, sizeof *out);
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

static NativeResult nat_put(VM *vm, const int32_t *args, int32_t *ret,
                            void *user)
{
    (void)ret;
    LevelRun *run = user;
    int32_t v = args[0];
    if (run->outbox_len >= run->expected_len) {
        vm_error(vm, "put(%d): the outbox is already full. It only needs "
                     "%d parcels.", (int)v, run->expected_len);
        return NATIVE_ERROR;
    }
    /* Check each output immediately, like HRM does.
     * HRM처럼 출력할 때마다 바로 검사한다. */
    int32_t want = run->expected[run->outbox_len];
    if (v != want) {
        vm_error(vm, "put(%d): wrong parcel. The outbox expected %d next.",
                 (int)v, (int)want);
        return NATIVE_ERROR;
    }
    run->outbox[run->outbox_len++] = v;
    run->event.type = LEVEL_EVENT_PUT;
    run->event.value = v;
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

/* ---- the list / 목록 ---------------------------------------------------- */

const LevelDef *const LEVEL_LIST[LEVEL_SLOTS] = {
    &LEVEL_MAILROOM, &LEVEL_NIGHT_SHIFT, NULL, &LEVEL_WEIGH_STATION, NULL,
    &LEVEL_HEAD_COUNT, &LEVEL_BIGGEST_BOX, NULL, &LEVEL_COUNTDOWN, NULL, NULL
};

const char *const LEVEL_NAMES[LEVEL_SLOTS] = {
    "Mailroom", "Night Shift", "Damaged Goods", "Weigh Station", "Two Belts",
    "Head Count", "Biggest Box", "Stamp Press", "Countdown", "Back Shelf",
    "Holiday Rush"
};
