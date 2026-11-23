/*
 * level.h - level rules, test generation and game natives (no raylib here)
 * level.h - 레벨 규칙, 테스트 생성, 게임 네이티브 함수 (raylib 미사용)
 *
 * A level follows the Human Resource Machine model: an inbox of values, an
 * expected outbox, and natives (take, put, ...) the player's code calls.
 * Natives record a LevelEvent for the scene and YIELD the VM while the
 * animation plays (unless running headless for Submit).
 *
 * 레벨은 Human Resource Machine 방식을 따른다: 입력함(inbox)의 값들,
 * 기대되는 출력함(outbox), 그리고 플레이어 코드가 호출하는 네이티브
 * 함수(take, put 등). 네이티브는 장면을 위한 LevelEvent를 기록하고,
 * 애니메이션이 재생되는 동안 VM을 YIELD 한다 (Submit용 헤드리스 실행은 예외).
 */
#ifndef LEVEL_H
#define LEVEL_H

#include "vm.h"

#define LEVEL_MAX_ITEMS 32

typedef enum {
    LEVEL_EVENT_NONE,
    LEVEL_EVENT_TAKE,
    LEVEL_EVENT_PUT
} LevelEventType;

typedef struct {
    LevelEventType type;
    int32_t        value;
} LevelEvent;

struct LevelDef;

typedef struct LevelRun {
    const struct LevelDef *def;
    unsigned seed;
    bool     headless;          /* true: never yield (Submit tests)
                                   true: YIELD 하지 않음 (Submit 테스트) */

    int32_t  inbox[LEVEL_MAX_ITEMS];
    int      inbox_len;
    int      inbox_pos;

    int32_t  expected[LEVEL_MAX_ITEMS];
    int      expected_len;

    int32_t  outbox[LEVEL_MAX_ITEMS];
    int      outbox_len;

    LevelEvent event;           /* most recent action, for the scene
                                   장면에 전달할 가장 최근 동작 */
} LevelRun;

typedef struct LevelDef {
    int         number;
    const char *title;
    const char *concept;
    const char *brief;
    const char *tools;          /* one-line list of available functions
                                   사용 가능한 함수 목록 (한 줄) */
    const char *starter;        /* code shown when the level opens
                                   레벨을 열 때 보여 줄 시작 코드 */

    void (*register_natives)(Program *p);
    void (*generate)(LevelRun *run, unsigned seed);
    void (*solve)(LevelRun *run);

    int par_size;               /* statements / 문장 수            */
    int par_speed;              /* average instructions per test
                                   테스트당 평균 실행 명령 수 */
} LevelDef;

typedef struct {
    bool   passed;
    int    tests_run;
    int    failed_test;         /* 1-based, 0 if none / 1부터 시작, 없으면 0 */
    double avg_steps;
    char   message[512];
} SubmitResult;

void level_run_init(LevelRun *run, const LevelDef *def, unsigned seed,
                    bool headless);
bool level_check_finished(const LevelRun *run, char *msg, size_t n);
void level_submit(const LevelDef *def, const Program *prog, int tests,
                  SubmitResult *out);
void level_format_items(const int32_t *items, int count, char *buf, size_t n);

extern const LevelDef LEVEL_MAILROOM;

#endif
