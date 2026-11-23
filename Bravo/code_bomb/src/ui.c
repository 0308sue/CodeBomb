/*
 * ui.c - shared look and widgets (ported from Sumin's main_SOO.c)
 * ui.c - 공용 모양과 위젯 (수민의 main_SOO.c에서 옮김)
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>

/* Colours from main_SOO.c / main_SOO.c의 색상 */
const Color UI_BG        = { 29, 23, 51, 255 };
const Color UI_PANEL     = { 48, 39, 75, 255 };
const Color UI_INK       = { 255, 246, 251, 255 };
const Color UI_MUTED     = { 199, 188, 225, 255 };
const Color UI_SAFE      = { 39, 116, 96, 255 };
const Color UI_FAULT     = { 152, 62, 97, 255 };
const Color UI_ACCENT    = { 177, 228, 255, 255 };
const Color UI_LETTERBOX = { 19, 15, 34, 255 };
const Color UI_HEADER    = { 38, 30, 65, 255 };
const Color UI_ERROR     = { 255, 147, 157, 255 };
const Color UI_SUCCESS   = { 93, 227, 184, 255 };
const Color UI_PINK      = { 249, 184, 221, 255 };
const Color UI_BAR_BG    = { 45, 58, 79, 255 };
const Color UI_BORDER    = { 91, 74, 123, 255 };
const Color UI_SELECTED  = { 85, 65, 127, 255 };
const Color UI_HOVER     = { 103, 77, 143, 255 };

/* ---- virtual mouse / 가상 마우스 ---------------------------------------- */

static float g_scale = 1.0f, g_ox = 0.0f, g_oy = 0.0f;

void ui_set_transform(float scale, float offset_x, float offset_y)
{
    g_scale = scale > 0.0f ? scale : 1.0f;
    g_ox = offset_x;
    g_oy = offset_y;
}

Vector2 ui_mouse(void)
{
    /* Same transform as the drawing, reversed (Sumin's approach).
     * 그리기와 같은 변환을 거꾸로 적용한다 (수민의 방식). */
    Vector2 raw = GetMousePosition();
    return (Vector2){ (raw.x - g_ox) / g_scale, (raw.y - g_oy) / g_scale };
}

bool ui_clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(ui_mouse(), r);
}

/* ---- text / 텍스트 ------------------------------------------------------ */

int ui_line_height(int size) { return size * 14 / 10; }

void ui_text_right(const char *text, int x, int y, int size, Color c)
{
    DrawText(text, x - MeasureText(text, size), y, size, c);
}

int ui_lines(const char *text, int x, int y, int size, Color c)
{
    char line[256];
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);
        if (len > (int)sizeof line - 1) len = (int)sizeof line - 1;
        memcpy(line, p, (size_t)len);
        line[len] = '\0';
        DrawText(line, x, y, size, c);
        y += ui_line_height(size);
        if (!end) break;
        p = end + 1;
    }
    return y;
}

/* Sumin's word wrap, with an optional "measure only" mode.
 * 수민의 단어 단위 줄바꿈. 높이만 재는 모드도 있다. */
static int wrap(const char *text, int x, int y, int width, int size, Color c,
                bool draw)
{
    char word[256], line[512] = "";
    const char *p = text;
    int lh = ui_line_height(size);
    while (*p) {
        int len = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && len < 254) word[len++] = *p++;
        word[len] = 0;
        if (!len) break;
        char candidate[768];
        snprintf(candidate, sizeof candidate, "%s%s%s", line, line[0] ? " " : "",
                 word);
        if (MeasureText(candidate, size) > width && line[0]) {
            if (draw) DrawText(line, x, y, size, c);
            y += lh;
            line[0] = 0;
        }
        size_t used = strlen(line);
        snprintf(line + used, sizeof line - used, "%s%s", used ? " " : "", word);
    }
    if (line[0]) {
        if (draw) DrawText(line, x, y, size, c);
        y += lh;
    }
    return y;
}

int ui_wrapped(const char *text, int x, int y, int width, int size, Color c)
{
    return wrap(text, x, y, width, size, c, true);
}

int ui_wrapped_height(const char *text, int width, int size)
{
    return wrap(text, 0, 0, width, size, UI_INK, false);
}

/* ---- widgets / 위젯 ----------------------------------------------------- */

bool ui_button(Rectangle r, const char *label, bool enabled, bool selected)
{
    bool hover = enabled && CheckCollisionPointRec(ui_mouse(), r);
    Color fill = selected ? UI_SELECTED : UI_PANEL;
    if (hover) fill = UI_HOVER;
    DrawRectangleRounded(r, 0.13f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.13f, 6, 2.0f,
                                selected ? UI_ACCENT : UI_BORDER);
    /* Text stays at 20 px (a multiple of 10) for sharp letters.
     * 선명한 글자를 위해 20픽셀(10의 배수)을 유지한다. */
    int size = FS_SMALL;
    int tw = MeasureText(label, size);
    DrawText(label, (int)(r.x + (r.width - tw) / 2),
             (int)(r.y + (r.height - size) / 2), size,
             enabled ? UI_INK : UI_MUTED);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void ui_panel(Rectangle r)
{
    DrawRectangleRounded(r, 0.025f, 6, UI_PANEL);
}

/* Sumin's little robot, 1.5x. / 수민의 작은 로봇, 1.5배. */
void ui_mascot(float x, float y)
{
    DrawCircleV((Vector2){ x, y }, 25.0f, UI_PINK);
    DrawLineEx((Vector2){ x, y - 27 }, (Vector2){ x + 8, y - 39 }, 2.0f,
               UI_ACCENT);
    DrawCircleV((Vector2){ x + 9, y - 39 }, 4.5f, UI_ACCENT);
    DrawCircleV((Vector2){ x - 9, y - 2 }, 3.0f, UI_BG);
    DrawCircleV((Vector2){ x + 10, y - 2 }, 3.0f, UI_BG);
    DrawLineEx((Vector2){ x - 5, y + 9 }, (Vector2){ x, y + 12 }, 2.0f, UI_BG);
    DrawLineEx((Vector2){ x, y + 12 }, (Vector2){ x + 6, y + 8 }, 2.0f, UI_BG);
}

/* Dotted workshop background (Sumin's Workshop()).
 * 점무늬 작업장 배경 (수민의 Workshop()). */
void ui_background(void)
{
    ClearBackground(UI_BG);
    Color dot = { 65, 52, 92, 255 };
    for (int x = 24; x < UI_W; x += 36)
        for (int y = 24; y < UI_H; y += 36)
            DrawRectangle(x - 1, y - 1, 2, 2, dot);
}

/* Compact top bar: mascot and title only. A subtitle, if any, goes in the
 * second row so the bar stays uncluttered.
 * 간결한 위쪽 막대: 마스코트와 제목만. 부제목이 있으면 두 번째 줄에 둔다. */
void ui_header(const char *section, const char *subtitle)
{
    DrawRectangleRounded((Rectangle){ 30, 16, 1860, 80 }, 0.3f, 10, UI_HEADER);
    ui_mascot(78, 62);
    DrawText(TextFormat("Circuit Pals / %s", section), 124, 36, FS_TITLE,
             UI_INK);
    if (subtitle) DrawText(subtitle, 45, 128, FS_SMALL, UI_MUTED);
}
