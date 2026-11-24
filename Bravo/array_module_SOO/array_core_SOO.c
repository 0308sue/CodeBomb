#include "array_core_SOO.h"
#include <stdio.h>
#include <string.h>

/* 한글: 각 레벨은 화면 코드 대신 데이터로 정의한다. 0=SAFE, 1=FAULT.
 * English: Levels are data, not separate screens. 0=SAFE, 1=FAULT.
 * 2D 배열은 row * cols + col 위치에 평탄화해서 저장한다.
 * Store 2D arrays flat at row * cols + col. */

const SooLevel SOO_LEVELS[SOO_LEVEL_COUNT] = {
    {"01  First connection", "Array index", "Set cards[2] to 0. Indexing starts at zero.",
     1, 6, {0,0,1,0,0,0}, {0}, 20},
    {"02  Sweep the circuit", "For loop", "Visit every slot and change all FAULT cards to SAFE.",
     1, 8, {1,0,1,1,0,1,0,1}, {0}, 40},
    {"03  Return signal", "Reverse loop", "Repair the row while scanning from the last index to zero.",
     1, 8, {1,0,1,0,1,0,1,1}, {0}, 40},
    {"04  Memory matrix", "2D array and nested loops", "Repair all 12 cards using row and column indices.",
     3, 4, {1,0,1,0,0,1,0,1,1,1,0,1}, {0}, 50},
    {"05  Diagonal route", "2D diagonal access", "Repair only grid[i][i]. Leave the other cards unchanged.",
     4, 4, {1,1,0,0,0,1,1,0,0,0,1,1,1,0,0,1},
     {0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0}, 16},
    {"06  Signal deadline", "Algorithm efficiency", "Repair the row within 20 reads and writes. Avoid repeated scans.",
     1, 10, {1,0,1,0,1,0,1,0,1,1}, {0}, 20}
};

/* 한글: 실패 후 다음 이벤트가 실행되지 않도록 상태를 고정한다.
 * English: Latch failure so later events cannot hide the original error. */
static bool Fail(SooState *s, const char *message)
{
    s->status = SOO_FAILURE;
    snprintf(s->message, sizeof(s->message), "%s", message);
    return false;
}

/* 한글: 재시작 또는 이전 단계 재생 시 초기 데이터로 완전히 복원한다.
 * English: Restore original data for restart or deterministic replay. */
// 수 리셋 셋츠 업 더 스타팅 스테이트.
// 잇 카피즈 더 레벨즈 어리저널 밸류즈 / 인투 더 커런트 어레이.
// 잇 올소 리셋츠 디 오퍼레이션 카운트.
void SooReset(SooState *s, const SooLevel *level)
{
    memset(s, 0, sizeof(*s));
    s->level = level;
    memcpy(s->cells, level->initial, sizeof(s->cells));
    s->active_row = s->active_col = s->i = s->j = -1;
    s->status = SOO_READY;
    snprintf(s->message, sizeof(s->message), "Ready. Select a trace, then press Play or Step.");
}

/* 한글: 최종 결과뿐 아니라 레벨 3의 역방향 순회도 이벤트 순서로 검사한다.
 * English: Goals compare every cell; reverse traversal is also checked below. */
// 웬 더 프로그램 엔즈 / 수 골 리치트 첵스 에브리 셀.
// 잇 컴페어즈 더 커런트 밸류즈 / 위드 더 타겟 밸류즈.
// 이프 데이 올 매치 / 디 엔진 마크스 더 레벨 애즈 컴플리트.
bool SooGoalReached(const SooState *s)
{
    for (int k = 0; k < s->level->rows * s->level->cols; ++k)
        if (s->cells[k] != s->level->goal[k]) return false;
    return true;
}

/* 한글: 외부 에디터가 보내는 이벤트도 이 한 함수로 검증한다.
 * English: Editor-provided events go through this same validator.
 * 범위 → 예산 → 실제 변경 순서로 처리하므로 잘못된 메모리에 접근하지 않는다.
 * Validate bounds and budget before accessing or changing a cell. */

