/*
 * screen_arrays.h - Sumin's array module with the code editor built in
 * screen_arrays.h - 코드 에디터가 들어간 수민의 배열 모듈
 *
 * The left panel is a 3D bomb on a wooden table (bomb3d.c): a flat bomb for
 * levels 1-6 and a 6-sided cube bomb with five harder modules.
 * 왼쪽 패널은 나무 탁자 위의 3D 폭탄이다 (bomb3d.c): 레벨 1-6은 납작한 폭탄,
 * 더 어려운 모듈 다섯 개는 6면 큐브 폭탄.
 *
 * Modes / 모드:
 *   Write code   - the player writes C; the trace plays on the bomb
 *                  플레이어가 C를 작성하고 실행 기록이 폭탄 위에서 재생됨
 *   Predict      - Sumin's quiz mode            / 수민의 퀴즈 모드
 *   Explore      - Sumin's sample comparisons   / 수민의 샘플 비교
 *   Find the bug - Sumin's bounds mission       / 수민의 범위 오류 미션
 */
#ifndef SCREEN_ARRAYS_H
#define SCREEN_ARRAYS_H

#include "editor_ui.h"

#include <stdbool.h>

typedef struct ArraysScreen ArraysScreen;

ArraysScreen *arrays_create(const EditorStyle *style);
void          arrays_destroy(ArraysScreen *s);

/* Keyboard, editor input and timers. Call before arrays_draw().
 * 키보드, 에디터 입력, 타이머. arrays_draw()보다 먼저 호출. */
void arrays_update(ArraysScreen *s, float dt);

/* Draw the whole canvas; buttons are handled while drawing (Sumin's
 * immediate-mode style).
 * 전체 화면을 그린다. 버튼은 그리면서 처리한다 (수민의 즉시 모드 방식). */
void arrays_draw(ArraysScreen *s);

/* Draw the 3D bomb into its render texture. Call outside any other
 * BeginTextureMode (render textures can't be nested).
 * 3D 폭탄을 렌더 텍스처에 그린다. 다른 BeginTextureMode 밖에서 호출. */
void arrays_render_view(ArraysScreen *s);

/* True while the code editor has keyboard focus.
 * 코드 에디터가 키보드 포커스를 가진 동안 true. */
bool arrays_typing(const ArraysScreen *s);

/* Open bomb 0-5 (levels 1-6) or 6 (cube) in Write code mode, or Sumin's
 * Find the bug mission. / 폭탄 0-5 (레벨 1-6)나 6 (큐브)을 Write code
 * 모드로 열거나, 수민의 Find the bug 미션을 연다. */
void arrays_open(ArraysScreen *s, int level, bool find_the_bug);

/* Which bombs are defused (7 entries). / 해체한 폭탄 (7개). */
void arrays_defused(const ArraysScreen *s, bool out[7]);

/* Keeps the Cube bomb's clock running while a pop-up covers the screen.
 * 팝업이 화면을 덮는 동안에도 큐브 폭탄의 시계가 계속 간다. */
void arrays_tick(ArraysScreen *s, float dt);

/* Called when another screen is opened. / 다른 화면을 열 때 호출. */
void arrays_leave(ArraysScreen *s);

/* Scripted state for screenshots (--shot). mode: "code", "predict",
 * "explore", "bug". variant: code mode 0 = example solution,
 * 1 = bounds bug, 2 = compile error, 3 = starter code.
 * 스크린샷(--shot)용 상태 설정. */
void arrays_qa(ArraysScreen *s, const char *mode, int level, int steps,
               int variant);

/* Advance the 3D view's animations without input (for screenshots).
 * 입력 없이 3D 뷰 애니메이션을 진행한다 (스크린샷용). */
void arrays_qa_settle(ArraysScreen *s, float seconds);

/* --selftest: checks the cube's click picking. Returns failures.
 * --selftest: 큐브 클릭 선택을 검사한다. 실패 수를 돌려준다. */
int arrays_self_test(ArraysScreen *s);

#endif
