/*
 * array_bridge.c - VM natives that produce Sumin's SooEvents
 * array_bridge.c - 수민의 SooEvent를 만들어 내는 VM 네이티브
 */
#include "array_bridge.h"

#include "compiler.h"

#include <stdio.h>
#include <string.h>

/* Hidden native names: '$' can't appear in C identifiers, so the player
 * can only reach them through cards[i].
 * 숨겨진 네이티브 이름: '$'는 C 식별자에 쓸 수 없으므로 플레이어는
 * cards[i]를 통해서만 이 함수들에 닿는다. */
#define READ_NATIVE  "$read"
#define WRITE_NATIVE "$write"

const char *array_name(const SooLevel *level)
{
    return level->rows == 1 ? "cards" : "grid";
}

/* ---- loop variables / 반복 변수 ----------------------------------------- */

/* Current value of the visible variable called name (i or j), or -1 if it
 * doesn't exist here or has no value yet. Sumin's view shows -1 as "--".
 * 지금 보이는 name(i 또는 j) 변수의 현재 값. 없거나 값이 아직 없으면 -1.
 * 수민의 화면은 -1을 "--"로 보여 준다. */
static int loop_var(const VM *vm, const char *name)
{
    if (vm->frame_count == 0) return -1;
    const Frame *fr = &vm->frames[vm->frame_count - 1];
    int pc = vm->pc - 1;                   /* the CALL_NATIVE itself
                                              CALL_NATIVE 명령 자신 */
    int d = prog_visible_local(vm->prog, fr->func, pc, name);
    if (d < 0) return -1;
    int32_t v;
    if (!vm_peek_i32(vm, fr->fp + vm->prog->locals[d].offset, &v)) return -1;
    return (int)v;
}

/* ---- level 3's reverse rule for random bombs / 무작위 폭탄의 레벨 3 규칙 --
 * Sumin's engine checks level 3's right-to-left scan only for her own
 * level (by address). Random versions of level 3 are registered here and
 * get the same check with the same message.
 * 수민의 엔진은 레벨 3의 오른쪽에서 왼쪽 순회를 자신의 레벨(주소)에만
 * 검사한다. 레벨 3의 무작위 버전은 여기 등록되어 같은 메시지로 같은 검사를
 * 받는다. */
#define REVERSE_SLOTS 8
static const SooLevel *g_reverse[REVERSE_SLOTS];
static int g_reverse_next;

void array_mark_reverse(const SooLevel *level)
{
    for (int k = 0; k < REVERSE_SLOTS; ++k)
        if (g_reverse[k] == level) return;
    g_reverse[g_reverse_next] = level;
    g_reverse_next = (g_reverse_next + 1) % REVERSE_SLOTS;
}

void array_unmark_reverse(const SooLevel *level)
{
    for (int k = 0; k < REVERSE_SLOTS; ++k)
        if (g_reverse[k] == level) g_reverse[k] = NULL;
}

bool array_apply(SooState *s, SooEvent e)
{
    bool marked = false;
    for (int k = 0; k < REVERSE_SLOTS; ++k)
        if (g_reverse[k] && g_reverse[k] == s->level) marked = true;
    if (marked && e.kind == SOO_READ && s->status != SOO_FAILURE &&
        s->status != SOO_SUCCESS) {
        int expected = s->active_col < 0 ? s->level->cols - 1
                                         : s->active_col - 1;
        if (e.col != expected) {
            s->status = SOO_FAILURE;
            snprintf(s->message, sizeof s->message,
                     "Reverse route: start at the last card, then decrease i "
                     "by one.");
            return false;
        }
    }
    return SooApplyEvent(s, e);
}

/* ---- trace recording / 실행 기록 ---------------------------------------- */

static bool push_event(ArrayTrace *t, VM *vm, SooEvent e)
{
    if (t->count >= SOO_MAX_EVENTS) {
        /* Sumin's rule: never cut a long trace and call it a success.
         * 수민의 규칙: 긴 기록을 잘라서 성공으로 처리하지 않는다. */
        vm_error(vm, "This program makes more than %d array steps, which is "
                     "too many to replay. Try fewer reads and writes.",
                 SOO_MAX_EVENTS);
        return false;
    }
    t->events[t->count] = e;
    t->lines[t->count] = vm->exec_line;
    t->count++;
    return true;
}

/* Send the event to Sumin's engine right away. If it refuses (out of
 * bounds, over budget, wrong direction, bad value), the program stops on
 * this line with her message.
 * 이벤트를 수민의 엔진에 바로 보낸다. 엔진이 거부하면 (범위 밖, 예산 초과,
 * 잘못된 방향, 잘못된 값) 프로그램은 이 줄에서 그 메시지와 함께 멈춘다. */