// 수 어플라이 이벤트 / 테이크스 더 커런트 스테이트 앤 원 이벤트.
// 잇 첵스 더 로우 앤 칼럼 퍼스트.
// 포 이그잼플 / 언 어레이 위드 에이트 셀즈 / 해즈 인디시즈 지로우 투 세븐.
// 언 액세스 투 인덱스 에이트 / 스탑스 위드 언 에러.
bool SooApplyEvent(SooState *s, SooEvent e)
{
    if (s->status == SOO_FAILURE || s->status == SOO_SUCCESS) return false;
    s->status = SOO_RUNNING;
    s->i = e.i; s->j = e.j;
    if (e.kind == SOO_END) {
        if (!SooGoalReached(s)) return Fail(s, "Incomplete: compare the remaining cards with TARGET.");
        s->status = SOO_SUCCESS;
        snprintf(s->message, sizeof(s->message), "DEFUSED. Target reached in %d operations.", s->operations);
        return true;
    }
    if (e.kind == SOO_LOOP) {
        snprintf(s->message, sizeof(s->message), "Loop variables updated. No memory operation yet.");
        return true;
    }
    if (e.kind != SOO_READ && e.kind != SOO_WRITE)
        return Fail(s, "Unknown event type. Check the editor trace.");
    if (e.row < 0 || e.row >= s->level->rows || e.col < 0 || e.col >= s->level->cols) {
        char message[192];
        /* 한글: 1D 오류에는 화면의 cards[i] 표기를 그대로 사용한다.
         * English: Match the 1D error to the cards[i] notation on screen. */
        if (s->level->rows == 1 && e.row == 0)
            snprintf(message, sizeof(message), "Invalid access cards[%d]. Valid indices: 0..%d.",
                     e.col, s->level->cols - 1);
        else
            snprintf(message, sizeof(message), "Invalid access [%d][%d]. Valid rows: 0..%d, columns: 0..%d.",
                     e.row, e.col, s->level->rows - 1, s->level->cols - 1);
        return Fail(s, message);
    }
    if (s->operations >= s->level->budget)
        return Fail(s, "Operation limit exceeded. Try fewer memory reads and writes.");
    /* 한글: 역방향 레벨은 첫 접근과 다음 READ 방향을 검사한다.
     * English: The reverse level validates the first access and each READ direction. */
    if (s->level == &SOO_LEVELS[2] && e.kind == SOO_READ) {
        int expected = s->active_col < 0 ? s->level->cols - 1 : s->active_col - 1;
        if (e.col != expected) return Fail(s, "Reverse route: start at the last card, then decrease i by one.");
    }
    s->operations++;
    s->active_row = e.row; s->active_col = e.col;
    int index = e.row * s->level->cols + e.col;
    if (e.kind == SOO_WRITE) { // 잇 올소 첵스 디 오퍼레이션 리밋 / 비포 체인징 어 밸류.
        if (e.value != 0 && e.value != 1) return Fail(s, "Card values must be 0 (SAFE) or 1 (FAULT)."); // 포 어 라이트 이벤트 / 디스 라인 업데이트츠 디 어레이.
        s->cells[index] = e.value;  // 히어 / 인덱스 이즈 더 셀 퍼지션. 이 닷 밸류 이즈 더 뉴 밸류. 더 스크린 덴 드로즈 디 업데이트드 스테이트.
        snprintf(s->message, sizeof(s->message), "WRITE [%d][%d] = %d. Check this against TARGET.", e.row, e.col, e.value);
    } else {
        snprintf(s->message, sizeof(s->message), "READ [%d][%d] -> %d. A read does not change the card.", e.row, e.col, s->cells[index]);
    }
    return true;
}

/* 한글: 고정 크기 버퍼를 넘기지 않는 이벤트 추가 도우미다.
 * English: Append safely without overrunning the fixed-size trace buffer. */
