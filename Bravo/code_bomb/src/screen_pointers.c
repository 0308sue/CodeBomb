/*
 * screen_pointers.c - Sumin's DrawPointer() and PointerView, 1.5x
 * screen_pointers.c - 수민의 DrawPointer()와 PointerView, 1.5배
 */
#include "screen_pointers.h"

#include "pointer_SOO.h"
#include "ui.h"

#include "raylib.h"

#include <stdio.h>

static const Rectangle PL = { 30, 180, 1125, 884 };    /* left panel  / 왼쪽  */
static const Rectangle PR = { 1179, 180, 711, 884 };   /* right panel / 오른쪽 */
#define PRX 1212
#define PRW 645

void pointers_init(PointersScreen *v)
{
    *v = (PointersScreen){ .choice = -1 };
}

static void PointerNext(PointersScreen *v)
{
    if (v->choice >= 0 && v->step < SOO_SWAP_LAST) v->step++;
    if (v->step == SOO_SWAP_LAST) v->playing = false;
}

static void PointerAnswer(PointersScreen *v, int choice)
{
    if (v->choice < 0) {
        v->choice = choice;
        v->playing = true;
        v->elapsed = 0;
    }
}

void pointers_update(PointersScreen *v, float dt)
{
    if (IsKeyPressed(KEY_R)) pointers_init(v);
    if (IsKeyPressed(KEY_A)) PointerAnswer(v, 0);
    if (IsKeyPressed(KEY_B)) PointerAnswer(v, 1);
    if (IsKeyPressed(KEY_C)) PointerAnswer(v, 2);
    if (IsKeyPressed(KEY_SPACE) && v->choice >= 0 && v->step < SOO_SWAP_LAST)
        v->playing = !v->playing;
    if (IsKeyPressed(KEY_RIGHT)) {
        v->playing = false;
        PointerNext(v);
    }
    if (IsKeyPressed(KEY_LEFT) && v->step > 0) {
        v->playing = false;
        v->step--;
    }
    if (v->playing) {
        v->elapsed += dt;
        if (v->elapsed >= 1.35f) {
            v->elapsed = 0;
            PointerNext(v);
        }
    }
}

void pointers_leave(PointersScreen *v) { v->playing = false; }

void pointers_qa(PointersScreen *v, int choice, int step)
{
    pointers_init(v);
    if (choice >= 0) PointerAnswer(v, choice);
    v->step = step < 0 ? 0 : step > SOO_SWAP_LAST ? SOO_SWAP_LAST : step;
    v->playing = false;
}

/* Pointer targets are drawn apart from values to show address vs value.
 * 주소와 값의 차이를 보이도록 포인터 대상과 값을 따로 그린다. */
