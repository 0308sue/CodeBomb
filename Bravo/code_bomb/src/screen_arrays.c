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
 */
#include "screen_arrays.h"

#include "array_bridge.h"
#include "bomb3d.h"
#include "bomb_modules.h"
#include "compiler.h"
#include "editor.h"
#include "quiz_SOO.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { MODE_CODE, MODE_PREDICT, MODE_EXPLORE, MODE_BUG } ArraysMode;

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
};

static const SooLevel *cur_level(const ArraysScreen *a)
{
    return a->level == LEVEL_CUBE ? &CUBE_MODULES[a->module]
                                  : &SOO_LEVELS[a->level];
}

static bool is_cube(const ArraysScreen *a) { return a->level == LEVEL_CUBE; }

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
            if (all) a->completed[LEVEL_CUBE] = true;
        } else {
            a->completed[a->level] = true;
        }
    }
    if (code_done(a)) a->cplaying = false;
}

static void code_run(ArraysScreen *a)
{
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
    snprintf(s->lcd, sizeof s->lcd, "%02d/%02d", st->operations,
             st->level->budget);
    snprintf(s->lcd_caption, sizeof s->lcd_caption, "OPS");
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
    ed_init(a->ed);
    code_load(a);
    a->ed->focused = true;
    Load(a);
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

    if (a->mode == MODE_CODE) {
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
    int clicked = bomb_view_update(a->bomb, &a->scene, dt, local, in_bomb);
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
    for (int n = 0; n < 4; n++) {
        bool enabled = !(is_cube(a) && n != MODE_CODE);
        if (ui_button((Rectangle){ 1162 + n * 182, ROW2_Y, 170, 52 }, modes[n],
                      enabled, a->mode == (ArraysMode)n))
            set_mode(a, (ArraysMode)n);
    }
    for (int n = 0; n < LEVEL_TOTAL; n++) {
        Rectangle r = { 30 + n * 160, ROW2_Y, 150, 52 };
        const char *label = n == LEVEL_CUBE ? "Cube bomb"
                                            : TextFormat("Level %d", n + 1);
        bool enabled = a->mode != MODE_BUG || n == 1;
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
    DrawTextureRec(t, (Rectangle){ 0, 0, (float)t.width, (float)-t.height },
                   (Vector2){ LEFT.x, LEFT.y }, WHITE);
    DrawRectangleLinesEx(LEFT, 2.0f, UI_BORDER);

    const SooLevel *l = cur_level(a);
    const char *title = is_cube(a)
        ? TextFormat("Cube bomb  /  %s", l->title + 4)
        : l->title;
    DrawText(title, (int)LEFT.x + 24, (int)LEFT.y + 18, FS_BODY, UI_INK);
    DrawText(l->concept, (int)LEFT.x + 24, (int)LEFT.y + 56, FS_SMALL,
             UI_ACCENT);
    if (is_cube(a))
        ui_text_right("Drag to turn the bomb. Click a face to pick it.",
                      (int)(LEFT.x + LEFT.width) - 24, (int)LEFT.y + 24,
                      FS_SMALL, UI_MUTED);

    int y = (int)(LEFT.y + LEFT.height) - 96;
    DrawText(heading, (int)LEFT.x + 24, y, FS_SMALL, hcol);
    if (step) ui_text_right(step, (int)(LEFT.x + LEFT.width) - 24, y, FS_SMALL,
                            UI_MUTED);
    ui_wrapped(message, (int)LEFT.x + 24, y + 30, (int)LEFT.width - 48,
               FS_SMALL, hcol);
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
    char step[32] = "";
    if (code_fresh(a))
        snprintf(step, sizeof step, "Step %d / %d", a->ccursor, code_length(a));
    draw_bomb_panel(a, heading, hcol, message, step[0] ? step : NULL);

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

    DrawText("SPEED", 52, ROW_Y + 18, FS_SMALL, UI_MUTED);
    for (int n = 0; n < 3; n++)
        if (ui_button((Rectangle){ 160 + n * 170, ROW_Y, 156, ROW_H },
                      SPEED_NAMES[n], true, a->interval == SPEEDS[n])) {
            a->interval = SPEEDS[n];
            a->ed->focused = true;
        }

    float bw = (RIGHT.width - 3 * 14) / 4;
    bool can_step = !code_fresh(a) || !code_done(a);
    bool control = false;
    if (ui_button((Rectangle){ RIGHT.x, ROW_Y, bw, ROW_H },
                  a->cplaying ? "Pause" : "Run", true, a->cplaying)) {
        code_run(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + (bw + 14), ROW_Y, bw, ROW_H }, "Step",
                  can_step, false)) {
        code_step_key(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + 2 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Back", code_fresh(a) && a->ccursor > 0, false)) {
        code_back(a);
        control = true;
    }
    if (ui_button((Rectangle){ RIGHT.x + 3 * (bw + 14), ROW_Y, bw, ROW_H },
                  "Reset", true, false)) {
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
        /* variant 20+: solve modules 0..(variant-21) first; 10+k: module k
           variant 20 이상: 앞의 모듈을 먼저 푼다; 10+k: k번 모듈 */
        if (variant >= 20) {
            int solved = variant - 20;
            for (int k = 0; k < solved && k < CUBE_MODULE_COUNT; k++) {
                select_module(a, k);
                qa_play(a, cube_example_solution(k), 1000);
                code_save(a);
            }
            select_module(a, solved < CUBE_MODULE_COUNT ? solved : 0);
        } else if (variant >= 10) {
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
    for (float t = 0; t < seconds; t += 1.0f / 60.0f) {
        build_scene(a);
        bomb_view_update(a->bomb, &a->scene, 1.0f / 60.0f, off, false);
    }
}

int arrays_self_test(ArraysScreen *a)
{
    set_level(a, LEVEL_CUBE);
    build_scene(a);
    return bomb_view_self_test(a->bomb, &a->scene);
}
