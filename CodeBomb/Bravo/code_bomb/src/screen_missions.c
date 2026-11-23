/*
 * screen_missions.c - mission map built from Sumin's roadmap data
 * screen_missions.c - 수민의 로드맵 데이터로 만든 미션 지도
 *
 * Same layout idea as her DrawRoadmap() (two columns of chapter cards with
 * numbered level slots), drawn natively at 1920 x 1200 in the game's style.
 * 수민의 DrawRoadmap()과 같은 배치 (번호 칸이 있는 챕터 카드 두 줄)를
 * 게임 스타일로 1920 x 1200에 직접 그린다.
 */
#include "screen_missions.h"

#include "roadmap_SOO.h"
#include "ui.h"

#include <stdio.h>

/* Chapter colours from her DrawRoadmap() / 수민의 DrawRoadmap() 챕터 색 */
static const Color CHAPTER_COLORS[8] = {
    { 255, 187, 220, 255 }, { 173, 231, 211, 255 }, { 255, 209, 164, 255 },
    { 187, 196, 255, 255 }, { 167, 223, 250, 255 }, { 231, 193, 255, 255 },
    { 254, 222, 151, 255 }, { 255, 175, 182, 255 }
};

#define CHAPTERS ((int)(sizeof SOO_CHAPTERS / sizeof SOO_CHAPTERS[0]))

static bool slot(Rectangle r, const char *label, bool ready, Color tint)
{
    bool hover = ready && CheckCollisionPointRec(ui_mouse(), r);
    DrawRectangleRounded(r, 0.36f, 8,
                         ready ? (hover ? (Color){ 255, 255, 255, 255 } : tint)
                               : UI_BG);
    int tw = MeasureText(label, FS_BODY);
    DrawText(label, (int)(r.x + (r.width - tw) / 2),
             (int)(r.y + (r.height - FS_BODY) / 2), FS_BODY,
             ready ? UI_BG : UI_MUTED);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

MissionPick missions_draw(const bool bomb_done[MISSION_BOMBS])
{
    MissionPick pick = { PICK_NONE, 0 };
    ui_background();
    ui_header("Mission map", NULL);

    int ready = 0, total = 0;
    for (int n = 0; n < CHAPTERS; n++)
        for (int l = SOO_CHAPTERS[n].first; l <= SOO_CHAPTERS[n].last; l++) {
            total++;
            if (SOO_ROUTES[l]) ready++;
        }
    DrawText("Learn with Sumin's lessons, then defuse bombs with real C.", 45,
             126, FS_SMALL, UI_INK);
    ui_text_right(TextFormat("%d lessons  /  %d ready to play", total, ready),
                  1875, 126, FS_SMALL, UI_MUTED);

    /* chapter cards / 챕터 카드 */
    for (int n = 0; n < CHAPTERS; n++) {
        const SooChapter *ch = &SOO_CHAPTERS[n];
        float x = 30 + (n % 2) * 945.0f, y = 170 + (n / 2) * 196.0f;
        Rectangle card = { x, y, 915, 180 };
        Color tint = CHAPTER_COLORS[n % 8];
        DrawRectangleRounded((Rectangle){ x, y + 6, 915, 180 }, 0.16f, 10,
                             (Color){ 22, 17, 39, 255 });
        DrawRectangleRounded(card, 0.16f, 10, UI_PANEL);
        DrawRectangleRoundedLinesEx(card, 0.16f, 10, 2.0f, Fade(tint, 0.42f));
        DrawCircle((int)x + 46, (int)y + 48, 26, tint);
        const char *num = TextFormat("%d", n + 1);
        DrawText(num, (int)x + 46 - MeasureText(num, FS_BODY) / 2, (int)y + 34,
                 FS_BODY, UI_BG);
        DrawText(ch->title, (int)x + 90, (int)y + 26, FS_BODY, UI_INK);
        DrawText(ch->difficulty, (int)x + 90, (int)y + 64, FS_SMALL, tint);
        for (int l = ch->first; l <= ch->last; l++) {
            Rectangle r = { x + 90 + (l - ch->first) * 128.0f, y + 106, 114, 56 };
            char label[8];
            snprintf(label, sizeof label, "%02d", l);
            if (slot(r, label, SOO_ROUTES[l] != 0, tint)) {
                pick.kind = PICK_LESSON;
                pick.level = l;
            }
        }
    }

    /* bomb defusal row / 폭탄 해체 줄 */
    Rectangle bombs = { 30, 960, 1860, 160 };
    ui_panel(bombs);
    DrawText("Bomb defusal", 57, 978, FS_BODY, UI_INK);
    DrawText("Write real C. Every read and write plays out on the bomb.", 57,
             1018, FS_SMALL, UI_MUTED);
    float bx = 57, by = 1052;
    if (ui_button((Rectangle){ bx, by, 200, 52 }, "Tutorial", true, false))
        pick.kind = PICK_MAILROOM;
    for (int b = 0; b < MISSION_BOMBS; b++) {
        Rectangle r = { bx + 214.0f * (b + 1), by, 200, 52 };
        const char *label = b == MISSION_BOMBS - 1 ? "Cube bomb"
                                                   : TextFormat("Bomb %d", b + 1);
        if (ui_button(r, label, true, false)) {
            pick.kind = PICK_BOMB;
            pick.level = b;
        }
        if (bomb_done[b])
            DrawCircle((int)(r.x + r.width - 14), (int)(r.y + 14), 5, UI_SUCCESS);
    }
    ui_text_right("Bright numbers open a lesson", 1863, 978, FS_SMALL, UI_MUTED);
    return pick;
}
