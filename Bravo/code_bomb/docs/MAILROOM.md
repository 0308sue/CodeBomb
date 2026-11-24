# Mailroom: levels, machines and the 3D scene / 우편실: 레벨, 기계, 3D 장면

## English

### Status

All eleven levels are built. Each one has a starter, a one-line goal, a
field-manual topic with a diagram of what goes in and comes out, two or three
programs in its Examples menu that show typical mistakes (never the answer),
and a reference answer in `levels.c` that is used only by the tests and the
screenshots.

| Level | C idea | Inbox | Output | New machine or native | Par size / speed |
|---|---|---|---|---|---|
| 1 Mailroom | statements, calls | 3 parcels | the same | `take()`, `put(n)` | 3 / 7 |
| 2 Night Shift | `while` | 2-7 parcels | the same | `has_input()` | 2 / 28 |
| 3 Damaged Goods | `if` / `else`, `<` | 3-7 parcels, some below 0 (and 0) | good ones; damaged ones to `shred(n)` | shredder | 5 / 54 |
| 4 Weigh Station | variables, `+` | 1-3 pairs | each pair's sum | | 4 / 23 |
| 5 Two Belts | `%`, `==` | 3-8 parcels | even to `put_even(n)`, odd to `put_odd(n)` | second (ODD) belt | 5 / 84 |
| 6 Head Count | counters, sentinel | 2-3 groups ended by 0 | each group's count | | 5 / 59 |
| 7 Biggest Box | comparison, best so far | 1-3 groups of 3 | each group's biggest | | 7 / 92 |
| 8 Stamp Press | functions, parameters, `return` | 2-5 parcels of 1-9 | `press(press(n))`, `press(n) = 3n + 1` | stamp press | 3 / 76 |
| 9 Countdown | `for` | 1-2 parcels of 1-4 | n, n-1, ..., 1 for each | | 4 / 61 |
| 10 Back Shelf | arrays, indices | 2-8 parcels | reverse order | `int shelf[8]`, back shelf | 6 / 148 |
| 11 Holiday Rush | everything | 2-3 groups ended by 0 | per group: shred damaged, deliver the rest reversed | all of them | 11 / 169 |

Par is the reference answer's size (statements) and speed (average
instructions) on the game's 5 Submit tests.

### What the new levels check

- **Two outputs (levels 3, 5, 11).** `LevelRun` has a second output next to
  the outbox (`side`, `side_expected`): the shredder or the ODD belt. Each
  parcel is checked the moment it arrives, and a run only finishes when both
  outputs are complete ("only 1 of 2 parcels reached the shredder").
- **Clear mistakes.** A parcel sent to the wrong output says where it
  belongs: `put(-5)` "that parcel is damaged ... go to shred()", `shred(0)`
  "that parcel isn't damaged", `put_even(7)` "7 is odd (7 % 2 is 1)".
- **Your own function (level 8).** Submit also requires a helper that takes
  a parameter, returns an int and is called from somewhere else
  (`level_has_helper`). Any name works; a helper that is never called does
  not. The Examples menu shows the right numbers without a function, so the
  player sees why Submit still says no, and the pass-by-value mistake
  (`void press(int n) { n = 3 * n + 1; }`).
- **The shelf (levels 10, 11).** `int shelf[8]` and `SHELF_SIZE` come from
  the level, the same way the bombs give `cards[]` and `N`: the compiler turns
  `shelf[i]` into two hidden natives (`$shelf_get`, `$shelf_set`), so every
  access is checked. `shelf[8]` and `shelf[-1]` stop with "outside the
  shelf", and reading a slot that was never written stops with "shelf[5] is
  empty ... In C an unset slot holds garbage". No compiler change was needed.

### Stars and progress

Each level has three stars: solved, size par met, speed par met. They are
earned by Submit, kept as the best result, and drawn in the level row, which
now shows every level as a chip (the open one also shows its name) with the
total on the right ("4 / 11 solved   9 / 33"). The mission map's first Bomb
defusal button reads "Mailroom 4/11" and gets a green dot when all eleven are
solved.

### The robot

The robot keeps every parcel it takes in a stack in its arms. Each call
becomes a short queue of tasks (walk, prepare, hand over), and the VM waits
until the queue is empty:

