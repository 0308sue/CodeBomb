/*
 * editor.c - text editing core (no raylib, tested headless)
 * editor.c - 텍스트 편집 핵심 (raylib 미사용, 헤드리스 테스트 대상)
 */
#include "editor.h"

#include "lexer.h"

#include <string.h>

/* Kinds of edit, used to group typing into one undo step.
 * 편집 종류. 연속 입력을 실행 취소 한 단계로 묶는 데 쓴다. */
enum { EK_NONE, EK_TYPE, EK_DELETE, EK_OTHER };

static bool is_word(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ---- buffer primitives / 버퍼 기본 연산 --------------------------------- */

static void rebuild_lines(Editor *ed)
{
    ed->line_count = 1;
    ed->line_start[0] = 0;
    for (int i = 0; i < ed->len; ++i)
        if (ed->text[i] == '\n') ed->line_start[ed->line_count++] = i + 1;
    /* sentinel: line_start[count] - 1 is the end of the last line
       경계값: line_start[count] - 1 이 마지막 줄의 끝 */
    ed->line_start[ed->line_count] = ed->len + 1;
}

/* Call after every change to the text.
 * 텍스트가 바뀔 때마다 호출한다. */
static void touched(Editor *ed)
{
    ed->text[ed->len] = '\0';
    rebuild_lines(ed);
    ed->version++;
    ed->want_scroll = true;
    ed->blink = 0.0f;
    ed->last_cursor = ed->cursor;
}

/* Insert/delete raw bytes and shift the cursor and anchor to match.
 * 바이트를 그대로 삽입/삭제하고 커서와 기준점을 그에 맞게 옮긴다. */
static int raw_insert(Editor *ed, int pos, const char *s, int n)
{
    int room = ED_MAX_TEXT - 1 - ed->len;
    if (n > room) n = room;
    if (n <= 0) return 0;
    pos = clampi(pos, 0, ed->len);
    memmove(ed->text + pos + n, ed->text + pos, (size_t)(ed->len - pos));
    memcpy(ed->text + pos, s, (size_t)n);
    ed->len += n;
    if (ed->cursor >= pos) ed->cursor += n;
    if (ed->anchor >= pos) ed->anchor += n;
    return n;
}

static void shift_for_delete(int *p, int a, int b)
{
    if (*p >= b) *p -= b - a;
    else if (*p > a) *p = a;
}

static void raw_delete(Editor *ed, int a, int b)
{
    a = clampi(a, 0, ed->len);
    b = clampi(b, 0, ed->len);
    if (b <= a) return;
    memmove(ed->text + a, ed->text + b, (size_t)(ed->len - b));
    ed->len -= b - a;
    shift_for_delete(&ed->cursor, a, b);
    shift_for_delete(&ed->anchor, a, b);
}

/* Keep only what the editor supports: printable ASCII and newlines.
 * Tabs become spaces, Windows "\r\n" becomes "\n", and each non-ASCII
 * character (e.g. Korean typed by mistake) becomes one '?'.
 * 에디터가 지원하는 것만 남긴다: 출력 가능한 ASCII와 줄바꿈.
 * 탭은 공백으로, Windows의 "\r\n"은 "\n"으로, ASCII가 아닌 글자(실수로
 * 입력한 한글 등)는 '?' 하나로 바꾼다. */
static int filter_text(const char *in, int n, char *out, int cap)
{
    int o = 0;
    for (int i = 0; i < n && o < cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '\r') continue;
        if (c == '\t') {
            for (int k = 0; k < ED_TAB && o < cap; ++k) out[o++] = ' ';
            continue;
        }
        if (c == '\n' || (c >= 32 && c < 127)) {
            out[o++] = (char)c;
            continue;
        }
        if (c >= 0xC0) out[o++] = '?';  /* UTF-8 lead byte / UTF-8 첫 바이트 */
    }
    return o;
}

/* ---- undo / 실행 취소 --------------------------------------------------- */

static void snapshot(const Editor *ed, EdSnapshot *s)
{
    memcpy(s->text, ed->text, (size_t)ed->len);
    s->len = ed->len;
    s->cursor = ed->cursor;
}

static void push_snapshot(EdSnapshot *stack, int *count, const Editor *ed)
{
    if (*count == ED_UNDO_MAX) {
        memmove(stack, stack + 1, sizeof(EdSnapshot) * (ED_UNDO_MAX - 1));
        (*count)--;
    }
    snapshot(ed, &stack[(*count)++]);
}

