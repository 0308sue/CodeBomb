/*
 * screen_mailroom.c - the Mailroom levels: editor, 3D scene and VM inspector
 * screen_mailroom.c - 우편실 레벨: 에디터, 3D 장면, VM 인스펙터
 *
 * The level row under the header lists every level of docs/MAILROOM.md;
 * planned ones are dimmed. Each level keeps its own code.
 * 헤더 아래 레벨 줄은 docs/MAILROOM.md의 모든 레벨을 보여 준다. 계획된
 * 레벨은 흐리게 표시한다. 레벨마다 코드가 따로 저장된다.
 *
 * The VM runs live here: take() and put() pause it while the robot moves.
 * (The Arrays screen instead records a trace first, for Sumin's Back.)
 * 여기서는 VM이 실시간으로 실행된다: take()와 put()은 로봇이 움직이는 동안
 * VM을 멈춘다. (배열 화면은 수민의 Back을 위해 먼저 기록을 만든다.)
 */
#include "screen_mailroom.h"

#include "compiler.h"
#include "editor.h"
#include "help.h"
#include "level.h"
#include "programs.h"
#include "scene.h"
#include "ui.h"
#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- layout / 화면 배치 -------------------------------------------------- */
static const Rectangle LPANEL = { 30, 180, 760, 884 };
static const Rectangle VIEW   = { 814, 180, 1076, 560 };
static const Rectangle INSP   = { 814, 754, 1076, 310 };
#define MX 57
#define MW 706
#define ROW_Y 1082
#define ROW_H 56
#define SUBMIT_TESTS 5
#define INSTR_RATE 3.0f      /* instructions per second at 1x / 1배속 초당 명령 */

static const float SPEEDS[] = { 1.0f, 2.0f, 4.0f, 16.0f };
static const char *const SPEED_NAMES[] = { "1x", "2x", "4x", "Fast" };
#define SPEED_COUNT 4

typedef enum { RUN_IDLE, RUN_GO, RUN_STEP_LINE, RUN_STEP_OP } RunMode;

struct MailroomScreen {
    const LevelDef    *level;
    Program           *prog;
    VM                *vm;
    LevelRun           run;
    Scene              scene;
    RenderTexture2D    view;

    Editor            *ed;
    const EditorStyle *style;
    char               known[VM_NATIVES_MAX][VM_NAME_MAX];
    const char        *known_ptrs[VM_NATIVES_MAX];

    bool               compiled_ok;
    unsigned           compiled_version;
    unsigned           seen_version;
    CompileResult      compile;

    RunMode            mode;
    bool               event_sent;
    bool               finish_reported;
    int                step_line;
    int                speed_index;
    float              instr_accum;

    char               message[512];
    Color              message_color;

    bool               examples_open;    /* Examples menu / Examples 메뉴 */
    int                slot;             /* LEVEL_LIST index / 레벨 칸 */
    char              *code[LEVEL_SLOTS];/* saved code per level / 레벨별 코드 */
    bool               passed[LEVEL_SLOTS];
    bool               machine_view;     /* inspector: Simple or Machine
                                            인스펙터: Simple 또는 Machine */
};

/* ---- helpers / 도우미 --------------------------------------------------- */

static void set_message(MailroomScreen *a, Color c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->message, sizeof a->message, fmt, ap);
    va_end(ap);
    a->message_color = c;
}

/* Editor rectangle below the brief (brief height varies).
 * 설명 아래의 에디터 영역 (설명 높이에 따라 달라짐). */
/* The long explanation lives in the Help screen; the panel keeps the
 * title, a one-line goal and the functions.
 * 긴 설명은 도움말 화면에 있고, 패널에는 제목, 한 줄 목표, 함수만 둔다. */
static int examples_y(const MailroomScreen *a)
{
    (void)a;
    return 356;
}

static Rectangle editor_rect(const MailroomScreen *a)
{
    int top = examples_y(a) + 44 + 14;
    return (Rectangle){ MX, (float)top, MW, (float)(940 - top) };
}

/* ---- compile and run / 컴파일과 실행 ------------------------------------ */

