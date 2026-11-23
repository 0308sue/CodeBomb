/*
 * screen_arrays.c - the bomb defusal screen: Sumin's array module with the
 * 3D bomb on the left and the code editor on the right
 * screen_arrays.c - 폭탄 해체 화면: 왼쪽에는 3D 폭탄, 오른쪽에는 코드
 * 에디터가 있는 수민의 배열 모듈
 *
 * Levels 1-6 are Sumin's levels on a flat bomb. The Cube bomb holds five
 * harder modules (bomb_modules.c), one per face; the player picks a module
 * by clicking its face or its chip and solves each one with its own code.
 * 레벨 1-6은 납작한 폭탄 위의 수민의 레벨이다. 큐브 폭탄에는 더 어려운 모듈
 * 다섯 개(bomb_modules.c)가 면마다 하나씩 있다. 플레이어는 면이나 버튼을
 * 클릭해 모듈을 고르고, 각 모듈을 자기 코드로 푼다.
 *
 * Predict, Explore and Find the bug keep Sumin's logic (main_SOO.c).
 * Predict, Explore, Find the bug은 수민의 로직(main_SOO.c)을 그대로 쓴다.
 *
 * Levels 1-6 are untimed practice. The Cube bomb is the timed challenge:
 *   READY    - the player reads the missions and presses START
 *   RUNNING  - the countdown runs on the bomb's top display
 *   FUSE     - a run failed: a short pause on the strike, then...
 *   EXPLODED - the explosion plays, then a RESTART button appears
 *   DEFUSED  - all five modules solved in time
 * A run that ends in a strike or leaves its module unsolved, or the clock
 * reaching 00:00, explodes the bomb. Compile errors never do.
 * 레벨 1-6은 시간 제한 없는 연습이다. 큐브 폭탄은 시간 제한 도전이다:
 *   READY    - 미션을 읽고 START를 누른다
 *   RUNNING  - 폭탄 윗면 표시창에서 카운트다운이 진행된다
 *   FUSE     - 실행이 실패했다: 스트라이크를 잠깐 보여 준 뒤...
 *   EXPLODED - 폭발이 재생되고 RESTART 버튼이 나타난다
 *   DEFUSED  - 시간 안에 다섯 모듈을 모두 풀었다
 * 스트라이크로 끝나거나 모듈을 풀지 못한 채 끝난 실행, 또는 시계가 00:00에
 * 닿으면 폭탄이 터진다. 컴파일 오류는 터지지 않는다.
 */
#include "screen_arrays.h"

#include "array_bridge.h"
#include "bomb3d.h"
#include "bomb_modules.h"
#include "compiler.h"
#include "editor.h"
#include "quiz_SOO.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { MODE_CODE, MODE_PREDICT, MODE_EXPLORE, MODE_BUG } ArraysMode;

typedef enum { CH_READY, CH_RUNNING, CH_FUSE, CH_EXPLODED, CH_DEFUSED }
    ChallengePhase;

#define CUBE_TIME_LIMIT 600.0f   /* 10 minutes / 10분 */
#define FUSE_SECONDS    0.8f     /* strike shown before the blast
                                    폭발 전에 스트라이크를 보여 주는 시간 */
#define RESTART_DELAY   1.6f     /* let the explosion play first
                                    폭발을 먼저 보여 준다 */
#define ALERT_SECONDS   30.0f    /* display blinks / 표시창 깜박임 */

#define LEVEL_CUBE  SOO_LEVEL_COUNT            /* index of the cube bomb */
#define LEVEL_TOTAL (SOO_LEVEL_COUNT + 1)

/* ---- layout (1920 x 1200) / 화면 배치 ------------------------------------ */
static const Rectangle LEFT  = { 30, 180, 1120, 900 };
static const Rectangle RIGHT = { 1174, 180, 716, 900 };
#define RX     1201                    /* right panel content x / 오른쪽 내용 x */
#define RW     662                     /* right panel content w / 오른쪽 내용 폭 */
#define RTOP   206                     /* first line in the right panel       */
#define ROW2_Y 112                     /* level chips + mode buttons          */
#define ROW_Y  1096                    /* bottom buttons                      */
#define ROW_H  56
#define HINT_Y 1166

static const float SPEEDS[3] = { 0.45f, 0.15f, 0.05f };   /* Sumin's S key */
static const char *const SPEED_NAMES[3] = { "Slow", "Normal", "Fast" };

struct ArraysScreen {
    ArraysMode mode;
    int        level;                          /* 0-5 or LEVEL_CUBE      */
    int        module;                         /* cube module 0-4        */
    bool       completed[LEVEL_TOTAL];
    bool       module_done[CUBE_MODULE_COUNT];
    SooState   module_state[CUBE_MODULE_COUNT];/* what each face shows   */
    float      interval;

    /* Sumin's sample playback / 수민의 샘플 재생 */
    SooState    state;
    SooEvent    trace[SOO_MAX_EVENTS];
    int         count, cursor;
    SooScenario scenario;
    bool        playing;
    int         answer;
    float       timer;

    /* Write code mode / 코드 작성 모드 */
    Editor            *ed;
    const EditorStyle *style;
    char              *code[SOO_LEVEL_COUNT];
    char              *cube_code[CUBE_MODULE_COUNT];
    Program           *prog;
    VM                *vm;
    ArrayTrace        *ctrace;
    SooState           cstate;
    int                ccursor;
    bool               cplaying;
    float              ctimer;
    bool               compiled_ok;
    bool               have_trace;
    unsigned           compiled_version;
    unsigned           seen_version;
    CompileResult      compile;
    char               note[256];

    BombView          *bomb;
    BombScene          scene;

    /* Cube bomb challenge / 큐브 폭탄 도전 */
    ChallengePhase     phase;
    float              time_left;
    float              phase_t;          /* seconds in FUSE / EXPLODED */
    char               boom_reason[200];
    float              qa_settle;        /* screenshot override / 스크린샷용 */
};

static const SooLevel *cur_level(const ArraysScreen *a)
{
    return a->level == LEVEL_CUBE ? &CUBE_MODULES[a->module]
                                  : &SOO_LEVELS[a->level];
}

static bool is_cube(const ArraysScreen *a) { return a->level == LEVEL_CUBE; }

/* Editor and Run are locked before START and after the explosion.
 * START 전과 폭발 후에는 에디터와 Run이 잠긴다. */
static bool locked(const ArraysScreen *a)
{
    return is_cube(a) && a->mode == MODE_CODE &&
           (a->phase == CH_READY || a->phase == CH_FUSE ||
            a->phase == CH_EXPLODED);
}

static void challenge_fail(ArraysScreen *a, const char *reason);

/* ========================================================================
 * Sumin's sample logic (unchanged behaviour) / 수민의 샘플 로직 (동작 동일)
 * ====================================================================== */

static void Load(ArraysScreen *a)
{
    const SooLevel *l = is_cube(a) ? &SOO_LEVELS[0] : &SOO_LEVELS[a->level];
    SooReset(&a->state, l);
    a->count = SooBuildSample(a->state.level, a->scenario, a->trace,
                              SOO_MAX_EVENTS);
    a->cursor = 0;
    a->timer = 0;
    a->playing = false;
    a->answer = -1;
    if (a->count < 0) {
        a->count = 0;
        a->state.status = SOO_FAILURE;
    }
}

static bool quiz(const ArraysScreen *a)
{
    return a->mode == MODE_PREDICT || a->mode == MODE_BUG;
}

