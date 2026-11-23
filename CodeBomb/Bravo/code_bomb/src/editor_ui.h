/*
 * editor_ui.h - raylib input and drawing for the code editor
 * editor_ui.h - 코드 에디터의 raylib 입력 처리와 그리기
 *
 * Text is drawn on a fixed character grid, so even raylib's default
 * (proportional) font lines up like a monospace font. Use a font size that
 * is a multiple of 10 (20 by default) so the pixel font stays sharp.
 * Mouse positions come from ui_mouse(), so the editor works on the scaled
 * canvas.
 * 글자를 고정 격자 위에 그리므로 raylib 기본(가변폭) 폰트도 고정폭처럼
 * 정렬된다. 픽셀 폰트가 선명하도록 글자 크기는 10의 배수(기본 20)를 쓴다.
 * 마우스 위치는 ui_mouse()에서 가져오므로 확대된 화면에서도 동작한다.
 *
 * Shortcuts (Ctrl on Windows, Cmd or Ctrl on macOS):
 * 단축키 (Windows는 Ctrl, macOS는 Cmd 또는 Ctrl):
 *   Ctrl+Z undo, Ctrl+Y / Ctrl+Shift+Z redo, Ctrl+A/C/X/V, Ctrl+/ comment,
 *   Tab / Shift+Tab indent, Ctrl(Win) or Alt(Mac)+arrows word jump,
 *   Home/End, Ctrl+Home/End, PageUp/PageDown, Esc to leave the editor.
 */
#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#include "editor.h"

#include "raylib.h"

typedef struct {
    Font  font;
    float size;            /* font size in pixels / 폰트 크기 (픽셀)   */
    float line_h;          /* line height / 줄 높이                    */
    float cell_w;          /* width of one character cell / 글자 칸 너비 */
    float glyph_off[128];  /* x offset that centres a glyph in its cell
                              글자를 칸 가운데에 놓기 위한 x 오프셋 */
} EditorStyle;

EditorStyle editor_style_make(Font font, float size);

/* Handle mouse and keyboard. Call once per frame before drawing.
 * 마우스와 키보드를 처리한다. 매 프레임 그리기 전에 한 번 호출. */
void editor_ui_update(Editor *ed, Rectangle bounds, const EditorStyle *st);
void editor_ui_draw(Editor *ed, Rectangle bounds, const EditorStyle *st);

#endif