static bool done(const MailroomScreen *a)
{
    return a->vm->status == VM_HALTED || a->vm->status == VM_ERROR;
}

static bool fresh(const MailroomScreen *a)
{
    return a->compiled_ok && a->ed->version == a->compiled_version;
}

static void reset_run(MailroomScreen *a)
{
    level_run_init(&a->run, a->level, 0, false);
    vm_init(a->vm, a->prog, &a->run);
    a->scene.reset(a->scene.state, &a->run);
    a->mode = RUN_IDLE;
    a->event_sent = false;
    a->finish_reported = !a->compiled_ok;
    a->instr_accum = 0.0f;
}

static bool compile_now(MailroomScreen *a)
{
    prog_init(a->prog);
    a->level->register_natives(a->prog);
    a->compiled_ok = compile_program(a->ed->text, a->ed->len, a->prog,
                                     &a->compile);
    a->compiled_version = a->ed->version;
    reset_run(a);
    if (!a->compiled_ok) {
        set_message(a, UI_ERROR, "Line %d, column %d: %s", a->compile.line,
                    a->compile.col, a->compile.message);
        return false;
    }
    set_message(a, UI_MUTED, "Compiled: %d statement%s, %d instructions.",
                a->compile.statements, a->compile.statements == 1 ? "" : "s",
                a->prog->code_len);
    return true;
}

static void start(MailroomScreen *a, RunMode mode)
{
    if (!fresh(a)) {
        if (!compile_now(a)) return;
    } else if (done(a)) {
        reset_run(a);
    }
    if (a->vm->steps == 0 && a->mode == RUN_IDLE)
        set_message(a, UI_MUTED, "Running...");
    a->mode = mode;
    a->step_line = vm_current_line(a->vm);
    a->instr_accum = 1.0f;
}

static void toggle_run(MailroomScreen *a)
{
    if (a->mode == RUN_GO) a->mode = RUN_IDLE;
    else start(a, RUN_GO);
}

static void submit(MailroomScreen *a)
{
    if (!fresh(a) && !compile_now(a)) return;
    SubmitResult r;
    level_submit(a->level, a->prog, SUBMIT_TESTS, &r);
    if (r.passed) {
        int size = a->compile.statements;
        set_message(a, UI_SUCCESS,
                    "%s  Size: %d statements (par %d) %s.  Speed: %.1f steps "
                    "average (par %d) %s.",
                    r.message, size, a->level->par_size,
                    size <= a->level->par_size ? "MET" : "missed",
                    r.avg_steps, a->level->par_speed,
                    r.avg_steps <= a->level->par_speed ? "MET" : "missed");
        a->passed[a->slot] = true;
    } else {
        set_message(a, UI_ERROR, "%s", r.message);
    }
}

static void tick(MailroomScreen *a, float dt)
{
    VM *vm = a->vm;
    a->scene.update(a->scene.state, dt * SPEEDS[a->speed_index]);

    if (vm->status == VM_YIELDED) {
        if (!a->event_sent) {
            a->scene.on_event(a->scene.state, a->run.event);
            a->event_sent = true;
        }
        if (a->scene.busy(a->scene.state)) return;
        a->event_sent = false;
        vm_resume(vm);
        if (a->mode == RUN_STEP_OP) {
            a->mode = RUN_IDLE;
            return;
        }
        if (a->mode == RUN_STEP_LINE && vm_current_line(vm) != a->step_line) {
            a->mode = RUN_IDLE;
            return;
        }
    }
    if (vm->status != VM_PAUSED) return;

    switch (a->mode) {
    case RUN_IDLE:
        break;
    case RUN_STEP_OP:
        vm_run(vm, 1);
        if (vm->status != VM_YIELDED) a->mode = RUN_IDLE;
        break;
    case RUN_STEP_LINE:
        for (int i = 0; i < 10000 && vm->status == VM_PAUSED; ++i) {
            vm_run(vm, 1);
            if (vm->status == VM_PAUSED && vm_current_line(vm) != a->step_line) {
                a->mode = RUN_IDLE;
                break;
            }
        }
        break;
    case RUN_GO: {
        a->instr_accum += dt * INSTR_RATE * SPEEDS[a->speed_index];
        int n = (int)a->instr_accum;
        if (n > 0) {
            a->instr_accum -= (float)n;
            vm_run(vm, n);
        }
        break;
    }
    }
}