static void Step(ArraysScreen *a)
{
    if (a->cursor >= a->count || a->state.status >= SOO_SUCCESS) {
        a->playing = false;
        return;
    }
    SooApplyEvent(&a->state, a->trace[a->cursor++]);
    bool debug = a->mode == MODE_BUG;
    if (a->state.status == SOO_SUCCESS && a->scenario == SOO_CORRECT &&
        (!quiz(a) || (debug ? a->answer == 1
                            : a->answer == SOO_QUIZZES[a->level].answer)))
        a->completed[a->level] = true;
    if (a->state.status >= SOO_SUCCESS || a->cursor >= a->count)
        a->playing = false;
}

static void Back(ArraysScreen *a)
{
    int target = a->cursor > 0 ? a->cursor - 1 : 0;
    SooReset(&a->state, &SOO_LEVELS[a->level]);
    a->cursor = 0;
    a->playing = false;
    a->timer = 0;
    while (a->cursor < target) Step(a);
}

static void Answer(ArraysScreen *a, int choice)
{
    if (!quiz(a) || a->answer >= 0 || choice < 0 || choice > 2) return;
    a->answer = choice;
    a->playing = true;
    a->timer = 0;
}

static void CompareBounds(ArraysScreen *a, SooScenario scenario)
{
    int answer = a->answer;
    a->scenario = scenario;
    Load(a);
    a->answer = answer;
    a->playing = true;
}

/* ========================================================================
 * Write code mode / 코드 작성 모드
 * ====================================================================== */

static char *code_slot(ArraysScreen *a)
{
    return is_cube(a) ? a->cube_code[a->module] : a->code[a->level];
}

static void code_set_known(ArraysScreen *a)
{
    static const char *const one_d[] = { "cards", "N" };
    static const char *const two_d[] = { "grid", "ROWS", "COLS" };
    if (cur_level(a)->rows == 1) ed_set_known(a->ed, one_d, 2);
    else ed_set_known(a->ed, two_d, 3);
}

static void code_clear_run(ArraysScreen *a)
{
    a->have_trace = false;
    a->compiled_ok = true;
    a->cplaying = false;
    a->ccursor = 0;
    a->ctimer = 0;
    a->note[0] = '\0';
    SooReset(&a->cstate, cur_level(a));
}

/* Put the saved code in the editor, cursor inside run(). A cube module
 * shows its last state again (solved modules stay solved).
 * 저장된 코드를 에디터에 넣고 커서를 run() 안에 둔다. 큐브 모듈은 마지막
 * 상태를 다시 보여 준다 (푼 모듈은 풀린 채로 남는다). */
static void code_load(ArraysScreen *a)
{
    ed_set_text(a->ed, code_slot(a), false);
    const char *body = strstr(a->ed->text, "{\n");
    int pos = body ? (int)(body - a->ed->text) + 2 : 0;
    while (pos < a->ed->len && a->ed->text[pos] == ' ') pos++;
    ed_set_cursor(a->ed, pos, false);
    code_set_known(a);
    code_clear_run(a);
    if (is_cube(a)) a->cstate = a->module_state[a->module];
    a->seen_version = a->ed->version;
    a->compiled_version = a->ed->version - 1;
}

static void code_save(ArraysScreen *a)
{
    memcpy(code_slot(a), a->ed->text, (size_t)a->ed->len + 1);
    if (is_cube(a)) a->module_state[a->module] = a->cstate;
}

static bool code_fresh(const ArraysScreen *a)
{
    return a->have_trace && a->ed->version == a->compiled_version;
}

static int code_length(const ArraysScreen *a)
{
    return a->have_trace ? array_trace_length(a->ctrace) : 0;
}

static bool code_done(const ArraysScreen *a)
{
    return a->cstate.status >= SOO_SUCCESS || a->ccursor >= code_length(a);
}

static bool code_prepare(ArraysScreen *a)
{
    if (code_fresh(a)) return true;
    code_clear_run(a);
    prog_init(a->prog);
    array_register_level(a->prog, cur_level(a));
    a->compiled_version = a->ed->version;
    a->compiled_ok = compile_program(a->ed->text, a->ed->len, a->prog,
                                     &a->compile);
    if (!a->compiled_ok) {
        snprintf(a->note, sizeof a->note, "Line %d, column %d: %.200s",
                 a->compile.line, a->compile.col, a->compile.message);
        return false;
    }
    array_record(a->ctrace, a->prog, cur_level(a), a->vm);
    a->have_trace = true;
    snprintf(a->cstate.message, sizeof a->cstate.message,
             "Compiled. %d step%s recorded. Watch them play on the bomb.",
             code_length(a), code_length(a) == 1 ? "" : "s");
    return true;
}

static void code_step(ArraysScreen *a)
{
    if (array_playback_step(a->ctrace, &a->cstate, &a->ccursor) &&
        a->cstate.status == SOO_SUCCESS) {
        if (is_cube(a)) {
            a->module_done[a->module] = true;
            a->module_state[a->module] = a->cstate;
            bool all = true;
            for (int m = 0; m < CUBE_MODULE_COUNT; m++)
                if (!a->module_done[m]) all = false;
            if (all) {
                a->completed[LEVEL_CUBE] = true;
                if (a->phase == CH_RUNNING) a->phase = CH_DEFUSED;
            }
        } else {
            a->completed[a->level] = true;
        }
    }
    if (code_done(a)) a->cplaying = false;

    /* In the challenge a failed run sets the bomb off.
     * 도전 중에는 실패한 실행이 폭탄을 터뜨린다. */
    if (is_cube(a) && a->phase == CH_RUNNING && code_done(a) &&
        a->cstate.status != SOO_SUCCESS) {
        const char *why = a->cstate.status == SOO_FAILURE
                              ? a->cstate.message
                              : "The code finished, but the cards don't match "
                                "the target.";
        challenge_fail(a, TextFormat("Module %d (%s): %s", a->module + 1,
                                     cube_module_name(a->module), why));
    }
}

static void code_run(ArraysScreen *a)
{
    if (locked(a)) return;
    if (a->cplaying) {
        a->cplaying = false;
        return;
    }
    if (!code_prepare(a)) return;
    if (code_done(a)) array_playback_to(a->ctrace, &a->cstate, &a->ccursor, 0);
    a->cplaying = true;
    a->ctimer = a->interval;
}

static void code_step_key(ArraysScreen *a)
{
    if (locked(a)) return;
    a->cplaying = false;
    if (!code_prepare(a)) return;
    code_step(a);
}

static void code_back(ArraysScreen *a)
{
    if (!code_fresh(a) || a->ccursor == 0) return;
    a->cplaying = false;
    array_playback_to(a->ctrace, &a->cstate, &a->ccursor, a->ccursor - 1);
}

static void code_reset(ArraysScreen *a)
{
    a->cplaying = false;
    a->ccursor = 0;
    SooReset(&a->cstate, cur_level(a));
    if (code_fresh(a))
        snprintf(a->cstate.message, sizeof a->cstate.message,
                 "Back to the start. Press Run or Step to replay.");
}

static void code_watch_edits(ArraysScreen *a)
{
    if (a->ed->version == a->seen_version) return;
    a->seen_version = a->ed->version;
    if (a->have_trace && (a->ccursor > 0 || a->cplaying)) {
        a->cplaying = false;
        a->ccursor = 0;
        SooReset(&a->cstate, cur_level(a));
        snprintf(a->note, sizeof a->note,
                 "Code changed. Press Run to compile and try it again.");
    }
}

