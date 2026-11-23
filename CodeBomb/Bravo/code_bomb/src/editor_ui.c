/*
 * editor_ui.c - raylib input and drawing for the code editor
 * editor_ui.c - 코드 에디터의 raylib 입력 처리와 그리기
 */
#include "editor_ui.h"

#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define GUTTER_W   56.0f
#define TEXT_PAD   10.0f
#define TOP_PAD    8.0f

/* Colours matched to Sumin's Circuit Pals palette.
 * 수민의 Circuit Pals 색상표에 맞춘 색. */
static const Color COL_EDIT_BG   = { 24, 19, 43, 255 };
static const Color COL_GUTTER_BG = { 34, 27, 58, 255 };
static const Color COL_LINE_NUM  = { 120, 108, 152, 255 };
static const Color COL_CUR_LINE  = { 40, 32, 68, 255 };
static const Color COL_EXEC_LINE = { 46, 78, 102, 255 };
static const Color COL_ERR_LINE  = { 92, 36, 62, 255 };
static const Color COL_SELECTION = { 85, 65, 127, 255 };
static const Color COL_CURSOR    = { 177, 228, 255, 255 };
static const Color COL_BORDER    = { 91, 74, 123, 255 };
static const Color COL_ERR       = { 255, 147, 157, 255 };

static const Color CLASS_COLORS[CLS_COUNT] = {
    [CLS_TEXT]    = { 255, 246, 251, 255 },
    [CLS_KEYWORD] = { 249, 184, 221, 255 },
    [CLS_TYPE]    = { 177, 228, 255, 255 },
    [CLS_NUMBER]  = { 255, 205, 150, 255 },
    [CLS_COMMENT] = { 140, 128, 175, 255 },
    [CLS_STRING]  = { 200, 235, 160, 255 },
    [CLS_BUILTIN] = { 165, 244, 207, 255 },
    [CLS_FUNC]    = { 255, 228, 150, 255 },
    [CLS_PUNCT]   = { 199, 188, 225, 255 },
    [CLS_ERROR]   = { 255, 147, 157, 255 },
    [CLS_PREPROC] = { 249, 184, 221, 255 },
};

EditorStyle editor_style_make(Font font, float size)
{
    EditorStyle st;
    memset(&st, 0, sizeof st);
    st.font = font;
    st.size = size;
    st.line_h = roundf(size * 1.3f);

    /* Cell width = the typical letter width plus the font's own letter
     * spacing (size / 10, as raylib's DrawText uses) plus one pixel. With
     * the default pixel font at 20 px that is 10 + 2 + 1 = 13 px: code reads
     * like normal text, and wide capitals (M, W, T in ROWS, COLS) don't
     * collide with their neighbours.
     * 칸 너비 = 보통 글자 너비 + 폰트 자체의 글자 간격 (raylib DrawText처럼
     * size / 10) + 1픽셀. 기본 픽셀 폰트 20픽셀에서는 10 + 2 + 1 = 13픽셀이라
     * 코드가 보통 글처럼 읽히고, 넓은 대문자(ROWS, COLS의 M, W, T)가 옆
     * 글자와 겹치지 않는다. */
    float widths[128] = { 0 };
    int hist[64] = { 0 };
    for (int c = 32; c < 127; ++c) {
        char s[2] = { (char)c, '\0' };
        widths[c] = MeasureTextEx(font, s, size, 0.0f).x;
        bool alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z');
        int w = (int)ceilf(widths[c]);
        if (alnum && w >= 0 && w < 64) hist[w]++;
    }
    int median = 0, seen = 0;
    for (int w = 0; w < 64; ++w) {
        seen += hist[w];
        if (seen * 2 >= 62) {        /* 62 letters and digits / 영문자와 숫자 62개 */
            median = w;
            break;
        }
    }
    st.cell_w = (float)median + floorf(size / 10.0f) + 1.0f;
    for (int c = 32; c < 127; ++c)
        st.glyph_off[c] = floorf((st.cell_w - widths[c]) * 0.5f);
    return st;
}

/* ---- geometry / 위치 계산 ----------------------------------------------- */

static int visible_rows(Rectangle r, const EditorStyle *st)
{
    int rows = (int)((r.height - TOP_PAD * 2) / st->line_h);
    return rows < 1 ? 1 : rows;
}

