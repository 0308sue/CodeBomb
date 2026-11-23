/*
 * screen_mailroom.c - level 1 "Mailroom": editor, 3D scene and VM inspector
 * screen_mailroom.c - 레벨 1 "우편실": 에디터, 3D 장면, VM 인스펙터
 *
 * The VM runs live here: take() and put() pause it while the robot moves.
 * (The Arrays screen instead records a trace first, for Sumin's Back.)
 * 여기서는 VM이 실시간으로 실행된다: take()와 put()은 로봇이 움직이는 동안
 * VM을 멈춘다. (배열 화면은 수민의 Back을 위해 먼저 기록을 만든다.)
 */
#include "screen_mailroom.h"

#include "compiler.h"
#include "editor.h"
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
static int examples_y(const MailroomScreen *a)
{
    int y = 280 + ui_wrapped_height(a->level->brief, MW, FS_SMALL);
    return y + ui_line_height(FS_SMALL) + 12;
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
    a->level = &LEVEL_MAILROOM;
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
    free(a->ed);
    free(a->vm);
    free(a->prog);
    free(a);
}

bool mailroom_typing(const MailroomScreen *a) { return a->ed->focused; }

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
    editor_ui_update(a->ed, editor_rect(a), a->style);
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
    DrawText(TextFormat("Speed %s", SPEED_NAMES[a->speed_index]), 16, 14,
             FS_SMALL, UI_MUTED);
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

static void draw_inspector(MailroomScreen *a)
{
    const VM *vm = a->vm;
    const Program *p = a->prog;
    char buf[128];
    const int lh = 28;
    ui_panel(INSP);

    /* column 1: machine state / 1열: 기계 상태 */
    int x = (int)INSP.x + 24, y = (int)INSP.y + 16;
    DrawText("MACHINE", x, y, FS_SMALL, UI_ACCENT);
    y += 34;
    const char *status = a->compiled_ok ? vm_status_name(vm->status)
                                        : "NOT COMPILED";
    DrawText(TextFormat("Status  %s", status), x, y, FS_SMALL,
             a->compiled_ok ? status_color(vm->status) : UI_ERROR);
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
    y += lh;
    level_format_items(a->run.expected, a->run.expected_len, buf, sizeof buf);
    DrawText(TextFormat("Goal    %s", buf), x, y, FS_SMALL, UI_MUTED);

    /* column 2: variables and operand stack / 2열: 변수와 피연산자 스택 */
    x = (int)INSP.x + 380;
    y = (int)INSP.y + 16;
    DrawText("VARIABLES", x, y, FS_SMALL, UI_ACCENT);
    y += 34;
    int bottom = (int)(INSP.y + INSP.height) - 100;
    if (vm->frame_count == 0) {
        DrawText("(no active function)", x, y, FS_SMALL, UI_MUTED);
        y += lh;
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
    y = bottom + 10;
    DrawText("OPERAND STACK", x, y, FS_SMALL, UI_ACCENT);
    y += 32;
    if (vm->sp == 0) DrawText("(empty)", x, y, FS_SMALL, UI_MUTED);
    else
        DrawText(TextFormat("top> %d%s", (int)vm->ops[vm->sp - 1],
                            vm->sp > 1 ? "  ..." : ""),
                 x, y, FS_SMALL, UI_INK);

    /* column 3: bytecode / 3열: 바이트코드 */
    x = (int)INSP.x + 720;
    y = (int)INSP.y + 16;
    DrawText("BYTECODE", x, y, FS_SMALL, UI_ACCENT);
    y += 34;
    int start_pc = vm->pc - 3;
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

void mailroom_draw(MailroomScreen *a)
{
    ui_background();
    ui_header("Mailroom", "LEVEL 1: STATEMENTS & FUNCTION CALLS");

    /* left panel: brief, examples, editor, messages
     * 왼쪽 패널: 설명, 예제, 에디터, 메시지 */
    ui_panel(LPANEL);
    DrawText(TextFormat("Level %d: %s", a->level->number, a->level->title), MX,
             198, FS_BODY, UI_INK);
    DrawText(a->level->concept, MX, 240, FS_SMALL, UI_ACCENT);
    int y = ui_wrapped(a->level->brief, MX, 280, MW, FS_SMALL, UI_MUTED);
    DrawText(a->level->tools, MX, y + 4, FS_SMALL, UI_ACCENT);

    y = examples_y(a);
    DrawText("Examples", MX, y + 12, FS_SMALL, UI_MUTED);
    int n = demo_count() + 1;
    float bx = MX + 116.0f;
    float bw = (MW - 116.0f - (n - 1) * 10.0f) / n;
    if (ui_button((Rectangle){ bx, (float)y, bw, 44 }, "Starter", true, false))
        load_code(a, a->level->starter);
    for (int i = 0; i < demo_count(); ++i)
        if (ui_button((Rectangle){ bx + (i + 1) * (bw + 10.0f), (float)y, bw, 44 },
                      demo_get(i)->name, true, false))
            load_code(a, demo_get(i)->source);

    editor_ui_draw(a->ed, editor_rect(a), a->style);

    Rectangle msg = { MX, 952, MW, 96 };
    DrawRectangleRounded(msg, 0.08f, 6, UI_BG);
    ui_wrapped(a->message, MX + 14, 962, MW - 28, FS_SMALL, a->message_color);

    /* right: 3D scene and inspector / 오른쪽: 3D 장면과 인스펙터 */
    DrawTextureRec(a->view.texture,
                   (Rectangle){ 0, 0, VIEW.width, -VIEW.height },
                   (Vector2){ VIEW.x, VIEW.y }, WHITE);
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

    DrawText("F5 / Ctrl+Enter: run    F6: step    F7: step op    F8: reset    "
             "F9: submit    Esc: leave editor    (Mac: Cmd+Enter, fn+F5)",
             45, 1160, FS_SMALL, UI_MUTED);

    watch_edits(a);         /* example buttons may have changed the code
                               예제 버튼이 코드를 바꿨을 수 있음 */
    check_finished(a);
}

void mailroom_qa(MailroomScreen *a, int demo, int steps)
{
    if (demo >= 0 && demo < demo_count()) load_code(a, demo_get(demo)->source);
    watch_edits(a);
    compile_now(a);
    a->run.headless = true;          /* no animation waits / 애니메이션 대기 없음 */
    vm_run(a->vm, steps);
    a->run.headless = false;
    a->scene.reset(a->scene.state, &a->run);
    check_finished(a);
    update_markers(a);
}