static void code_update_markers(ArraysScreen *a)
{
    Editor *ed = a->ed;
    ed->exec_line = ed->error_line = ed->error_col = ed->error_len = 0;
    if (!a->compiled_ok && ed->version == a->compiled_version) {
        ed->error_line = a->compile.line;
        ed->error_col = a->compile.col;
        ed->error_len = a->compile.len;
    } else if (code_fresh(a) && a->ccursor > 0) {
        int line = array_line_at(a->ctrace, a->ccursor);
        if (a->cstate.status == SOO_FAILURE) ed->error_line = line;
        else if (a->cstate.status != SOO_SUCCESS) ed->exec_line = line;
    }
}

/* ========================================================================
 * Right panel / 오른쪽 패널
 * ====================================================================== */

/* Height of the cube's module chip row (0 for flat bombs).
 * 큐브의 모듈 버튼 줄 높이 (납작한 폭탄은 0). */
static int chips_height(const ArraysScreen *a)
{
    return is_cube(a) && a->mode == MODE_CODE ? 70 : 0;
}

static Rectangle code_editor_rect(const ArraysScreen *a)
{
    int y = RTOP + chips_height(a) + ui_line_height(FS_SMALL);
    y += ui_wrapped_height(cur_level(a)->objective, RW, FS_BODY);
    y += 8 + ui_line_height(FS_SMALL) + 12;
    return (Rectangle){ RX, (float)y, RW,
                        RIGHT.y + RIGHT.height - 24 - (float)y };
}

static int mission_text(int y, const char *text)
{
    DrawText("MISSION", RX, y, FS_SMALL, UI_ACCENT);
    return ui_wrapped(text, RX, y + ui_line_height(FS_SMALL), RW, FS_BODY,
                      UI_INK);
}

static int choice_buttons(ArraysScreen *a, int y, const char *const labels[3])
{
    int w = (RW - 2 * 14) / 3;
    for (int n = 0; n < 3; n++)
        if (ui_button((Rectangle){ RX + n * (w + 14), y, w, 60 }, labels[n],
                      a->answer < 0, a->answer == n))
            Answer(a, n);
    return y + 60;
}

static const char *ij_text(const SooState *st)
{
    static char buf[48];
    char iv[12], jv[12];
    if (st->i < 0) snprintf(iv, sizeof iv, "--");
    else snprintf(iv, sizeof iv, "%d", st->i);
    if (st->j < 0) snprintf(jv, sizeof jv, "--");
    else snprintf(jv, sizeof jv, "%d", st->j);
    snprintf(buf, sizeof buf, "i %s   j %s", iv, jv);
    return buf;
}

static void DrawQuiz(ArraysScreen *a)
{
    const SooQuiz *q = &SOO_QUIZZES[a->level];
    int y = mission_text(RTOP, cur_level(a)->objective) + 20;
    DrawText("READ THE CODE", RX, y, FS_SMALL, UI_ACCENT);
    y = ui_lines(SooSampleCode(a->state.level, SOO_CORRECT), RX, y + 34,
                 FS_SMALL, UI_INK) + 20;
    y = ui_wrapped(q->question, RX, y, RW, FS_BODY, UI_INK) + 12;
    char labels[3][50];
    const char *ptrs[3];
    for (int n = 0; n < 3; n++) {
        snprintf(labels[n], sizeof labels[n], "%c  %s", 'A' + n, q->choices[n]);
        ptrs[n] = labels[n];
    }
    y = choice_buttons(a, y, ptrs) + 20;
    if (a->answer < 0) {
        DrawText("Choose A, B or C to reveal the trace.", RX, y, FS_SMALL,
                 UI_MUTED);
        return;
    }
    bool correct = a->answer == q->answer;
    DrawText(correct ? "Correct!"
                     : TextFormat("Not quite. Answer: %c", 'A' + q->answer),
             RX, y, FS_BODY, correct ? UI_ACCENT : UI_ERROR);
    ui_wrapped(q->reason, RX, y + 44, RW, FS_SMALL, UI_INK);
}

static void DrawBounds(ArraysScreen *a)
{
    bool fixed = a->scenario == SOO_CORRECT;
    int y = mission_text(RTOP, "Fix the loop so it checks all 8 cards without "
                               "going out of bounds.") + 20;
    DrawText(fixed ? "FIXED SAMPLE" : "ORIGINAL SAMPLE", RX, y, FS_SMALL,
             UI_ACCENT);
    y = ui_lines(TextFormat("for (int i = 0; i %s 8; ++i) {\n"
                            "    if (cards[i] == 1)\n"
                            "        cards[i] = 0;\n}",
                            fixed ? "<" : "<="),
                 RX, y + 34, FS_BODY, UI_INK) + 16;
    y = ui_wrapped("Which loop condition visits all 8 cards safely?", RX, y, RW,
                   FS_BODY, UI_INK) + 12;
    static const char *const choices[3] = { "A  i <= 8", "B  i < 8",
                                            "C  i < 7" };
    y = choice_buttons(a, y, choices) + 20;
    if (a->answer < 0) {
        DrawText("Choose A, B or C.", RX, y, FS_SMALL, UI_MUTED);
        return;
    }
    DrawText(a->answer == 1 ? "Correct: i < 8" : "Not quite. Use i < 8.", RX, y,
             FS_BODY, a->answer == 1 ? UI_ACCENT : UI_ERROR);
    ui_wrapped(a->answer == 2 ? "i < 7 skips card 7. i < 8 visits indices 0 to 7."
                              : "Eight cards use indices 0 to 7. i <= 8 tries "
                                "to read card 8.",
               RX, y + 44, RW, FS_SMALL, UI_INK);
    if (ui_button((Rectangle){ 52, ROW_Y, 330, ROW_H }, "Run original", true,
                  !fixed))
        CompareBounds(a, SOO_BOUNDS);
    if (ui_button((Rectangle){ 400, ROW_Y, 330, ROW_H }, "Run fix", true, fixed))
        CompareBounds(a, SOO_CORRECT);
}

static void DrawExplore(ArraysScreen *a)
{
    const SooLevel *l = cur_level(a);
    int y = mission_text(RTOP, l->objective) + 24;
    DrawText("REFERENCE TRACE / NOT EDITABLE", RX, y, FS_SMALL, UI_MUTED);
    ui_lines(SooSampleCode(l, a->scenario), RX, y + 40, FS_SMALL, UI_INK);
    DrawText("Each read or write costs 1 op. Loop updates are free.", RX,
             (int)(RIGHT.y + RIGHT.height) - 48, FS_SMALL, UI_MUTED);
    static const char *const scenarios[4] = { "Correct", "Missing step",
                                              "Out of bounds",
                                              "Repeated scans" };
    DrawText("COMPARE", 52, ROW_Y + 18, FS_SMALL, UI_MUTED);
    for (int n = 0; n < 4; n++)
        if (ui_button((Rectangle){ 190 + n * 240, ROW_Y, 226, ROW_H },
                      scenarios[n], true, a->scenario == (SooScenario)n)) {
            a->scenario = (SooScenario)n;
            Load(a);
        }
}

/* ========================================================================
 * Cube bomb challenge / 큐브 폭탄 도전
 * ====================================================================== */

/* A fresh bomb: all modules armed, full time. The player's code is kept.
 * 새 폭탄: 모든 모듈이 다시 무장되고 시간이 가득 찬다. 코드는 그대로 둔다. */
