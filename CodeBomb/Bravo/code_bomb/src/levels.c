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
    .tools = "int take(void);   void put(int n);",
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