| Call | What the robot does |
|---|---|
| `take()` | drives to the INBOX and lifts the front parcel onto its stack |
| `put(n)` | gets parcel n ready where it stands (keeps a parcel stamped n and recycles the rest; merges parcels that add up to n with a pulse, a glow, sparks and a "10 + 7 = 17" pop-up; restamps; or a new parcel pops up), then sets it on the OUTBOX |
| `shred(n)` | the same preparation, then tosses the parcel into the shredder, which shakes, flashes its lamp and spits paper strips out of the chute ("-5 < 0: shred") |
| `put_odd(n)` | onto the ODD belt ("7 % 2 = 1: ODD"); `put_even(n)` goes to the EVEN belt |
| `put(n)` in level 8 | a changed number is made in the stamp press: the parcel goes onto the bed, the ram slams down, the parcel glows with the new number and comes back ("2 -> press -> 22") |
| `shelf[i] = n` | walks to slot i and places the parcel; an old parcel in that slot goes to the bin. The slot's index lights up ("shelf[2] = 27") |
| reading `shelf[i]` | a copy flies from slot i to the robot; the slot keeps its parcel, as in C ("shelf[2] is 27") |

The robot works beside each machine, not in front of it, so the camera can
always see the machine. Number stickers and floor stencils are real 3D
decals, so the robot and its stack hide what is behind them.

### The 3D scene: physically based rendering

The Mailroom is drawn by a small renderer in `src/pbr.c` with materials made
in code by `src/pbr_textures.c`. The game still ships no image files.

- **Materials.** Every material is a set of three textures, like the ones a
  PBR texturing tool exports: albedo, MRA (red metallic, green roughness,
  blue ambient occlusion) and a tangent-space normal map. They are painted at
  start-up from tileable noise: polished concrete with saw-cut joints, stains
  and pits; kraft cardboard with fibres and fluting (and a water-stained,
  creased, torn version for damaged parcels); safety-yellow paint with
  scratches that show bare steel; brushed and oily steel; a cleated rubber
  belt; hazard stripes; corrugated wall sheet; pallet wood; plastic, paper
  labels and packing tape. A height field gives each normal map and its
  occlusion.
- **Shading.** Cook-Torrance with a GGX distribution, Smith visibility and
  Schlick Fresnel; ambient light from the sky and the floor; reflections of a
  simple warehouse environment (skylight strips above, warm concrete below)
  with Karis' fit of the split-sum BRDF; light fog; ACES tone mapping and
  gamma. Glowing parts (parcels after a merge, eyes, lamps, sparks) use an
  emissive term plus soft additive halos.
- **Lights.** One shadow-casting key light from the skylights (a 2048 x 2048
  depth map with 5 x 5 filtering and a tight depth range so the bias stays
  small), three warm work lamps over the lane, and the shredder's warning
  lamp, which lights the floor while it runs.
- **Geometry.** Boxes have chamfered edges that catch the light; meshes are
  built in code with tangents (raylib 5.5's `GenMeshTangents` ignores index
  buffers, so the renderer does not use it). Belts scroll by moving their
  texture.
- **Supersampling.** The scene is rendered at twice the view's size and
  scaled down, so edges stay smooth without MSAA. `Scene.prerender` runs the
  shadow pass and this render before the screen's own render texture is
  bound, because render textures can't be nested.
- **Fallback.** The shaders are GLSL 330, which Windows, Linux and macOS
  (OpenGL 3.3 core) accept. If they can't be used (for example raylib's
  software renderer), the same calls draw the albedo textures without
  lighting, so the Mailroom still works everywhere.

A slow frame is capped at 0.1 seconds (`main.c`), and the stamp press
triggers its slam by time rather than by an exact ram position, so a long
frame can't skip the slam.

### Tests

`tests/test_levels.c` checks every level: the starter compiles; the reference
passes 50 random inboxes and meets par; every example fails Submit, and nine
of them fail with the message their Examples line promises; inboxes and both
outputs hold 1-9 parcels so they fit on the belts, and levels with a machine
give it work on the first tests; the shelf's bounds and empty-slot errors;
level 8 accepts a helper with another name and rejects one that is never
called. `--selftest` plays every level's answer live through the 3D scene to
the end, submits it, and checks 11 solved and 33 stars.