static void challenge_reset(ArraysScreen *a)
{
    bool here = is_cube(a) && a->mode == MODE_CODE;
    if (here) code_save(a);
    for (int m = 0; m < CUBE_MODULE_COUNT; m++) {
        a->module_done[m] = false;
        SooReset(&a->module_state[m], &CUBE_MODULES[m]);
    }
    a->phase = CH_READY;
    a->time_left = CUBE_TIME_LIMIT;
    a->phase_t = 0;
    a->boom_reason[0] = '\0';
    if (here) code_load(a);
}

static void challenge_start(ArraysScreen *a)
{
    challenge_reset(a);
    a->phase = CH_RUNNING;
    a->ed->focused = true;
}

static void challenge_fail(ArraysScreen *a, const char *reason)
{
    if (a->phase != CH_RUNNING) return;
    a->phase = CH_FUSE;
    a->phase_t = 0;
    a->cplaying = false;
    a->ed->focused = false;
    snprintf(a->boom_reason, sizeof a->boom_reason, "%s", reason);
}

/* Leaving the cube (another level or screen) abandons a running attempt.
 * 큐브를 떠나면 (다른 레벨이나 화면) 진행 중인 도전은 포기한다. */
static void challenge_abandon(ArraysScreen *a)
{
    if (a->phase != CH_DEFUSED && a->phase != CH_READY) challenge_reset(a);
}

static void challenge_tick(ArraysScreen *a, float dt, bool countdown)
{
    switch (a->phase) {
    case CH_RUNNING:
        if (!countdown) break;
        a->time_left -= dt;
        if (a->time_left <= 0) {
            a->time_left = 0;
            a->cplaying = false;
            a->ed->focused = false;
            a->phase = CH_EXPLODED;
            a->phase_t = 0;
            snprintf(a->boom_reason, sizeof a->boom_reason,
                     "Time ran out before all five modules were defused.");
        }
        break;
    case CH_FUSE:
        a->phase_t += dt;
        if (a->phase_t >= FUSE_SECONDS) {
            a->phase = CH_EXPLODED;
            a->phase_t = 0;
        }
        break;
    case CH_EXPLODED: a->phase_t += dt; break;
    default: break;
    }
}

static const char *clock_text(float seconds)
{
    int s = (int)ceilf(seconds);
    if (s < 0) s = 0;
    return TextFormat("%02d:%02d", s / 60, s % 60);
}

static int modules_defused(const ArraysScreen *a)
{
    int n = 0;
    for (int m = 0; m < CUBE_MODULE_COUNT; m++) n += a->module_done[m];
    return n;
}

/* ========================================================================
 * Selection / 선택
 * ====================================================================== */

static void select_module(ArraysScreen *a, int module)
{
    if (!is_cube(a) || module == a->module || module < 0 ||
        module >= CUBE_MODULE_COUNT)
        return;
    code_save(a);
    a->module = module;
    code_load(a);
    a->ed->focused = true;
    bomb_view_select(a->bomb, module, true);
}

static void set_mode(ArraysScreen *a, ArraysMode mode)
{
    if (a->mode == mode) return;
    if (a->mode == MODE_CODE) {
        code_save(a);
        a->cplaying = false;
        a->ed->focused = false;
    }
    a->mode = mode;
    a->playing = false;
    if (mode != MODE_CODE && is_cube(a)) a->level = 0;   /* cube = code only */
    if (mode == MODE_BUG) {
        a->level = 1;
        a->scenario = SOO_BOUNDS;
    } else {
        a->scenario = SOO_CORRECT;
    }
    if (mode == MODE_CODE) {
        code_load(a);
        a->ed->focused = true;
    }
    Load(a);
    bomb_view_reset(a->bomb);
}

static void set_level(ArraysScreen *a, int level)
{
    if (level == a->level || level < 0 || level >= LEVEL_TOTAL) return;
    if (level == LEVEL_CUBE && a->mode != MODE_CODE) {
        set_mode(a, MODE_CODE);
        if (level == a->level) return;
    }
    if (a->mode == MODE_CODE) code_save(a);
    if (is_cube(a)) challenge_abandon(a);
    a->level = level;
    if (a->mode == MODE_CODE) {
        code_load(a);
        a->ed->focused = true;
    }
    Load(a);
    bomb_view_reset(a->bomb);
    if (is_cube(a)) bomb_view_select(a->bomb, a->module, false);
}

/* ========================================================================
 * Bomb scene / 폭탄 장면
 * ====================================================================== */

static const SooState *shown_state(const ArraysScreen *a)
{
    return a->mode == MODE_CODE ? &a->cstate : &a->state;
}

static void build_scene(ArraysScreen *a)
{
    BombScene *s = &a->scene;
    memset(s, 0, sizeof *s);
    const SooState *st = shown_state(a);
    if (is_cube(a)) {
        s->cube = true;
        s->count = CUBE_MODULE_COUNT;
        int solved = 0;
        for (int m = 0; m < CUBE_MODULE_COUNT; m++) {
            s->modules[m].state = m == a->module ? &a->cstate
                                                 : &a->module_state[m];
            s->modules[m].solved = a->module_done[m];
            if (a->module_done[m]) solved++;
        }
        s->selected = a->module;
        if (solved == CUBE_MODULE_COUNT)
            snprintf(s->lcd_small, sizeof s->lcd_small, "DEFUSED");
        else
            snprintf(s->lcd_small, sizeof s->lcd_small, "MODULES %d/%d", solved,
                     CUBE_MODULE_COUNT);
    } else {
        s->count = 1;
        s->modules[0].state = st;
        s->modules[0].solved = st->status == SOO_SUCCESS;
        snprintf(s->lcd_small, sizeof s->lcd_small, "%.23s", ij_text(st));
    }
    if (is_cube(a)) {
        /* the countdown lives on the top display / 카운트다운은 윗면 표시창 */
        snprintf(s->lcd, sizeof s->lcd, "%s", clock_text(a->time_left));
        snprintf(s->lcd_caption, sizeof s->lcd_caption, "TIME");
        s->lcd_alert = a->phase == CH_FUSE ||
                       (a->phase == CH_RUNNING && a->time_left < ALERT_SECONDS);
        s->exploded = a->phase == CH_EXPLODED;
    } else {
        snprintf(s->lcd, sizeof s->lcd, "%02d/%02d", st->operations,
                 st->level->budget);
        snprintf(s->lcd_caption, sizeof s->lcd_caption, "OPS");
    }
    s->running = a->mode == MODE_CODE ? a->cplaying : a->playing;
}

/* ========================================================================
 * Lifecycle / 생성과 해제
 * ====================================================================== */

