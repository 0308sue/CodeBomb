# Editor ↔ array module integration / 에디터 ↔ 배열 모듈 연결

## English

### What was asked for

Sumin's `CODE_EXPLANATION_SOO.md` (section 6) defines the contract: the
editor must produce `SOO_READ`, `SOO_WRITE`, `SOO_LOOP` and `SOO_END` events
from real execution and pass them to `SooApplyEvent()`. Her report of
10 November describes the screen: the mission stays at the top right, the
editor replaces the sample code below it, and the cards on the left show the
player's own changes. Both are now implemented.

### How the events are produced

```text
player's C  ──compile──▶  bytecode  ──run (headless)──▶  SooEvent trace  ──▶  Sumin's playback
 cards[i]                 CALL_NATIVE $read(0, i)         READ  row 0, col i
 cards[i] = 0             CALL_NATIVE $write(0, i, 0)     WRITE row 0, col i, value 0
 i or j changed           (checked before each access)    LOOP  i, j
 run() returns            (after the VM halts)            END
```

1. `array_register_level()` gives the compiler the level's array and
   constants: `cards` and `N` for 1D levels, `grid`, `ROWS` and `COLS` for 2D.
2. The compiler turns every element access into a call to one of two hidden
   natives (`$read`, `$write`). `$` can't appear in a C name, so players
   can't call them directly.
3. `array_record()` runs the program once without animation. Each native
   builds a `SooEvent` and passes it to `SooApplyEvent()` straight away, so:
   - reads return the real card value, and conditions like
     `if (cards[i] == 1)` behave correctly;
   - if her engine refuses an event (out of bounds, over budget, wrong
     direction on level 3, a value other than 0/1), the program stops on
     that source line with her message.
4. The screen replays the recorded trace with her Play / Step / Back / Reset
   logic. Back still resets and replays, as in her design.

| Event | Fields filled in by the bridge | Cost |
|---|---|---|
| LOOP | `i`, `j` read from the variables named `i` and `j` that are in scope (-1 if missing) | 0 |
| READ | `row`, `col`, `i`, `j` | 1 |
| WRITE | `row`, `col`, `value`, `i`, `j` | 1 |
| END | sent when `run()` returns | 0 |

### Her rules that are kept

- Traces longer than 512 events are an error, never cut and passed.
- A failure is latched: nothing after it can change the cards.
- Success means every card matches TARGET (level 5 keeps some FAULT cards).
- Each read or write costs 1; LOOP costs 0.

### Errors the engine never sees

Compile errors appear before anything runs, with a squiggle in the editor.
Runtime errors from the VM (an uninitialised variable, an endless loop, a
division by zero) are shown as one extra playback step after the last event,
in her execution log, with the line number.

### Sumin's lessons (her November update)

Her new version adds a mission map with 8 chapters and 40 levels, all
playable. Her screens are hosted without copying or editing them:

- `src/soo_lessons.c` defines scaled versions of the few raylib drawing
  calls her screens use (`DrawText`, `MeasureText`, `DrawRectangle`,
  `DrawRectangleRounded`, `DrawRectangleRoundedLinesEx`, `DrawCircle`,
  `DrawLine`, `DrawLineEx`, `DrawTriangle`), points her calls at them with
  macros, then includes her `main_SOO.c` with its `main()` renamed.
- Her 1280 x 800 layout is drawn at exactly 1.5x on our 1920 x 1200 canvas.
  Font sizes are rounded to 20, 30 or 40 (multiples of 10) and centred on
  her line, so the layout is unchanged. The mouse is converted back to her
  coordinates, so her buttons work as before.
- Her keyboard handling lives inside her `main()`, which can't be called,
  so it is repeated in `soo_lessons_update()` with the same keys and
  timings. Her "Mission map" button is replaced by our **Missions** tab, and
  her Stage 1 "Find the bug" button opens our Bomb screen in Find the bug
  mode.
- The map (`src/screen_missions.c`) is drawn natively from her
  `SOO_CHAPTERS` and `SOO_ROUTES`, so levels she adds there appear
  automatically. A new kind of stage needs one line in each switch in
  `soo_lessons.c`.
- `stage6_SOO.c`, `stage7_SOO.c` and `stage8_SOO.c` are compiled from her
  folder.
- Each chapter card has a **Help** button that opens a short field manual
  for that chapter (`src/help.c`). It also opens by itself before the first
  lesson of a chapter. The chapter titles and colours come from her roadmap.

Suggestion for Sumin: if her shared helpers (`Workshop`, `Button`,
`Wrapped`, colours) move from `main_SOO.c` into a header, the adapter can
include that header instead of the whole file.

### Ownership

Nothing in `array_module_SOO/` was changed (checked file by file against her
latest upload). `array_core_SOO.c/h`, `pointer_SOO.c/h`, `quiz_SOO.h` and
`stage6-8_SOO.c` are compiled from her folder, and her lesson screens are
included unchanged through `soo_lessons.c`. Her original Predict, Explore
and Find the bug drawing was ported to `screen_arrays.c` for the 3D bomb;
positions and font sizes changed, behaviour did not. The old
`screen_pointers.c` was removed: her lessons 07-11 replace it. Her
standalone app (`make -f Makefile_SOO`) still builds and runs as before.

