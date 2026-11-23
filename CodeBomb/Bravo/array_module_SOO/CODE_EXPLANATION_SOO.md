# 코드 설명과 연결 방법 SOO

## 한국어

### 1 파일별 역할

| 파일 | 역할 |
|---|---|
| `array_core_SOO.h` | 레벨, 이벤트, 상태의 공통 자료형과 함수 선언 |
| `array_core_SOO.c` | 6개 레벨 데이터, 이벤트 검증, 목표 판정, 샘플 기록 생성 |
| `main_SOO.c` | Raylib 창, 카드와 버튼, 단계 재생, 변수와 오류 표시 |
| `test_core_SOO.c` | 화면 없이 검증 엔진의 성공/실패/안전성 검사 |
| `Makefile_SOO` | C11 빌드와 sanitizer 테스트 명령 |
| `run_SOO.command` | 이 Mac에서 더블 클릭하여 빌드/실행 |

### 2 전체 흐름

```text
현재: 레벨 데이터 → SooBuildSample → 이벤트 목록
                                           ↓
                               SooApplyEvent → SooState
                                                  ↓
                                         Raylib 화면 표시

향후: 팀원의 코드 에디터 → 실제 실행에서 얻은 이벤트 목록
                                           ↓
                              같은 SooApplyEvent 사용
```

로직 파일은 Raylib를 포함하지 않습니다. 화면과 검증을 분리해서 팀원의 프로그램에서도 사용할 수 있고, GUI 없이 테스트할 수 있게 했습니다.

### 3 자료형

**SooLevel**은 레벨 설명, 행/열 크기, 초기값 `initial`, 정답 `goal`, 예산 `budget`을 보관합니다. 초기값은 Reset마다 복사하며 원본을 바꾸지 않습니다. 새 레벨도 같은 형식으로 추가할 수 있지만 현재 화면은 6개 레벨, 최대 4행·10열 범위에 맞춰 배치했습니다. 화면 배치와 번호 버튼도 함께 조정해야 합니다.

**SooEvent**는 한 단계입니다. `SOO_LOOP`는 반복 변수 변경, `SOO_READ`는 읽기, `SOO_WRITE`는 쓰기, `SOO_END`는 종료와 목표 검사를 뜻합니다. `row`, `col`은 접근 위치, `value`는 쓸 값, `i`, `j`는 화면에 보여 줄 변수입니다. 1D 배열도 `row=0`, `col=인덱스`로 표현합니다. 설정하지 않은 반복 변수는 `-1`로 보냅니다.

**SooState**는 현재 카드 값, 연산 횟수, 강조 위치, 변수, 실행 상태와 설명문을 보관합니다. 상태는 READY → RUNNING → SUCCESS 또는 FAILURE로 바뀝니다. 성공/실패 이후 이벤트는 받지 않으므로 실패 뒤의 쓰기가 결과를 바꿀 수 없습니다.

### 4 핵심 함수

**SooReset**: 현재 상태를 비우고 레벨 초기값을 복사합니다. 강조 위치와 변수는 `-1`, 횟수는 0으로 되돌립니다.

**SooApplyEvent**: 종료/반복 이벤트를 먼저 처리합니다. 읽기/쓰기는 종류와 좌표, 예산을 검사한 뒤 반영합니다. 실제 배열 접근 전에 범위를 검사하므로 `cards[N]` 같은 잘못된 접근을 게임의 오류로 보여 줄 수 있습니다. 역방향 레벨에서는 READ의 방향도 검사합니다. 화면용 이벤트는 실제 CPU 명령이 아니라 실행 도구가 만들어 주는 설명 기록입니다.

**SooGoalReached**: 모든 현재 값을 목표 값과 비교합니다. 카드가 모두 초록색인지로 판정하지 않습니다. 대각선 레벨은 일부 결함을 그대로 두는 것이 목표이기 때문입니다. `SOO_END`가 와야 최종 성공/미완료를 결정합니다.

**SooBuildSample**: 시연용 정답 기록과 오류 기록을 만듭니다. C 코드를 파싱하거나 실행하지 않습니다. 이벤트를 추가할 때마다 버퍼 크기를 확인하고 공간이 부족하면 `-1`을 반환합니다.

**SooSampleCode**: 오른쪽 참고 코드를 만듭니다. 오류 시나리오에서 실제 추가/누락 기록은 위쪽 주석으로 표시합니다. 참고 코드를 수정해도 게임 로직이 변하는 구조는 아닙니다.

**Step**: 다음 이벤트 하나를 적용하고 커서를 증가시킵니다. 성공/실패하면 자동 재생을 멈춥니다.

**Back**: 커서를 한 단계 줄인 위치까지 초기 상태부터 재생합니다. 값을 억지로 역변환하지 않으므로 읽기, 쓰기, 실패 상황도 같은 규칙으로 복원됩니다. 최대 512개 기록이므로 이 방식으로 충분합니다.

**Cards**: `row * cols + col`로 2D 좌표를 1차원 저장 위치로 바꿉니다. 예를 들어 3×4에서 `[1][2]`는 `1*4+2=6`입니다. 화면은 그 값을 다시 행/열로 배치합니다.

