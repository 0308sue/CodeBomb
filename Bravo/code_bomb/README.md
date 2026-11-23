# Code Bomb / Circuit Pals

## English

A bomb defusal game for learning C. The code editor, compiler and VM work
together with Sumin's array module (`../array_module_SOO`): players write real
C, and every `cards[i]` read and write becomes one of Sumin's trace events and
plays out on a 3D bomb on a wooden table, step by step. Levels 1-6 use a flat
bomb; the Cube bomb has five harder modules, one per face (see
`docs/BOMB.md`).

Written in C11 with raylib (5.5 or 6.0). Designed for a 1920 x 1200 canvas.

### Run it

**macOS (Sumin's setup):** `brew install raylib pkg-config` once, then
double-click `run.command`, or in Terminal:

```sh
make
./build/codebomb
```

**Windows:** install raylib from <https://raysan5.itch.io/raylib> (default
folder `C:\raylib`), then double-click `run_windows.bat`. Or open
`C:\raylib\w64devkit\w64devkit.exe`, `cd` to this folder and run `make run`.
Visual Studio users can open the folder and use `CMakeLists.txt`.

**Any platform with CMake:**

```sh
cmake -B build-cmake
cmake --build build-cmake --config Release
```

On a monitor of at least 1920 x 1200 the game starts in borderless full
screen at exactly 1:1, so the pixel font is sharp. Smaller screens (such as
a MacBook) get a window scaled to fit, as in Sumin's version. F11 switches
between full screen and a window.

### Screens

| Tab | What it is | Source |
|---|---|---|
| Missions | The mission map (the game opens here): Sumin's 8 chapters and 40 lessons, plus a Bomb defusal row | `screen_missions.c` + her `roadmap_SOO.h` |
| Missions → 01-40 | Sumin's lessons, her screens drawn at 1.5x | `soo_lessons.c` + her `main_SOO.c` and `stage*_SOO` files |
| Bomb → Levels 1-6 | Untimed practice: write C for each of Sumin's 6 levels on a flat bomb; the bomb replays your code. Play randomised (F9) runs it on a new random bomb with the same rule; Hint gives three hints | editor + `array_bridge.c` + `bomb3d.c` + Sumin's engine |
| Bomb → Cube bomb | The timed challenge: press START, defuse 5 modules in 10:00 with the countdown on the bomb; a wrong run or time-out explodes it, then RESTART | `screen_arrays.c` + `bomb3d.c` |
| Bomb → Predict / Explore / Find the bug | Sumin's modes, unchanged logic (levels 1-6 only) | ported from `main_SOO.c` |
| Mailroom | Levels with the 3D robot: 1 Mailroom, 2 Night Shift, 4 Weigh Station, 6 Head Count, 7 Biggest Box, 9 Countdown (the rest of `docs/MAILROOM.md` is shown dimmed) | editor + VM + `levels.c` |
| Help (every screen, every chapter card) | The field manual: short pages with a diagram, a code example and a tip for each concept. Opens by itself the first time | `help.c` |
| Exit | Quit the game (asks first) | `main.c` |

### Controls

| Action | Windows | macOS |
|---|---|---|
| Run / pause your code | F5 or Ctrl+Enter | Cmd+Enter (or fn+F5) |
| Step | F6 | fn+F6 |
| Back (Bomb) / Step op (Mailroom) | F7 | fn+F7 |
| Reset | F8 | fn+F8 |
| Leave the editor | Esc | Esc |
| Full screen | F11 | fn+F11 |

In Predict, Explore and Find the bug, Sumin's keys work as before: Space,
arrows, R, 1-6, A/B/C and S. In Write code they are switched off, so typing
always goes into your code.

Cube bomb: drag on the bomb to turn it; click a face (or a module button) to
work on that module.

In Write code mode you can use `cards[i]` with `N` (1D levels) or
`grid[i][j]` with `ROWS` and `COLS` (2D levels). Card values are 0 (SAFE)
and 1 (FAULT).

### Tests

```sh
make test        # 377 checks, no raylib needed
make test-asan   # the same with Address/UB sanitizers (macOS/Linux)
```

The bridge tests solve all six of Sumin's levels and all five cube modules
with real C, and check the results against her own samples (for example 13
operations on level 2, 16 on level 6, and the `cards[8]` bounds failure).
`./build/codebomb --selftest` checks the Cube bomb challenge rules (locked
before START, a compile error is safe, an unsolved run or a strike or the
clock reaching 00:00 explodes, five solved modules defuse, leaving abandons
the attempt), clicking on the cube's faces, all 40 of Sumin's lessons and
every page of the field manual.

In Sumin's lessons her own keys work: A/B/C to answer, Space to play or
pause, arrows to step, R to restart. **Missions** returns to the map.

### Files