static void restore(Editor *ed, const EdSnapshot *s)
{
    memcpy(ed->text, s->text, (size_t)s->len);
    ed->len = s->len;
    ed->cursor = ed->anchor = clampi(s->cursor, 0, s->len);
    touched(ed);
    ed->last_kind = EK_NONE;
}

/* Save an undo point unless this edit continues the previous one.
 * 이번 편집이 이전 편집의 연속이 아니면 실행 취소 지점을 저장한다. */
static void checkpoint(Editor *ed, int kind)
{
    bool new_group = kind == EK_OTHER || kind != ed->last_kind ||
                     ed->cursor != ed->last_cursor || ed_has_selection(ed);
    if (new_group) push_snapshot(ed->undo, &ed->undo_len, ed);
    ed->redo_len = 0;
    ed->last_kind = kind;
}

void ed_undo(Editor *ed)
{
    if (ed->undo_len == 0) return;
    push_snapshot(ed->redo, &ed->redo_len, ed);
    restore(ed, &ed->undo[--ed->undo_len]);
}

void ed_redo(Editor *ed)
{
    if (ed->redo_len == 0) return;
    push_snapshot(ed->undo, &ed->undo_len, ed);
    restore(ed, &ed->redo[--ed->redo_len]);
}

/* ---- queries / 조회 ----------------------------------------------------- */

bool ed_has_selection(const Editor *ed) { return ed->cursor != ed->anchor; }

void ed_selection(const Editor *ed, int *start, int *end)
{
    *start = ed->cursor < ed->anchor ? ed->cursor : ed->anchor;
    *end = ed->cursor < ed->anchor ? ed->anchor : ed->cursor;
}