**DrawApp**: 레벨 버튼, 미션, 변수, 참고 코드, 예산 막대와 결과를 그립니다. 사용자 입력용 코드 에디터는 포함하지 않습니다.

**main**: 창을 만들고 입력 → 재생 시간 갱신 → 화면 그리기를 반복합니다. 렌더 텍스처를 쓰므로 창을 늘려도 비율이 유지됩니다. 마우스 좌표도 같은 비율로 바꿔 버튼 위치와 맞춥니다. 종료 시 그래픽 자원을 해제합니다.

### 5 연산 예산의 의미

읽기 1회 = 1, 쓰기 1회 = 1, 변수 표시 갱신 = 0입니다. 조건 비교, 반복 증가, 컴파일 시간 등은 계산하지 않습니다. 마지막 레벨은 10개 읽기 + 결함 6개 쓰기 = 16회입니다. 반복해서 읽는 샘플은 20회 뒤 다음 접근에서 실패합니다. 예산을 초과하는 쓰기는 실행되지 않습니다.

이는 효율성을 설명하기 위한 단순 모델입니다. 실제 밀리초나 모든 C 연산을 측정한 결과라고 설명하면 안 됩니다.

### 6 에디터 연결 계약

팀원에게 먼저 **실행 중 배열 접근 기록을 어떻게 얻을 것인지** 확인해야 합니다. 일반 C 컴파일러의 출력이나 printf 결과만으로 모든 읽기/쓰기와 반복 변수를 자동으로 알 수 없습니다. 제한된 실행기, 계측 코드 삽입 등 에디터 측 실행 방식이 필요합니다. 이 프로젝트에는 그 도구가 아직 없습니다.

연결 시에는 다음 정보를 전달합니다.

| 이벤트 | 필요한 정보 | 예산 |
|---|---|---|
| LOOP | 현재 i, j | 0 |
| READ | row, col, 현재 i, j | 1 |
| WRITE | row, col, value, 현재 i, j | 1 |
| END | 실행이 정상 종료됐음 | 0 |

컴파일 오류는 이벤트 재생 전에 에디터에서 보여 줍니다. 무한 루프/실행 시간 초과는 에디터 측에서 별도로 중단해야 합니다. 현재 모듈은 외부 프로그램을 실행하거나 격리하지 않습니다. 신뢰할 수 있는 실행 기록을 시각화/검사하는 역할이며, 기록 자체가 제출된 코드와 일치하는지는 검증하지 않습니다.

첫 레벨의 연결 예시는 아래와 같습니다. 이것은 함수 사용 예시이며, 실제 에디터 연결 완료를 의미하지 않습니다.

```c
SooState state;
SooReset(&state, &SOO_LEVELS[0]);
SooEvent trace[] = {
    { SOO_READ,  0, 2, 0, 2, -1 },
    { SOO_WRITE, 0, 2, 0, 2, -1 },
    { SOO_END,   0, 0, 0, -1, -1 }
};
for (int k = 0; k < 3; ++k) {
    if (!SooApplyEvent(&state, trace[k])) break;
    /* Draw the updated state here, one event per playback step. */
}
```

실제 화면에 붙일 때 `Load`에서 `SooBuildSample`로 생성하던 부분을 에디터가 보내는 이벤트로 바꾸고, `a.count`에 개수를 넣습니다. 최대 512개를 넘는 기록은 잘라서 성공 처리하지 말고 명확한 오류로 처리해야 합니다. `state.level`은 재생이 끝날 때까지 유효한 레벨 데이터를 가리켜야 합니다.

먼저 첫 레벨의 READ/WRITE/END 세 이벤트를 연결하고 같은 결과가 나오는지 확인한 뒤 LOOP와 2D를 추가하는 순서를 권장합니다.

### 7 확인한 내용과 남은 작업

- 경고를 오류로 취급하는 C11 빌드 통과
- sanitizer를 켠 검증 로직 테스트 통과
- 실제 Raylib 창 실행 및 2D 화면/범위 오류 화면 캡처 확인
- 남은 작업: 팀원 에디터 연결, 강의 범위 대조, 추가 레벨, 진행 저장 여부 결정

Raylib 화면 함수는 공식 API를 참고했습니다: https://www.raylib.com/cheatsheet/cheatsheet.html

## English

### Structure

`array_core_SOO.h` defines the shared level, event and state types. `array_core_SOO.c` contains level data, validation, goal checking and sample traces. `main_SOO.c` owns the Raylib interface. `test_core_SOO.c` tests the engine without opening a window.

The flow is: level data → sample events → `SooApplyEvent` → updated state → drawing. When the editor is ready, its execution events replace the sample builder.

### Important functions

- `SooReset` copies the original data and clears counters and highlights.
- `SooApplyEvent` validates event type, indices and budget before changing memory. Terminal states reject later events.
- `SooGoalReached` compares all cells with their targets. `SOO_END` triggers final validation.
- `SooBuildSample` generates demonstrations, not executable player code.
- `Step` applies one event. `Back` resets and replays to the previous position.
- `Cards` maps `[row][col]` to `row * cols + col` and draws the values.
- `DrawApp` renders controls and explanations. `main` handles input, timing and scaling.