static void check_finished(MailroomScreen *a)
{
    if (a->finish_reported || !done(a)) return;
    a->finish_reported = true;
    a->mode = RUN_IDLE;
    if (a->vm->status == VM_ERROR) {
        set_message(a, UI_ERROR, "Line %d: %s", a->vm->error_line, a->vm->error);
        return;
    }
    char why[160];
    if (level_check_finished(&a->run, why, sizeof why))
        set_message(a, UI_SUCCESS, "%s Finished in %ld steps. Press Submit to "
                                   "test it against %d random inboxes.",
                    why, a->vm->steps, SUBMIT_TESTS);
    else
        set_message(a, UI_ERROR, "%s", why);
}

static void watch_edits(MailroomScreen *a)
{
    if (a->ed->version == a->seen_version) return;
    a->seen_version = a->ed->version;
    if (a->compiled_ok && (a->vm->steps > 0 || a->mode != RUN_IDLE)) {
        reset_run(a);
        set_message(a, UI_MUTED, "Code changed, so the run was stopped. Press "
                                 "Run to compile and start again.");
    }
}

static void update_markers(MailroomScreen *a)
{
    Editor *ed = a->ed;
    const VM *vm = a->vm;
    ed->exec_line = ed->error_line = ed->error_col = ed->error_len = 0;
    if (!a->compiled_ok && ed->version == a->compiled_version) {
        ed->error_line = a->compile.line;
        ed->error_col = a->compile.col;
        ed->error_len = a->compile.len;
    } else if (fresh(a) && vm->status == VM_ERROR) {
        ed->error_line = vm->error_line;
    } else if (fresh(a) && (vm->status == VM_PAUSED || vm->status == VM_YIELDED) &&
               (vm->steps > 0 || a->mode != RUN_IDLE)) {
        ed->exec_line = vm_current_line(vm);
    }
}

static void load_code(MailroomScreen *a, const char *src)
{
    ed_set_text(a->ed, src, true);
    a->ed->focused = true;
}

static void reset_button(MailroomScreen *a)
{
    reset_run(a);
    set_message(a, UI_MUTED, "Reset.");
}

/* ---- public / 공개 함수 ------------------------------------------------- */

MailroomScreen *mailroom_create(const EditorStyle *style)
{
    MailroomScreen *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->prog = calloc(1, sizeof *a->prog);
    a->vm = calloc(1, sizeof *a->vm);
    a->ed = calloc(1, sizeof *a->ed);
    a->scene = scene_mailroom_create();
    if (!a->prog || !a->vm || !a->ed || !a->scene.state) {
        mailroom_destroy(a);
        return NULL;
    }
    for (int s = 0; s < LEVEL_SLOTS; ++s) {
        if (!LEVEL_LIST[s]) continue;
        a->code[s] = calloc(1, ED_MAX_TEXT);
        if (!a->code[s]) {
            mailroom_destroy(a);
            return NULL;
        }
        snprintf(a->code[s], ED_MAX_TEXT, "%s", LEVEL_LIST[s]->starter);
    }
    a->level = &LEVEL_MAILROOM;
    a->slot = 0;
    a->style = style;
    a->view = LoadRenderTexture((int)VIEW.width, (int)VIEW.height);

    /* Built-in names for syntax colours / 문법 색상용 내장 이름 */
    prog_init(a->prog);
    a->level->register_natives(a->prog);
    int n = a->prog->native_count;
    for (int i = 0; i < n; ++i) {
        snprintf(a->known[i], sizeof a->known[i], "%s", a->prog->natives[i].name);
        a->known_ptrs[i] = a->known[i];
    }
    ed_init(a->ed);
    ed_set_known(a->ed, a->known_ptrs, n);
    ed_set_text(a->ed, a->level->starter, false);
    ed_set_cursor(a->ed, ed_pos_at(a->ed, 2, 4), false);
    compile_now(a);
    a->seen_version = a->ed->version;
    set_message(a, UI_MUTED, "Write your program, then press Run (F5).");
    return a;
}