int ed_line_of(const Editor *ed, int pos)
{
    int lo = 0, hi = ed->line_count - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (ed->line_start[mid] <= pos) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

int ed_line_len(const Editor *ed, int line)
{
    if (line < 0 || line >= ed->line_count) return 0;
    return ed->line_start[line + 1] - 1 - ed->line_start[line];
}

int ed_col_of(const Editor *ed, int pos)
{
    return pos - ed->line_start[ed_line_of(ed, pos)];
}

int ed_pos_at(const Editor *ed, int line, int col)
{
    line = clampi(line, 0, ed->line_count - 1);
    return ed->line_start[line] + clampi(col, 0, ed_line_len(ed, line));
}

int ed_copy(const Editor *ed, char *out, int cap)
{
    int a, b;
    ed_selection(ed, &a, &b);
    int n = b - a;
    if (n > cap - 1) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(out, ed->text + a, (size_t)n);
    out[n] = '\0';
    return n;
}

/* ---- setup / 초기화 ----------------------------------------------------- */

void ed_init(Editor *ed)
{
    memset(ed, 0, sizeof *ed);
    ed->last_cursor = -1;
    touched(ed);
}

void ed_set_text(Editor *ed, const char *text, bool undoable)
{
    static char buf[ED_MAX_TEXT];
    if (undoable) {
        checkpoint(ed, EK_OTHER);
    } else {
        ed->undo_len = 0;
        ed->redo_len = 0;
    }
    int n = filter_text(text, (int)strlen(text), buf, ED_MAX_TEXT - 1);
    ed->len = 0;
    ed->cursor = ed->anchor = 0;
    raw_insert(ed, 0, buf, n);
    ed->cursor = ed->anchor = 0;
    ed->preferred_col = 0;
    ed->scroll_line = ed->scroll_col = 0;
    touched(ed);
    ed->last_kind = EK_NONE;
}

void ed_set_known(Editor *ed, const char *const *names, int count)
{
    if (count > ED_KNOWN_MAX) count = ED_KNOWN_MAX;
    for (int i = 0; i < count; ++i) {
        strncpy(ed->known[i], names[i], sizeof ed->known[i] - 1);
        ed->known[i][sizeof ed->known[i] - 1] = '\0';
    }
    ed->known_count = count;
    ed->cls_valid = false;
}

/* ---- editing / 편집 ----------------------------------------------------- */

static bool delete_selection(Editor *ed)
{
    if (!ed_has_selection(ed)) return false;
    int a, b;
    ed_selection(ed, &a, &b);
    raw_delete(ed, a, b);
    return true;
}

static bool only_spaces(const Editor *ed, int from, int to)
{
    for (int i = from; i < to; ++i)
        if (ed->text[i] != ' ') return false;
    return true;
}

void ed_type_char(Editor *ed, int ch)
{
    if (ch < 32 || ch > 126) return;
    checkpoint(ed, EK_TYPE);
    delete_selection(ed);

    /* Typing '}' on an empty line moves it back one indent level.
     * 빈 줄에서 '}'를 입력하면 들여쓰기를 한 단계 줄인다. */
    if (ch == '}') {
        int ls = ed->line_start[ed_line_of(ed, ed->cursor)];
        int col = ed->cursor - ls;
        if (col > 0 && only_spaces(ed, ls, ed->cursor)) {
            int rm = col % ED_TAB ? col % ED_TAB : ED_TAB;
            raw_delete(ed, ed->cursor - rm, ed->cursor);
        }
    }
    char c = (char)ch;
    raw_insert(ed, ed->cursor, &c, 1);
    ed->anchor = ed->cursor;
    ed->preferred_col = ed_col_of(ed, ed->cursor);
    touched(ed);
}

void ed_insert_text(Editor *ed, const char *s, int n)
{
    static char buf[ED_MAX_TEXT];
    checkpoint(ed, EK_OTHER);
    delete_selection(ed);
    int m = filter_text(s, n, buf, ED_MAX_TEXT - 1);
    raw_insert(ed, ed->cursor, buf, m);
    ed->anchor = ed->cursor;
    ed->preferred_col = ed_col_of(ed, ed->cursor);
    touched(ed);
}

/* Enter keeps the indentation, and adds a level after '{'.
 * Enter between "{}" puts the '}' on its own line.
 * Enter는 들여쓰기를 유지하고, '{' 뒤에서는 한 단계 더 들여쓴다.
 * "{}" 사이에서 Enter를 누르면 '}'가 자기 줄로 내려간다. */
void ed_newline(Editor *ed)
{
    checkpoint(ed, EK_OTHER);
    delete_selection(ed);

    int ls = ed->line_start[ed_line_of(ed, ed->cursor)];
    int indent = 0;
    while (ls + indent < ed->cursor && ed->text[ls + indent] == ' ') indent++;

    int p = ed->cursor - 1;
    while (p >= ls && ed->text[p] == ' ') p--;
    bool opens = p >= ls && ed->text[p] == '{';
    bool closes = ed->cursor < ed->len && ed->text[ed->cursor] == '}';

    char buf[256];
    int n = 0;
    int inner = indent + (opens ? ED_TAB : 0);
    if (inner > 100) inner = 100;
    if (indent > 100) indent = 100;
    buf[n++] = '\n';
    for (int i = 0; i < inner; ++i) buf[n++] = ' ';
    int mid = n;
    if (opens && closes) {
        buf[n++] = '\n';
        for (int i = 0; i < indent; ++i) buf[n++] = ' ';
    }
    int start = ed->cursor;
    int added = raw_insert(ed, start, buf, n);
    ed->cursor = start + (added < mid ? added : mid);
    ed->anchor = ed->cursor;
    ed->preferred_col = ed_col_of(ed, ed->cursor);
    touched(ed);
}

static int word_left(const Editor *ed, int pos)
{
    int p = pos;
    while (p > 0 && ed->text[p - 1] == ' ') p--;
    if (p > 0 && is_word(ed->text[p - 1])) {
        while (p > 0 && is_word(ed->text[p - 1])) p--;
    } else if (p > 0 && p == pos) {
        p--;
    }
    return p;
}

static int word_right(const Editor *ed, int pos)
{
    int p = pos;
    while (p < ed->len && ed->text[p] == ' ') p++;
    if (p < ed->len && is_word(ed->text[p])) {
        while (p < ed->len && is_word(ed->text[p])) p++;
    } else if (p < ed->len && p == pos) {
        p++;
    }
    return p;
}

void ed_backspace(Editor *ed, bool word)
{
    if (ed_has_selection(ed)) {
        checkpoint(ed, EK_DELETE);
        delete_selection(ed);
        touched(ed);
        return;
    }
    if (ed->cursor == 0) return;
    checkpoint(ed, EK_DELETE);

    int start;
    if (word) {
        start = word_left(ed, ed->cursor);
    } else {
        /* In leading spaces, delete back to the previous tab stop.
         * 줄 앞 공백에서는 이전 탭 위치까지 지운다. */
        int ls = ed->line_start[ed_line_of(ed, ed->cursor)];
        int col = ed->cursor - ls;
        if (col > 0 && only_spaces(ed, ls, ed->cursor))
            start = ed->cursor - (col % ED_TAB ? col % ED_TAB : ED_TAB);
        else
            start = ed->cursor - 1;
    }
    raw_delete(ed, start, ed->cursor);
    ed->preferred_col = ed_col_of(ed, ed->cursor);
    touched(ed);
}

void ed_delete(Editor *ed, bool word)
{
    if (ed_has_selection(ed)) {
        checkpoint(ed, EK_DELETE);
        delete_selection(ed);
        touched(ed);
        return;
    }
    if (ed->cursor >= ed->len) return;
    checkpoint(ed, EK_DELETE);
    int end = word ? word_right(ed, ed->cursor) : ed->cursor + 1;
    raw_delete(ed, ed->cursor, end);
    touched(ed);
}

void ed_cut(Editor *ed)
{
    if (!ed_has_selection(ed)) return;
    checkpoint(ed, EK_OTHER);
    delete_selection(ed);
    touched(ed);
}

/* Lines touched by the selection (or the cursor line).
 * 선택 영역(또는 커서 줄)이 걸친 줄 범위. */
static void selected_lines(const Editor *ed, int *first, int *last)
{
    int a, b;
    ed_selection(ed, &a, &b);
    *first = ed_line_of(ed, a);
    *last = ed_line_of(ed, b);
    if (ed_has_selection(ed) && *last > *first && b == ed->line_start[*last])
        (*last)--;
}

void ed_tab(Editor *ed, bool shift)
{
    if (!shift && !ed_has_selection(ed)) {
        checkpoint(ed, EK_TYPE);
        int col = ed_col_of(ed, ed->cursor);
        int n = ED_TAB - col % ED_TAB;
        raw_insert(ed, ed->cursor, "        ", n);
        ed->anchor = ed->cursor;
        ed->preferred_col = ed_col_of(ed, ed->cursor);
        touched(ed);
        return;
    }

    int first, last;
    selected_lines(ed, &first, &last);
    checkpoint(ed, EK_OTHER);
    /* Bottom-up, so earlier line starts stay valid.
     * 아래에서 위로 처리하여 위쪽 줄의 시작 위치가 그대로 유효하다. */
    for (int line = last; line >= first; --line) {
        int ls = ed->line_start[line];
        if (shift) {
            int k = 0;
            while (k < ED_TAB && ls + k < ed->len && ed->text[ls + k] == ' ')
                k++;
            raw_delete(ed, ls, ls + k);
        } else {
            raw_insert(ed, ls, "    ", ED_TAB);
        }
    }
    touched(ed);
}

/* Ctrl+/ : add or remove "// " on every selected line.
 * Ctrl+/ : 선택한 모든 줄에 "// "를 붙이거나 뗀다. */
void ed_toggle_comment(Editor *ed)
{
    int first, last;
    selected_lines(ed, &first, &last);

    bool all_commented = true, any = false;
    int min_indent = 1 << 30;
    for (int line = first; line <= last; ++line) {
        int ls = ed->line_start[line];
        int le = ls + ed_line_len(ed, line);
        int p = ls;
        while (p < le && ed->text[p] == ' ') p++;
        if (p == le) continue;                    /* blank / 빈 줄 */
        any = true;
        if (p - ls < min_indent) min_indent = p - ls;
        if (!(p + 1 < le && ed->text[p] == '/' && ed->text[p + 1] == '/'))
            all_commented = false;
    }
    if (!any) return;

    checkpoint(ed, EK_OTHER);
    for (int line = last; line >= first; --line) {
        int ls = ed->line_start[line];
        int le = ls + ed_line_len(ed, line);
        int p = ls;
        while (p < le && ed->text[p] == ' ') p++;
        if (p == le) continue;
        if (all_commented) {
            int n = (p + 2 < le && ed->text[p + 2] == ' ') ? 3 : 2;
            raw_delete(ed, p, p + n);
        } else {
            raw_insert(ed, ls + min_indent, "// ", 3);
        }
    }
    touched(ed);
}

/* ---- movement / 이동 ---------------------------------------------------- */

static void move_to(Editor *ed, int pos, bool select)
{
    ed->cursor = clampi(pos, 0, ed->len);
    if (!select) ed->anchor = ed->cursor;
    ed->want_scroll = true;
    ed->blink = 0.0f;
    ed->last_kind = EK_NONE;
}

void ed_set_cursor(Editor *ed, int pos, bool select)
{
    move_to(ed, pos, select);
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

void ed_move_h(Editor *ed, int dir, bool word, bool select)
{
    if (!select && ed_has_selection(ed)) {
        int a, b;
        ed_selection(ed, &a, &b);
        move_to(ed, dir < 0 ? a : b, false);
    } else {
        int pos;
        if (word) pos = dir < 0 ? word_left(ed, ed->cursor)
                                : word_right(ed, ed->cursor);
        else pos = ed->cursor + dir;
        move_to(ed, pos, select);
    }
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

void ed_move_v(Editor *ed, int lines, bool select)
{
    int line = ed_line_of(ed, ed->cursor) + lines;
    int pos;
    if (line < 0) pos = 0;
    else if (line >= ed->line_count) pos = ed->len;
    else pos = ed_pos_at(ed, line, ed->preferred_col);
    move_to(ed, pos, select);
}

/* Smart Home: first press goes to the code, second to column 0.
 * 스마트 Home: 처음엔 코드 시작으로, 다시 누르면 0열로 이동. */
void ed_home(Editor *ed, bool select)
{
    int line = ed_line_of(ed, ed->cursor);
    int ls = ed->line_start[line];
    int le = ls + ed_line_len(ed, line);
    int first = ls;
    while (first < le && ed->text[first] == ' ') first++;
    move_to(ed, ed->cursor == first ? ls : first, select);
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

void ed_end(Editor *ed, bool select)
{
    int line = ed_line_of(ed, ed->cursor);
    move_to(ed, ed->line_start[line] + ed_line_len(ed, line), select);
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

void ed_doc_start(Editor *ed, bool select)
{
    move_to(ed, 0, select);
    ed->preferred_col = 0;
}

void ed_doc_end(Editor *ed, bool select)
{
    move_to(ed, ed->len, select);
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

void ed_select_all(Editor *ed)
{
    ed->anchor = 0;
    ed->cursor = ed->len;
    ed->want_scroll = true;
    ed->last_kind = EK_NONE;
}

void ed_select_word(Editor *ed, int pos)
{
    pos = clampi(pos, 0, ed->len);
    int a = pos, b = pos;
    while (a > 0 && is_word(ed->text[a - 1])) a--;
    while (b < ed->len && is_word(ed->text[b])) b++;
    if (a == b && b < ed->len && ed->text[b] != '\n') b++;
    ed->anchor = a;
    ed->cursor = b;
    ed->want_scroll = true;
    ed->last_kind = EK_NONE;
    ed->preferred_col = ed_col_of(ed, ed->cursor);
}

/* ---- syntax classes / 문법 분류 ----------------------------------------- */

static uint8_t class_of(const Editor *ed, Token t)
{
    switch (t.kind) {
    case TK_COMMENT: return CLS_COMMENT;
    case TK_NUMBER:  return CLS_NUMBER;
    case TK_STRING:  return CLS_STRING;
    case TK_HASH:    return CLS_PREPROC;
    case TK_ERROR:   return CLS_ERROR;
    case TK_INT:
    case TK_VOID:    return CLS_TYPE;
    case TK_IF: case TK_ELSE: case TK_WHILE: case TK_FOR: case TK_RETURN:
    case TK_BREAK: case TK_CONTINUE: case TK_RESERVED:
        return CLS_KEYWORD;
    case TK_IDENT: {
        for (int i = 0; i < ed->known_count; ++i)
            if ((int)strlen(ed->known[i]) == t.len &&
                memcmp(ed->known[i], ed->text + t.start, (size_t)t.len) == 0)
                return CLS_BUILTIN;
        int p = t.start + t.len;
        while (p < ed->len && ed->text[p] == ' ') p++;
        return (p < ed->len && ed->text[p] == '(') ? CLS_FUNC : CLS_TEXT;
    }
    default:
        return CLS_PUNCT;
    }
}

void ed_refresh_classes(Editor *ed)
{
    if (ed->cls_valid && ed->cls_version == ed->version) return;
    memset(ed->cls, CLS_TEXT, (size_t)ed->len);
    Lexer lx;
    lexer_init(&lx, ed->text, ed->len, true);
    for (;;) {
        Token t = lexer_next(&lx);
        if (t.kind == TK_EOF) break;
        uint8_t c = class_of(ed, t);
        int end = t.start + t.len;
        if (end > ed->len) end = ed->len;
        for (int i = t.start; i < end; ++i) ed->cls[i] = c;
    }
    ed->cls_valid = true;
    ed->cls_version = ed->version;
}