### Verification

- `make test`: 326 checks, including all six levels solved with real C and
  compared with her samples (operation counts 2, 13, 13, 19, 8 and 16; her
  bounds sample fails at `cards[8]` after 13 operations; repeated scans stop
  at 20).
- Builds with clang and gcc (`-std=c11 -Wall -Wextra -Wpedantic`) with no
  warnings against the raylib 6.0 and 5.5 headers.
- `--selftest` opens, answers, plays and draws all 40 of her lessons, and
  runs clean under Address and UB sanitizers.
- Every screen was rendered at 1920 x 1200 with raylib 6.0's software
  renderer and checked visually.

### Screenshots

Rendered at 1920 x 1200 with `--shot` (see `docs/screenshots/`). The 3D bomb
that now fills the left panel is described in `docs/BOMB.md`.

### The bomb

The left panel is now a 3D bomb on a wooden table: a flat bomb for levels
1-6 and a rotating 6-sided cube with five harder modules. Writes flash and
spark, failures shake the bomb with a red flash, success turns the lights
green. See `docs/BOMB.md`.

## 한국어

### 요청 내용

수민의 `CODE_EXPLANATION_SOO.md` 6절에 연결 계약이 정해져 있습니다. 에디터는
실제 실행에서 `SOO_READ`, `SOO_WRITE`, `SOO_LOOP`, `SOO_END` 이벤트를 만들어
`SooApplyEvent()`에 넘겨야 합니다. 수민의 11월 10일 보고서에는 화면 계획이
있습니다: 미션은 오른쪽 위에 그대로 두고, 에디터가 그 아래의 샘플 코드를
대신하며, 왼쪽 카드는 플레이어 코드가 바꾼 내용을 보여 줍니다. 두 가지 모두
구현했습니다.

### 이벤트를 만드는 방법

```text
플레이어 C  ──컴파일──▶  바이트코드  ──화면 없이 실행──▶  SooEvent 기록  ──▶  수민의 재생
 cards[i]                CALL_NATIVE $read(0, i)          READ  행 0, 열 i
 cards[i] = 0            CALL_NATIVE $write(0, i, 0)      WRITE 행 0, 열 i, 값 0
 i 또는 j 변경            (접근할 때마다 확인)              LOOP  i, j
 run() 반환              (VM이 멈춘 뒤)                    END
```

1. `array_register_level()`이 레벨의 배열과 상수를 컴파일러에 알려 줍니다.
   1D 레벨은 `cards`와 `N`, 2D 레벨은 `grid`, `ROWS`, `COLS`입니다.
2. 컴파일러는 원소 접근을 숨겨진 네이티브 두 개(`$read`, `$write`) 중 하나의
   호출로 바꿉니다. `$`는 C 이름에 쓸 수 없으므로 플레이어가 직접 호출할 수
   없습니다.
3. `array_record()`는 프로그램을 애니메이션 없이 한 번 실행합니다. 각
   네이티브는 `SooEvent`를 만들어 바로 `SooApplyEvent()`에 넘깁니다. 그래서:
   - 읽기는 실제 카드 값을 돌려주고, `if (cards[i] == 1)` 같은 조건이 올바르게
     동작합니다.
   - 엔진이 이벤트를 거부하면 (범위 밖, 예산 초과, 레벨 3의 잘못된 방향,
     0/1이 아닌 값) 프로그램은 그 소스 줄에서 수민의 메시지와 함께 멈춥니다.
4. 화면은 기록된 이벤트를 수민의 Play / Step / Back / Reset 로직으로 다시
   재생합니다. Back은 수민의 설계대로 초기화 후 다시 재생합니다.

| 이벤트 | 연결 코드가 채우는 값 | 비용 |
|---|---|---|
| LOOP | 현재 범위에 있는 `i`, `j` 변수의 값 (없으면 -1) | 0 |
| READ | `row`, `col`, `i`, `j` | 1 |
| WRITE | `row`, `col`, `value`, `i`, `j` | 1 |
| END | `run()`이 반환될 때 보냄 | 0 |

### 그대로 지킨 수민의 규칙

- 512개를 넘는 기록은 잘라서 성공 처리하지 않고 오류로 처리합니다.
- 실패는 고정됩니다: 실패 뒤의 어떤 이벤트도 카드를 바꿀 수 없습니다.
- 성공은 모든 카드가 TARGET과 같을 때입니다 (레벨 5는 일부 FAULT 카드가 남음).
- 읽기와 쓰기는 각각 1, LOOP는 0입니다.

### 엔진이 보지 못하는 오류

컴파일 오류는 실행 전에 에디터에 물결 밑줄과 함께 보입니다. VM의 실행 오류
(초기화하지 않은 변수, 끝나지 않는 반복, 0으로 나누기)는 마지막 이벤트 다음의
추가 단계 하나로, 줄 번호와 함께 수민의 실행 로그에 표시됩니다.