### Ideas for later

- **Ghost run:** after a successful Submit, replay the fastest test.
- **Hint ladder:** a "Stuck?" button with a hint, then the shape of the loop,
  never the full answer (the Bomb screen already has one).
- **Saved progress:** stars are kept while the game runs; saving them to a
  file would keep them between sessions.

## 한국어

### 진행 상황

레벨 11개를 모두 만들었습니다. 레벨마다 시작 코드, 한 줄 목표, 들어오고 나가는
것을 보여 주는 그림이 있는 현장 매뉴얼 주제, 흔한 실수를 보여 주는 Examples
메뉴의 프로그램 2-3개(정답은 아님), 그리고 테스트와 스크린샷에만 쓰는
`levels.c`의 정답이 있습니다.

| 레벨 | C 개념 | 입력함 | 출력 | 새 기계 또는 네이티브 | 기준 크기 / 속도 |
|---|---|---|---|---|---|
| 1 Mailroom | 문장, 함수 호출 | 소포 3개 | 그대로 | `take()`, `put(n)` | 3 / 7 |
| 2 Night Shift | `while` | 소포 2-7개 | 그대로 | `has_input()` | 2 / 28 |
| 3 Damaged Goods | `if` / `else`, `<` | 소포 3-7개, 일부는 0 미만 (0도 있음) | 좋은 소포, 손상된 것은 `shred(n)` | 파쇄기 | 5 / 54 |
| 4 Weigh Station | 변수, `+` | 1-3쌍 | 각 쌍의 합 | | 4 / 23 |
| 5 Two Belts | `%`, `==` | 소포 3-8개 | 짝수는 `put_even(n)`, 홀수는 `put_odd(n)` | 두 번째 (ODD) 벨트 | 5 / 84 |
| 6 Head Count | 카운터, 끝 표시 | 0으로 끝나는 묶음 2-3개 | 각 묶음의 개수 | | 5 / 59 |
| 7 Biggest Box | 비교, 지금까지 최고 | 3개씩 1-3묶음 | 각 묶음의 최댓값 | | 7 / 92 |
| 8 Stamp Press | 함수, 매개변수, `return` | 1-9인 소포 2-5개 | `press(press(n))`, `press(n) = 3n + 1` | 스탬프 프레스 | 3 / 76 |
| 9 Countdown | `for` | 1-4인 소포 1-2개 | 각각 n, n-1, ..., 1 | | 4 / 61 |
| 10 Back Shelf | 배열, 인덱스 | 소포 2-8개 | 거꾸로 | `int shelf[8]`, 뒤 선반 | 6 / 148 |
| 11 Holiday Rush | 종합 | 0으로 끝나는 묶음 2-3개 | 묶음마다: 손상된 것은 파쇄, 나머지는 거꾸로 배달 | 모든 기계 | 11 / 169 |

기준(par)은 게임의 Submit 테스트 5개에서 정답의 크기(문장 수)와 속도(평균
명령 수)입니다.

### 새 레벨이 검사하는 것

