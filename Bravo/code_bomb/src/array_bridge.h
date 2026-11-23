/*
 * array_bridge.h - connects the code editor's VM to Sumin's array engine
 * array_bridge.h - 코드 에디터의 VM과 수민의 배열 엔진을 연결한다
 *
 * This is the "editor -> SooEvent array -> validation engine -> view" link
 * described in array_module_SOO/CODE_EXPLANATION_SOO.md, section 6.
 * array_module_SOO/CODE_EXPLANATION_SOO.md 6절에 설명된
 * "에디터 -> SooEvent 배열 -> 검증 엔진 -> 화면" 연결 부분이다.
 *
 * How it works / 동작 방식:
 *   1. array_register_level() gives the compiler the level's array
 *      (cards[] or grid[][]) and constants (N or ROWS/COLS).
 *      array_register_level()은 레벨의 배열(cards[] 또는 grid[][])과
 *      상수(N 또는 ROWS/COLS)를 컴파일러에 알려 준다.
 *   2. The compiler turns cards[i] into calls to two hidden natives.
 *      컴파일러는 cards[i]를 숨겨진 네이티브 두 개의 호출로 바꾼다.
 *   3. array_record() runs the player's program once without animation.
 *      Each array access becomes a SooEvent (LOOP/READ/WRITE, then END),
 *      checked immediately by SooApplyEvent() so reads return real values
 *      and illegal accesses stop the program on the right line.
 *      array_record()는 플레이어의 프로그램을 애니메이션 없이 한 번 실행한다.
 *      배열 접근마다 SooEvent(LOOP/READ/WRITE, 마지막에 END)가 만들어지고,
 *      SooApplyEvent()가 바로 검사한다. 그래서 읽기는 실제 값을 돌려주고,
 *      잘못된 접근은 정확한 줄에서 프로그램을 멈춘다.
 *   4. The screen replays the recorded trace with Sumin's Play/Step/Back.
 *      화면은 기록된 실행 기록을 수민의 Play/Step/Back으로 다시 재생한다.
 *
 * Sumin's files are used unchanged; this file only calls her public API.
 * 수민의 파일은 수정하지 않고, 이 파일은 그 공개 API만 호출한다.
 */
#ifndef ARRAY_BRIDGE_H
#define ARRAY_BRIDGE_H

#include "array_core_SOO.h"
#include "vm.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const SooLevel *level;
    SooState  live;                      /* state while recording / 기록 중 상태 */
    SooEvent  events[SOO_MAX_EVENTS];
    int       lines[SOO_MAX_EVENTS];     /* source line of each event
                                            각 이벤트의 소스 줄 */
    int       count;
    int       last_i, last_j;            /* last loop values sent
                                            마지막으로 보낸 반복 변수 값 */

    /* outcome / 결과 */
    bool      finished;        /* run() returned normally, END was added
                                  run()이 정상 반환, END 추가됨 */
    bool      failed;          /* the program stopped with an error
                                  프로그램이 오류로 멈춤 */
    bool      engine_failed;   /* that error came from SooApplyEvent, so the
                                  failing event is the last one in the trace
                                  그 오류가 SooApplyEvent에서 왔으므로 실패한
                                  이벤트가 기록의 마지막에 있다 */
    char      error[256];
    int       error_line;
    long      steps;           /* VM instructions executed / 실행한 VM 명령 수 */
} ArrayTrace;

/* "cards" for 1D levels, "grid" for 2D levels (same names as Sumin's
 * reference code). / 1D 레벨은 "cards", 2D 레벨은 "grid" (수민의 참고
 * 코드와 같은 이름). */
const char *array_name(const SooLevel *level);

/* Register natives, the array and the constants. Call after prog_init().
 * 네이티브, 배열, 상수를 등록한다. prog_init() 다음에 호출. */
void array_register_level(Program *prog, const SooLevel *level);

/* Run the compiled program headless and record its trace. vm is scratch
 * space (a VM is large, so the caller owns it).
 * 컴파일된 프로그램을 화면 없이 실행하고 실행 기록을 남긴다. vm은 작업
 * 공간이다 (VM은 크므로 호출자가 소유한다). */
void array_record(ArrayTrace *t, const Program *prog, const SooLevel *level,
                  VM *vm);

/* Playback helpers shared by the screen and the tests.
 * 화면과 테스트가 함께 쓰는 재생 도우미. */

/* Number of playback steps: the events, plus one step for a VM error the
 * engine never saw (e.g. an uninitialised variable).
 * 재생 단계 수: 이벤트 수, 그리고 엔진이 보지 못한 VM 오류(예: 초기화하지
 * 않은 변수)가 있으면 1단계 추가. */
int  array_trace_length(const ArrayTrace *t);

/* Apply the next step. Returns false when there is nothing more to apply.
 * 다음 단계를 적용한다. 더 적용할 것이 없으면 false. */
bool array_playback_step(const ArrayTrace *t, SooState *state, int *cursor);

/* Source line for the step just applied (0 = none), for the editor's
 * running-line highlight.
 * 방금 적용한 단계의 소스 줄 (0 = 없음). 에디터의 실행 줄 강조에 쓴다. */
int  array_line_at(const ArrayTrace *t, int cursor);

/* Reset and replay up to target events (Sumin's deterministic Back).
 * 초기화 후 target개 이벤트까지 다시 재생한다 (수민의 결정적 Back 방식). */
void array_playback_to(const ArrayTrace *t, SooState *state, int *cursor,
                       int target);

/* Run the whole trace and return the final state (used by tests).
 * 전체 기록을 재생하고 최종 상태를 돌려준다 (테스트용). */
SooState array_playback_all(const ArrayTrace *t);

/* Starter code and the one-line "you can use" hint for a level.
 * 레벨의 시작 코드와 "사용 가능" 한 줄 안내. */
void array_starter_code(const SooLevel *level, char *buf, size_t n);
void array_tools_text(const SooLevel *level, char *buf, size_t n);

/* A working solution for SOO_LEVELS[level] (tests and screenshots).
 * SOO_LEVELS[level]의 동작하는 정답 (테스트와 스크린샷용). */
const char *array_example_solution(int level);

/* Apply one event like SooApplyEvent, plus level 3's right-to-left rule for
 * random copies of level 3 registered with array_mark_reverse().
 * SooApplyEvent처럼 이벤트를 적용하고, array_mark_reverse()로 등록된 레벨 3의
 * 무작위 복사본에는 오른쪽에서 왼쪽 규칙도 적용한다. */
bool array_apply(SooState *state, SooEvent event);
void array_mark_reverse(const SooLevel *level);
void array_unmark_reverse(const SooLevel *level);

/* Compile src for level, run it headless and play it to the end. Returns
 * true when the bomb is defused; otherwise why says what went wrong.
 * level에 맞게 src를 컴파일하고 화면 없이 실행해 끝까지 재생한다. 폭탄이
 * 해체되면 true, 아니면 why에 이유를 적는다. */
bool array_check_source(const char *src, int len, const SooLevel *level,
                        Program *prog, VM *vm, ArrayTrace *t, char *why,
                        size_t n);

#endif