void mailroom_destroy(MailroomScreen *a)
{
    if (!a) return;
    if (a->view.id != 0) UnloadRenderTexture(a->view);
    if (a->scene.state) a->scene.destroy(a->scene.state);
    for (int s = 0; s < LEVEL_SLOTS; ++s) free(a->code[s]);
    free(a->ed);
    free(a->vm);
    free(a->prog);
    free(a);
}

bool mailroom_typing(const MailroomScreen *a) { return a->ed->focused; }

static const int HELP_FOR_SLOT[LEVEL_SLOTS] = {
    HELP_MAILROOM, HELP_NIGHT_SHIFT, HELP_MAILROOM, HELP_WEIGH_STATION,
    HELP_MAILROOM, HELP_HEAD_COUNT, HELP_BIGGEST_BOX, HELP_MAILROOM,
    HELP_COUNTDOWN, HELP_MAILROOM, HELP_MAILROOM
};

int mailroom_help_topic(const MailroomScreen *a) { return HELP_FOR_SLOT[a->slot]; }

bool mailroom_select_level(MailroomScreen *a, int slot)
{
    if (slot < 0 || slot >= LEVEL_SLOTS || !LEVEL_LIST[slot]) return false;
    if (slot == a->slot) return true;
    snprintf(a->code[a->slot], ED_MAX_TEXT, "%s", a->ed->text);   /* save */
    a->slot = slot;
    a->level = LEVEL_LIST[slot];
    a->examples_open = false;
    if (a->mode == RUN_GO) a->mode = RUN_IDLE;

    /* The level's functions get syntax colours. / 레벨 함수에 문법 색상. */
    prog_init(a->prog);
    a->level->register_natives(a->prog);
    int n = a->prog->native_count;
    for (int i = 0; i < n; ++i) {
        snprintf(a->known[i], sizeof a->known[i], "%s", a->prog->natives[i].name);
        a->known_ptrs[i] = a->known[i];
    }
    ed_set_known(a->ed, a->known_ptrs, n);
    ed_set_text(a->ed, a->code[slot], false);
    ed_set_cursor(a->ed, ed_pos_at(a->ed, 2, 4), false);
    compile_now(a);
    a->seen_version = a->ed->version;
    set_message(a, UI_MUTED, "Level %d: %s. Write your program, then press Run "
                             "(F5).", a->level->number, a->level->title);
    help_open_once((HelpTopic)mailroom_help_topic(a));
    return true;
}

void mailroom_leave(MailroomScreen *a)
{
    if (a->mode == RUN_GO) a->mode = RUN_IDLE;
    a->ed->focused = false;
    if (a->ed->ibeam) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        a->ed->ibeam = false;
    }
}

void mailroom_update(MailroomScreen *a, float dt)
{
    /* the Examples menu covers the editor while it is open
       Examples 메뉴가 열려 있으면 에디터를 덮는다 */
    if (!a->examples_open) editor_ui_update(a->ed, editor_rect(a), a->style);
    watch_edits(a);

    bool cmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
               IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (IsKeyPressed(KEY_F5) || (cmd && IsKeyPressed(KEY_ENTER))) toggle_run(a);
    if (IsKeyPressed(KEY_F6)) start(a, RUN_STEP_LINE);
    if (IsKeyPressed(KEY_F7)) start(a, RUN_STEP_OP);
    if (IsKeyPressed(KEY_F8)) reset_button(a);
    if (IsKeyPressed(KEY_F9)) submit(a);

    tick(a, dt);
    check_finished(a);
    update_markers(a);
}

void mailroom_render_view(MailroomScreen *a)
{
    BeginTextureMode(a->view);
    a->scene.draw(a->scene.state, (int)VIEW.width, (int)VIEW.height);
    EndTextureMode();
}

static Color status_color(VMStatus s)
{
    switch (s) {
    case VM_PAUSED:  return UI_INK;
    case VM_YIELDED: return UI_ACCENT;
    case VM_HALTED:  return UI_SUCCESS;
    case VM_ERROR:   return UI_ERROR;
    }
    return UI_INK;
}

