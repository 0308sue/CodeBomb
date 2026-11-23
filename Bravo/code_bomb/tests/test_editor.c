/*
 * test_editor.c - tests for the editor's text logic (no window needed)
 * test_editor.c - 에디터 텍스트 로직 테스트 (창이 필요 없음)
 */
#include "editor.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

static void type(Editor *ed, const char *s)
{
    for (; *s; ++s) {
        if (*s == '\n') ed_newline(ed);
        else ed_type_char(ed, *s);
    }
}

static bool text_is(const Editor *ed, const char *want)
{
    return strcmp(ed->text, want) == 0;
}

void run_editor_tests(void)
{
    Editor *ed = malloc(sizeof *ed);
    if (!ed) return;

    /* Typing with auto-indent / 자동 들여쓰기와 입력 */
    ed_init(ed);
    type(ed, "void run(void) {\nput(take());\n}");
    CHECK(text_is(ed, "void run(void) {\n    put(take());\n}"),
          "Enter after '{' indents, typing '}' dedents");
    CHECK(ed->line_count == 3, "three lines");

    /* Enter between {} opens a block / {} 사이의 Enter는 블록을 연다 */
    ed_set_text(ed, "if (x) {}", false);
    ed_set_cursor(ed, 8, false);
    ed_newline(ed);
    CHECK(text_is(ed, "if (x) {\n    \n}"), "Enter between braces");
    CHECK(ed_line_of(ed, ed->cursor) == 1 && ed_col_of(ed, ed->cursor) == 4,
          "cursor lands on the indented middle line");

    /* Smart backspace removes a whole indent / 스마트 백스페이스 */
    ed_backspace(ed, false);
    CHECK(text_is(ed, "if (x) {\n\n}"), "backspace in indent removes 4 spaces");

    /* Undo groups continuous typing, redo restores
     * 연속 입력은 한 번에 실행 취소, 다시 실행으로 복원 */
    ed_set_text(ed, "", false);
    type(ed, "int x");
    ed_newline(ed);
    type(ed, "abc");
    ed_undo(ed);
    CHECK(text_is(ed, "int x\n"), "undo removes the last typed word");
    ed_undo(ed);
    CHECK(text_is(ed, "int x"), "undo removes the newline");
    ed_redo(ed);
    ed_redo(ed);
    CHECK(text_is(ed, "int x\nabc"), "redo restores both");

    /* Paste filters Windows line endings, tabs and non-ASCII
     * 붙여넣기는 Windows 줄끝, 탭, 비ASCII 문자를 거른다 */
    ed_set_text(ed, "", false);
    const char *clip = "a\r\n\tb \xED\x95\x9C\xEA\xB8\x80";   /* "한글" */
    ed_insert_text(ed, clip, (int)strlen(clip));
    CHECK(text_is(ed, "a\n    b ??"), "paste normalises text");

    /* Selection replace, copy, cut / 선택 교체, 복사, 잘라내기 */
    ed_set_text(ed, "hello world", false);
    ed_set_cursor(ed, 0, false);
    ed_move_h(ed, 1, true, true);           /* select "hello" */
    char buf[64];
    ed_copy(ed, buf, sizeof buf);
    CHECK(strcmp(buf, "hello") == 0, "word selection + copy");
    ed_type_char(ed, 'X');
    CHECK(text_is(ed, "X world"), "typing replaces the selection");
    ed_select_all(ed);
    ed_cut(ed);
    CHECK(ed->len == 0, "cut all");

    /* Block indent and dedent / 블록 들여쓰기와 내어쓰기 */
    ed_set_text(ed, "a\nb\nc", false);
    ed_select_all(ed);
    ed_tab(ed, false);
    CHECK(text_is(ed, "    a\n    b\n    c"), "Tab indents selected lines");
    ed_tab(ed, true);
    CHECK(text_is(ed, "a\nb\nc"), "Shift+Tab dedents selected lines");

    /* Toggle comment / 주석 토글 */
    ed_set_text(ed, "    x = 1;\n    y = 2;", false);
    ed_select_all(ed);
    ed_toggle_comment(ed);
    CHECK(text_is(ed, "    // x = 1;\n    // y = 2;"), "comment lines");
    ed_toggle_comment(ed);
    CHECK(text_is(ed, "    x = 1;\n    y = 2;"), "uncomment lines");

    /* Movement / 이동 */
    ed_set_text(ed, "    abcdef\nxy\nlonger line", false);
    ed_set_cursor(ed, 8, false);            /* col 8 on line 0 */
    ed_move_v(ed, 1, false);
    CHECK(ed_col_of(ed, ed->cursor) == 2, "down clamps to short line");
    ed_move_v(ed, 1, false);
    CHECK(ed_col_of(ed, ed->cursor) == 8, "preferred column is remembered");
    ed_set_cursor(ed, 7, false);
    ed_home(ed, false);
    CHECK(ed->cursor == 4, "Home goes to the first non-space");
    ed_home(ed, false);
    CHECK(ed->cursor == 0, "second Home goes to column 0");

    /* Syntax classes / 문법 분류 */
    const char *known[] = { "take" };
    ed_set_known(ed, known, 1);
    ed_set_text(ed, "int x = take(); // hi", false);
    ed_refresh_classes(ed);
    CHECK(ed->cls[0] == CLS_TYPE, "'int' is a type");
    CHECK(ed->cls[8] == CLS_BUILTIN, "'take' is a built-in");
    CHECK(ed->cls[16] == CLS_COMMENT, "comment is coloured");

    /* Capacity is respected / 용량 제한을 지킨다 */
    ed_set_text(ed, "", false);
    for (int i = 0; i < ED_MAX_TEXT + 100; ++i) ed_type_char(ed, 'a');
    CHECK(ed->len == ED_MAX_TEXT - 1, "buffer never overflows");

    free(ed);
}