### Editor integration

Use `SOO_READ`, `SOO_WRITE`, `SOO_LOOP` and `SOO_END` events. Each read/write must include a row and column; writes include the new value. Supply the displayed `i` and `j` values, or -1 when unset. For 1D arrays, row is zero.

A normal compiler's output alone does not provide all memory accesses. The editor needs a tracing or instrumentation method. Compilation errors, infinite loops and execution timeouts are the editor's responsibility. This module does not execute untrusted C or verify that a trace matches the submitted source.

Replace the sample-builder call in `Load` with editor-supplied events, validate the capacity and keep the level data alive during playback. Start with one READ, one WRITE and END for level 1.

### Operation model and limits

Each read/write costs one operation; loop display updates cost zero. This is a teaching model, not a wall-clock measurement or a full C operation count. The efficiency level's correct solution uses 16 operations against a limit of 20.

The module currently contains six sample levels, not forty. Completion is session-only. No code editor, pointer/list levels or multiplayer is included. The visual layout should be updated before adding larger grids or more level buttons.

The C11 warning-clean build and sanitizer tests passed. The Raylib window and selected visual states were also checked. Next steps are editor integration and checking the content against the lecture notes.

## 결과 예측 모드 / Predict mode

`quiz_SOO.h`에는 레벨별 질문, 선택지, 정답과 해설이 있습니다. `Answer()`는 답을 한 번만 받고 기존 정답 샘플 재생을 시작합니다. `DrawQuiz()`가 선택지와 해설을 그립니다. 답을 고르기 전에는 재생을 잠급니다. 오답을 골라도 실제 예제의 동작을 볼 수 있습니다. 퀴즈 점검과 샘플의 성공 판정은 별개입니다. Reset으로 다시 시도할 수 있습니다.

`quiz_SOO.h` stores the questions, choices, answer keys and explanations. `Answer()` accepts one choice and starts the existing sample trace. `DrawQuiz()` draws the choices and feedback. Playback is locked until the player answers. The correct trace plays after either answer outcome, so the player can learn from a mistake. Reset clears the answer. This mode does not need the editor.

## 배열 범위 오류 미션 / Array bounds mission

카드 8장의 인덱스는 0부터 7까지입니다. `i <= 8`은 `i == 8`일 때도 반복문 본문에 들어가므로 존재하지 않는 `cards[8]`을 읽으려고 합니다. `i < 8`은 8에서 반복을 끝냅니다. `i < 7`은 너무 일찍 끝나 마지막 카드 7을 건너뜁니다.

- `DrawBounds()`는 레벨 2의 C 예제, 조건 선택지와 해설을 그립니다. 선택 후 원본 예제가 먼저 재생됩니다.
- `CompareBounds()`는 선택한 답을 보관한 채 `Load()`로 카드와 재생 위치를 초기화합니다. 원본에는 `SOO_BOUNDS`, 수정본에는 `SOO_CORRECT`를 사용합니다.
- 두 예제는 0~7번 카드에서 같은 읽기·쓰기를 합니다. 원본은 마지막에 8번 읽기 이벤트를 더 보냅니다. `SooApplyEvent()`가 범위 밖 접근을 실제 읽기 전에 차단합니다. 수정본은 그 대신 종료 이벤트에서 목표값을 검사하고 성공합니다.
- 카드를 모두 초록색으로 바꿔도 범위 오류가 발생하면 실패입니다. 테스트는 이 차이와 원본의 실패 인덱스가 8인지 확인합니다.

Eight cards have indices 0 through 7. With `i <= 8`, the loop tries to read `cards[8]`. With `i < 8`, it stops before that read. With `i < 7`, it skips card 7.

`DrawBounds()` displays the examples, choices and feedback. `CompareBounds()` resets the board to the same starting values and keeps the player's answer. Both runs use the existing trace engine. The original adds an invalid read at index 8, which is rejected before memory is accessed. The fixed trace reaches the end and passes the goal check. The app shows prepared traces; it does not compile or run the displayed C code.

## 포인터 교환 미션 / Pointer swap mission

앱 오른쪽 위 **Pointers**를 누르고, a와 b가 어떻게 바뀔지 고릅니다. **B (7, 3)**가 정답입니다. Pause, Step, Back으로 각 줄의 결과를 확인하세요.

1. 시작할 때 a는 3, b는 7입니다. `&a`는 숫자 3이 아니라 a의 주소입니다. 함수에서 p는 그 주소를 받아 a를 가리킵니다. q도 같은 방식으로 b를 가리킵니다.
2. `temp = *p`는 p가 가리키는 a의 값 3을 보관합니다. temp에는 주소가 아니라 숫자 3이 들어갑니다.
3. `*p = *q`는 q가 가리키는 b의 값 7을 읽어서 p가 가리키는 a에 씁니다. 잠시 a와 b 모두 7입니다. 두 포인터의 대상은 그대로입니다.
4. `*q = temp`는 보관한 3을 b에 씁니다. 이제 a는 7, b는 3입니다.