| File | Role |
|---|---|
| `src/array_bridge.*` | Connects the VM to Sumin's `SooApplyEvent` (see `docs/INTEGRATION.md`) |
| `src/screen_arrays.*` | The bomb defusal screen: Sumin's modes plus Write code |
| `src/bomb3d.*` | 3D table and bomb (flat and cube), effects, camera fit |
| `src/bomb_modules.*` | The five Cube bomb modules |
| `src/screen_missions.*` | Mission map built from her `roadmap_SOO.h` |
| `src/soo_lessons.*` | Hosts her 40 lessons unchanged, scaled 1.5x (see `docs/INTEGRATION.md`) |
| `src/help.*` | Field manual pages, diagrams and the Help pop-up |
| `src/bomb_variants.*` | Random bombs with the same rule, and the hints |
| `src/screen_mailroom.*`, `src/scene_mailroom.c` | Level 1 with the 3D scene |
| `src/ui.*` | Shared theme, buttons and text (Sumin's palette and widgets) |
| `src/main.c` | Window, 1920 x 1200 canvas scaling, tabs (Missions, Bomb, Mailroom) |
| `src/vm.*`, `lexer.*`, `parser.*`, `compiler.*` | C subset compiler and VM |
| `src/editor.*`, `src/editor_ui.*` | Code editor |
| `../array_module_SOO/` | Sumin's module, used in place and not modified |

All font sizes are multiples of 10 (20, 30, 40). raylib's default font is a
10 px pixel font, so these sizes scale it by exactly 2x, 3x or 4x and the
letters stay sharp.

QA screenshots (for reports): `./build/codebomb --shot out.png arrays code 4 14 0`
(screen, mode, level, steps, variant), `--shot map.png map` or
`--shot l28.png lesson - 28`. See the comment at the top of
`src/main.c`.

## 한국어

C를 배우는 폭탄 해체 게임입니다. 코드 에디터, 컴파일러, VM이 수민의 배열
모듈(`../array_module_SOO`)과 함께 동작합니다: 플레이어가 실제 C 코드를 쓰면
`cards[i]` 읽기와 쓰기가 하나씩 수민의 실행 기록 이벤트가 되어, 나무 탁자 위의
3D 폭탄에서 한 단계씩 재생됩니다. 레벨 1-6은 납작한 폭탄을, 큐브 폭탄은 면마다
하나씩 더 어려운 모듈 다섯 개를 씁니다 (`docs/BOMB.md` 참고).

C11과 raylib(5.5 또는 6.0)로 만들었고, 1920 x 1200 화면에 맞춰 설계했습니다.

### 실행

**macOS (수민의 환경):** 처음 한 번 `brew install raylib pkg-config`를 실행한
뒤 `run.command`를 더블 클릭하세요. 또는 터미널에서:

```sh
make
./build/codebomb
```

**Windows:** <https://raysan5.itch.io/raylib>에서 raylib를 설치하고(기본 폴더
`C:\raylib`), `run_windows.bat`를 더블 클릭하세요. 또는
`C:\raylib\w64devkit\w64devkit.exe`를 열고 이 폴더로 이동해 `make run`을
실행하세요. Visual Studio에서는 이 폴더를 열면 `CMakeLists.txt`를 사용합니다.

**CMake가 있는 모든 플랫폼:**

```sh
cmake -B build-cmake
cmake --build build-cmake --config Release
```

1920 x 1200 이상의 모니터에서는 테두리 없는 전체 화면에서 정확히 1:1로
시작하므로 픽셀 폰트가 선명합니다. 맥북처럼 더 작은 화면에서는 수민의 버전처럼
창 크기에 맞춰 축소됩니다. F11로 전체 화면과 창을 전환합니다.

### 화면

| 탭 | 내용 | 출처 |
|---|---|---|
| Missions | 미션 지도 (게임이 여기서 시작): 수민의 8개 챕터와 40개 수업, 그리고 폭탄 해체 줄 | `screen_missions.c` + 수민의 `roadmap_SOO.h` |
| Missions → 01-40 | 수민의 수업, 수민의 화면을 1.5배로 그림 | `soo_lessons.c` + 수민의 `main_SOO.c`와 `stage*_SOO` 파일 |
| Bomb → Levels 1-6 | 시간 제한 없는 연습: 납작한 폭탄에서 수민의 6개 레벨을 C로 풀면 폭탄이 코드를 재생. Play randomised (F9)는 같은 규칙의 새 무작위 폭탄에서 실행, Hint는 힌트 세 개 | 에디터 + `array_bridge.c` + `bomb3d.c` + 수민의 엔진 |
| Bomb → Cube bomb | 시간 제한 도전: START를 누르고 폭탄의 카운트다운 10:00 안에 모듈 5개를 해체. 잘못된 실행이나 시간 초과는 폭발, 그 뒤 RESTART | `screen_arrays.c` + `bomb3d.c` |
| Bomb → Predict / Explore / Find the bug | 수민의 모드, 로직 그대로 (레벨 1-6만) | `main_SOO.c`에서 옮김 |
| Mailroom | 3D 로봇이 있는 레벨: 1 Mailroom, 2 Night Shift, 4 Weigh Station, 6 Head Count, 7 Biggest Box, 9 Countdown (`docs/MAILROOM.md`의 나머지는 흐리게 표시) | 에디터 + VM + `levels.c` |
| Help (모든 화면, 모든 챕터 카드) | 현장 매뉴얼: 개념마다 그림, 코드 예제, 팁이 있는 짧은 페이지. 처음에는 저절로 열림 | `help.c` |
| Exit | 게임 종료 (먼저 확인) | `main.c` |

