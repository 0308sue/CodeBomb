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