### 수민의 수업 (11월 업데이트)

수민의 새 버전에는 8개 챕터와 40개 레벨의 미션 지도가 있고, 모두 플레이할
수 있습니다. 수민의 화면은 복사하거나 고치지 않고 그대로 실행합니다:

- `src/soo_lessons.c`는 수민의 화면이 쓰는 몇 가지 raylib 그리기 함수
  (`DrawText`, `MeasureText`, `DrawRectangle`, `DrawRectangleRounded`,
  `DrawRectangleRoundedLinesEx`, `DrawCircle`, `DrawLine`, `DrawLineEx`,
  `DrawTriangle`)의 배율 버전을 정의하고, 매크로로 수민의 호출을 그쪽으로
  돌린 뒤, `main()`의 이름을 바꿔 `main_SOO.c`를 포함합니다.
- 수민의 1280 x 800 배치를 우리 1920 x 1200 화면에 정확히 1.5배로 그립니다.
  글자 크기는 20, 30, 40(10의 배수)으로 맞추고 수민의 줄 가운데에 놓아
  배치가 바뀌지 않습니다. 마우스 좌표는 수민의 좌표로 되돌려 버튼이 전처럼
  동작합니다.
- 수민의 키보드 처리는 호출할 수 없는 `main()` 안에 있으므로
  `soo_lessons_update()`에 같은 키와 시간으로 옮겼습니다. 수민의 "Mission
  map" 버튼은 우리 **Missions** 탭이 대신하고, Stage 1의 "Find the bug"
  버튼은 Bomb 화면의 Find the bug 모드를 엽니다.
- 지도(`src/screen_missions.c`)는 수민의 `SOO_CHAPTERS`와 `SOO_ROUTES`로
  직접 그리므로, 수민이 레벨을 추가하면 자동으로 나타납니다. 새로운 종류의
  단계는 `soo_lessons.c`의 각 switch에 한 줄씩 추가하면 됩니다.
- `stage6_SOO.c`, `stage7_SOO.c`, `stage8_SOO.c`는 수민의 폴더에서 바로
  컴파일합니다.
- 챕터 카드마다 그 챕터의 짧은 현장 매뉴얼을 여는 **Help** 버튼이 있습니다
  (`src/help.c`). 챕터의 첫 수업 전에도 저절로 열립니다. 챕터 제목과 색은
  수민의 로드맵에서 가져옵니다.

수민에게 제안: 공용 도우미(`Workshop`, `Button`, `Wrapped`, 색상)를
`main_SOO.c`에서 헤더로 옮기면, 어댑터가 파일 전체 대신 그 헤더를 포함할 수
있습니다.

### 소유권

`array_module_SOO/` 안의 파일은 하나도 바꾸지 않았습니다 (수민의 최신
업로드와 파일별로 비교). `array_core_SOO.c/h`, `pointer_SOO.c/h`,
`quiz_SOO.h`, `stage6-8_SOO.c`는 수민의 폴더에서 바로 컴파일하고, 수업 화면은
`soo_lessons.c`를 통해 그대로 포함합니다. 원래의 Predict, Explore, Find the
bug 그리기 코드는 3D 폭탄에 맞춰 `screen_arrays.c`로 옮겼습니다. 위치와 글자
크기만 바뀌었고 동작은 같습니다. 예전 `screen_pointers.c`는 삭제했습니다:
수민의 07-11 수업이 대신합니다. 수민의 단독 앱(`make -f Makefile_SOO`)도
전처럼 빌드되고 실행됩니다.

### 검증

- `make test`: 326개 검사. 수민의 6개 레벨을 모두 실제 C로 풀고 샘플과
  비교합니다 (연산 횟수 2, 13, 13, 19, 8, 16. 범위 오류 샘플은 13회 연산 뒤
  `cards[8]`에서 실패. 반복 스캔은 20회에서 멈춤).
- clang과 gcc(`-std=c11 -Wall -Wextra -Wpedantic`)로 raylib 6.0과 5.5 헤더에
  대해 경고 없이 빌드됩니다.
- `--selftest`는 수민의 40개 수업을 모두 열고, 답하고, 재생하고, 그리며,
  Address/UB 새니타이저에서도 오류가 없습니다.
- 모든 화면을 raylib 6.0 소프트웨어 렌더러로 1920 x 1200에서 그려 눈으로
  확인했습니다.

### 스크린샷

`--shot`으로 1920 x 1200에서 그린 화면입니다 (`docs/screenshots/`). 이제 왼쪽
패널을 채우는 3D 폭탄은 `docs/BOMB.md`에 설명되어 있습니다.

### 폭탄

이제 왼쪽 패널은 나무 탁자 위의 3D 폭탄입니다: 레벨 1-6은 납작한 폭탄, 더
어려운 모듈 다섯 개는 돌릴 수 있는 6면 큐브. 쓰기는 번쩍이며 불꽃을 튀기고,
실패하면 폭탄이 흔들리며 붉게 번쩍이고, 성공하면 불빛이 초록색이 됩니다.
`docs/BOMB.md`를 참고하세요.