void pointers_draw(PointersScreen *v)
{
    static const char *const notes[] = {
        "Predict the result before running the sample.",
        "&a and &b give the addresses. p points to a, and q points to b.",
        "temp = *p saves the original value of a (3).",
        "*p = *q copies 7 into a. Both a and b now hold 7.",
        "*q = temp copies the saved 3 into b. The values are swapped."
    };
    ui_background();
    ui_header("Pointers", "PASS BY ADDRESS");
    ui_panel(PL);
    ui_panel(PR);

    DrawText("Swap two values", 63, 213, FS_BODY, UI_INK);
    DrawText("Pointers hold addresses", 63, 258, FS_SMALL, UI_ACCENT);

    SooSwapFrame f = SooSwapAt(v->step);
    if (v->step >= 1) {
        DrawText("p holds &a", 285 - MeasureText("p holds &a", FS_BODY) / 2, 385,
                 FS_BODY, UI_ACCENT);
        DrawText("q holds &b", 625 - MeasureText("q holds &b", FS_BODY) / 2, 385,
                 FS_BODY, UI_ACCENT);
        for (int x = 285; x <= 625; x += 340) {
            DrawLineEx((Vector2){ x, 430 }, (Vector2){ x, 486 }, 3.0f, UI_ACCENT);
            DrawTriangle((Vector2){ x - 11, 482 }, (Vector2){ x, 498 },
                         (Vector2){ x + 11, 482 }, UI_ACCENT);
        }
    }
    const char *names[] = { "a", "b", "temp" };
    char values[3][16];
    snprintf(values[0], sizeof values[0], "%d", f.a);
    snprintf(values[1], sizeof values[1], "%d", f.b);
    if (v->step >= 2) snprintf(values[2], sizeof values[2], "%d", f.temp);
    else snprintf(values[2], sizeof values[2], "--");
    for (int k = 0; k < 3; k++) {
        int x = 158 + k * 340;
        bool changed = (v->step == 2 && k == 2) || (v->step == 3 && k == 0) ||
                       (v->step == 4 && k == 1);
        DrawRectangleRounded((Rectangle){ x, 498, 255, 158 }, 0.1f, 6,
                             changed ? UI_SAFE : UI_BG);
        DrawText(names[k], x + 22, 519, FS_BODY, UI_INK);
        DrawText(values[k], x + (255 - MeasureText(values[k], FS_TITLE)) / 2,
                 566, FS_TITLE, UI_INK);
    }
    DrawText(TextFormat("STEP %d / %d", v->step, SOO_SWAP_LAST), 63, 760,
             FS_BODY, UI_ACCENT);
    ui_wrapped(notes[v->step], 63, 817, 1050, FS_BODY, UI_INK);

    DrawText("MISSION", PRX, 213, FS_SMALL, UI_ACCENT);
    int y = ui_wrapped("After swap_ints(&a, &b), what values do a and b hold?",
                       PRX, 250, PRW, FS_BODY, UI_INK) + 20;
    y = ui_lines("void swap_ints(int *p, int *q) {\n"
                 "    int temp = *p;\n"
                 "    *p = *q;\n"
                 "    *q = temp;\n"
                 "}\n"
                 "\n"
                 "int a = 3, b = 7;\n"
                 "swap_ints(&a, &b);",
                 PRX, y, FS_SMALL, UI_INK) + 30;
    static const char *const choices[3] = { "A  3, 7", "B  7, 3", "C  7, 7" };
    int w = (PRW - 2 * 14) / 3;
    for (int k = 0; k < 3; k++)
        if (ui_button((Rectangle){ PRX + k * (w + 14), y, w, 60 }, choices[k],
                      v->choice < 0, v->choice == k))
            PointerAnswer(v, k);
    y += 90;
    if (v->choice >= 0) {
        DrawText(SooSwapCorrect(v->choice) ? "Correct!" : "Not quite. Answer: B",
                 PRX, y, FS_BODY, SooSwapCorrect(v->choice) ? UI_ACCENT
                                                            : UI_ERROR);
        ui_wrapped("temp keeps the old value of a before a is overwritten.",
                   PRX, y + 50, PRW, FS_SMALL, UI_INK);
    } else {
        DrawText("Choose A, B or C.", PRX, y, FS_SMALL, UI_MUTED);
    }

    bool more = v->choice >= 0 && v->step < SOO_SWAP_LAST;
    if (ui_button((Rectangle){ 52, 1082, 232, 56 },
                  v->playing ? "Pause" : "Play", more, false))
        v->playing = !v->playing;
    if (ui_button((Rectangle){ 307, 1082, 232, 56 }, "Step", more, false)) {
        v->playing = false;
        PointerNext(v);
    }
    if (ui_button((Rectangle){ 562, 1082, 232, 56 }, "Back", v->step > 0,
                  false)) {
        v->playing = false;
        v->step--;
    }
    if (ui_button((Rectangle){ 817, 1082, 232, 56 }, "Reset", true, false))
        pointers_init(v);
    DrawText("A/B/C: answer    Space: play/pause    Arrows: step/back    "
             "R: reset", 45, 1160, FS_SMALL, UI_MUTED);
}