### 조작

| 동작 | Windows | macOS |
|---|---|---|
| 코드 실행 / 일시정지 | F5 또는 Ctrl+Enter | Cmd+Enter (또는 fn+F5) |
| 한 단계 | F6 | fn+F6 |
| 이전 단계 (Bomb) / 명령 하나 (Mailroom) | F7 | fn+F7 |
| 초기화 | F8 | fn+F8 |
| 에디터에서 나가기 | Esc | Esc |
| 전체 화면 | F11 | fn+F11 |

Predict, Explore, Find the bug에서는 수민의 키가 그대로 동작합니다: Space,
방향키, R, 1-6, A/B/C, S. Write code에서는 이 키들을 꺼서 입력이 항상 코드로
들어갑니다.

큐브 폭탄: 폭탄을 끌어서 돌리고, 면(또는 모듈 버튼)을 클릭해 그 모듈을
작업합니다.

Write code 모드에서는 1D 레벨에서 `cards[i]`와 `N`, 2D 레벨에서
`grid[i][j]`와 `ROWS`, `COLS`를 쓸 수 있습니다. 카드 값은 0(SAFE)과
1(FAULT)입니다.

### 테스트

```sh
make test        # 377개 검사, raylib 불필요
make test-asan   # 같은 검사를 Address/UB 새니타이저로 (macOS/Linux)
```

연결 테스트는 수민의 6개 레벨과 큐브 모듈 5개를 모두 실제 C 코드로 풀고,
결과를 수민의 샘플과 비교합니다 (예: 레벨 2는 13회 연산, 레벨 6은 16회,
`cards[8]` 범위 오류). `./build/codebomb --selftest`는 큐브 폭탄 도전
규칙(START 전에는 잠김, 컴파일 오류는 안전, 풀지 못한 실행·스트라이크·00:00
도달은 폭발, 모듈 5개를 풀면 해체, 떠나면 도전 포기), 큐브 면 클릭, 수민의
40개 수업, 현장 매뉴얼의 모든 페이지를 검사합니다.

수민의 수업에서는 수민의 키가 그대로 동작합니다: A/B/C로 답하기, Space로
재생/일시정지, 방향키로 단계 이동, R로 다시 시작. **Missions**를 누르면
지도로 돌아갑니다.

### 파일

| 파일 | 역할 |
|---|---|
| `src/array_bridge.*` | VM을 수민의 `SooApplyEvent`에 연결 (`docs/INTEGRATION.md` 참고) |
| `src/screen_arrays.*` | 폭탄 해체 화면: 수민의 모드와 Write code |
| `src/bomb3d.*` | 3D 탁자와 폭탄 (납작한 폭탄, 큐브), 효과, 카메라 맞춤 |
| `src/bomb_modules.*` | 큐브 폭탄 모듈 5개 |
| `src/screen_missions.*` | 수민의 `roadmap_SOO.h`로 만든 미션 지도 |
| `src/soo_lessons.*` | 수민의 40개 수업을 수정 없이 1.5배로 실행 (`docs/INTEGRATION.md` 참고) |
| `src/help.*` | 현장 매뉴얼 페이지, 그림, Help 팝업 |
| `src/bomb_variants.*` | 같은 규칙의 무작위 폭탄과 힌트 |
| `src/screen_mailroom.*`, `src/scene_mailroom.c` | 3D 장면이 있는 레벨 1 |
| `src/ui.*` | 공용 테마, 버튼, 텍스트 (수민의 색상표와 위젯) |
| `src/main.c` | 창, 1920 x 1200 캔버스 배율, 탭 (Missions, Bomb, Mailroom) |
| `src/vm.*`, `lexer.*`, `parser.*`, `compiler.*` | C 부분 집합 컴파일러와 VM |
| `src/editor.*`, `src/editor_ui.*` | 코드 에디터 |
| `../array_module_SOO/` | 수민의 모듈. 수정하지 않고 그대로 사용 |

모든 글자 크기는 10의 배수(20, 30, 40)입니다. raylib 기본 폰트는 10픽셀 픽셀
폰트라서 이 크기들은 정확히 2배, 3배, 4배로 확대되어 글자가 선명합니다.

QA 스크린샷 (보고서용): `./build/codebomb --shot out.png arrays code 4 14 0`
(화면, 모드, 레벨, 단계 수, 변형). 자세한 내용은 `src/main.c` 맨 위 주석을
보세요.
