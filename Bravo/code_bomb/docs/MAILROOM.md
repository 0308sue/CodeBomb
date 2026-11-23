# Mailroom: cleanup and expansion plan / 우편실: 정리와 확장 계획

## English

### Status

| Phase | State |
|---|---|
| 1 | Partly done: the level list (`LEVEL_LIST` in `levels.c`), the level row on the Mailroom screen (planned levels dimmed, a green dot after a passed Submit), saved code per level and a Help topic per level. Still to do: stars and progress on the mission map. |
| 2 | **Done:** 2 Night Shift, 4 Weigh Station, 6 Head Count, 7 Biggest Box, 9 Countdown. |
| 3-5 | Not started. |

Each phase 2 level has a starter, a one-line goal, a two-page Help topic
with a diagram of what goes in and comes out, and two programs in its
Examples menu that show typical mistakes (never the answer). A reference
answer lives in `levels.c` for the tests and screenshots only.

Until level 3 (Damaged Goods) exists, level 7's Help topic also teaches
`if` and `else`.

`tests/test_levels.c` checks every built level: the starter compiles, the
reference passes 50 random inboxes and meets the level's par (par is the
reference's size and speed on the game's 5 Submit tests), every example
fails Submit, and every inbox and outbox holds 1-9 parcels so they fit on
the belts.

| Level | Inbox | Outbox | Par size / speed |
|---|---|---|---|
| 2 Night Shift | 2-7 parcels | the same | 2 / 28 |
| 4 Weigh Station | 1-3 pairs | each pair's sum | 4 / 23 |
| 6 Head Count | 2-3 groups of 0-2 parcels, each ended by 0 | each group's count | 5 / 59 |
| 7 Biggest Box | 1-3 groups of 3 | each group's biggest | 7 / 92 |
| 9 Countdown | 1-2 parcels of 1-4 | n, n-1, ..., 1 for each | 4 / 61 |

### What changed in the first cleanup

The Mailroom screen had every explanation on screen at once. It now keeps
only what the player needs while coding:

- **Left panel:** the level title, a one-line goal, the two functions, the
  editor and the message box. The paragraph about parcels and the controls
  line moved into the Help screen (three pages: Your job, Statements and
  calls, Run step and submit). It opens by itself on the first visit.
- **Examples:** the row of five buttons became **Starter code** and an
  **Examples** menu. Each example has one line saying what it shows, for
  example "Bug: take() from an empty inbox".
- **Inspector:** it opens in a **Simple** view, with the parcels drawn as
  chips, the program status and the variables. Bytecode and the operand
  stack moved to a **Machine** view, used with Step op.
- **Removed:** the duplicated level subtitle, the "Speed 1x" label in the 3D
  view (the speed buttons already show it) and the row of shortcut keys.

### Design rules for new levels

1. **One new C idea per level.** The level's Help page teaches it before
   the player starts, as in the field manual for the lessons.
2. **Submit uses random inboxes.** A level is solved only by code that works
   for every input, not just the one on screen. This already works through
   `LevelDef.generate` and `solve`.
3. **Two optional goals:** size (statements) and speed (average
   instructions). `par_size` and `par_speed` already exist in `LevelDef`;
   they become stars.
4. **The factory floor grows.** Each level adds one machine to the 3D scene,
   so the scene shows the new idea: a shredder for `if`, a second belt for
   `%`, a shelf of numbered slots for arrays.
5. **It leads into the bombs.** The last levels introduce arrays and
   indices, which are what the bomb defusal levels need.

### Proposed levels