static bool Push(SooEvent *events, int *count, int capacity,
                 SooEventKind kind, int r, int c, int value, int i, int j)
{
    if (*count >= capacity) return false;
    events[(*count)++] = (SooEvent){kind,r,c,value,i,j};
    return true;
}

/* 한글: 이것은 C 인터프리터가 아닌 시연용 실행 기록 생성기다.
 * English: This generates demo traces; it is not a C interpreter.
 * 올바른 풀이, 마지막 작업 누락, 범위 오류, 비효율 스캔을 비교한다.
 * Compare a correct trace, missing work, bounds errors and repeated scans. */
int SooBuildSample(const SooLevel *l, SooScenario scenario, SooEvent *events, int capacity)
{
    if (!events || capacity <= 0) return -1;
    int count = 0;
    bool single = l == &SOO_LEVELS[0];
    bool reverse = l == &SOO_LEVELS[2];
    bool diagonal = l == &SOO_LEVELS[4];
    int visits = single ? 1 : diagonal ? l->rows : l->rows * l->cols;
    #define ADD(kind,r,c,v,i,j) do { if (!Push(events,&count,capacity,kind,r,c,v,i,j)) return -1; } while (0)
    /* 한글: 여섯 번 전체를 읽는 낭비. 효율성 레벨에서는 예산 초과.
     * English: Six redundant scans exceed the efficiency level's budget. */
    if (scenario == SOO_SLOW) {
        for (int pass = 0; pass < 6; ++pass)
            for (int k = 0; k < l->rows * l->cols; ++k)
                ADD(SOO_READ,k/l->cols,k%l->cols,0,pass,k);
    }
    int stop = visits - (scenario == SOO_MISSING ? 1 : 0);
    for (int k = 0; k < stop; ++k) {
        int index = single ? 2 : reverse ? visits - 1 - k : k;
        int r = diagonal ? k : index / l->cols;
        int c = diagonal ? k : index % l->cols;
        int i = l->rows == 1 ? c : r;
        int j = l->rows == 1 ? -1 : c;
        ADD(SOO_LOOP,r,c,0,i,j);
        ADD(SOO_READ,r,c,0,i,j);
        if (l->initial[r*l->cols+c] == 1) ADD(SOO_WRITE,r,c,0,i,j);
    }
    if (scenario == SOO_BOUNDS) ADD(SOO_READ,0,l->cols,0,l->cols,-1);
    ADD(SOO_END,0,0,0,-1,-1);
    #undef ADD
    return count;
}

/* 한글: 실제로 컴파일되는 사용자 입력이 아니라 선택한 샘플의 설명용 코드다.
 * English: These snippets describe the selected trace, not compiled player input. */
const char *SooSampleCode(const SooLevel *l, SooScenario scenario)
{
    static char code[1200];
    const char *base;
    if (l == &SOO_LEVELS[0]) base = "if (cards[2] == 1)\n    cards[2] = 0;";
    else if (l == &SOO_LEVELS[2]) base = "for (int i = N - 1; i >= 0; --i) {\n    if (cards[i] == 1)\n        cards[i] = 0;\n}";
    else if (l == &SOO_LEVELS[4]) base = "for (int i = 0; i < ROWS; ++i) {\n    if (grid[i][i] == 1)\n        grid[i][i] = 0;\n}";
    else if (l->rows > 1) base = "for (int i = 0; i < ROWS; ++i) {\n    for (int j = 0; j < COLS; ++j) {\n        if (grid[i][j] == 1)\n            grid[i][j] = 0;\n    }\n}";
    else base = "for (int i = 0; i < N; ++i) {\n    if (cards[i] == 1)\n        cards[i] = 0;\n}";
    const char *note = scenario == SOO_MISSING ? "// Trace omits its last visit.\n" :
        scenario == SOO_BOUNDS ? "// Trace adds an invalid read at [0][COLS].\n" :
        scenario == SOO_SLOW ? "// Trace reads every card 6 extra times first.\n" : "// Correct sample trace\n";
    snprintf(code,sizeof(code),"%s%s",note,base);
    return code;
}