static bool send(ArrayTrace *t, VM *vm, SooEvent e)
{
    if (!push_event(t, vm, e)) return false;
    if (!array_apply(&t->live, e)) {
        vm_error(vm, "%s", t->live.message);
        return false;
    }
    return true;
}

/* Emit a LOOP event when i or j changed since the last event, so the view
 * shows "Loop variables updated" just like Sumin's sample traces.
 * i나 j가 지난 이벤트 이후 바뀌었으면 LOOP 이벤트를 보낸다. 그래서 화면은
 * 수민의 샘플 기록처럼 "Loop variables updated"를 보여 준다. */
static bool sync_loop(ArrayTrace *t, VM *vm, int *i, int *j)
{
    *i = loop_var(vm, "i");
    *j = loop_var(vm, "j");
    if (*i == t->last_i && *j == t->last_j) return true;
    t->last_i = *i;
    t->last_j = *j;
    SooEvent e = { SOO_LOOP, 0, 0, 0, *i, *j };
    return send(t, vm, e);
}

static NativeResult nat_read(VM *vm, const int32_t *args, int32_t *ret,
                             void *user)
{
    ArrayTrace *t = user;
    int i, j;
    if (!sync_loop(t, vm, &i, &j)) return NATIVE_ERROR;
    SooEvent e = { SOO_READ, args[0], args[1], 0, i, j };
    if (!send(t, vm, e)) return NATIVE_ERROR;
    /* The engine checked the bounds, so this index is valid.
     * 엔진이 범위를 검사했으므로 이 인덱스는 유효하다. */
    *ret = t->live.cells[args[0] * t->level->cols + args[1]];
    return NATIVE_OK;
}

static NativeResult nat_write(VM *vm, const int32_t *args, int32_t *ret,
                              void *user)
{
    ArrayTrace *t = user;
    int i, j;
    if (!sync_loop(t, vm, &i, &j)) return NATIVE_ERROR;
    SooEvent e = { SOO_WRITE, args[0], args[1], args[2], i, j };
    if (!send(t, vm, e)) return NATIVE_ERROR;
    *ret = args[2];             /* (cards[i] = v) has the value v / 값은 v */
    return NATIVE_OK;
}

void array_register_level(Program *prog, const SooLevel *level)
{
    int rd = prog_add_native(prog, READ_NATIVE, 2, true, nat_read);
    int wr = prog_add_native(prog, WRITE_NATIVE, 3, true, nat_write);
    if (level->rows == 1) {
        prog_add_array(prog, "cards", 1, rd, wr);
        prog_add_const(prog, "N", level->cols);
    } else {
        prog_add_array(prog, "grid", 2, rd, wr);
        prog_add_const(prog, "ROWS", level->rows);
        prog_add_const(prog, "COLS", level->cols);
    }
}

void array_record(ArrayTrace *t, const Program *prog, const SooLevel *level,
                  VM *vm)
{
    memset(t, 0, sizeof *t);
    t->level = level;
    t->last_i = t->last_j = -1;
    SooReset(&t->live, level);

    vm_init(vm, prog, t);
    while (vm->status == VM_PAUSED || vm->status == VM_YIELDED) {
        vm_resume(vm);
        vm_run(vm, 1 << 16);
    }
    t->steps = vm->steps;

    if (vm->status == VM_HALTED) {
        /* run() returned: ask the engine whether the goal is reached.
         * run()이 반환됨: 목표에 도달했는지 엔진에 묻는다. */
        SooEvent end = { SOO_END, 0, 0, 0, -1, -1 };
        if (push_event(t, vm, end)) {
            array_apply(&t->live, end);
            t->finished = true;
            return;
        }
    }
    t->failed = true;
    t->engine_failed = t->live.status == SOO_FAILURE;
    snprintf(t->error, sizeof t->error, "%s", vm->error);
    t->error_line = vm->error_line;
}

/* ---- playback / 재생 ---------------------------------------------------- */

/* A VM error the engine never saw (uninitialised variable, endless loop,
 * ...) is shown as one extra step after the last event.
 * 엔진이 보지 못한 VM 오류 (초기화하지 않은 변수, 끝나지 않는 반복 등)는
 * 마지막 이벤트 다음의 추가 단계 하나로 보여 준다. */
static bool has_vm_step(const ArrayTrace *t)
{
    return t->failed && !t->engine_failed;
}

int array_trace_length(const ArrayTrace *t)
{
    return t->count + (has_vm_step(t) ? 1 : 0);
}