| # | Name | Puzzle | C idea | New machine or native | Compiler work |
|---|---|---|---|---|---|
| 1 | Mailroom (done) | Move 3 parcels in order | statements, calls | `take()`, `put(n)` | none |
| 2 | Night Shift | Any number of parcels | `while`, loop condition | `has_input()` (already there) | none |
| 3 | Damaged Goods | Parcels below 0 go to the shredder | `if` / `else`, `<` | `shred(n)`, shredder | none |
| 4 | Weigh Station | Parcels come in pairs: deliver their sum | variables, `+` `-` | scale display | none |
| 5 | Two Belts | Even to the left belt, odd to the right | `%`, `==` | `put_left(n)`, `put_right(n)`, second belt | none |
| 6 | Head Count | Count parcels until a 0, deliver the count | counters, sentinel loop | counter display | none |
| 7 | Biggest Box | Deliver the largest parcel of each group of 3 | comparison, "best so far" | none | none |
| 8 | Stamp Press | Deliver every parcel times 3 plus 1, reusing a helper | defining a function, parameters, `return` | stamp press | none (functions already compile) |
| 9 | Countdown | Parcel n becomes n, n-1, ..., 1 | `for` loops | none | none |
| 10 | Back Shelf | Deliver the parcels in reverse order | arrays, indices | shelf with numbered slots | local `int shelf[8]` |
| 11 | Holiday Rush (boss) | Mixed rules from levels 3-9 | everything so far | all machines | none |

Levels 2-9 need no compiler changes: the compiler already handles `if`,
`else`, `while`, `for`, `break`, `continue`, `return` and functions. Level 10
needs local array declarations. Today arrays exist only as the bomb's
`cards` and `grid`, through the hidden natives.

### Other ideas

- **Level chips** in the empty row under the header, like the Bomb screen's
  level row, with stars for size and speed.
- **Progress on the mission map:** the Tutorial button becomes "Mailroom 4 /
  11".
- **Ghost run:** after a successful Submit, replay the fastest test so the
  player can see why it was fast.
- **Hint ladder:** a "Stuck?" button that shows a hint first, then the shape
  of the loop, and never the full answer.
- **Mistake gallery:** each level's Examples menu holds one typical wrong
  program, such as an off-by-one or a forgotten `else`, to run and explain.

### Implementation plan