`pointer_SOO.c`의 `SooSwapAt()`은 매번 초기값에서 요청한 단계까지 실제 C 포인터 연산을 적용합니다. 함수가 반환하는 것은 숫자 상태뿐입니다. 지역 변수의 주소를 저장하거나 돌려주지 않습니다. `main_SOO.c`의 `PointerView`는 선택, 단계, 재생 시간을 기억합니다. `DrawPointer()`는 화살표와 카드, 현재 단계 설명을 표시합니다. Back은 단계 번호를 줄이고 처음부터 해당 단계까지 다시 계산합니다. 화면에 표시한 예제는 편집하거나 컴파일하지 않습니다.

Open **Pointers** and choose the final values. The correct answer is B: a becomes 7 and b becomes 3. p holds the address of a, and q holds the address of b. First, temp saves 3. Next, the write through p changes a to 7. Finally, the write through q changes b to 3. The pointer targets do not change.

`SooSwapAt()` applies the pointer operations from the starting values up to the requested step. It returns only the values. `PointerView` tracks the answer and playback position. `DrawPointer()` draws the targets, cards and explanations. Back recomputes the earlier state. The lesson uses a fixed example and works without the editor.

## 창 크기와 화면 테마 / Window sizing and theme

`Workshop()`은 배경과 작은 로봇을 Raylib 도형으로 그리고, `CardFace()`는 고장/정상 표정을 그립니다.
카드의 숫자와 SAFE/FAULT 글자는 그대로 남아서 색을 구분하지 않아도 상태를 읽을 수 있습니다.
`main()`은 기본 화면을 1280 × 800으로 그리고, 창에 맞는 가로·세로 배율 중 작은 값을 택합니다.
같은 배율을 마우스 좌표에 역으로 적용해 창을 줄여도 버튼 위치가 맞습니다.

`Workshop()` draws the backdrop and mascot with Raylib shapes. `CardFace()` adds a face to each array card.
The card keeps its number and status text. The app scales the fixed canvas to fit the window
and reverses the same transform for mouse clicks.

## 미션 지도 / Mission map

`roadmap_SOO.h`의 `SOO_CHAPTERS`는 챕터별 범위, 이름, 난이도를 보관합니다. `SOO_ROUTES`는 지도 번호를 이미 구현한 미션에 연결합니다. 예를 들어 지도 18번은 기존 2D 배열 미션을 엽니다. 값이 0이면 아직 미션을 열지 않습니다. `DrawRoadmap()`은 이 데이터를 그려 클릭한 미션의 경로를 반환합니다.

`SOO_CHAPTERS` stores the course ranges and difficulty. `SOO_ROUTES` maps course numbers to implemented lessons. For example, course level 18 opens the existing 2D array lesson. A zero route is a planned level. `DrawRoadmap()` draws the map and returns the selected route.

## Stage 1 배열과 반복문 / Loops and Arrays

`stage1_SOO.h`의 SOO_INTRO에는 문제, 보기, 예제 코드가 있습니다. IntroLoad는 예제를 LOOP, READ, WRITE, RETURN, END 단계로 기록합니다. IntroAt은 초기값부터 현재 단계까지 적용해서 화면에 표시할 상태를 계산합니다. 그래서 Back도 카드 값을 정확히 복원합니다. 예제 실행은 에디터 없이 동작합니다.

- 01: 0~4번에 1을 씁니다. 이번 챕터의 활성 상태는 1입니다.
- 02: 3, 5, 7을 순서대로 읽고, 값 7의 위치인 2를 반환하면 통과합니다.
- 03: 0을 읽은 경우만 1을 씁니다.
- 04: 위쪽 source는 그대로 둡니다. 아래쪽 target에 쓸 때 전달 방향을 화살표로 보여줍니다.
- 05: 한 쌍을 읽고 두 번 씁니다. 현재 쌍을 연결하고 temp를 표시합니다.
- 06: 3×3 카드에 행과 열 순서로 1을 씁니다.

IntroLoad builds the sample events. IntroAt replays those events to the chosen step. The search checks the return value. The other missions compare the final board with their own target. A wrong quiz answer still lets the player watch the correct sample.

## Stage 2 포인터 / Pointers and Swapping

`PointerLessonLoad()`는 미리 준비된 예제의 각 연산 뒤에 카드 값, 가리키는 위치, 임시 값과 누적값을 복사합니다. `DrawStage2()`는 현재 단계의 복사본을 그립니다. Back으로 돌아갈 때 원래 배열을 다시 수정할 필요가 없습니다. 예제에서 `arr + i`는 위치(주소), `*(arr + i)`는 그곳의 값입니다.

07–08은 기존 값을 temp에 먼저 보관하고 숫자 또는 문자를 바꿉니다. 09는 읽을 때마다 최댓값을 갱신합니다. 10은 양쪽 포인터가 가리키는 값을 교환하고 가운데로 이동합니다. 11은 읽은 값을 합계에 더하고 게이지에 표시합니다.

`PointerLessonLoad()` stores a snapshot after each operation. `DrawStage2()` draws the selected snapshot. The player can go back without undoing writes. The sample code uses pointer arithmetic and dereferencing. The app plays built-in examples and does not compile player code.

## Stage 3 배열 조작 / Array Operations