ArraysScreen *arrays_create(const EditorStyle *style)
{
    ArraysScreen *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->ed = calloc(1, sizeof *a->ed);
    a->prog = calloc(1, sizeof *a->prog);
    a->vm = calloc(1, sizeof *a->vm);
    a->ctrace = calloc(1, sizeof *a->ctrace);
    a->bomb = bomb_view_create((int)LEFT.width, (int)LEFT.height);
    bool ok = a->ed && a->prog && a->vm && a->ctrace && a->bomb;
    for (int n = 0; n < SOO_LEVEL_COUNT && ok; n++) {
        a->code[n] = calloc(1, ED_MAX_TEXT);
        if (!a->code[n]) ok = false;
        else array_starter_code(&SOO_LEVELS[n], a->code[n], ED_MAX_TEXT);
    }
    for (int m = 0; m < CUBE_MODULE_COUNT && ok; m++) {
        a->cube_code[m] = calloc(1, ED_MAX_TEXT);
        if (!a->cube_code[m]) ok = false;
        else array_starter_code(&CUBE_MODULES[m], a->cube_code[m], ED_MAX_TEXT);
        SooReset(&a->module_state[m], &CUBE_MODULES[m]);
    }
    if (!ok) {
        arrays_destroy(a);
        return NULL;
    }
    a->style = style;
    a->interval = SPEEDS[0];
    a->mode = MODE_CODE;
    a->phase = CH_READY;
    a->time_left = CUBE_TIME_LIMIT;
    ed_init(a->ed);
    code_load(a);
    a->ed->focused = true;
    Load(a);
    build_scene(a);          /* never draw an empty scene / 빈 장면은 그리지 않음 */
    return a;
}

void arrays_destroy(ArraysScreen *a)
{
    if (!a) return;
    for (int n = 0; n < SOO_LEVEL_COUNT; n++) free(a->code[n]);
    for (int m = 0; m < CUBE_MODULE_COUNT; m++) free(a->cube_code[m]);
    bomb_view_destroy(a->bomb);
    free(a->ctrace);
    free(a->vm);
    free(a->prog);
    free(a->ed);
    free(a);
}

bool arrays_typing(const ArraysScreen *a)
{
    return a->mode == MODE_CODE && a->ed->focused;
}

void arrays_leave(ArraysScreen *a)
{
    if (is_cube(a)) challenge_abandon(a);
    a->playing = false;
    a->cplaying = false;
    a->ed->focused = false;
    if (a->ed->ibeam) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        a->ed->ibeam = false;
    }
}

static bool cmd_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

/* ========================================================================
 * Update / 업데이트
 * ====================================================================== */