| Phase | Work | Size |
|---|---|---|
| 1 | Level list in `levels.c`, level chips and stars on the Mailroom screen, per-level Help pages (one `HelpTopic` per level, or pages chosen by level) | 1-2 days |
| 2 | Levels 2, 4, 6, 7 and 9: only new `generate`, `solve`, Help text and examples; the scene is unchanged | 1-2 days |
| 3 | Scene machines: shredder (3), second belt (5), stamp press (8) as new `LevelEvent` types and animations in `scene_mailroom.c` | 2-3 days |
| 4 | Local arrays in the compiler and VM (`int shelf[8]`, bounds checks with the bomb's error messages), shelf scene, level 10 | 2-3 days |
| 5 | Boss level, mission map progress, hint ladder | 1-2 days |

Each level gets the same tests as level 1: the example solution passes
Submit, every example from the Examples menu fails the way its description
says, and `--shot mailroom - N` screenshots are checked visually.

## 한국어

### 진행 상황

| 단계 | 상태 |
|---|---|
| 1 | 일부 완료: 레벨 목록 (`levels.c`의 `LEVEL_LIST`), 우편실 화면의 레벨 줄 (계획된 레벨은 흐리게, Submit을 통과하면 초록 점), 레벨별 코드 저장, 레벨별 도움말 주제. 남은 것: 별, 미션 지도의 진행 상황. |
| 2 | **완료:** 2 Night Shift, 4 Weigh Station, 6 Head Count, 7 Biggest Box, 9 Countdown. |
| 3-5 | 시작 안 함. |

2단계 레벨마다 시작 코드, 한 줄 목표, 들어오고 나가는 것을 보여 주는 그림이
있는 두 쪽짜리 도움말, 흔한 실수를 보여 주는 Examples 메뉴의 프로그램 두
개(정답은 아님)가 있습니다. 정답은 테스트와 스크린샷 전용으로 `levels.c`에
있습니다.

레벨 3 (Damaged Goods)이 생기기 전까지는 레벨 7의 도움말이 `if`와 `else`도
가르칩니다.

`tests/test_levels.c`는 만들어진 모든 레벨을 검사합니다: 시작 코드가
컴파일되고, 정답이 무작위 입력함 50개를 통과하며 레벨의 기준(par)을 만족하고
(기준은 게임의 Submit 테스트 5개에서 정답의 크기와 속도), 모든 예제는
Submit에 실패하며, 모든 입력함과 출력함이 벨트에 들어가도록 소포 1-9개를
담습니다.

| 레벨 | 입력함 | 출력함 | 기준 크기 / 속도 |
|---|---|---|---|
| 2 Night Shift | 소포 2-7개 | 그대로 | 2 / 28 |
| 4 Weigh Station | 1-3쌍 | 각 쌍의 합 | 4 / 23 |
| 6 Head Count | 소포 0-2개인 묶음 2-3개, 각각 0으로 끝남 | 각 묶음의 개수 | 5 / 59 |
| 7 Biggest Box | 3개씩 1-3묶음 | 각 묶음의 최댓값 | 7 / 92 |
| 9 Countdown | 1-4인 소포 1-2개 | 각각 n, n-1, ..., 1 | 4 / 61 |

### 첫 정리에서 바뀐 것

우편실 화면에는 모든 설명이 한꺼번에 나와 있었습니다. 이제 코딩하는 동안
필요한 것만 남겼습니다:

- **왼쪽 패널:** 레벨 제목, 한 줄 목표, 두 함수, 에디터, 메시지 상자. 소포에
  대한 문단과 단축키 줄은 도움말 화면으로 옮겼습니다 (세 쪽: Your job,
  Statements and calls, Run step and submit). 처음 들어오면 저절로 열립니다.
- **예제:** 다섯 개 버튼 줄은 **Starter code**와 **Examples** 메뉴가
  되었습니다. 각 예제에는 무엇을 보여 주는지 한 줄 설명이 있습니다 (예:
  "Bug: take() from an empty inbox").
- **인스펙터:** **Simple** 보기로 시작합니다. 소포는 칩으로 그리고,
  프로그램 상태와 변수를 보여 줍니다. 바이트코드와 피연산자 스택은
  Step op와 함께 쓰는 **Machine** 보기로 옮겼습니다.
- **삭제:** 중복된 레벨 부제목, 3D 화면의 "Speed 1x" 글자 (속도 버튼이 이미
  보여 줌), 단축키 줄.

### 새 레벨의 설계 규칙

1. **레벨마다 새로운 C 개념 하나.** 시작하기 전에 레벨의 도움말 쪽이 그
   개념을 가르칩니다 (수업의 현장 매뉴얼처럼).
2. **Submit은 무작위 입력함을 씁니다.** 화면의 입력뿐 아니라 모든 입력에서
   동작하는 코드만 통과합니다. `LevelDef.generate`와 `solve`로 이미
   동작합니다.
3. **선택 목표 두 가지:** 크기(문장 수)와 속도(평균 명령 수).
   `LevelDef`에 `par_size`와 `par_speed`가 이미 있으며, 이것이 별이 됩니다.
4. **공장 바닥이 커집니다.** 레벨마다 3D 장면에 기계 하나를 더해 새 개념을
   보여 줍니다: `if`에는 파쇄기, `%`에는 두 번째 벨트, 배열에는 번호 붙은
   선반.
5. **폭탄으로 이어집니다.** 마지막 레벨들은 폭탄 해체에 필요한 배열과
   인덱스를 소개합니다.

### 제안 레벨

| # | 이름 | 퍼즐 | C 개념 | 새 기계 또는 네이티브 | 컴파일러 작업 |
|---|---|---|---|---|---|
| 1 | Mailroom (완료) | 소포 3개를 순서대로 옮기기 | 문장, 함수 호출 | `take()`, `put(n)` | 없음 |
| 2 | Night Shift | 소포 개수가 정해지지 않음 | `while`, 반복 조건 | `has_input()` (이미 있음) | 없음 |
| 3 | Damaged Goods | 0보다 작은 소포는 파쇄기로 | `if` / `else`, `<` | `shred(n)`, 파쇄기 | 없음 |
| 4 | Weigh Station | 소포가 두 개씩 옴: 합을 배달 | 변수, `+` `-` | 저울 표시창 | 없음 |
| 5 | Two Belts | 짝수는 왼쪽, 홀수는 오른쪽 벨트 | `%`, `==` | `put_left(n)`, `put_right(n)`, 두 번째 벨트 | 없음 |
| 6 | Head Count | 0이 나올 때까지 세고 개수를 배달 | 카운터, 끝 표시 반복 | 카운터 표시창 | 없음 |
| 7 | Biggest Box | 세 개씩 묶음에서 가장 큰 소포 배달 | 비교, "지금까지 최고" | 없음 | 없음 |
| 8 | Stamp Press | 모든 소포를 3배 더하기 1로, 도우미 함수 재사용 | 함수 정의, 매개변수, `return` | 도장 기계 | 없음 (함수는 이미 컴파일됨) |
| 9 | Countdown | 소포 n을 n, n-1, ..., 1로 | `for` 반복 | 없음 | 없음 |
| 10 | Back Shelf | 소포를 거꾸로 배달 | 배열, 인덱스 | 번호 붙은 선반 | 지역 `int shelf[8]` |
| 11 | Holiday Rush (보스) | 레벨 3-9의 규칙이 섞임 | 지금까지의 모든 것 | 모든 기계 | 없음 |

레벨 2-9는 컴파일러를 바꿀 필요가 없습니다: 컴파일러는 이미 `if`, `else`,
`while`, `for`, `break`, `continue`, `return`, 함수를 처리합니다. 레벨 10에는
지역 배열 선언이 필요합니다. 지금 배열은 숨겨진 네이티브를 통한 폭탄의
`cards`와 `grid`뿐입니다.

### 그 밖의 아이디어

- 헤더 아래 빈 줄에 Bomb 화면처럼 **레벨 버튼**을 두고, 크기와 속도 별을
  표시합니다.
- **미션 지도의 진행 상황:** Tutorial 버튼이 "Mailroom 4 / 11"이 됩니다.
- **고스트 실행:** Submit에 성공하면 가장 빠른 테스트를 다시 재생해 왜
  빨랐는지 보여 줍니다.
- **힌트 사다리:** "Stuck?" 버튼은 먼저 힌트를, 그다음 반복문의 모양을
  보여 주고, 정답 전체는 보여 주지 않습니다.
- **실수 모음:** 레벨마다 Examples 메뉴에 흔한 잘못된 프로그램 하나(하나
  차이 오류, 빠뜨린 `else` 등)를 넣어 실행하고 설명합니다.

### 구현 계획

| 단계 | 작업 | 규모 |
|---|---|---|
| 1 | `levels.c`의 레벨 목록, 우편실 화면의 레벨 버튼과 별, 레벨별 도움말 쪽 (레벨마다 `HelpTopic` 하나 또는 레벨에 따라 고른 쪽) | 1-2일 |
| 2 | 레벨 2, 4, 6, 7, 9: 새 `generate`, `solve`, 도움말 글, 예제만 필요하고 장면은 그대로 | 1-2일 |
| 3 | 장면 기계: 파쇄기(3), 두 번째 벨트(5), 도장 기계(8)를 새 `LevelEvent` 종류와 `scene_mailroom.c`의 애니메이션으로 | 2-3일 |
| 4 | 컴파일러와 VM의 지역 배열 (`int shelf[8]`, 폭탄과 같은 오류 메시지의 범위 검사), 선반 장면, 레벨 10 | 2-3일 |
| 5 | 보스 레벨, 미션 지도 진행 상황, 힌트 사다리 | 1-2일 |

레벨마다 레벨 1과 같은 테스트를 둡니다: 예시 정답이 Submit을 통과하고,
Examples 메뉴의 모든 예제가 설명대로 실패하며, `--shot mailroom - N`
스크린샷을 눈으로 확인합니다.