`ArrayLessonLoad()`는 단계마다 배열 상태를 복사합니다. `used`는 실제 데이터 수이므로, 카드 값이 0이어도 빈 칸으로 취급하지 않습니다. `DrawStage3()`는 현재 상태를 읽고 이동 방향을 그립니다.

12번은 최댓값과 같은 값을 두 번째 후보로 세지 않습니다. 13번은 뒤쪽부터 오른쪽으로 복사해서 원래 값을 지킵니다. 14번은 삭제 위치부터 왼쪽으로 당기고 유효 길이를 줄입니다. 15번은 3을 읽을 때만 횟수를 올립니다. 16번은 처음의 1을 저장하고, 값을 왼쪽으로 당긴 후 마지막에 1을 넣습니다. 17번은 `ArrayPick()`으로 서로 다른 두 위치를 선택하고 `ArrayPairSum()`으로 합을 구합니다. 두 장의 합이 120이어야 통과합니다.

`ArrayLessonLoad()` saves a board after each step. `used` tracks the number of occupied slots. The insert moves from right to left, the delete shifts from left to right, and the rotation saves the first value before shifting. The final mission checks two different battery slots and their exact sum.

## Stage 4 격자 / 2D Arrays and Patterns

`GridFind()`는 행 순서로 한 칸씩 가면서 그 위치의 가로 패턴을 먼저, 없으면 세로를 확인합니다. 먼저 발견한 1~4의 연속 구간만 반환합니다. 0, -1, -2는 제외합니다. `GridLessonLoad()`는 탐색, 제거, 점수 갱신, 중력 단계를 복사해 Back으로 되돌릴 수 있게 합니다.

중력은 열마다 아래에서 위로 읽고, 별도의 dest 위치에 값을 쌓아 순서를 보존합니다. 옮긴 자리는 -1, 최종 위쪽 공간은 -2가 됩니다. 22번은 제거 한 번과 중력 한 번을 보여줍니다. 자동으로 무한 반복하거나 랜덤 값을 넣지 않습니다. 기존처럼 예제 기반이며, 에디터가 작성한 C 코드를 실행하는 기능은 아직 없습니다.

`GridFind()` checks horizontal before vertical at each row-major position, not all horizontal matches before all vertical ones. Only values 1–4 can match. Gravity packs each column from the bottom while preserving order and fills the top with -2. Level 22 performs one removal and one gravity pass.

## Stage 5 문자열과 문자 포인터 / Strings and Char Pointers

`stage5_SOO.h`의 `StringBuild()`는 예제 안에서 실제 문자 포인터를 이동하고, 각 단계의 문자 메모리를 복사합니다. 지역 포인터 주소를 저장하지 않고 문자 값과 위치만 저장하므로 이전 단계도 안전하게 볼 수 있습니다. `initialized` 비트는 아직 쓰지 않은 칸과 실제로 복사한 `\0`을 구분합니다.

- 23: HELLO의 문자를 하나씩 읽고 카운터를 올립니다. 마지막 `\0`은 길이에 포함하지 않습니다.
- 24: OpenAI에서 O, e, A, I를 찾습니다. 찾은 모음은 초록색이며 대소문자 모두 처리합니다.
- 25: CODE의 문자 네 개와 널 문자 한 개까지 복사합니다. `--`는 아직 쓰지 않은 칸입니다.
- 26: BOMB의 끝을 찾아 SAFE를 이어 씁니다. 결과 BOMBSAFE에는 9바이트가 필요합니다. `Try 8-byte buffer`는 공간 부족 시 아무것도 쓰지 않는 실패 예제입니다.
- 27: 입력란에 최대 8개의 영문/ASCII 문자를 입력하고 Verify를 누릅니다. 비교 순서는 저장된 BOMB 대 입력 문자열입니다. `0`이면 일치, 음수면 저장된 문자열이 앞, 양수면 뒤입니다. 빈 문자열이나 접두어도 널 문자를 비교해서 처리합니다. 중간 BYTE DIFF는 해당 단계의 차이이며, 최종 RETURN이 함수 결과입니다.

`StringCompare()`는 두 unsigned char 값을 비교합니다. `DrawStage5()`는 화면과 버튼을 그립니다. 이 모듈은 준비된 문자열 예제를 실행하며, 사용자 C 코드를 편집/컴파일하는 기능은 아닙니다. 목적지 버퍼 검사는 게임에서 추가한 보호 장치이고, 오른쪽 시험용 함수는 공간이 충분하다는 가정을 사용합니다.

`StringBuild()` executes the built-in pointer example and stores memory snapshots. It saves values and offsets, not local addresses. `initialized` distinguishes a copied null byte from an unwritten slot. Length excludes the terminator; copy and concat include it. The game checks destination capacity before copying. The displayed exam-style functions assume sufficient space. Level 27 accepts up to eight ASCII characters and compares stored BOMB against the input. Final RETURN gives the result; intermediate BYTE DIFF shows the current comparison. This is a standalone lesson, not a C code editor.

## Stage 6 연결 리스트 / Linked Lists

### 실행 방법

지도에서 28–33번을 고릅니다. A/B/C로 코드 이해 문제를 풀고, Step이나 Back으로 연결이 바뀌는 과정을 확인합니다. 예제 코드의 Node는 소스의 SooListNode에 해당합니다. 앱 구현은 C11이며 new/delete 대신 malloc/free와 NULL을 사용합니다.