/* Variables of every active function, newest first. Returns the next y.
 * 실행 중인 함수의 변수 (최근 것부터). 다음 y를 돌려준다. */
static int draw_variables(const MailroomScreen *a, int x, int y, int bottom)
{
    const VM *vm = a->vm;
    const Program *p = a->prog;
    const int lh = 28;
    if (vm->frame_count == 0) {
        DrawText("(no active function)", x, y, FS_SMALL, UI_MUTED);
        return y + lh;
    }
    for (int f = vm->frame_count - 1; f >= 0 && y < bottom; --f) {
        const Frame *fr = &vm->frames[f];
        DrawText(TextFormat("%s()", p->funcs[fr->func].name), x, y, FS_SMALL,
                 f == vm->frame_count - 1 ? UI_ACCENT : UI_MUTED);
        y += lh;
        int shown = 0;
        for (int i = 0; i < p->local_count && y < bottom; ++i) {
            const DebugLocal *d = &p->locals[i];
            if (d->func != fr->func) continue;
            int32_t v;
            if (vm_peek_i32(vm, fr->fp + d->offset, &v))
                DrawText(TextFormat("  int %s = %d", d->name, (int)v), x, y,
                         FS_SMALL, UI_INK);
            else
                DrawText(TextFormat("  int %s = ?", d->name), x, y, FS_SMALL,
                         UI_ERROR);
            y += lh;
            shown++;
        }
        if (!shown) {
            DrawText("  (no variables)", x, y, FS_SMALL, UI_MUTED);
            y += lh;
        }
    }
    return y;
}

/* A row of parcel chips: "INBOX  [5] [1] [79]".
 * 소포 칩 한 줄: "INBOX  [5] [1] [79]". */
static void draw_parcels(const char *label, const int32_t *items, int n, int x,
                         int y, Color c)
{
    DrawText(label, x, y + 10, FS_SMALL, c);
    int cx = x + 120;
    if (n == 0) DrawText("(empty)", cx, y + 10, FS_SMALL, UI_MUTED);
    for (int k = 0; k < n && cx < x + 480; k++) {
        const char *t = TextFormat("%d", (int)items[k]);
        int w = MeasureText(t, FS_SMALL) + 28;
        DrawRectangleRounded((Rectangle){ (float)cx, (float)y, (float)w, 40 },
                             0.25f, 6, (Color){ 150, 104, 60, 255 });
        DrawText(t, cx + 14, y + 10, FS_SMALL, UI_INK);
        cx += w + 8;
    }
}

/* Simple view: parcels, program state and variables. Machine view adds the
 * operand stack and bytecode for Step op.
 * Simple 보기: 소포, 프로그램 상태, 변수. Machine 보기는 Step op를 위한
 * 피연산자 스택과 바이트코드를 더한다. */
