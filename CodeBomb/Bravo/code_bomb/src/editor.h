/*
 * editor.h - text editing core for the in-game code editor (no raylib)
 * editor.h - 게임 내 코드 에디터의 텍스트 편집 핵심 (raylib 미사용)
 *
 * The text is one ASCII buffer. The cursor and the selection anchor are byte
 * offsets; the selection is the range between them. editor_ui.c turns
 * keyboard/mouse input into calls to these functions and draws the result.
 * 텍스트는 ASCII 버퍼 하나다. 커서와 선택 기준점(anchor)은 바이트 위치이며
 * 둘 사이가 선택 영역이다. editor_ui.c가 키보드/마우스 입력을 이 함수들의
 * 호출로 바꾸고 결과를 그린다.
 */
#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <stdint.h>

#define ED_MAX_TEXT   8192
#define ED_UNDO_MAX   40
#define ED_KNOWN_MAX  16
#define ED_TAB        4

/* Syntax colour classes, one per character.
 * 글자마다 하나씩 붙는 문법 색상 분류. */
typedef enum {
    CLS_TEXT, CLS_KEYWORD, CLS_TYPE, CLS_NUMBER, CLS_COMMENT, CLS_STRING,
    CLS_BUILTIN, CLS_FUNC, CLS_PUNCT, CLS_ERROR, CLS_PREPROC, CLS_COUNT
} SyntaxClass;

typedef struct {
    char text[ED_MAX_TEXT];
    int  len;
    int  cursor;
} EdSnapshot;

typedef struct Editor {
    /* text / 텍스트 */
    char     text[ED_MAX_TEXT];
    int      len;
    int      line_start[ED_MAX_TEXT + 2];
    int      line_count;
    unsigned version;              /* bumps on every edit / 편집마다 증가 */

    /* cursor and selection / 커서와 선택 */
    int      cursor;
    int      anchor;
    int      preferred_col;        /* column kept while moving up/down
                                      위/아래 이동 시 유지할 열 */

    /* syntax highlighting / 문법 강조 */
    uint8_t  cls[ED_MAX_TEXT];
    unsigned cls_version;
    bool     cls_valid;
    char     known[ED_KNOWN_MAX][32];   /* built-in names (take, put)
                                           내장 함수 이름 */
    int      known_count;

    /* undo / redo / 실행 취소, 다시 실행 */
    EdSnapshot undo[ED_UNDO_MAX];
    int      undo_len;
    EdSnapshot redo[ED_UNDO_MAX];
    int      redo_len;
    int      last_kind;
    int      last_cursor;

    /* view state, owned by editor_ui.c / 화면 상태 (editor_ui.c 소유) */
    int      scroll_line;
    int      scroll_col;
    bool     want_scroll;
    bool     focused;
    bool     dragging;
    bool     ibeam;
    float    blink;
    float    wheel;
    double   last_click_time;
    int      last_click_pos;

    /* markers set by the game / 게임이 설정하는 표시 */
    int      exec_line;            /* 0 = none / 0 = 없음 */
    int      error_line;
    int      error_col;
    int      error_len;
} Editor;

void ed_init(Editor *ed);
void ed_set_text(Editor *ed, const char *text, bool undoable);
void ed_set_known(Editor *ed, const char *const *names, int count);

/* editing / 편집 */
void ed_type_char(Editor *ed, int ch);
void ed_insert_text(Editor *ed, const char *s, int n);   /* paste / 붙여넣기 */
void ed_newline(Editor *ed);
void ed_backspace(Editor *ed, bool word);
void ed_delete(Editor *ed, bool word);
void ed_tab(Editor *ed, bool shift);
void ed_toggle_comment(Editor *ed);
void ed_cut(Editor *ed);
void ed_undo(Editor *ed);
void ed_redo(Editor *ed);

/* movement (select = extend the selection) / 이동 (select = 선택 확장) */
void ed_move_h(Editor *ed, int dir, bool word, bool select);
void ed_move_v(Editor *ed, int lines, bool select);
void ed_home(Editor *ed, bool select);
void ed_end(Editor *ed, bool select);
void ed_doc_start(Editor *ed, bool select);
void ed_doc_end(Editor *ed, bool select);
void ed_set_cursor(Editor *ed, int pos, bool select);
void ed_select_all(Editor *ed);
void ed_select_word(Editor *ed, int pos);

/* queries / 조회 */
bool ed_has_selection(const Editor *ed);
void ed_selection(const Editor *ed, int *start, int *end);
int  ed_copy(const Editor *ed, char *out, int cap);
int  ed_line_of(const Editor *ed, int pos);
int  ed_col_of(const Editor *ed, int pos);
int  ed_line_len(const Editor *ed, int line);
int  ed_pos_at(const Editor *ed, int line, int col);

/* recompute syntax classes if the text changed
 * 텍스트가 바뀌었으면 문법 분류를 다시 계산 */
void ed_refresh_classes(Editor *ed);

#endif
