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

#define LEVEL_SHELF_SIZE 8   /* slots on the Back Shelf / 뒤 선반의 칸 수 */

typedef enum {
    LEVEL_EVENT_NONE,
    LEVEL_EVENT_TAKE,
    LEVEL_EVENT_PUT,
    LEVEL_EVENT_SHRED,       /* shred(n): into the shredder / 파쇄기로     */
    LEVEL_EVENT_PUT_ODD,     /* put_odd(n): onto the ODD belt / ODD 벨트로 */
    LEVEL_EVENT_SHELF_SET,   /* shelf[index] = value / 선반에 놓기          */
    LEVEL_EVENT_SHELF_GET    /* reading shelf[index] / 선반에서 읽기        */
} LevelEventType;

typedef struct {
    LevelEventType type;
    int32_t        value;
    int            index;   /* shelf slot for SHELF_SET / SHELF_GET
                               SHELF_SET / SHELF_GET의 선반 칸 */
} LevelEvent;

struct LevelDef;

/* A program in the level's Examples menu (usually a typical mistake).
 * 레벨의 Examples 메뉴에 있는 프로그램 (보통 흔한 실수). */
typedef struct {
    const char *name;
    const char *about;          /* one line / 한 줄 설명 */
    const char *source;
} LevelExample;

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

    /* Second output: the shredder (level 3) or the ODD belt (level 5).
     * 두 번째 출력: 파쇄기 (레벨 3) 또는 ODD 벨트 (레벨 5). */
    int32_t  side_expected[LEVEL_MAX_ITEMS];
    int      side_expected_len;
    int32_t  side[LEVEL_MAX_ITEMS];
    int      side_len;

    /* The Back Shelf (levels 10-11): int shelf[8], empty until written.
     * 뒤 선반 (레벨 10-11): int shelf[8], 쓰기 전에는 비어 있다. */
    int32_t  shelf[LEVEL_SHELF_SIZE];
    bool     shelf_used[LEVEL_SHELF_SIZE];

    LevelEvent event;           /* most recent action, for the scene
                                   장면에 전달할 가장 최근 동작 */
} LevelRun;

typedef struct LevelDef {
    int         number;
    const char *title;
    const char *concept;
    const char *brief;          /* full explanation (Help screen)
                                   전체 설명 (도움말 화면) */
    const char *goal;           /* one line shown on screen / 화면의 한 줄 */
    const char *tools;          /* one-line list of available functions
                                   사용 가능한 함수 목록 (한 줄) */
    const char *starter;        /* code shown when the level opens
                                   레벨을 열 때 보여 줄 시작 코드 */
    const LevelExample *examples;   /* NULL: level 1 uses programs.c
                                       NULL: 레벨 1은 programs.c 사용 */
    int         example_count;
    const char *reference;      /* a working answer, for tests and
                                   screenshots only (never shown)
                                   테스트와 스크린샷 전용 정답 (화면에 없음) */

    void (*register_natives)(Program *p);
    void (*generate)(LevelRun *run, unsigned seed);
    void (*solve)(LevelRun *run);

    const char *side_name;      /* "shredder", "ODD belt" or NULL
                                   "shredder", "ODD belt" 또는 NULL */
    bool        has_shelf;      /* the level gives int shelf[8]
                                   레벨이 int shelf[8]을 준다 */
    bool        needs_helper;   /* Submit requires a function like
                                   int press(int n) that run() calls
                                   Submit에 int press(int n) 같은 함수가
                                   필요하다 */

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
/* Level 8: does the program define and call a helper like int press(int n)?
 * 레벨 8: 프로그램이 int press(int n) 같은 도우미를 정의하고 호출하는가? */
bool level_has_helper(const Program *prog);

extern const LevelDef LEVEL_MAILROOM;
extern const LevelDef LEVEL_DAMAGED_GOODS, LEVEL_TWO_BELTS, LEVEL_STAMP_PRESS,
                      LEVEL_BACK_SHELF, LEVEL_HOLIDAY_RUSH;

/* The Mailroom levels in plan order (docs/MAILROOM.md). NULL = planned,
 * not built yet; its number is shown dimmed.
 * 계획 순서의 우편실 레벨 (docs/MAILROOM.md). NULL = 계획됨, 아직 없음.
 * 번호는 흐리게 표시된다. */
#define LEVEL_SLOTS 11
extern const LevelDef *const LEVEL_LIST[LEVEL_SLOTS];
extern const char *const LEVEL_NAMES[LEVEL_SLOTS];

#endif