static int visible_cols(Rectangle r, const EditorStyle *st)
{
    int cols = (int)((r.width - GUTTER_W - TEXT_PAD * 2) / st->cell_w);
    return cols < 1 ? 1 : cols;
}

static float text_x(Rectangle r) { return r.x + GUTTER_W + TEXT_PAD; }

static int pos_from_point(const Editor *ed, Rectangle r, const EditorStyle *st,
                          Vector2 m)
{
    int line = ed->scroll_line + (int)floorf((m.y - r.y - TOP_PAD) / st->line_h);
    if (line < 0) line = 0;
    if (line >= ed->line_count) line = ed->line_count - 1;
    int col = ed->scroll_col +
              (int)floorf((m.x - text_x(r)) / st->cell_w + 0.5f);
    return ed_pos_at(ed, line, col);
}

static void clamp_scroll(Editor *ed)
{
    if (ed->scroll_line > ed->line_count - 1) ed->scroll_line = ed->line_count - 1;
    if (ed->scroll_line < 0) ed->scroll_line = 0;
    if (ed->scroll_col < 0) ed->scroll_col = 0;
}

static void scroll_to_cursor(Editor *ed, Rectangle r, const EditorStyle *st)
{
    int rows = visible_rows(r, st), cols = visible_cols(r, st);
    int line = ed_line_of(ed, ed->cursor);
    int col = ed_col_of(ed, ed->cursor);
    if (line < ed->scroll_line) ed->scroll_line = line;
    if (line >= ed->scroll_line + rows) ed->scroll_line = line - rows + 1;
    if (col < ed->scroll_col) ed->scroll_col = col;
    if (col >= ed->scroll_col + cols - 1) ed->scroll_col = col - cols + 2;
    clamp_scroll(ed);
}

/* ---- input / 입력 ------------------------------------------------------- */

static bool hit(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

void editor_ui_update(Editor *ed, Rectangle r, const EditorStyle *st)
{
    Vector2 m = ui_mouse();          /* canvas coordinates / 논리 화면 좌표 */
    bool inside = CheckCollisionPointRec(m, r);

    /* Always drain typed characters so they don't pile up while unfocused.
     * 포커스가 없을 때 입력 글자가 쌓이지 않도록 항상 꺼내서 비운다. */
    int typed[32];
    int ntyped = 0;
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed())
        if (ntyped < 32) typed[ntyped++] = ch;

    if (inside != ed->ibeam) {
        SetMouseCursor(inside ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT);
        ed->ibeam = inside;
    }

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    bool super = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool cmd = ctrl || super;        /* Ctrl (Windows) or Cmd (macOS)
                                        Ctrl (Windows) 또는 Cmd (macOS) */
    bool word = ctrl || alt;         /* word jump: Ctrl (Win) / Option (Mac)
                                        단어 이동: Ctrl (Win) / Option (Mac) */

    /* Mouse wheel (trackpads send fractional steps).
     * 마우스 휠 (트랙패드는 소수 단위로 보낸다). */
    if (inside) {
        ed->wheel += GetMouseWheelMove() * 3.0f;
        int steps = (int)ed->wheel;
        if (steps != 0) {
            ed->scroll_line -= steps;
            ed->wheel -= (float)steps;
            clamp_scroll(ed);
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (inside) {
            ed->focused = true;
            int pos = pos_from_point(ed, r, st, m);
            double now = GetTime();
            if (now - ed->last_click_time < 0.35 && pos == ed->last_click_pos) {
                ed_select_word(ed, pos);          /* double-click / 더블클릭 */
                ed->dragging = false;
            } else {
                ed_set_cursor(ed, pos, shift);
                ed->dragging = true;
            }
            ed->last_click_time = now;
            ed->last_click_pos = pos;
        } else {
            ed->focused = false;
            ed->dragging = false;
        }
    }
    if (ed->dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (m.y < r.y) ed->scroll_line--;
            if (m.y > r.y + r.height) ed->scroll_line++;
            clamp_scroll(ed);
            ed_set_cursor(ed, pos_from_point(ed, r, st, m), true);
        } else {
            ed->dragging = false;
        }
    }

    if (ed->focused) {
        ed->blink += GetFrameTime();

        /* Text input. Alt+key is allowed because AltGr = Ctrl+Alt on many
         * European keyboards (needed for { } [ ] etc.).
         * 텍스트 입력. 많은 유럽 키보드에서 AltGr = Ctrl+Alt 이므로
         * ({ } [ ] 입력에 필요) Alt가 눌린 경우는 허용한다. */
        if (!cmd || alt)
            for (int i = 0; i < ntyped; ++i) ed_type_char(ed, typed[i]);

        int page = visible_rows(r, st) - 1;

        if ((hit(KEY_ENTER) || hit(KEY_KP_ENTER)) && !cmd) ed_newline(ed);
        if (hit(KEY_BACKSPACE)) ed_backspace(ed, word);
        if (hit(KEY_DELETE)) ed_delete(ed, word);
        if (hit(KEY_TAB)) ed_tab(ed, shift);

        if (hit(KEY_LEFT)) {
            if (super) ed_home(ed, shift);
            else ed_move_h(ed, -1, word, shift);
        }
        if (hit(KEY_RIGHT)) {
            if (super) ed_end(ed, shift);
            else ed_move_h(ed, 1, word, shift);
        }
        if (hit(KEY_UP)) {
            if (super) ed_doc_start(ed, shift);
            else ed_move_v(ed, -1, shift);
        }
        if (hit(KEY_DOWN)) {
            if (super) ed_doc_end(ed, shift);
            else ed_move_v(ed, 1, shift);
        }
        if (hit(KEY_HOME)) {
            if (ctrl) ed_doc_start(ed, shift);
            else ed_home(ed, shift);
        }
        if (hit(KEY_END)) {
            if (ctrl) ed_doc_end(ed, shift);
            else ed_end(ed, shift);
        }
        if (hit(KEY_PAGE_UP)) ed_move_v(ed, -page, shift);
        if (hit(KEY_PAGE_DOWN)) ed_move_v(ed, page, shift);

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (ed_has_selection(ed)) ed->anchor = ed->cursor;
            else ed->focused = false;
        }

        if (cmd) {
            if (IsKeyPressed(KEY_A)) ed_select_all(ed);
            if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_X)) {
                static char clip[ED_MAX_TEXT];
                if (ed_copy(ed, clip, sizeof clip) > 0) {
                    SetClipboardText(clip);
                    if (IsKeyPressed(KEY_X)) ed_cut(ed);
                }
            }
            if (IsKeyPressed(KEY_V)) {
                const char *clip = GetClipboardText();
                if (clip) ed_insert_text(ed, clip, (int)strlen(clip));
            }
            if (hit(KEY_Z)) {
                if (shift) ed_redo(ed);
                else ed_undo(ed);
            }
            if (hit(KEY_Y)) ed_redo(ed);
            if (IsKeyPressed(KEY_SLASH)) ed_toggle_comment(ed);
        }
    }

    if (ed->want_scroll) {
        scroll_to_cursor(ed, r, st);
        ed->want_scroll = false;
    }
}