### 배열과 다른 점

화면의 N0, N1은 배열 인덱스가 아니라 변하지 않는 노드 ID입니다. 실제 주소 숫자를 흉내 낸 값도 아닙니다. 노드는 화면에서 움직이지 않고 next 연결을 따라 논리 순서가 결정됩니다. 예를 들어 33번 마지막 head는 N1입니다. 순서는 N1(20) → N0(10) → N3(40) → N2(30) → NULL입니다. 노드의 data는 바뀌지 않습니다. 파란 선은 next, 분홍 선은 prev입니다.

### 미션별 설명

- 28: head를 current에 넣고 current->next를 따라갑니다. 네 노드를 방문한 뒤 current가 NULL이 되면 종료합니다. data가 0이라서 멈추는 것이 아닙니다.
- 29: 새 N2(20)를 만듭니다. 새 노드가 먼저 기존 다음 노드 N1(30)을 가리키게 합니다. 그다음 N0(10)의 next를 새 노드로 바꿉니다. 순서를 바꾸면 원래 뒤쪽 리스트를 잃을 수 있습니다.
- 30: target을 저장하고, 앞 노드가 target의 다음 노드를 직접 가리키게 합니다. 그 뒤 free합니다. 화면의 freed 칸은 이미 해제된 노드의 기록이며 실제 메모리를 읽는 것이 아닙니다. free가 target을 자동으로 NULL로 바꾸지는 않으며, 해제한 주소는 다시 역참조하면 안 됩니다.
- 31: 5 → 12 → 99를 따라가며 값을 비교합니다. 반환값은 99라는 숫자가 아니라 그 노드의 주소입니다. 찾지 못하면 NULL을 반환합니다.
- 32: next를 먼저 저장합니다. current->next를 prev로 바꾸고 작업 포인터들을 앞으로 진행합니다. 끝나면 head를 prev로 바꿉니다. 노드와 data는 그대로이며 화살표 방향만 바뀝니다.
- 33: first, second, after를 먼저 보관하고, 앞쪽 연결 → second의 prev/next → first의 prev/next → after의 prev를 바꿉니다. 첫 쌍에서는 *head가 바뀝니다. 홀수 길이면 마지막 남은 노드는 그대로 둡니다. 연결을 한 줄씩 변경하는 도중에는 일시적으로 끊기거나 중복된 방향이 보일 수 있습니다. 한 쌍의 수정이 끝나면 양쪽 링크가 일치합니다.

### 파일과 함수

stage6_SOO.h는 실제 노드 구조체, 화면 상태, 함수 선언을 모읍니다. stage6_SOO.c에는 두 부분이 있습니다.

1. 실제 C 함수: SooListCreate는 malloc 실패를 검사합니다. SooListInsertAfter는 새 노드와 이웃을 연결합니다. SooListRemoveAfter는 먼저 우회하고 free합니다. SooListFind는 주소를 반환합니다. SooListReverse와 SooListSwapPairs는 Node** 형태로 시작 노드를 바꿉니다. SooListFree는 리스트 전체를 해제합니다. 이 함수들은 테스트에서 실제 힙 메모리로 실행합니다.
2. 화면용 기록: ListLoad는 준비된 미션을 노드 ID 그래프로 구성하고 각 줄 이후 상태를 복사합니다. ListChoose는 한 번 답을 받고 재생을 시작합니다. ListAdvance는 한 단계, ListBack은 이전 복사본으로 이동합니다. ListFrameValid는 최종 next 링크에 순환·끊김·해제 노드 참조가 없는지, 이중 리스트의 prev가 맞는지 검사합니다. MissionGoal은 해당 미션의 정확한 노드 ID 순서와 값을 확인합니다.

stage6_view_SOO.h의 DrawStage6은 화살표와 카드, 현재 포인터, 실행 줄을 그립니다. 화면의 메모리 그림은 기록된 ID를 사용하기 때문에 삭제된 노드를 Back으로 확인해도 실제로 해제된 메모리를 접근하지 않습니다. 이 모듈도 사용자 C 코드를 컴파일하는 에디터가 아니라 준비된 예제입니다.

### 테스트

make -f Makefile_SOO test로 이전 단계와 Stage 6을 함께 검사합니다. 여섯 예제의 순서, 노드 ID 보존, 방문 수, 찾은 포인터, 삭제 후 미참조, 다음/이전 연결을 검사합니다. 실제 C 함수에는 빈 리스트와 1–5개 노드를 만들어 교환·뒤집기를 적용합니다. 두 번 적용하면 같은 노드 주소 순서가 복구되는지도 검사합니다. AddressSanitizer와 UndefinedBehaviorSanitizer를 켜서 잘못된 메모리 접근도 확인합니다.

### English

Open levels 28–33, answer A/B/C, then step or go back. Node labels are stable identities, not array indices or printed memory addresses. Nodes stay in place while next links change. Blue lines are next; pink lines are prev.