static void draw_inspector(MailroomScreen *a)
{
    const VM *vm = a->vm;
    const Program *p = a->prog;
    char buf[128];
    const int lh = 28;
    ui_panel(INSP);

    float tx = INSP.x + INSP.width - 24 - 2 * 140 - 10;
    if (ui_button((Rectangle){ tx, INSP.y + 14, 140, 44 }, "Simple", true,
                  !a->machine_view))
        a->machine_view = false;
    if (ui_button((Rectangle){ tx + 150, INSP.y + 14, 140, 44 }, "Machine", true,
                  a->machine_view))
        a->machine_view = true;

    const char *status = a->compiled_ok ? vm_status_name(vm->status)
                                        : "NOT COMPILED";
    Color scol = a->compiled_ok ? status_color(vm->status) : UI_ERROR;

    if (!a->machine_view) {
        int x = (int)INSP.x + 24, y = (int)INSP.y + 22;
        DrawText("PARCELS", x, y, FS_SMALL, UI_ACCENT);
        y += 46;
        draw_parcels("INBOX", a->run.inbox + a->run.inbox_pos,
                     a->run.inbox_len - a->run.inbox_pos, x, y, UI_ACCENT);
        draw_parcels("OUTBOX", a->run.outbox, a->run.outbox_len, x, y + 62,
                     UI_SUCCESS);
        draw_parcels("GOAL", a->run.expected, a->run.expected_len, x, y + 124,
                     UI_MUTED);

        x = (int)INSP.x + 560;
        y = (int)INSP.y + 22;
        DrawText("PROGRAM", x, y, FS_SMALL, UI_ACCENT);
        y += 46;
        DrawText(TextFormat("Status  %s", status), x, y, FS_SMALL, scol);
        y += lh;
        DrawText(TextFormat("Line    %d", vm_current_line(vm)), x, y, FS_SMALL,
                 UI_INK);
        y += lh + 16;
        DrawText("VARIABLES", x, y, FS_SMALL, UI_ACCENT);
        draw_variables(a, x, y + 34, (int)(INSP.y + INSP.height) - 20);
        return;
    }

    /* column 1: machine state / 1열: 기계 상태 */
    int x = (int)INSP.x + 24, y = (int)INSP.y + 22;
    DrawText("MACHINE", x, y, FS_SMALL, UI_ACCENT);
    y += 40;
    DrawText(TextFormat("Status  %s", status), x, y, FS_SMALL, scol);
    y += lh;
    DrawText(TextFormat("Steps   %ld", vm->steps), x, y, FS_SMALL, UI_INK);
    y += lh;
    DrawText(TextFormat("Line    %d", vm_current_line(vm)), x, y, FS_SMALL,
             UI_INK);
    y += lh;
    DrawText(TextFormat("Size    %d stmt",
                        a->compiled_ok ? a->compile.statements : 0),
             x, y, FS_SMALL, UI_INK);
    y += lh + 8;
    level_format_items(a->run.inbox + a->run.inbox_pos,
                       a->run.inbox_len - a->run.inbox_pos, buf, sizeof buf);
    DrawText(TextFormat("Inbox   %s", buf), x, y, FS_SMALL, UI_ACCENT);
    y += lh;
    level_format_items(a->run.outbox, a->run.outbox_len, buf, sizeof buf);
    DrawText(TextFormat("Outbox  %s", buf), x, y, FS_SMALL, UI_SUCCESS);

    /* column 2: variables and operand stack / 2열: 변수와 피연산자 스택 */
    x = (int)INSP.x + 380;
    y = (int)INSP.y + 22;
    DrawText("VARIABLES", x, y, FS_SMALL, UI_ACCENT);
    int bottom = (int)(INSP.y + INSP.height) - 100;
    draw_variables(a, x, y + 40, bottom);
    y = bottom + 10;
    DrawText("OPERAND STACK", x, y, FS_SMALL, UI_ACCENT);
    y += 32;
    if (vm->sp == 0) DrawText("(empty)", x, y, FS_SMALL, UI_MUTED);
    else
        DrawText(TextFormat("top> %d%s", (int)vm->ops[vm->sp - 1],
                            vm->sp > 1 ? "  ..." : ""),
                 x, y, FS_SMALL, UI_INK);

    /* column 3: bytecode (below the view buttons) / 3열: 바이트코드 */
    x = (int)INSP.x + 720;
    y = (int)INSP.y + 70;
    DrawText("BYTECODE", x, y, FS_SMALL, UI_ACCENT);
    y += 34;
    int start_pc = vm->pc - 2;
    if (start_pc < 0) start_pc = 0;
    bool live = vm->status == VM_PAUSED || vm->status == VM_YIELDED;
    for (int i = start_pc; i < p->code_len &&
                           y < (int)(INSP.y + INSP.height) - 30; ++i) {
        vm_disasm(p, i, buf, sizeof buf);
        bool cur = live && a->compiled_ok && i == vm->pc;
        if (cur)
            DrawRectangle(x - 6, y - 4, (int)(INSP.x + INSP.width) - x - 12,
                          lh, UI_SELECTED);
        DrawText(buf, x, y, FS_SMALL, cur ? UI_ACCENT : UI_INK);
        y += lh;
    }
}

/* The Examples menu: each program with one line on what it shows.
 * Examples 메뉴: 각 프로그램과 그것이 보여 주는 것 한 줄. */
