/*
 * ui.h - shared look and widgets for every screen (Circuit Pals theme)
 * ui.h - 모든 화면이 함께 쓰는 모양과 위젯 (Circuit Pals 테마)
 *
 * The theme, buttons, word wrap and mascot come from Sumin's main_SOO.c,
 * redrawn for a 1920 x 1200 canvas.
 * 테마, 버튼, 줄바꿈, 마스코트는 수민의 main_SOO.c에서 가져와
 * 1920 x 1200 화면에 맞게 다시 그렸다.
 *
 * Font sizes are always multiples of 10. raylib's default font is a 10 px
 * pixel font, so 20, 30 and 40 scale it by exactly 2x, 3x and 4x and the
 * letters stay sharp.
 * 글자 크기는 항상 10의 배수다. raylib 기본 폰트는 10픽셀 픽셀 폰트이므로
 * 20, 30, 40은 정확히 2배, 3배, 4배로 확대되어 글자가 선명하게 유지된다.
 */
#ifndef CB_UI_H
#define CB_UI_H

#include "raylib.h"

#include <stdbool.h>

/* Logical canvas. Everything is drawn at this size, then scaled to the
 * window (1:1 on a 1920 x 1200 monitor in full screen).
 * 논리 화면 크기. 모든 것을 이 크기로 그린 뒤 창에 맞게 확대/축소한다
 * (1920 x 1200 모니터의 전체 화면에서는 1:1). */
#define UI_W 1920
#define UI_H 1200

/* Font sizes / 글자 크기 */
#define FS_SMALL 20      /* labels, code, body text / 라벨, 코드, 본문 */
#define FS_BODY  30      /* missions, headings      / 미션, 제목        */
#define FS_TITLE 40      /* screen titles, values   / 화면 제목, 값     */

/* Sumin's palette / 수민의 색상표 */
extern const Color UI_BG, UI_PANEL, UI_INK, UI_MUTED, UI_SAFE, UI_FAULT,
                   UI_ACCENT, UI_LETTERBOX, UI_HEADER, UI_ERROR, UI_SUCCESS,
                   UI_PINK, UI_BAR_BG, UI_BORDER, UI_SELECTED, UI_HOVER;

/* ---- virtual mouse / 가상 마우스 ----------------------------------------
 * The canvas is scaled and letterboxed, so window mouse coordinates are
 * converted back to canvas coordinates once per frame.
 * 화면이 확대되고 여백이 생기므로, 창의 마우스 좌표를 매 프레임 한 번
 * 논리 화면 좌표로 되돌린다. */
void    ui_set_transform(float scale, float offset_x, float offset_y);
Vector2 ui_mouse(void);
bool    ui_clicked(Rectangle r);

/* ---- drawing / 그리기 --------------------------------------------------- */
void ui_background(void);
void ui_panel(Rectangle r);
void ui_mascot(float x, float y);

/* Top bar "Circuit Pals / <section>"; subtitle (may be NULL) goes in the
 * row below. / 위쪽 막대 "Circuit Pals / <구역>"; 부제목(NULL 가능)은 아래 줄. */
void ui_header(const char *section, const char *subtitle);

/* Sumin's rounded button. Returns true when clicked.
 * 수민의 둥근 버튼. 클릭되면 true. */
bool ui_button(Rectangle r, const char *label, bool enabled, bool selected);

/* Word-wrapped text; returns the y just below the last line.
 * 단어 단위 줄바꿈 텍스트. 마지막 줄 바로 아래의 y를 돌려준다. */
int  ui_wrapped(const char *text, int x, int y, int width, int size, Color c);
int  ui_wrapped_height(const char *text, int width, int size);

/* Multi-line text (split on '\n'); returns the y below the last line.
 * 여러 줄 텍스트 ('\n'으로 나눔). 마지막 줄 아래의 y를 돌려준다. */
int  ui_lines(const char *text, int x, int y, int size, Color c);

/* Line height used by ui_wrapped / ui_lines. / 줄 높이. */
int  ui_line_height(int size);

/* Draw text right-aligned so it ends at x. / x에서 끝나도록 오른쪽 정렬. */
void ui_text_right(const char *text, int x, int y, int size, Color c);

#endif