- **출력 두 개 (레벨 3, 5, 11).** `LevelRun`에는 출력함 옆에 두 번째 출력
  (`side`, `side_expected`)이 있습니다: 파쇄기 또는 ODD 벨트. 소포는 도착하는
  순간 검사되고, 두 출력이 모두 채워져야 실행이 끝납니다 ("only 1 of 2 parcels
  reached the shredder").
- **분명한 실수 메시지.** 잘못된 출력으로 보낸 소포는 어디로 가야 하는지
  알려 줍니다: `put(-5)` "that parcel is damaged ... go to shred()",
  `shred(0)` "that parcel isn't damaged", `put_even(7)` "7 is odd (7 % 2 is 1)".
- **직접 쓴 함수 (레벨 8).** Submit은 매개변수를 받고 int를 돌려주며 다른
  곳에서 호출되는 도우미 함수도 요구합니다 (`level_has_helper`). 이름은
  자유지만 한 번도 호출하지 않는 도우미는 안 됩니다. Examples 메뉴에는 함수
  없이 숫자만 맞는 프로그램이 있어 Submit이 왜 여전히 거부하는지 볼 수 있고,
  값에 의한 전달 실수(`void press(int n) { n = 3 * n + 1; }`)도 있습니다.
- **선반 (레벨 10, 11).** `int shelf[8]`과 `SHELF_SIZE`는 폭탄이 `cards[]`와
  `N`을 주는 것과 같은 방식으로 레벨이 줍니다: 컴파일러가 `shelf[i]`를 숨겨진
  네이티브 두 개(`$shelf_get`, `$shelf_set`)로 바꾸므로 모든 접근이
  검사됩니다. `shelf[8]`과 `shelf[-1]`은 "outside the shelf"로 멈추고, 한 번도
  쓰지 않은 칸을 읽으면 "shelf[5] is empty ... In C an unset slot holds
  garbage"로 멈춥니다. 컴파일러는 바꿀 필요가 없었습니다.

### 별과 진행 상황

레벨마다 별이 세 개입니다: 해결, 크기 기준 달성, 속도 기준 달성. Submit으로
얻고 가장 좋은 결과가 남으며, 레벨 줄에 그려집니다. 레벨 줄은 이제 모든
레벨을 칩으로 보여 주고 (열린 레벨은 이름도), 오른쪽에 합계를 보여 줍니다
("4 / 11 solved   9 / 33"). 미션 지도의 첫 Bomb defusal 버튼은 "Mailroom
4/11"이 되고, 11개를 모두 풀면 초록 점이 붙습니다.

### 로봇

로봇은 가져온 상자를 모두 팔에 쌓아 듭니다. 호출마다 짧은 작업 대기열(걷기,
준비, 건네기)이 되고, VM은 대기열이 빌 때까지 기다립니다:

| 호출 | 로봇이 하는 일 |
|---|---|
| `take()` | 입력함으로 가서 맨 앞 상자를 더미에 올린다 |
| `put(n)` | 그 자리에서 n 상자를 준비한다 (n이 찍힌 상자만 남기고 나머지는 재활용, 합이 n인 상자들은 번쩍임, 빛, 불꽃, "10 + 7 = 17" 팝업과 함께 합치기, 다시 찍기, 또는 새 상자). 그다음 출력함에 놓는다 |
| `shred(n)` | 같은 준비 뒤 파쇄기에 던진다. 파쇄기는 흔들리고 경고등이 깜빡이며 배출구로 종이 조각을 내뱉는다 ("-5 < 0: shred") |
| `put_odd(n)` | ODD 벨트로 ("7 % 2 = 1: ODD"). `put_even(n)`은 EVEN 벨트로 |
| 레벨 8의 `put(n)` | 바뀐 숫자는 스탬프 프레스에서 만든다: 상자가 받침에 올라가고 램이 쾅 내려오며, 상자가 새 숫자로 빛나며 돌아온다 ("2 -> press -> 22") |
| `shelf[i] = n` | i칸으로 가서 상자를 놓는다. 그 칸의 이전 상자는 통으로 간다. 칸 번호가 빛난다 ("shelf[2] = 27") |
| `shelf[i]` 읽기 | 복사본이 i칸에서 로봇에게 날아온다. C처럼 칸에는 상자가 그대로 남는다 ("shelf[2] is 27") |

로봇은 기계 앞이 아니라 옆에서 일하므로 카메라가 항상 기계를 볼 수 있습니다.
숫자 스티커와 바닥 글자는 실제 3D 데칼이라서 로봇과 로봇이 든 더미가 뒤의
것을 가립니다.

### 3D 장면: 물리 기반 렌더링

우편실은 `src/pbr.c`의 작은 렌더러가 그리고, 재질은 `src/pbr_textures.c`가
코드로 만듭니다. 게임에는 여전히 이미지 파일이 없습니다.

- **재질.** 모든 재질은 PBR 텍스처 도구가 내보내는 것과 같은 텍스처 세 장입니다:
  알베도, MRA (빨강 금속성, 초록 거칠기, 파랑 주변광 차폐), 접선 공간 노멀 맵.
  시작할 때 이어지는 노이즈로 그립니다: 줄눈, 얼룩, 작은 구멍이 있는 연마
  콘크리트, 섬유와 골이 보이는 크라프트 골판지 (손상된 소포용으로 물 얼룩,
  주름, 찢긴 모서리가 있는 판), 긁힌 곳에 강철이 드러나는 안전 노랑 도장,
  헤어라인 강철과 기름 묻은 강철, 돌기가 있는 고무 벨트, 경고 줄무늬, 골함석
  벽, 팔레트 나무, 플라스틱, 종이 라벨, 포장 테이프. 높이 맵에서 노멀 맵과
  차폐를 만듭니다.
- **셰이딩.** GGX 분포, Smith 가시성, Schlick 프레넬을 쓰는 Cook-Torrance,
  하늘과 바닥에서 오는 주변광, 간단한 창고 환경(위의 천창 줄, 아래의 따뜻한
  콘크리트)의 반사와 Karis의 split-sum BRDF 근사, 옅은 안개, ACES 톤 매핑과
  감마. 빛나는 부분 (합쳐진 상자, 눈, 램프, 불꽃)은 발광 항과 부드러운 가산
  혼합 빛을 씁니다.
- **조명.** 천창에서 오는 그림자 주 조명 하나 (2048 x 2048 깊이 맵, 5 x 5
  필터, 바이어스를 작게 유지하는 좁은 깊이 범위), 통로 위의 따뜻한 작업등
  세 개, 작동하는 동안 바닥을 비추는 파쇄기 경고등.
- **형상.** 상자는 모서리를 깎아 빛을 받습니다. 메시는 접선과 함께 코드로
  만듭니다 (raylib 5.5의 `GenMeshTangents`는 인덱스 버퍼를 무시하므로 쓰지
  않습니다). 벨트는 텍스처를 움직여 흐릅니다.
- **슈퍼샘플링.** 장면을 뷰의 두 배 크기로 그린 뒤 줄여서, MSAA 없이도
  가장자리가 매끄럽습니다. 렌더 텍스처는 중첩할 수 없으므로
  `Scene.prerender`가 화면의 렌더 텍스처를 연결하기 전에 그림자 단계와 이
  렌더를 실행합니다.
- **대체 경로.** 셰이더는 GLSL 330이라 Windows, Linux, macOS (OpenGL 3.3
  core)에서 동작합니다. 쓸 수 없으면 (예: raylib 소프트웨어 렌더러) 같은
  호출이 조명 없이 알베도 텍스처만 그리므로 우편실은 어디서나 동작합니다.

느린 프레임은 0.1초로 제한되고 (`main.c`), 스탬프 프레스는 램의 정확한
위치가 아니라 시간으로 쾅을 일으키므로 긴 프레임이 쾅을 건너뛸 수 없습니다.

### 테스트

`tests/test_levels.c`는 모든 레벨을 검사합니다: 시작 코드가 컴파일되고, 정답이
무작위 입력함 50개를 통과하며 기준을 만족하고, 모든 예제가 Submit에
실패하며 그중 9개는 Examples 설명에 적힌 메시지로 실패하고, 입력함과 두 출력이
벨트에 들어가도록 소포 1-9개를 담으며, 기계가 있는 레벨은 첫 테스트에서 기계에
일을 줍니다. 선반의 범위 오류와 빈 칸 오류, 그리고 레벨 8이 다른 이름의
도우미는 받아들이고 호출하지 않는 도우미는 거부하는지도 검사합니다.
`--selftest`는 모든 레벨의 정답을 3D 장면으로 끝까지 실시간 재생하고, Submit한
뒤 11개 해결과 별 33개를 확인합니다.

### 나중을 위한 아이디어

- **고스트 실행:** Submit에 성공하면 가장 빠른 테스트를 다시 재생합니다.
- **힌트 사다리:** 힌트, 그다음 반복문의 모양을 보여 주고 정답 전체는 보여
  주지 않는 "Stuck?" 버튼 (Bomb 화면에는 이미 있습니다).
- **진행 저장:** 별은 게임이 실행되는 동안 남습니다. 파일에 저장하면 다음에도
  남습니다.