/* Level 1 uses the hand-built demos (programs.c); later levels list their
 * own examples. / 레벨 1은 programs.c의 데모, 이후 레벨은 자체 예제. */
static int example_count(const MailroomScreen *a)
{
    return a->level->examples ? a->level->example_count : demo_count();
}

static void example_get(const MailroomScreen *a, int i, const char **name,
                        const char **about, const char **source)
{
    if (a->level->examples) {
        *name = a->level->examples[i].name;
        *about = a->level->examples[i].about;
        *source = a->level->examples[i].source;
    } else {
        *name = demo_get(i)->name;
        *about = demo_get(i)->about;
        *source = demo_get(i)->source;
    }
}

static void draw_examples_menu(MailroomScreen *a, Rectangle anchor)
{
    int n = example_count(a);
    Rectangle box = { anchor.x, anchor.y + anchor.height + 8, 700,
                      18 + n * 72.0f };
    DrawRectangleRounded((Rectangle){ box.x, box.y + 6, box.width, box.height },
                         0.06f, 8, (Color){ 10, 6, 20, 200 });
    DrawRectangleRounded(box, 0.06f, 8, UI_PANEL);
    DrawRectangleRoundedLinesEx(box, 0.06f, 8, 2.0f, UI_ACCENT);
    Vector2 m = ui_mouse();
    for (int i = 0; i < n; ++i) {
        const char *name, *about, *source;
        example_get(a, i, &name, &about, &source);
        Rectangle r = { box.x + 9, box.y + 9 + i * 72.0f, box.width - 18, 64 };
        bool hover = CheckCollisionPointRec(m, r);
        if (hover) DrawRectangleRounded(r, 0.12f, 6, UI_HOVER);
        DrawText(name, (int)r.x + 16, (int)r.y + 8, FS_SMALL, UI_INK);
        DrawText(about, (int)r.x + 16, (int)r.y + 36, FS_SMALL, UI_MUTED);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            load_code(a, source);
            a->examples_open = false;
            return;
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(m, box) && !CheckCollisionPointRec(m, anchor))
        a->examples_open = false;
}

/* One button per level; planned levels show only their number, dimmed.
 * 레벨마다 버튼 하나. 계획된 레벨은 번호만 흐리게 보인다. */
static void draw_level_row(MailroomScreen *a)
{
    DrawText("LEVELS", 45, 128, FS_SMALL, UI_MUTED);
    float x = 150;
    for (int s = 0; s < LEVEL_SLOTS; ++s) {
        const LevelDef *l = LEVEL_LIST[s];
        const char *label = l ? TextFormat("%d  %s", s + 1, l->title)
                              : TextFormat("%d", s + 1);
        float w = (float)MeasureText(label, FS_SMALL) + 36;
        Rectangle r = { x, 112, w, 52 };
        if (ui_button(r, label, l != NULL, s == a->slot))
            mailroom_select_level(a, s);
        if (a->passed[s])
            DrawCircle((int)(r.x + r.width - 12), (int)(r.y + 12), 5,
                       UI_SUCCESS);
        x += w + 10;
    }
}