void arrays_update(ArraysScreen *a, float dt)
{
    Vector2 m = ui_mouse();
    bool in_bomb = CheckCollisionPointRec(m, LEFT);

    if (is_cube(a)) challenge_tick(a, dt, true);
    if (a->mode == MODE_CODE && locked(a)) {
        /* before START / after the blast: nothing to type or run
           START 전 / 폭발 후: 입력하거나 실행할 것이 없다 */
        while (GetCharPressed() > 0) {
        }
    } else if (a->mode == MODE_CODE) {
        editor_ui_update(a->ed, code_editor_rect(a), a->style);
        /* Turning or clicking the bomb keeps the editor focused, so typing
         * still goes into the code. Single-letter shortcuts are off in this
         * mode for the same reason.
         * 폭탄을 돌리거나 클릭해도 에디터 포커스를 유지하므로 입력은 계속 코드로
         * 간다. 같은 이유로 이 모드에서는 한 글자 단축키를 쓰지 않는다. */
        if (in_bomb && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            a->ed->focused = true;
        code_watch_edits(a);

        if (IsKeyPressed(KEY_F5) || (cmd_down() && IsKeyPressed(KEY_ENTER)))
            code_run(a);
        if (IsKeyPressed(KEY_F6)) code_step_key(a);
        if (IsKeyPressed(KEY_F7)) code_back(a);
        if (IsKeyPressed(KEY_F8)) code_reset(a);
        if (a->cplaying) {
            a->ctimer += dt;
            if (a->ctimer >= a->interval) {
                a->ctimer = 0;
                code_step(a);
            }
        }
    } else {
        /* Sumin's keys / 수민의 키 */
        if (IsKeyPressed(KEY_R)) {
            if (a->mode == MODE_BUG) a->scenario = SOO_BOUNDS;
            Load(a);
        }
        if (IsKeyPressed(KEY_A)) Answer(a, 0);
        if (IsKeyPressed(KEY_B)) Answer(a, 1);
        if (IsKeyPressed(KEY_C)) Answer(a, 2);
        bool revealed = !quiz(a) || a->answer >= 0;
        if (revealed && IsKeyPressed(KEY_SPACE) &&
            a->state.status < SOO_SUCCESS)
            a->playing = !a->playing;
        if (revealed && IsKeyPressed(KEY_RIGHT)) {
            a->playing = false;
            Step(a);
        }
        if (revealed && IsKeyPressed(KEY_LEFT)) Back(a);
        for (int n = 0; n < SOO_LEVEL_COUNT; n++)
            if (a->mode != MODE_BUG && IsKeyPressed(KEY_ONE + n)) set_level(a, n);
        if (IsKeyPressed(KEY_S))
            a->interval = a->interval > 0.3f ? 0.15f
                        : a->interval > 0.1f ? 0.05f : 0.45f;
        if (a->playing) {
            a->timer += dt;
            if (a->timer >= a->interval) {
                a->timer = 0;
                Step(a);
            }
        }
    }

    build_scene(a);
    Vector2 local = { m.x - LEFT.x, m.y - LEFT.y };
    int clicked = bomb_view_update(a->bomb, &a->scene, dt, local,
                                   in_bomb && !locked(a));
    if (clicked >= 0) {
        select_module(a, clicked);
        build_scene(a);
    }
    if (a->mode == MODE_CODE) code_update_markers(a);
}

void arrays_render_view(ArraysScreen *a)
{
    bomb_view_render(a->bomb, &a->scene);
}

/* ========================================================================
 * Draw / 그리기
 * ====================================================================== */

static void draw_top_row(ArraysScreen *a)
{
    static const char *const modes[4] = { "Write code", "Predict", "Explore",
                                          "Find the bug" };
    if (is_cube(a)) {
        /* The cube is write-code only, so its row shows the rules instead.
         * 큐브는 코드 작성만 있으므로 그 자리에 규칙을 보여 준다. */
        DrawText("TIMED CHALLENGE", 1174, ROW2_Y + 6, FS_SMALL, UI_ERROR);
        DrawText(TextFormat("%s  /  5 modules  /  a wrong run explodes",
                            clock_text(CUBE_TIME_LIMIT)),
                 1174, ROW2_Y + 30, FS_SMALL, UI_MUTED);
    } else {
        for (int n = 0; n < 4; n++)
            if (ui_button((Rectangle){ 1162 + n * 182, ROW2_Y, 170, 52 },
                          modes[n], true, a->mode == (ArraysMode)n))
                set_mode(a, (ArraysMode)n);
    }
    for (int n = 0; n < LEVEL_TOTAL; n++) {
        Rectangle r = { 30 + n * 160, ROW2_Y, 150, 52 };
        const char *label = n == LEVEL_CUBE ? "Cube bomb"
                                            : TextFormat("Level %d", n + 1);
        /* The cube can always be opened; it switches to Write code.
         * 큐브는 언제든 열 수 있다. Write code로 바뀐다. */
        bool enabled = a->mode != MODE_BUG || n == 1 || n == LEVEL_CUBE;
        if (ui_button(r, label, enabled, a->level == n)) set_level(a, n);
        if (a->completed[n])
            DrawCircle((int)(r.x + r.width - 14), (int)(r.y + 14), 5,
                       UI_SUCCESS);
    }
}

static Color status_color(SooStatus st)
{
    return st == SOO_FAILURE ? UI_ERROR : st == SOO_SUCCESS ? UI_SUCCESS
                                                            : UI_INK;
}

/* The 3D bomb with a title in the corner and a log strip at the bottom.
 * 모서리의 제목과 아래쪽 기록 줄이 있는 3D 폭탄. */
static void draw_bomb_panel(ArraysScreen *a, const char *heading, Color hcol,
                            const char *message, const char *step)
{
    Texture2D t = bomb_view_texture(a->bomb);
    ui_draw_view(t, (Rectangle){ 0, 0, (float)t.width, (float)-t.height },
                 (Rectangle){ LEFT.x, LEFT.y, (float)t.width, (float)t.height });
    DrawRectangleLinesEx(LEFT, 2.0f, UI_BORDER);

    const SooLevel *l = cur_level(a);
    const char *title = is_cube(a)
        ? TextFormat("Cube bomb  /  %s", l->title + 4)
        : l->title;
    DrawText(title, (int)LEFT.x + 24, (int)LEFT.y + 18, FS_BODY, UI_INK);
    DrawText(l->concept, (int)LEFT.x + 24, (int)LEFT.y + 56, FS_SMALL,
             UI_ACCENT);
    if (is_cube(a) && (a->phase == CH_RUNNING || a->phase == CH_FUSE)) {
        bool alert = a->phase == CH_FUSE || a->time_left < ALERT_SECONDS;
        ui_text_right(clock_text(a->time_left), (int)(LEFT.x + LEFT.width) - 24,
                      (int)LEFT.y + 16, FS_TITLE, alert ? UI_ERROR : UI_INK);
        ui_text_right("Drag to turn. Click a face to pick it.",
                      (int)(LEFT.x + LEFT.width) - 24, (int)LEFT.y + 62,
                      FS_SMALL, UI_MUTED);
    } else if (is_cube(a) && a->phase == CH_DEFUSED) {
        ui_text_right("Drag to turn the bomb. Click a face to pick it.",
                      (int)(LEFT.x + LEFT.width) - 24, (int)LEFT.y + 24,
                      FS_SMALL, UI_MUTED);
    }

    int y = (int)(LEFT.y + LEFT.height) - 96;
    DrawText(heading, (int)LEFT.x + 24, y, FS_SMALL, hcol);
    if (step) ui_text_right(step, (int)(LEFT.x + LEFT.width) - 24, y, FS_SMALL,
                            UI_MUTED);
    ui_wrapped(message, (int)LEFT.x + 24, y + 30, (int)LEFT.width - 48,
               FS_SMALL, hcol);
}

/* Start card, restart card and the defused banner on the bomb panel.
 * 폭탄 패널 위의 시작 카드, 재시작 카드, 해체 알림. */
static void draw_challenge_overlay(ArraysScreen *a)
{
    float cx = LEFT.x + LEFT.width / 2;
    if (a->phase == CH_READY || (a->phase == CH_EXPLODED &&
                                 a->phase_t >= RESTART_DELAY)) {
        bool ready = a->phase == CH_READY;
        if (ready)
            DrawRectangle((int)LEFT.x + 2, (int)LEFT.y + 90,
                          (int)LEFT.width - 4, (int)LEFT.height - 210,
                          (Color){ 12, 8, 6, 130 });
        Rectangle card = { cx - 330, LEFT.y + 230, 660, 400 };
        DrawRectangleRounded((Rectangle){ card.x, card.y + 8, card.width,
                                          card.height },
                             0.08f, 8, (Color){ 10, 6, 20, 220 });
        DrawRectangleRounded(card, 0.08f, 8, UI_PANEL);
        DrawRectangleRoundedLinesEx(card, 0.08f, 8, 3.0f,
                                    ready ? UI_ACCENT : UI_ERROR);
        const char *title = ready ? "TIMED CHALLENGE" : "THE BOMB EXPLODED";
        DrawText(title, (int)(cx - MeasureText(title, FS_TITLE) / 2),
                 (int)card.y + 36, FS_TITLE, ready ? UI_ACCENT : UI_ERROR);
        int y = (int)card.y + 100;
        if (ready) {
            y = ui_wrapped(TextFormat("Defuse all five modules before the clock "
                                      "reaches 00:00. You have %s.",
                                      clock_text(CUBE_TIME_LIMIT)),
                           (int)card.x + 44, y, (int)card.width - 88, FS_SMALL,
                           UI_INK) + 10;
            ui_wrapped("A run that strikes, or ends with its module unsolved, "
                       "sets the bomb off. Compile errors are safe.",
                       (int)card.x + 44, y, (int)card.width - 88, FS_SMALL,
                       UI_MUTED);
        } else {
            y = ui_wrapped(a->boom_reason, (int)card.x + 44, y,
                           (int)card.width - 88, FS_SMALL, UI_INK) + 10;
            DrawText(TextFormat("Modules defused: %d / %d", modules_defused(a),
                                CUBE_MODULE_COUNT),
                     (int)card.x + 44, y, FS_SMALL, UI_MUTED);
            DrawText("Your code is kept. Fix it and go again.",
                     (int)card.x + 44, y + 30, FS_SMALL, UI_MUTED);
        }
        Rectangle b = { cx - 160, card.y + card.height - 110, 320, 72 };
        if (ui_button(b, ready ? "START" : "RESTART", true, true))
            challenge_start(a);
    } else if (a->phase == CH_DEFUSED) {
        Rectangle card = { cx - 300, LEFT.y + LEFT.height - 330, 600, 190 };
        DrawRectangleRounded(card, 0.1f, 8, (Color){ 20, 40, 34, 235 });
        DrawRectangleRoundedLinesEx(card, 0.1f, 8, 3.0f, UI_SUCCESS);
        const char *t = "BOMB DEFUSED";
        DrawText(t, (int)(cx - MeasureText(t, FS_TITLE) / 2), (int)card.y + 24,
                 FS_TITLE, UI_SUCCESS);
        const char *left = TextFormat("with %s left on the clock",
                                      clock_text(a->time_left));
        DrawText(left, (int)(cx - MeasureText(left, FS_SMALL) / 2),
                 (int)card.y + 76, FS_SMALL, UI_INK);
        if (ui_button((Rectangle){ cx - 120, card.y + 116, 240, 52 },
                      "Play again", true, false))
            challenge_start(a);
    }
}

static void draw_code_mode(ArraysScreen *a)
{
    const SooState *st = &a->cstate;
    const char *heading, *message;
    Color hcol = UI_INK;
    bool compile_error = !a->compiled_ok &&
                         a->ed->version == a->compiled_version;
    if (compile_error) {
        heading = "COMPILE ERROR";
        hcol = UI_ERROR;
        message = a->note;
    } else if (code_fresh(a) && a->ccursor > 0) {
        heading = st->status == SOO_SUCCESS   ? "DEFUSED"
                : st->status == SOO_FAILURE   ? "STRIKE"
                                              : "EXECUTION LOG";
        hcol = status_color(st->status);
        message = st->message;
    } else if (code_fresh(a)) {
        heading = "READY";
        message = st->message;
    } else if (is_cube(a) && a->module_done[a->module]) {
        heading = "MODULE DEFUSED";
        hcol = UI_SUCCESS;
        message = "This module is safe. Pick another face, or run your code "
                  "again to replay it.";
    } else {
        heading = "WRITE YOUR CODE";
        message = a->note[0] ? a->note
                             : "Write C in the editor, then press Run. Each read "
                               "and write plays out on the bomb, one step at a "
                               "time.";
    }
    if (is_cube(a) && a->phase == CH_READY) {
        heading = "READ THE MISSIONS";
        hcol = UI_INK;
        message = "Click the module buttons on the right to read each mission. "
                  "The clock starts when you press START.";
    } else if (is_cube(a) && (a->phase == CH_FUSE || a->phase == CH_EXPLODED)) {
        heading = "BOOM";
        hcol = UI_ERROR;
        message = a->boom_reason;
    }
    char step[32] = "";
    if (code_fresh(a))
        snprintf(step, sizeof step, "Step %d / %d", a->ccursor, code_length(a));
    draw_bomb_panel(a, heading, hcol, message, step[0] ? step : NULL);
    if (is_cube(a)) draw_challenge_overlay(a);

    ui_panel(RIGHT);
    if (is_cube(a)) {
        int w = (RW - 4 * 10) / CUBE_MODULE_COUNT;
        for (int m = 0; m < CUBE_MODULE_COUNT; m++) {
            Rectangle r = { RX + m * (w + 10), RTOP - 6, w, 50 };
            if (ui_button(r, TextFormat("%d %s", m + 1, cube_module_name(m)),
                          true, m == a->module))
                select_module(a, m);
            if (a->module_done[m])
                DrawCircle((int)(r.x + r.width - 12), (int)(r.y + 12), 5,
                           UI_SUCCESS);
        }
    }
    int y = mission_text(RTOP + chips_height(a), cur_level(a)->objective) + 8;
    char tools[96];
    array_tools_text(cur_level(a), tools, sizeof tools);
    DrawText(TextFormat("You can use   %s", tools), RX, y, FS_SMALL, UI_MUTED);
    editor_ui_draw(a->ed, code_editor_rect(a), a->style);
    if (locked(a)) {
        Rectangle er = code_editor_rect(a);
        DrawRectangleRec(er, (Color){ 22, 17, 40, 200 });
        const char *t = a->phase == CH_READY
                            ? "Press START on the bomb to begin"
                            : "Press RESTART on the bomb to try again";
        DrawText(t, (int)(er.x + (er.width - MeasureText(t, FS_SMALL)) / 2),
                 (int)(er.y + er.height / 2 - 10), FS_SMALL, UI_INK);
    }

    DrawText("SPEED", 52, ROW_Y + 18, FS_SMALL, UI_MUTED);
    for (int n = 0; n < 3; n++)
        if (ui_button((Rectangle){ 160 + n * 170, ROW_Y, 156, ROW_H },
                      SPEED_NAMES[n], true, a->interval == SPEEDS[n])) {
            a->interval = SPEEDS[n];
            a->ed->focused = true;
        }

    float bw = (RIGHT.width - 3 * 14) / 4;
    bool can_step = (!code_fresh(a) || !code_done(a)) && !locked(a);
    bool control = false;
    if (ui_button((Rectangle){ RIGHT.x, ROW_Y, bw, ROW_H },
                  a->cplaying ? "Pause" : "Run", !locked(a), a->cplaying)) {
        code_run(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + (bw + 14), ROW_Y, bw, ROW_H }, "Step",
                  can_step, false)) {
        code_step_key(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + 2 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Back", code_fresh(a) && a->ccursor > 0 && !locked(a),
                  false)) {
        code_back(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + 3 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Reset", !locked(a), false)) {
        code_reset(a);
        control = true;
    }
    if (control) a->ed->focused = true;

#ifdef __APPLE__
    const char *keys = "Cmd+Enter run    fn+F6 step    fn+F7 back    "
                       "fn+F8 reset    Esc leaves the editor";
#else
    const char *keys = "F5 or Ctrl+Enter run    F6 step    F7 back    "
                       "F8 reset    Esc leaves the editor";
#endif
    ui_text_right(keys, (int)(RIGHT.x + RIGHT.width), HINT_Y, FS_SMALL,
                  UI_MUTED);
}

static void draw_sample_mode(ArraysScreen *a)
{
    bool sample_done = quiz(a) && a->state.status == SOO_SUCCESS;
    const char *heading = sample_done ? "SAMPLE COMPLETE"
                        : a->state.status == SOO_SUCCESS ? "DEFUSED"
                        : a->state.status == SOO_FAILURE ? "STRIKE"
                                                         : "EXECUTION LOG";
    const char *message =
        quiz(a) && a->answer < 0
            ? (a->mode == MODE_BUG ? "Find the condition that keeps i inside "
                                     "the array."
                                   : "Read the code and predict what happens.")
        : sample_done ? TextFormat("The sample reached the target in %d "
                                   "operations.", a->state.operations)
                      : a->state.message;
    char step[32];
    snprintf(step, sizeof step, "Step %d / %d", a->cursor, a->count);
    draw_bomb_panel(a, heading, status_color(a->state.status), message, step);

    ui_panel(RIGHT);
    if (a->mode == MODE_BUG) DrawBounds(a);
    else if (a->mode == MODE_PREDICT) DrawQuiz(a);
    else DrawExplore(a);

    bool revealed = !quiz(a) || a->answer >= 0;
    bool live = revealed && a->state.status < SOO_SUCCESS;
    float bw = (RIGHT.width - 3 * 14) / 4;
    if (ui_button((Rectangle){ RIGHT.x, ROW_Y, bw, ROW_H },
                  a->playing ? "Pause" : "Play", live, false))
        a->playing = !a->playing;
    if (ui_button((Rectangle){ RIGHT.x + (bw + 14), ROW_Y, bw, ROW_H }, "Step",
                  live, false)) {
        a->playing = false;
        Step(a);
    }
    if (ui_button((Rectangle){ RIGHT.x + 2 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Back", a->cursor > 0, false))
        Back(a);
    if (ui_button((Rectangle){ RIGHT.x + 3 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Reset", true, false)) {
        if (a->mode == MODE_BUG) a->scenario = SOO_BOUNDS;
        Load(a);
    }
    ui_text_right("Space play    Arrows step    R reset    1-6 level    "
                  "A/B/C answer    S speed",
                  (int)(RIGHT.x + RIGHT.width), HINT_Y, FS_SMALL, UI_MUTED);
}

void arrays_draw(ArraysScreen *a)
{
    ui_background();
    ui_header("Arrays", NULL);
    draw_top_row(a);
    if (a->mode == MODE_CODE) draw_code_mode(a);
    else draw_sample_mode(a);
}

/* ========================================================================
 * Screenshot scripting / 스크린샷 스크립트
 * ====================================================================== */

/* Compile, record and play an example to its end (used by --shot).
 * 예시를 컴파일하고 기록하여 끝까지 재생한다 (--shot에서 사용). */
static void qa_play(ArraysScreen *a, const char *src, int steps)
{
    ed_set_text(a->ed, src, true);
    code_watch_edits(a);
    code_prepare(a);
    for (int k = 0; k < steps; k++) code_step(a);
}

void arrays_qa(ArraysScreen *a, const char *mode, int level, int steps,
               int variant)
{
    ArraysMode m = MODE_CODE;
    if (strcmp(mode, "predict") == 0) m = MODE_PREDICT;
    else if (strcmp(mode, "explore") == 0) m = MODE_EXPLORE;
    else if (strcmp(mode, "bug") == 0) m = MODE_BUG;
    set_mode(a, m);
    if (m != MODE_BUG) set_level(a, level);

    if (m == MODE_CODE && is_cube(a)) {
        /* variant 0, no steps: READY (START card). Otherwise the challenge is
         * running with 6:52 left. 40/41/42: exploded (0.35 s, 1.5 s, with
         * the RESTART card). 43: 0:24 left, display blinking.
         * variant 20+: solve modules 0..(variant-21) first; 10+k: module k
         * variant 0이고 단계 없음: READY (START 카드). 그 밖에는 6:52가 남은
         * 도전 진행 중. 40/41/42: 폭발 (0.35초, 1.5초, RESTART 카드).
         * 43: 0:24 남음, 표시창 깜박임. 20 이상: 앞 모듈을 먼저 푼다. */
        challenge_reset(a);
        if (variant != 0 || steps > 0) {
            a->phase = CH_RUNNING;
            a->time_left = variant == 43 ? 24.0f : 412.0f;
        }
        if (variant >= 40 && variant <= 42) {
            select_module(a, 2);
            a->phase = CH_EXPLODED;
            a->phase_t = variant == 42 ? RESTART_DELAY : 0;
            a->qa_settle = variant == 40 ? 0.35f : 0;
            snprintf(a->boom_reason, sizeof a->boom_reason,
                     "Module 3 (Mirror): Out of bounds: cards[6] does not "
                     "exist.");
            build_scene(a);
            return;
        }
        if (variant >= 20 && variant < 40) {
            int solved = variant - 20;
            for (int k = 0; k < solved && k < CUBE_MODULE_COUNT; k++) {
                select_module(a, k);
                qa_play(a, cube_example_solution(k), 1000);
                code_save(a);
            }
            select_module(a, solved < CUBE_MODULE_COUNT ? solved : 0);
        } else if (variant >= 10 && variant < 20) {
            select_module(a, variant - 10);
        }
        if (steps > 0) qa_play(a, cube_example_solution(a->module), steps);
        bomb_view_select(a->bomb, a->module, false);
        code_update_markers(a);
    } else if (m == MODE_CODE) {
        const char *src = array_example_solution(a->level);
        if (variant == 1)
            src = "void run(void) {\n"
                  "    for (int i = 0; i <= N; ++i) {\n"
                  "        if (cards[i] == 1)\n"
                  "            cards[i] = 0;\n"
                  "    }\n"
                  "}\n";
        else if (variant == 2)
            src = "void run(void) {\n"
                  "    for (int i = 0; i < N; ++i) {\n"
                  "        if (card[i] == 1)\n"
                  "            cards[i] = 0\n"
                  "    }\n"
                  "}\n";
        if (variant != 3) qa_play(a, src, variant == 2 ? 0 : steps);
        a->ed->focused = variant != 1;
        code_update_markers(a);
    } else if (m == MODE_EXPLORE) {
        if (variant >= 0 && variant <= 3) {
            a->scenario = (SooScenario)variant;
            Load(a);
        }
        for (int k = 0; k < steps; k++) Step(a);
    } else {
        Answer(a, variant);
        a->playing = false;
        for (int k = 0; k < steps; k++) Step(a);
    }
    build_scene(a);
}

/* Let the 3D view settle (camera fit, card lift) before a screenshot.
 * 스크린샷 전에 3D 뷰가 자리를 잡게 한다 (카메라 맞춤, 카드 올라옴). */
void arrays_qa_settle(ArraysScreen *a, float seconds)
{
    Vector2 off = { -10000, -10000 };
    if (a->qa_settle > 0) seconds = a->qa_settle;
    for (float t = 0; t < seconds; t += 1.0f / 60.0f) {
        if (is_cube(a)) challenge_tick(a, 1.0f / 60.0f, false);
        build_scene(a);
        bomb_view_update(a->bomb, &a->scene, 1.0f / 60.0f, off, false);
    }
}

/* Type src, press Run and let playback finish, as a player would.
 * 플레이어처럼 src를 입력하고 Run을 눌러 재생이 끝날 때까지 둔다. */
static void st_run(ArraysScreen *a, const char *src)
{
    ed_set_text(a->ed, src, true);
    code_watch_edits(a);
    code_run(a);
    for (int k = 0; k < 2000 && a->cplaying; k++) code_step(a);
}

static int st_check(bool ok, const char *what)
{
    printf("  %s %s\n", what, ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}

int arrays_self_test(ArraysScreen *a)
{
    int failures = 0;
    puts("Cube bomb challenge rules:");
    set_mode(a, MODE_BUG);
    set_level(a, LEVEL_CUBE);
    failures += st_check(is_cube(a) && a->mode == MODE_CODE,
                         "Cube bomb tab works from Find the bug:");

    challenge_reset(a);
    st_run(a, cube_example_solution(a->module));
    failures += st_check(a->phase == CH_READY && !a->have_trace,
                         "Run is locked before START:");

    challenge_start(a);
    st_run(a, "void run(void) {\n    cards[0] = \n}\n");
    failures += st_check(a->phase == CH_RUNNING,
                         "a compile error does not explode:");

    st_run(a, "void run(void) {\n}\n");
    failures += st_check(a->phase == CH_FUSE, "an unsolved run lights the fuse:");
    challenge_tick(a, 1.0f, true);
    failures += st_check(a->phase == CH_EXPLODED, "...and then explodes:");

    challenge_start(a);
    st_run(a, "void run(void) {\n    cards[N] = 1;\n}\n");
    failures += st_check(a->phase == CH_FUSE, "an out-of-bounds strike explodes:");

    challenge_start(a);
    challenge_tick(a, CUBE_TIME_LIMIT - 1.0f, true);
    failures += st_check(a->phase == CH_RUNNING, "clock still running at 00:01:");
    challenge_tick(a, 2.0f, true);
    failures += st_check(a->phase == CH_EXPLODED, "time running out explodes:");

    challenge_start(a);
    for (int m = 0; m < CUBE_MODULE_COUNT; m++) {
        select_module(a, m);
        st_run(a, cube_example_solution(m));
    }
    failures += st_check(a->phase == CH_DEFUSED && modules_defused(a) == 5,
                         "solving all five modules defuses the bomb:");

    challenge_start(a);
    set_level(a, 0);
    set_level(a, LEVEL_CUBE);
    failures += st_check(a->phase == CH_READY && a->time_left == CUBE_TIME_LIMIT,
                         "leaving the cube abandons the attempt:");

    puts("Cube bomb click picking:");
    challenge_reset(a);
    build_scene(a);
    return failures + bomb_view_self_test(a->bomb, &a->scene);
}

/* Open a bomb from the mission map: 0-5 = bombs 1-6, 6 = cube.
 * 미션 지도에서 폭탄을 연다: 0-5 = 폭탄 1-6, 6 = 큐브. */
void arrays_open(ArraysScreen *a, int level, bool find_the_bug)
{
    if (find_the_bug) {
        set_mode(a, MODE_BUG);
        return;
    }
    if (a->mode != MODE_CODE) set_mode(a, MODE_CODE);
    set_level(a, level);
    a->ed->focused = true;
}

void arrays_tick(ArraysScreen *a, float dt)
{
    if (is_cube(a)) challenge_tick(a, dt, true);
    /* Keep the scene current (the 3D view is still drawn under the pop-up,
     * even on the first visit) and let animations such as the explosion
     * play, with no mouse input.
     * 장면을 최신으로 유지하고 (처음 들어왔을 때도 팝업 아래에서 3D 화면은
     * 그려진다) 폭발 같은 애니메이션을 마우스 입력 없이 재생한다. */
    build_scene(a);
    bomb_view_update(a->bomb, &a->scene, dt, (Vector2){ -10000, -10000 },
                     false);
}

void arrays_defused(const ArraysScreen *a, bool out[7])
{
    for (int n = 0; n < LEVEL_TOTAL && n < 7; n++) out[n] = a->completed[n];
}
