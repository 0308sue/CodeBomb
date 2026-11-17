#ifndef ARRAY_CORE_SOO_H
#define ARRAY_CORE_SOO_H

#include <stdbool.h>

/* 한글: 이 파일은 그래픽 라이브러리 없이도 사용할 수 있는 공개 인터페이스다.
 * English: This public interface works without a graphics library.
 * 에디터 → SooEvent 배열 → 검증 엔진 → Raylib 화면 순서로 연결한다.
 * Connect editor -> SooEvent array -> validation engine -> Raylib view. */
#define SOO_MAX_CELLS 24
#define SOO_MAX_EVENTS 512
#define SOO_LEVEL_COUNT 6

typedef struct {
    const char *title;
    const char *concept;
    const char *objective;
    int rows, cols;
    int initial[SOO_MAX_CELLS];
    int goal[SOO_MAX_CELLS];
    int budget;  /* 한글: 읽기/쓰기 각각 1회. English: Each read/write costs one. */
} SooLevel;

typedef enum { SOO_LOOP, SOO_READ, SOO_WRITE, SOO_END } SooEventKind;
typedef struct {
    SooEventKind kind;
    int row, col;
    int value; /* 한글: WRITE에서 사용할 새 값. English: New value for WRITE. */
    int i, j;  /* 한글: -1은 아직 미정. English: -1 means not set. */
} SooEvent;

typedef enum { SOO_READY, SOO_RUNNING, SOO_SUCCESS, SOO_FAILURE } SooStatus;
typedef struct {
    const SooLevel *level;
    int cells[SOO_MAX_CELLS];
    int operations, active_row, active_col, i, j;
    SooStatus status;
    char message[192];
} SooState;

typedef enum { SOO_CORRECT, SOO_MISSING, SOO_BOUNDS, SOO_SLOW } SooScenario;

extern const SooLevel SOO_LEVELS[SOO_LEVEL_COUNT];
void SooReset(SooState *state, const SooLevel *level);
bool SooApplyEvent(SooState *state, SooEvent event);
bool SooGoalReached(const SooState *state);
/* 한글: 반환값 -1은 버퍼 부족. 실제 에디터 연동 시 이 생성기를 대체한다.
 * English: -1 means insufficient capacity. Replace this builder with editor traces. */
int SooBuildSample(const SooLevel *level, SooScenario scenario,
                   SooEvent *events, int capacity);
const char *SooSampleCode(const SooLevel *level, SooScenario scenario);
#endif
