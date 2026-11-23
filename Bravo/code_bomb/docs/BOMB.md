# The 3D bomb / 3D 폭탄

## English

The left panel of the Bomb screen is a 3D scene: a bomb on a wooden table.
It is drawn by `src/bomb3d.c` from the game state the screen passes in; all
rules still come from Sumin's engine.

### Two kinds of bomb

| Levels | Bomb | What is on it |
|---|---|---|
| 1-6 (Sumin's levels) | Flat rectangular bomb | One challenge on the top face: the cards. A red display shows memory ops used / budget, a second display shows `i` and `j`, and a status light shows the result. |
| Cube bomb (harder) | 6-sided cube, like the VR bomb game | A different coding module on five faces; the top face holds the display and one light per module. |

### Cube bomb modules

Each module is an ordinary `SooLevel` (`src/bomb_modules.c`), so Sumin's
engine checks it the same way as her levels. Only the targets are new.

| Face | Module | Idea it teaches | Size | Budget |
|---|---|---|---|---|
| 1 front | Invert: flip every card | arithmetic on values (`1 - cards[i]`) | 6 | 12 |
| 2 right | Even slots: 1 at even indices, 0 at odd | `%` on the index | 6 | 12 |
| 3 back | Mirror: copy the left half onto the right | index arithmetic (`N - 1 - i`) | 6 | 12 |
| 4 left | Checkerboard: `grid[i][j] = (i + j) % 2` | 2D indices and `%` | 4 x 4 | 32 |
| 5 bottom | Border: outer ring 1, inside 0 | conditions with `||` | 4 x 4 | 32 |

Every module has an example solution, and the tests check that each one
passes, that no module starts already solved, and that typical wrong ideas
fail (for example Sumin's fault-repair sweep does not solve Invert).

### Playing the cube

- Drag on the bomb to turn it. Click a face (or a module button above the
  mission) to work on that module; the cube turns it toward you.
- Each module keeps its own code. Solved modules stay solved and their light
  turns green. When all five are green the display shows DEFUSED.

### Random bombs

Every bomb follows a rule ("every FAULT becomes SAFE", "copy the left half
onto the right half"). `src/bomb_variants.c` makes new bombs with the same
rule but different cards and a different size (`N`, or `ROWS` and `COLS`),
leaning on edge cases: all FAULT, nothing to fix, odd sizes for Mirror.

- **Run** always plays the bomb on screen, so it can be solved first.
- **Play randomised (F9)** compiles the code, makes a new random bomb,
  records the run and only then shows the bomb and plays it, so the code
  can't be changed to fit it. Every press gives a different bomb.
- The bomb panel shows "This bomb: done / not yet" and how many random
  bombs were defused. A practice level is complete (green dot) only after
  the bomb on screen and at least one random bomb are defused.
- The "You can use" line names the sizes as constants with the current
  value: `N (8 here)`. On a random bomb it shows that bomb's size.
- Editing the code or pressing Run returns to the bomb on screen.
- Level 3's right-to-left rule is checked by Sumin's engine only for her
  own level, so random copies get the same check, with the same message,
  through `array_apply()` in `array_bridge.c`.

`tests/test_variants.c` checks every kind: 300 random bombs fit the engine
and change size, the working answer defuses all of them, and code that
sets the shown bomb's cards one by one defuses the shown bomb but fails
within 10 random bombs. (Level 1 is the exception: `cards[2] = 0;` is the
rule itself.)

A random bomb must never be stricter than the bomb on screen. Each kind's
rule is one function, `bomb_rule_goal()`, and the tests check that it gives
exactly the shown bomb's goal, and that other correct ways of writing each
answer (a sweep with `if`, writing 0 everywhere, a `while` loop, `!cards[i]`,
two-pointer Mirror ...) defuse 300 random bombs. This caught a bug in level
1: random bombs had extra FAULT cards that had to stay, so a correct sweep
failed. Now, as on screen, `cards[2]` is the only FAULT and every card must
end SAFE.

When a random bomb ends with the wrong cards, the message names the first
wrong one ("Not defused: cards[4] is 1 but its target is 0") instead of
Sumin's general "Incomplete".

### Hints

The **Hint** button on the MISSION line opens a card with three hints per
bomb, revealed one at a time, from a gentle nudge to almost the answer.
The card opens below the mission so both can be read. The Mirror mission
was rewritten with the pairing spelled out and an example:
"1 1 0 | 0 0 1 becomes 1 1 0 | 0 1 1".

### The timed challenge

Levels 1-6 are untimed practice. The Cube bomb is the challenge:

1. **Ready:** the editor and Run are locked. The player can read all five
   missions (module buttons on the right), then presses **START** on the
   bomb panel.
2. **Running:** a 10:00 countdown runs on the cube's top display (and large
   in the panel's corner). Under 30 seconds the display blinks.
3. **Boom:** any run that ends in a strike, or ends with its module
   unsolved, sets the bomb off after a short pause on the strike, and so
   does the clock reaching 00:00. A module that is solved on screen also
   has to defuse 4 hidden random bombs; if it fails one, the bomb explodes
   and the card names the random bomb's size. Compile errors never explode,
   and Play randomised is safe practice.
4. **Restart:** when the explosion has played, a card shows why it exploded
   and a **RESTART** button. Restart re-arms all five modules and starts the
   clock again. The player's code is kept.
5. **Defused:** solving all five modules stops the clock and shows the time
   left, with **Play again**.

Leaving the cube (another level or screen) abandons a running attempt.
Predict, Explore and Find the bug are hidden on the cube, and the Cube bomb
button works from any mode.

### Effects

- The card being read or written lifts and gets a glowing frame.
- A write flashes the card and throws sparks.
- A failure (out of bounds, over budget, wrong result) is a strike: the bomb
  shakes, the screen flashes red and the status light turns red.
- Success turns the lights green with a soft green flash.
- The explosion: a white flash, an expanding fireball and shock ring, fire
  and smoke particles, pieces of the casing that fly and bounce on the table,
  then a scorch mark with burnt wreckage. The view is copied without blending
  (`ui_draw_view`), so translucent flashes and smoke don't let the
  background show through.

### Camera

The camera is fitted automatically: it moves as close as it can while every
corner of the bomb stays inside the free area of the panel (between the title
at the top and the log at the bottom), then centres the bomb there. Wide
levels are limited by width, tall ones by height, so the bomb always fills
the panel.

### No flicker (z-fighting)

Two surfaces at the same depth flicker as the camera moves, because the
depth buffer can't decide which is in front. The dark grey recess under the
cards used to sit at exactly the same height as the cube's face plate. Now
every face is a stack of layers, each `LAYER` (0.03) above the one below:
face plate, then recess, then the selection frame and the cards. The 3D view
also sets the camera's near clip plane to 1.0 instead of raylib's default
(0.01 in 5.5, 0.05 in 6.0), which gives the depth buffer far more precision
at the camera's distance; the old planes are restored afterwards.

### Readability rules kept from Sumin's design

Cards show their value, index and target as text, never by colour alone. All
text on the bomb is drawn upright at 20, 30 or 40 px (multiples of 10, so
the pixel font stays sharp). On the cube only the selected module carries
text, which keeps the other faces uncluttered.

### Checking it

- `make test`: 451 checks, including all cube modules.
- `./build/codebomb --selftest`: checks the challenge rules above
  (including card-by-card code exploding on hidden random bombs, and a
  failed Play randomised being safe), the practice rules (the shown bomb
  alone doesn't complete a level, Play randomised catches card-by-card
  code, each press gives a new bomb), then
  turns the cube to each module and checks that clicking the centre of the
  face picks that module.
- `--shot boom.png arrays code 7 0 40` (explosion), `... 0 42` (restart
  card), `... 0 43` (last 24 seconds), `... 0 0` (START card).
- `./build/codebomb --shot out.png arrays code 7 6 22`: a screenshot of the
  cube with two modules solved (see `src/main.c` for the options).

## 한국어

Bomb 화면의 왼쪽 패널은 3D 장면입니다: 나무 탁자 위의 폭탄. `src/bomb3d.c`가
화면이 넘겨준 게임 상태를 그대로 그리며, 모든 규칙은 여전히 수민의 엔진에서
나옵니다.

### 두 종류의 폭탄

| 레벨 | 폭탄 | 폭탄 위의 내용 |
|---|---|---|
| 1-6 (수민의 레벨) | 납작한 직사각형 폭탄 | 윗면에 도전 과제 하나: 카드. 빨간 표시창은 사용한 메모리 연산 / 예산, 두 번째 표시창은 `i`와 `j`, 상태 불빛은 결과를 보여 줍니다. |
| 큐브 폭탄 (더 어려움) | VR 폭탄 게임 같은 6면 큐브 | 다섯 면에 서로 다른 코딩 모듈, 윗면에는 표시창과 모듈별 불빛. |

### 큐브 폭탄 모듈

각 모듈은 보통 `SooLevel`(`src/bomb_modules.c`)이므로 수민의 엔진이 수민의
레벨과 똑같이 검사합니다. 새로운 것은 목표뿐입니다.

| 면 | 모듈 | 가르치는 개념 | 크기 | 예산 |
|---|---|---|---|---|
| 1 앞 | Invert: 모든 카드 뒤집기 | 값에 대한 산술 (`1 - cards[i]`) | 6 | 12 |
| 2 오른쪽 | Even slots: 짝수 인덱스는 1, 홀수는 0 | 인덱스에 `%` | 6 | 12 |
| 3 뒤 | Mirror: 왼쪽 절반을 오른쪽에 복사 | 인덱스 계산 (`N - 1 - i`) | 6 | 12 |
| 4 왼쪽 | Checkerboard: `grid[i][j] = (i + j) % 2` | 2D 인덱스와 `%` | 4 x 4 | 32 |
| 5 아래 | Border: 바깥 테두리는 1, 안쪽은 0 | `||` 조건 | 4 x 4 | 32 |

모든 모듈에 예시 정답이 있고, 테스트는 각 정답이 통과하는지, 처음부터 풀린
모듈이 없는지, 흔한 잘못된 생각이 실패하는지 확인합니다 (예: 수민의 결함
고치기 반복으로는 Invert를 풀 수 없음).

### 큐브 다루기

- 폭탄을 끌어서 돌립니다. 면을 클릭하거나 (미션 위의 모듈 버튼을 눌러) 그
  모듈을 작업하면 큐브가 그 면을 플레이어 쪽으로 돌립니다.
- 모듈마다 코드가 따로 저장됩니다. 푼 모듈은 풀린 채로 남고 불빛이 초록색이
  됩니다. 다섯 개가 모두 초록색이면 표시창에 DEFUSED가 나옵니다.

### 무작위 폭탄

모든 폭탄에는 규칙이 있습니다 ("모든 FAULT를 SAFE로", "왼쪽 절반을 오른쪽에
복사"). `src/bomb_variants.c`는 같은 규칙에 카드와 크기(`N`, 또는 `ROWS`와
`COLS`)가 다른 새 폭탄을 만들며, 경계 상황을 자주 만듭니다: 모두 FAULT,
고칠 것 없음, Mirror의 홀수 크기.

- **Run**은 항상 화면의 폭탄을 재생하므로 먼저 그것을 풀 수 있습니다.
- **Play randomised (F9)**는 코드를 컴파일하고, 새 무작위 폭탄을 만들고,
  실행을 기록한 뒤에야 폭탄을 보여 주고 재생하므로, 코드를 폭탄에 맞춰 바꿀
  수 없습니다. 누를 때마다 다른 폭탄이 나옵니다.
- 폭탄 패널에 "This bomb: done / not yet"과 해체한 무작위 폭탄 수가
  표시됩니다. 연습 레벨은 화면의 폭탄과 무작위 폭탄을 하나 이상 해체해야
  완료됩니다 (초록 점).
- "You can use" 줄은 크기를 현재 값과 함께 상수로 보여 줍니다: `N (8 here)`.
  무작위 폭탄에서는 그 폭탄의 크기를 보여 줍니다.
- 코드를 고치거나 Run을 누르면 화면의 폭탄으로 돌아갑니다.
- 수민의 엔진은 레벨 3의 오른쪽에서 왼쪽 규칙을 자신의 레벨에만 검사하므로,
  무작위 복사본은 `array_bridge.c`의 `array_apply()`로 같은 메시지의 같은
  검사를 받습니다.

`tests/test_variants.c`는 모든 종류를 검사합니다: 무작위 폭탄 300개가 엔진에
맞고 크기가 바뀌며, 정답은 모두 해체하고, 화면 폭탄의 카드를 하나씩 바꾸는
코드는 화면 폭탄은 해체하지만 무작위 폭탄 10개 안에 실패합니다. (레벨 1은
예외: `cards[2] = 0;`이 바로 규칙입니다.)

무작위 폭탄은 화면 폭탄보다 엄격하면 안 됩니다. 종류마다 규칙은 함수 하나
`bomb_rule_goal()`이며, 테스트는 그것이 화면 폭탄의 목표와 정확히 같은지,
그리고 각 답을 쓰는 다른 올바른 방법 (`if`가 있는 순회, 모두 0 쓰기, `while`
반복, `!cards[i]`, 두 포인터 Mirror ...)이 무작위 폭탄 300개를 해체하는지
확인합니다. 이 검사로 레벨 1의 버그를 찾았습니다: 무작위 폭탄에 그대로
남아야 하는 FAULT 카드가 더 있어서 올바른 순회가 실패했습니다. 이제 화면과
같이 `cards[2]`만 FAULT이고 모든 카드가 SAFE로 끝나야 합니다.

무작위 폭탄이 틀린 카드로 끝나면, 메시지는 수민의 일반적인 "Incomplete" 대신
처음 틀린 카드를 알려 줍니다 ("Not defused: cards[4] is 1 but its target is
0").

### 힌트

MISSION 줄의 **Hint** 버튼은 폭탄마다 힌트 세 개가 있는 카드를 엽니다.
가벼운 힌트부터 거의 정답까지 하나씩 보여 줍니다. 카드는 미션 아래에 열려
둘 다 읽을 수 있습니다. Mirror 미션은 짝을 맞추는 방법과 예를 넣어 다시
썼습니다: "1 1 0 | 0 0 1 becomes 1 1 0 | 0 1 1".

### 시간 제한 도전

레벨 1-6은 시간 제한 없는 연습입니다. 큐브 폭탄은 도전입니다:

1. **준비:** 에디터와 Run이 잠깁니다. 오른쪽 모듈 버튼으로 다섯 미션을 모두
   읽은 뒤 폭탄 패널의 **START**를 누릅니다.
2. **진행:** 큐브 윗면 표시창(그리고 패널 모서리에 크게)에서 10:00
   카운트다운이 진행됩니다. 30초 미만이면 표시창이 깜박입니다.
3. **폭발:** 스트라이크로 끝나거나 모듈을 풀지 못한 채 끝난 실행은 스트라이크를
   잠깐 보여 준 뒤 폭탄을 터뜨립니다. 시계가 00:00에 닿아도 터집니다.
   화면에서 풀린 모듈은 숨은 무작위 폭탄 4개도 해체해야 하며, 하나라도
   실패하면 폭탄이 터지고 카드에 그 무작위 폭탄의 크기가 나옵니다. 컴파일
   오류는 터지지 않고, Play randomised는 안전한 연습입니다.
4. **재시작:** 폭발이 끝나면 터진 이유와 **RESTART** 버튼이 있는 카드가
   나옵니다. 재시작하면 다섯 모듈이 다시 무장되고 시계가 다시 갑니다.
   플레이어의 코드는 그대로 남습니다.
5. **해체:** 다섯 모듈을 모두 풀면 시계가 멈추고 남은 시간과 **Play again**이
   나옵니다.

큐브를 떠나면 (다른 레벨이나 화면) 진행 중인 도전은 포기됩니다. 큐브에서는
Predict, Explore, Find the bug을 숨기고, Cube bomb 버튼은 어느 모드에서나
동작합니다.

### 효과

- 읽거나 쓰는 카드는 올라오고 빛나는 테두리가 생깁니다.
- 쓰기는 카드를 번쩍이고 불꽃을 튀깁니다.
- 실패(범위 밖, 예산 초과, 잘못된 결과)는 스트라이크입니다: 폭탄이 흔들리고
  화면이 붉게 번쩍이며 상태 불빛이 빨간색이 됩니다.
- 성공하면 불빛이 초록색이 되고 화면이 부드럽게 초록색으로 빛납니다.
- 폭발: 하얀 섬광, 커지는 불덩이와 충격파 고리, 불과 연기 입자, 날아가 탁자에서
  튀는 몸체 조각, 그 뒤 그을음 자국과 탄 잔해. 화면은 섞지 않고 복사하므로
  (`ui_draw_view`) 반투명 섬광과 연기 사이로 배경이 비치지 않습니다.

### 카메라

카메라는 자동으로 맞춰집니다: 폭탄의 모든 모서리가 패널의 빈 영역(위쪽 제목과
아래쪽 기록 사이) 안에 들어오는 가장 가까운 위치로 움직인 뒤, 폭탄을 그 영역
가운데에 놓습니다. 넓은 레벨은 너비, 높은 레벨은 높이에 맞춰지므로 폭탄이 항상
패널을 채웁니다.

### 깜빡임 없음 (z-fighting)

같은 깊이에 있는 두 면은 깊이 버퍼가 어느 쪽이 앞인지 정하지 못해 카메라가
움직일 때 깜빡입니다. 카드 아래의 짙은 회색 홈이 큐브 면 판과 정확히 같은
높이에 있었습니다. 이제 각 면은 층으로 쌓이며, 층마다 `LAYER`(0.03)만큼
높습니다: 면 판, 홈, 그리고 선택 테두리와 카드. 3D 화면은 카메라의 근거리 클립
평면도 raylib 기본값(5.5는 0.01, 6.0은 0.05) 대신 1.0으로 설정하여 카메라 거리에서
깊이 정밀도를 크게 높이고, 그린 뒤에는 원래 값으로 되돌립니다.

### 수민의 설계에서 지킨 가독성 규칙

카드는 값, 인덱스, 목표를 글자로 보여 주며 색에만 의존하지 않습니다. 폭탄
위의 글자는 모두 바로 선 채 20, 30, 40픽셀(10의 배수, 픽셀 폰트가 선명함)로
그립니다. 큐브에서는 선택한 모듈에만 글자를 넣어 다른 면을 깔끔하게
유지합니다.

### 확인 방법

- `make test`: 큐브 모듈을 포함한 451개 검사.
- `./build/codebomb --selftest`: 위의 도전 규칙 (숨은 무작위 폭탄에서 카드별
  코드가 터지는 것, 실패한 Play randomised가 안전한 것 포함)과 연습 규칙
  (화면 폭탄만으로는 완료 안 됨, Play randomised가 카드별 코드를 잡아냄,
  누를 때마다 새 폭탄)을 검사한 뒤, 큐브를 모듈마다
  돌리고 면 중심을 클릭하면 그 모듈이 골라지는지 확인합니다.
- `--shot boom.png arrays code 7 0 40` (폭발), `... 0 42` (재시작 카드),
  `... 0 43` (마지막 24초), `... 0 0` (START 카드).
- `./build/codebomb --shot out.png arrays code 7 6 22`: 모듈 두 개를 푼 큐브의
  스크린샷 (옵션은 `src/main.c` 참고).