void mailroom_draw(MailroomScreen *a)
{
    ui_background();
    ui_header("Mailroom", NULL);
    draw_level_row(a);

    /* left panel: goal, code buttons, editor, messages. The full story is
     * in Help. / 왼쪽 패널: 목표, 코드 버튼, 에디터, 메시지. 자세한 설명은
     * Help에 있다. */
    ui_panel(LPANEL);
    DrawText(TextFormat("Level %d: %s", a->level->number, a->level->title), MX,
             198, FS_BODY, UI_INK);
    DrawText(a->level->concept, MX, 240, FS_SMALL, UI_ACCENT);
    DrawText(a->level->goal, MX, 280, FS_SMALL, UI_INK);
    DrawText(a->level->tools, MX, 314, FS_SMALL, UI_MUTED);

    int y = examples_y(a);
    if (ui_button((Rectangle){ MX, (float)y, 220, 44 }, "Starter code", true,
                  false))
        load_code(a, a->level->starter);
    Rectangle ex = { MX + 234, (float)y, 220, 44 };
    if (ui_button(ex, "Examples", true, a->examples_open))
        a->examples_open = !a->examples_open;

    editor_ui_draw(a->ed, editor_rect(a), a->style);

    Rectangle msg = { MX, 952, MW, 96 };
    DrawRectangleRounded(msg, 0.08f, 6, UI_BG);
    ui_wrapped(a->message, MX + 14, 962, MW - 28, FS_SMALL, a->message_color);

    /* right: 3D scene and inspector / 오른쪽: 3D 장면과 인스펙터 */
    ui_draw_view(a->view.texture, (Rectangle){ 0, 0, VIEW.width, -VIEW.height },
                 VIEW);
    DrawRectangleLinesEx(VIEW, 2.0f, UI_BORDER);
    draw_inspector(a);

    /* bottom row / 아래 버튼 줄 */
    /* Game controls keep the editor focused (see screen_arrays.c).
     * 게임 조작 버튼은 에디터 포커스를 유지한다 (screen_arrays.c 참고). */
    bool control = false;
    float cw = (LPANEL.width - 3 * 14) / 4;
    if (ui_button((Rectangle){ LPANEL.x, ROW_Y, cw, ROW_H },
                  a->mode == RUN_GO ? "Pause" : "Run", true, a->mode == RUN_GO)) {
        toggle_run(a);
        control = true;
    }
    if (ui_button((Rectangle){ LPANEL.x + (cw + 14), ROW_Y, cw, ROW_H }, "Step",
                  true, a->mode == RUN_STEP_LINE)) {
        start(a, RUN_STEP_LINE);
        control = true;
    }
    if (ui_button((Rectangle){ LPANEL.x + 2 * (cw + 14), ROW_Y, cw, ROW_H },
                  "Step op", true, a->mode == RUN_STEP_OP)) {
        start(a, RUN_STEP_OP);
        control = true;
    }
    if (ui_button((Rectangle){ LPANEL.x + 3 * (cw + 14), ROW_Y, cw, ROW_H },
                  "Reset", true, false)) {
        reset_button(a);
        control = true;
    }

    DrawText("SPEED", (int)VIEW.x + 4, ROW_Y + 18, FS_SMALL, UI_MUTED);
    for (int i = 0; i < SPEED_COUNT; ++i)
        if (ui_button((Rectangle){ VIEW.x + 100 + i * 134, ROW_Y, 120, ROW_H },
                      SPEED_NAMES[i], true, i == a->speed_index)) {
            a->speed_index = i;
            control = true;
        }
    if (ui_button((Rectangle){ VIEW.x + VIEW.width - 340, ROW_Y, 340, ROW_H },
                  TextFormat("Submit  (%d random tests)", SUBMIT_TESTS), true,
                  false)) {
        submit(a);
        control = true;
    }
    if (control) a->ed->focused = true;

    /* the menu goes on top of everything else / 메뉴는 맨 위에 그린다 */
    if (a->examples_open) draw_examples_menu(a, ex);

    watch_edits(a);         /* example buttons may have changed the code
                               예제 버튼이 코드를 바꿨을 수 있음 */
    check_finished(a);
}

void mailroom_qa(MailroomScreen *a, int demo, int steps)
{
    /* demo 10+: Machine view; 20: Examples menu open
       demo 10 이상: Machine 보기; 20: Examples 메뉴 열림 */
    if (demo == 20) {
        a->examples_open = true;
        demo = -1;
    } else if (demo >= 10) {
        a->machine_view = true;
        demo -= 10;
    }
    /* demo 9: the level's reference answer / demo 9: 레벨의 정답 */
    if (demo == 9) {
        load_code(a, a->level->reference);
    } else if (demo >= 0 && demo < example_count(a)) {
        const char *name, *about, *source;
        example_get(a, demo, &name, &about, &source);
        load_code(a, source);
    }
    watch_edits(a);
    compile_now(a);
    a->run.headless = true;          /* no animation waits / 애니메이션 대기 없음 */
    vm_run(a->vm, steps);
    a->run.headless = false;
    a->scene.reset(a->scene.state, &a->run);
    check_finished(a);
    update_markers(a);
}