/* ---- drawing / 그리기 --------------------------------------------------- */

static void draw_squiggle(float x0, float x1, float y, Color c)
{
    bool up = true;
    for (float x = x0; x < x1; x += 3.0f) {
        float nx = x + 3.0f > x1 ? x1 : x + 3.0f;
        DrawLineV((Vector2){ x, up ? y : y + 2 }, (Vector2){ nx, up ? y + 2 : y },
                  c);
        up = !up;
    }
}

void editor_ui_draw(Editor *ed, Rectangle r, const EditorStyle *st)
{
    ed_refresh_classes(ed);

    DrawRectangleRec(r, COL_EDIT_BG);
    DrawRectangle((int)r.x, (int)r.y, (int)GUTTER_W, (int)r.height,
                  COL_GUTTER_BG);

    int rows = visible_rows(r, st) + 1;
    int cols = visible_cols(r, st) + 1;
    int cur_line = ed_line_of(ed, ed->cursor);
    int sa, sb;
    ed_selection(ed, &sa, &sb);
    float tx = text_x(r);
    float glyph_y = (st->line_h - st->size) * 0.5f;

    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);

    for (int i = 0; i < rows; ++i) {
        int line = ed->scroll_line + i;
        if (line >= ed->line_count) break;
        float y = r.y + TOP_PAD + i * st->line_h;
        int ln = line + 1;
        int ls = ed->line_start[line];
        int llen = ed_line_len(ed, line);

        /* line background / 줄 배경 */
        Color bg = { 0, 0, 0, 0 };
        if (ln == ed->error_line) bg = COL_ERR_LINE;
        else if (ln == ed->exec_line) bg = COL_EXEC_LINE;
        else if (ed->focused && line == cur_line) bg = COL_CUR_LINE;
        if (bg.a)
            DrawRectangle((int)(r.x + GUTTER_W), (int)y,
                          (int)(r.width - GUTTER_W), (int)st->line_h, bg);

        /* selection (includes the newline, shown as one extra cell)
         * 선택 영역 (줄바꿈도 포함하여 한 칸 더 표시) */
        if (sa != sb) {
            int s0 = sa > ls ? sa : ls;
            int s1 = sb < ls + llen + 1 ? sb : ls + llen + 1;
            if (s1 > s0) {
                float x0 = tx + (s0 - ls - ed->scroll_col) * st->cell_w;
                float w = (s1 - s0) * st->cell_w;
                if (x0 < tx) {
                    w -= tx - x0;
                    x0 = tx;
                }
                if (w > 0)
                    DrawRectangle((int)x0, (int)y, (int)w, (int)st->line_h,
                                  COL_SELECTION);
            }
        }

        /* characters / 글자 */
        for (int col = ed->scroll_col; col < llen && col < ed->scroll_col + cols;
             ++col) {
            char ch = ed->text[ls + col];
            if (ch == ' ') continue;
            int ci = (unsigned char)ch < 128 ? ch : '?';
            Color c = CLASS_COLORS[ed->cls[ls + col] < CLS_COUNT
                                       ? ed->cls[ls + col] : CLS_TEXT];
            float x = tx + (col - ed->scroll_col) * st->cell_w;
            DrawTextCodepoint(st->font, ci,
                              (Vector2){ x + st->glyph_off[ci], y + glyph_y },
                              st->size, c);
        }

        /* compile-error squiggle / 컴파일 오류 물결 밑줄 */
        if (ln == ed->error_line && ed->error_col > 0) {
            int c0 = ed->error_col - 1 - ed->scroll_col;
            int c1 = c0 + (ed->error_len > 0 ? ed->error_len : 1);
            float x0 = tx + (float)(c0 < 0 ? 0 : c0) * st->cell_w;
            float x1 = tx + c1 * st->cell_w;
            if (x1 > x0) draw_squiggle(x0, x1, y + st->line_h - 3, COL_ERR);
        }

        /* gutter: line number, and a marker for the running line
         * 거터: 줄 번호, 그리고 실행 중인 줄 표시 */
        const char *num = TextFormat("%d", ln);
        float nsize = st->size;          /* same multiple of 10 / 같은 10의 배수 */
        Vector2 nw = MeasureTextEx(st->font, num, nsize, 0.0f);
        Color ncol = ln == ed->error_line ? COL_ERR
                   : ln == ed->exec_line  ? COL_CURSOR
                                          : COL_LINE_NUM;
        DrawTextEx(st->font, num,
                   (Vector2){ r.x + GUTTER_W - 8 - nw.x,
                              y + (st->line_h - nsize) * 0.5f },
                   nsize, 0.0f, ncol);
        if (ln == ed->exec_line)
            DrawTriangle((Vector2){ r.x + 6, y + 6 },
                         (Vector2){ r.x + 6, y + st->line_h - 6 },
                         (Vector2){ r.x + 16, y + st->line_h * 0.5f },
                         COL_CURSOR);
    }

    /* blinking cursor / 깜박이는 커서 */
    if (ed->focused && fmodf(ed->blink, 1.0f) < 0.6f) {
        int line = cur_line - ed->scroll_line;
        int col = ed_col_of(ed, ed->cursor) - ed->scroll_col;
        if (line >= 0 && line < rows && col >= 0)
            DrawRectangle((int)(tx + col * st->cell_w) - 1,
                          (int)(r.y + TOP_PAD + line * st->line_h), 3,
                          (int)st->line_h, COL_CURSOR);
    }
    EndScissorMode();

    /* scrollbar / 스크롤바 */
    int shown = visible_rows(r, st);
    if (ed->line_count > shown) {
        float track = r.height - 4;
        float thumb = track * shown / ed->line_count;
        if (thumb < 16) thumb = 16;
        float t = (float)ed->scroll_line / (ed->line_count - shown);
        if (t > 1) t = 1;
        DrawRectangle((int)(r.x + r.width - 8), (int)(r.y + 2 + t * (track - thumb)),
                      6, (int)thumb, COL_BORDER);
    }

    DrawRectangleLinesEx(r, 2.0f, ed->focused ? COL_CURSOR : COL_BORDER);
}
