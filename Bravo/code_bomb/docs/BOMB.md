# The 3D bomb / 3D 폭탄

## English

The left panel of the Arrays screen is a 3D scene: a bomb on a wooden table.
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

### Effects

- The card being read or written lifts and gets a glowing frame.
- A write flashes the card and throws sparks.
- A failure (out of bounds, over budget, wrong result) is a strike: the bomb
  shakes, the screen flashes red and the status light turns red.
- Success turns the lights green with a soft green flash.

### Camera

The camera is fitted automatically: it moves as close as it can while every
corner of the bomb stays inside the free area of the panel (between the title
at the top and the log at the bottom), then centres the bomb there. Wide
levels are limited by width, tall ones by height, so the bomb always fills
the panel.

### Readability rules kept from Sumin's design

Cards show their value, index and target as text, never by colour alone. All
text on the bomb is drawn upright at 20, 30 or 40 px (multiples of 10, so
the pixel font stays sharp). On the cube only the selected module carries
text, which keeps the other faces uncluttered.

### Checking it

- `make test`: 180 checks, including all cube modules.
- `./build/codebomb --selftest`: turns the cube to each module and checks
  that clicking the centre of the face picks that module.
- `./build/codebomb --shot out.png arrays code 7 6 22`: a screenshot of the
  cube with two modules solved (see `src/main.c` for the options).

## 한국어

배열 화면의 왼쪽 패널은 3D 장면입니다: 나무 탁자 위의 폭탄. `src/bomb3d.c`가
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

### 효과

- 읽거나 쓰는 카드는 올라오고 빛나는 테두리가 생깁니다.
- 쓰기는 카드를 번쩍이고 불꽃을 튀깁니다.
- 실패(범위 밖, 예산 초과, 잘못된 결과)는 스트라이크입니다: 폭탄이 흔들리고
  화면이 붉게 번쩍이며 상태 불빛이 빨간색이 됩니다.
- 성공하면 불빛이 초록색이 되고 화면이 부드럽게 초록색으로 빛납니다.

### 카메라

카메라는 자동으로 맞춰집니다: 폭탄의 모든 모서리가 패널의 빈 영역(위쪽 제목과
아래쪽 기록 사이) 안에 들어오는 가장 가까운 위치로 움직인 뒤, 폭탄을 그 영역
가운데에 놓습니다. 넓은 레벨은 너비, 높은 레벨은 높이에 맞춰지므로 폭탄이 항상
패널을 채웁니다.

### 수민의 설계에서 지킨 가독성 규칙

카드는 값, 인덱스, 목표를 글자로 보여 주며 색에만 의존하지 않습니다. 폭탄
위의 글자는 모두 바로 선 채 20, 30, 40픽셀(10의 배수, 픽셀 폰트가 선명함)로
그립니다. 큐브에서는 선택한 모듈에만 글자를 넣어 다른 면을 깔끔하게
유지합니다.

### 확인 방법

- `make test`: 큐브 모듈을 포함한 180개 검사.
- `./build/codebomb --selftest`: 큐브를 모듈마다 돌리고, 면 중심을 클릭하면 그
  모듈이 골라지는지 확인합니다.
- `./build/codebomb --shot out.png arrays code 7 6 22`: 모듈 두 개를 푼 큐브의
  스크린샷 (옵션은 `src/main.c` 참고).