The C implementation uses malloc, free and NULL. SooListInsertAfter saves the old successor before replacing the current link. SooListRemoveAfter reconnects before freeing. SooListFind returns a node pointer. SooListReverse saves the successor before reversing each link. SooListSwapPairs changes links and the caller's head without exchanging data. Odd trailing nodes stay in place.

ListLoad records each prepared lesson as graph snapshots. The view uses IDs instead of live heap addresses, so Back after deletion is safe. A freed address must never be read; free does not automatically set a pointer to NULL. ListFrameValid checks final consistency, and MissionGoal checks the expected node identity order. The actual heap-based functions are tested separately. This remains a built-in lesson viewer rather than an editor that executes player code.

## Stage 7 동적 메모리 / Memory Allocation

### 조작

지도에서 34–37번을 고르고 A/B/C로 답합니다. 34–35는 안전한 예제를, 36–37은 원본 오류 예제를 먼저 재생합니다. 정답 여부는 이해 문제의 정답 여부이며, 원본 프로그램이 안전하다는 뜻은 아닙니다. 36–37의 Run fix로 수정본을 비교하세요. Reset은 답과 재생 위치를 초기화합니다.

- 34: 네 int 칸을 확보하고 하나에 100을 쓴 후 free와 NULL 대입을 합니다. Try malloc failure는 할당 결과가 NULL이어서 모든 접근을 건너뛰는 예제입니다.
- 35: 다섯 칸에 10,20,30,40,50을 쓴 후 delete[]로 해제합니다. Try delete (wrong)는 잘못된 해제 방식을 차단합니다.
- 36: 원본은 함수가 끝나도 여덟 칸이 해제되지 않아 빨간색으로 남습니다. Run fix에서는 delete[]를 실행해 누수를 없앱니다.
- 37: 원본은 delete 뒤 읽기를 시도합니다. 모델은 위험한 접근을 실행하기 전에 멈춥니다. 수정본은 읽기 → delete → nullptr 순서입니다. Alias case는 ptr을 nullptr로 바꿔도 alias가 여전히 해제된 대상을 가리키는 것을 보여줍니다.

### 화면과 C 코드

점(.)은 빈 칸, ?는 할당됐지만 아직 쓰지 않은 값입니다. 한 칸은 바이트 하나가 아니라 int 하나 크기의 학습용 공간입니다. 실제 운영체제 힙 배치나 주소를 표시하지 않습니다. 해제된 메모리는 다시 읽어서 표시하는 것이 아니라 저장된 상태로 보여줍니다.

stage7_SOO.h의 MemoryFrame은 값, 할당 종류, 유효 길이, 포인터/별칭 상태를 보관합니다. MemoryApply는 MEM_ALLOC, MEM_WRITE, MEM_READ, MEM_FREE 등을 하나씩 검증합니다. NULL, 범위 초과, 초기화 전 읽기, 잘못된 해제 종류, 중복 해제, 해제 후 접근을 차단합니다. MEM_END에서 아직 할당이 남으면 누수로 판정합니다. MemoryBuild는 예제를 이 이벤트들로 구성하고 복사본을 남깁니다. MemoryBack은 이전 복사본을 보여줍니다. DrawStage7은 칸, 경고와 비교 버튼을 그립니다.

앱과 엔진은 C11로 컴파일됩니다. 35–37의 C++ 코드는 문자열로 표시하는 학습 예제이며 C++ 실행기를 추가한 것이 아닙니다. new[]/delete[]와 new/delete는 각각 맞춰야 하며 malloc으로 얻은 메모리는 free로 해제합니다. 일반적인 throwing new의 실패는 NULL 반환이 아니라 예외입니다. NULL 실패 예제는 malloc에만 적용합니다. read(*ptr)는 예제의 출력/읽기를 뜻하는 설명용 표기입니다. new int 뒤의 *ptr=100은 new int(100)의 초기화 의미를 두 줄로 보여주는 구성입니다.

NULL/nullptr 대입은 다른 포인터를 지우거나 메모리를 해제하지 않습니다. 이 미션은 안전한 마무리를 보여주려고 포인터까지 비우게 하지만, 이것만으로 모든 별칭 문제가 해결되지는 않습니다. 실제로 범위를 벗어난 메모리나 해제된 주소를 읽지는 않습니다.

### 테스트

make -f Makefile_SOO test는 Stage 1–7을 검사합니다. test_stage7_SOO.c는 네 수정 예제, 누수 원본, 해제 후 접근 원본, 별칭, malloc 실패, 잘못된 delete, 범위, 초기화 전 읽기, 이중 해제를 검사합니다. AddressSanitizer와 UndefinedBehaviorSanitizer도 사용합니다.

### English

Open levels 34–37. Choose A/B/C, then use Play, Step or Back. Levels 36–37 first show the broken original. A correct quiz answer does not make that original safe: choose Run fix to see the corrected example. The alias experiment shows that clearing ptr does not clear another pointer to the same released allocation.

MemoryApply checks each event in a safe teaching model. It does not execute native use-after-free or invalid delete. MemoryBuild saves snapshots; MemoryBack selects an earlier snapshot. Slots represent ints rather than bytes. A dot is free space; a question mark is allocated but uninitialized.