bool array_playback_step(const ArrayTrace *t, SooState *s, int *cursor)
{
    if (s->status == SOO_SUCCESS || s->status == SOO_FAILURE) return false;
    if (*cursor < t->count) {
        array_apply(s, t->events[(*cursor)++]);
        return true;
    }
    if (*cursor == t->count && has_vm_step(t)) {
        (*cursor)++;
        s->status = SOO_FAILURE;
        snprintf(s->message, sizeof s->message, "Line %d: %.170s",
                 t->error_line, t->error);
        return true;
    }
    return false;
}

int array_line_at(const ArrayTrace *t, int cursor)
{
    if (cursor <= 0) return 0;
    if (cursor > t->count) return t->error_line;
    return t->lines[cursor - 1];
}

void array_playback_to(const ArrayTrace *t, SooState *s, int *cursor,
                       int target)
{
    SooReset(s, t->level);
    *cursor = 0;
    while (*cursor < target && array_playback_step(t, s, cursor)) {
    }
}

SooState array_playback_all(const ArrayTrace *t)
{
    SooState s;
    int cursor = 0;
    SooReset(&s, t->level);
    while (array_playback_step(t, &s, &cursor)) {
    }
    return s;
}

/* ---- example solutions / 예시 정답 ---------------------------------------- */

/* One working solution per level, written the way a student would.
 * Used by the tests and the --shot screenshots.
 * 레벨마다 학생이 쓸 법한 정답 하나. 테스트와 --shot 스크린샷에 쓴다. */
static const char *const SOLUTIONS[SOO_LEVEL_COUNT] = {
    /* 1  First connection */
    "void run(void) {\n"
    "    if (cards[2] == 1)\n"
    "        cards[2] = 0;\n"
    "}\n",
    /* 2  Sweep the circuit */
    "void run(void) {\n"
    "    for (int i = 0; i < N; ++i) {\n"
    "        if (cards[i] == 1)\n"
    "            cards[i] = 0;\n"
    "    }\n"
    "}\n",
    /* 3  Return signal (reverse) */
    "void run(void) {\n"
    "    for (int i = N - 1; i >= 0; --i) {\n"
    "        if (cards[i] == 1)\n"
    "            cards[i] = 0;\n"
    "    }\n"
    "}\n",
    /* 4  Memory matrix (nested loops) */
    "void run(void) {\n"
    "    for (int i = 0; i < ROWS; ++i) {\n"
    "        for (int j = 0; j < COLS; ++j) {\n"
    "            if (grid[i][j] == 1)\n"
    "                grid[i][j] = 0;\n"
    "        }\n"
    "    }\n"
    "}\n",
    /* 5  Diagonal route */
    "void run(void) {\n"
    "    for (int i = 0; i < ROWS; ++i) {\n"
    "        if (grid[i][i] == 1)\n"
    "            grid[i][i] = 0;\n"
    "    }\n"
    "}\n",
    /* 6  Signal deadline (efficiency) */
    "void run(void) {\n"
    "    for (int i = 0; i < N; i++)\n"
    "        if (cards[i]) cards[i] = 0;\n"
    "}\n",
};

const char *array_example_solution(int level)
{
    if (level < 0 || level >= SOO_LEVEL_COUNT) return "";
    return SOLUTIONS[level];
}

/* ---- starter text / 시작 코드 ------------------------------------------- */

void array_starter_code(const SooLevel *level, char *buf, size_t n)
{
    snprintf(buf, n,
             "// %s\n"
             "void run(void) {\n"
             "    \n"
             "}\n",
             level->rows == 1 ? "cards[0] .. cards[N - 1]"
                              : "grid[0][0] .. grid[ROWS - 1][COLS - 1]");
}

/* The sizes are named constants: random bombs change them, so code that
 * uses N, ROWS and COLS works on every bomb.
 * 크기는 이름 있는 상수다: 무작위 폭탄은 크기를 바꾸므로 N, ROWS, COLS를
 * 쓰는 코드는 모든 폭탄에서 동작한다. */
void array_tools_text(const SooLevel *level, char *buf, size_t n)
{
    if (level->rows == 1)
        snprintf(buf, n, "cards[i]   N (%d here)   0 = SAFE, 1 = FAULT",
                 level->cols);
    else
        snprintf(buf, n, "grid[i][j]   ROWS (%d here)   COLS (%d here)",
                 level->rows, level->cols);
}

bool array_check_source(const char *src, int len, const SooLevel *level,
                        Program *prog, VM *vm, ArrayTrace *t, char *why,
                        size_t n)
{
    CompileResult r;
    prog_init(prog);
    array_register_level(prog, level);
    if (!compile_program(src, len, prog, &r)) {
        snprintf(why, n, "Line %d: %s", r.line, r.message);
        return false;
    }
    array_record(t, prog, level, vm);
    SooState end = array_playback_all(t);
    if (end.status == SOO_SUCCESS) return true;
    snprintf(why, n, "%s", end.message);
    return false;
}