The app is still C11. C++ examples are displayed as text. Match malloc/free, new/delete and new[]/delete[]. Normal throwing new does not return NULL on failure; only the malloc lesson simulates a NULL allocation result. The read(...) notation describes an illustrative access. All Stage 7 operations run against a bounded model rather than a real general-purpose heap allocator.

## Stage 8 Vector와 Iterator / Vector and Iterator

### 실행 방법

지도에서 38–40번을 고릅니다. 38번과 39번은 A/B/C로 답하고 Step/Back으로 예제를 확인합니다. 39번의 Original은 잘못된 반복자를 읽으려다 멈춥니다. Run fix는 erase가 돌려준 반복자를 사용합니다. Last element는 마지막 값을 지워서 end()가 반환되는 예제입니다. end() 자체는 유효한 위치지만 읽을 수 있는 원소가 아닙니다.

40번은 직접 조작하는 최종 미션입니다.

1. Scan next로 10을 읽고 99로 이동합니다.
2. Scan next를 다시 눌러 99를 찾습니다.
3. it = erase(it)로 제거합니다. 반환된 반복자가 20을 가리킵니다.
4. Scan next로 20과 30을 검사하고 end()까지 갑니다.
5. push_back(40)으로 새 원소를 넣습니다. 이전 반복자는 다시 사용하지 않습니다.
6. Verify circuit으로 정확한 순서 10,20,30,40을 확인합니다. 너무 일찍 검증하면 실패합니다. Back 또는 Reset으로 다시 시도할 수 있습니다.

### size와 capacity

size는 현재 원소 수, capacity는 재할당 없이 보관할 수 있는 원소 수입니다. 데모는 38번 시작 capacity를 3으로 잡고 공간이 부족하면 두 배로 늘립니다. 이 숫자는 실제 STL 구현에서 측정한 값이 아닙니다. 실제 초기 capacity와 증가 비율은 구현마다 다를 수 있습니다. 앱은 여전히 C11이며, std::vector를 컴파일해서 실행하는 기능이 추가된 것은 아닙니다.

### 반복자 유효성

- 재할당이 일어나면 기존 원소 반복자와 예전 end()가 모두 무효화됩니다.
- push_back에 재할당이 없으면 앞선 원소 반복자는 유지되지만 예전 end()는 무효화됩니다.
- erase는 삭제 위치와 그 뒤의 반복자를 무효화합니다. 앞의 반복자는 유지됩니다.
- erase가 반환한 반복자는 다음 원소를 가리키거나, 마지막을 삭제했다면 새로운 end()입니다.
- 같은 화면 위치에 20이 남아 보여도 예전 반복자를 그대로 읽으면 안 됩니다. 위치 숫자와 반복자 유효성은 다릅니다.

### 파일과 핵심 함수

stage8_SOO.h는 벡터의 값, size, capacity, 반복자 위치와 유효 상태를 정의합니다. stage8_SOO.c의 VectorPush는 공간을 확인하고, 필요한 경우 데모 저장소를 늘리면서 반복자를 무효화합니다. VectorErase는 뒤의 원소를 당기고, 기존 반복자를 무효화하거나 반환값으로 새 반복자를 줍니다. VectorRead는 무효 반복자와 end() 읽기를 차단합니다. VectorAdvance는 end()를 넘어가지 못하게 합니다.

VectorBoss는 최종 미션의 찾기, 제거, 추가, 검증을 처리합니다. 단계가 맞지 않으면 오류를 알려줍니다. stage8_view_SOO.h의 BossAct는 조작 결과를 저장하고, Back은 이전 복사본으로 돌아갑니다. 기존 상태에서 다른 조작을 하면 그 위치부터 새로운 기록을 만듭니다. DrawStage8은 size/capacity, 반복자 표시, 예제 코드와 버튼을 그립니다.

본 모듈은 최대 8개 원소의 안전한 C 시뮬레이션입니다. 실제 STL의 모든 동작을 구현한 것이 아니며, 사용자 C++ 코드를 실행하지 않습니다. 화면의 read(*it)는 값을 읽는 동작을 뜻하는 설명용 표기입니다.

### 테스트

test_stage8_SOO.c는 재할당 유무에 따른 push_back, 예전 end(), erase 앞/뒤 반복자, 마지막 원소 삭제, 빈 벡터, 잘못된 읽기, 최종 미션의 전체 흐름을 검사합니다. make -f Makefile_SOO test로 Stage 1–8을 함께 검사합니다.

### English

Levels 38–39 use prepared traces. Level 40 is interactive: scan to find 99, erase it using the returned iterator, finish the scan, append 40, then verify. Back and Reset allow retries.

size counts elements; capacity counts storage available before reallocation. The demonstration doubles capacity from a chosen starting value, but this is not a required STL growth rule. Reallocation invalidates all old iterators. Without reallocation, push_back still invalidates the old end iterator. erase invalidates at and after the erased position. Its return can be end(), which must not be dereferenced.

The app remains C11. VectorPush, VectorErase and VectorRead implement a bounded teaching model, not a full STL or a C++ runtime. The final mission records player actions for Back. Tests cover iterator lifetime rules and the full final repair sequence.

Reference rules: https://eel.is/c++draft/vector.modifiers and https://eel.is/c++draft/vector.capacity
