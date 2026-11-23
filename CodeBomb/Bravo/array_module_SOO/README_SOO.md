# Array Circuit SOO

## 한국어

Sumin 담당인 배열·반복문 시각화 모듈의 첫 실행 버전입니다. **C11 + Raylib**로 만들었습니다. 모든 소스 파일명에 `_SOO`를 넣었습니다.

### 구현한 것

- 인덱스, 현재 값, 목표 값이 보이는 1D 카드와 2D 격자
- 읽는 카드/쓰는 카드 강조, 변수 `i`, `j` 표시, 상태 설명
- 재생/일시정지, 한 단계 실행, 이전 단계, 초기화, 속도 변경
- 올바른 풀이·마지막 단계 누락·잘못된 인덱스·중복 탐색 비교
- 범위를 벗어난 접근과 연산 예산 초과 차단
- 성공한 레벨 표시. 이 표시는 실행 중에만 유지됩니다.
- 서울–더블린 연결 테마의 공통 화면

### 현재 범위

이 버전은 **샘플 실행 기록을 보여주는 담당 모듈**입니다. 사용자 C 코드를 편집하거나 컴파일하지 않습니다. 오른쪽 코드는 수정 가능한 에디터가 아니라 선택한 기록을 설명하는 참고 코드입니다. 잘못된 샘플은 주석에 표시된 추가/누락 이벤트를 포함합니다.

팀원의 에디터가 실제 코드 실행에서 읽기·쓰기 이벤트를 만들어 보내야 실제 사용자 코드와 연동됩니다. 아직 포인터/연결 리스트, 온라인 협동, 진행 저장, 실제 실행 시간 측정은 없습니다. 모든 6개 레벨은 자유롭게 선택할 수 있습니다. 강의 자료를 아직 확인하지 못했으므로 시험 범위에 맞춘 문제 확정은 다음 단계입니다.

### 실행

이 Mac에서는 Raylib 6.0과 pkg-config를 확인했습니다. Finder에서 `run_SOO.command`를 더블 클릭하세요. 또는 터미널에서 이 폴더로 이동한 뒤:

```sh
make -f Makefile_SOO all
./array_circuit_SOO
```

`make`, C 컴파일러, Raylib, pkg-config가 필요합니다. 다른 컴퓨터에서는 해당 도구와 Raylib를 먼저 준비해야 합니다. 포함된 실행 파일은 이 Mac용이며 다른 운영체제에서는 소스로 다시 빌드해야 합니다.

### 조작과 시연 순서

- `1`~`6`: 레벨 선택
- `Space`: 재생/일시정지
- `→`: 한 단계, `←`: 이전 단계
- `R`: 현재 샘플 초기화, `S`: 속도 변경
- 버튼 네 개로 Correct / Missing step / Out of bounds / Repeated scans 선택
- 첫 시연은 레벨 2 → Correct → Step으로 읽기와 쓰기를 비교합니다.
- 레벨 4에서는 중첩 반복과 `i`, `j`를 봅니다.
- 레벨 5에서는 대각선만 고치므로 일부 빨간 카드가 남아도 정답입니다. 각 카드의 `to` 값을 보세요.
- 레벨 6에서는 Correct의 16회 연산과 Repeated scans의 20회 후 실패를 비교합니다.

### 레벨

| 번호 | 개념 | 목표 |
|---|---|---|
| 1 | 단일 인덱스 | 2번 카드를 0으로 변경 |
| 2 | 정방향 반복 | 한 줄의 결함 카드 수리 |
| 3 | 역방향 반복 | 마지막 인덱스부터 수리 |
| 4 | 2D와 중첩 반복 | 3×4 카드 수리 |
| 5 | 대각선 접근 | 4×4의 대각선만 수리 |
| 6 | 효율성 | 읽기와 쓰기 합계 20회 이내 수리 |

### 테스트

```sh
make -f Makefile_SOO test
```

AddressSanitizer와 UndefinedBehaviorSanitizer를 켜고 검사합니다. 여섯 레벨 성공/미완료/범위 오류, 음수 인덱스, 실패 후 변경 차단, 초과 쓰기 차단, 작은 이벤트 버퍼를 확인합니다.

자세한 구조, 함수 설명과 팀원 연결 예시는 `CODE_EXPLANATION_SOO.md`를 보세요.

## English

This is Sumin's first runnable array and loop visualisation module, written in C11 and Raylib. All source filenames include `_SOO`.

It includes six levels, 1D/2D memory cards, indices and target values, loop variables, step-by-step playback, backward replay, error messages and an operation budget.

**This is a sample trace viewer, not the code editor or a C interpreter.** The teammate's editor must supply read/write/loop events from actual code execution. Reference snippets are not editable. Notes identify extra or missing events in faulty samples.

Double-click `run_SOO.command` on this Mac, or run `make -f Makefile_SOO all` and `./array_circuit_SOO`. Other computers need a C compiler, make, Raylib and pkg-config. The included executable is for this Mac.

Use 1–6 to select levels, Space to play/pause, Right to step, Left to go back, R to reset and S to change speed. Compare the four sample scenarios using the buttons.

Level 5 intentionally leaves non-diagonal FAULT cards unchanged. Success means matching every TARGET, not making every card green. Level 6 measures memory operations, not milliseconds: each read or write costs one, while loop-variable updates cost zero. This is a teaching model, not a benchmark of arbitrary C code.

Completion badges last only for the current session. The editor, pointers, linked lists, online multiplayer, persistent progress and full course alignment are not included yet.

Run `make -f Makefile_SOO test` for the sanitizer-enabled core checks. See `CODE_EXPLANATION_SOO.md` for detailed explanations and the integration contract.

## Predict mode / 결과 예측 모드

앱을 열면 Predict 모드가 시작됩니다. 레벨을 고르고 코드와 배열을 읽은 뒤 A, B, C 중 하나를 클릭하거나 키를 누르세요. 정답과 해설이 나오고 예제가 재생됩니다. 답을 고른 다음에는 Pause, Step, Back으로 확인할 수 있습니다. Reset은 현재 문제를 다시 시작합니다. Explore는 기존 샘플과 오류를 비교하는 모드입니다.

The app opens in Predict mode. Pick a level, read the code, and choose A, B or C. The app explains the answer and plays the sample. After answering, pause, step or go back to review it. Reset lets you try the question again. Explore keeps the original sample and error comparisons.

## Find the bug / 범위 오류 찾기

상단 **Find the bug**를 누르면 레벨 2의 범위 오류 미션이 열립니다. A, B, C 중 답을 고르면 해설과 원본 예제가 나옵니다. **Run original**과 **Run fix**를 눌러 같은 초기 배열에서 두 실행을 비교하세요. **Step / Back**으로 마지막 단계를 살펴볼 수 있습니다. **Reset**은 선택을 지우고 처음으로 돌아갑니다.

Open **Find the bug** and choose A, B or C. The app explains the answer and starts the original trace. Use **Run original** and **Run fix** to compare the two runs from the same initial array. Use **Step / Back** to inspect the ending, or **Reset** to try again.

## Pointers / 포인터

상단 **Pointers**에서 두 값 교환 미션을 열고 A/B/C를 고르세요. Pause, Step, Back, Reset으로 확인하고 **Array missions**로 돌아갈 수 있습니다.

Open **Pointers**, choose A/B/C, and follow how the two values swap. Use Pause, Step, Back or Reset. Select **Array missions** to return.

## Circuit Pals window / 창 크기

창 가장자리를 드래그해 크기를 조절하세요. 최소 크기는 768 × 480입니다.
F11로 전체 화면을 켜거나 끌 수 있습니다. Mac에서는 설정에 따라 fn + F11을 누르세요.
화면 비율을 유지하므로 창 모양에 따라 여백이 생길 수 있습니다.

Drag a window edge to resize. The minimum is 768 × 480. Press F11
(or fn + F11 on some Macs) to toggle full screen. The layout keeps its aspect ratio.

## Mission map / 미션 지도

앱을 열면 8개 챕터와 40개 레벨 계획이 표시됩니다. 밝은 번호는 현재 플레이할 수 있는 미션이고, 어두운 번호는 개발 예정입니다. **Mission map**으로 돌아올 수 있습니다. 01–06은 Stage 1 미션입니다. 07–11은 포인터 미션, 18–19는 기존 2D 예제입니다.

The app opens on the eight-chapter map. Bright numbers open playable missions. Dim numbers are planned. Use **Mission map** to return. 01–06 open Stage 1, 07–11 open Stage 2 pointers, and 18–19 open the existing 2D examples.

## Stage 3 / 배열 조작

12–16은 답을 고르고 Step이나 Back으로 배열을 확인합니다. 17은 배터리 카드 두 장을 직접 선택합니다. 다시 누르면 선택을 해제합니다. 두 장의 합이 120이면 성공입니다.

For levels 12–16, choose an answer and use Step or Back. In level 17, click two battery cards; click a selected card again to remove it. A pair must total 120.

## Stage 4

18–22 now open Grid Scanner, Pattern Detector, Cluster Destroyer, Gravity System and Chain Reaction. Choose A/B/C, then pause, step or go back. The board is 4 by 6. Green marks the detected cluster; -1 is empty and -2 is new. All levels use prepared examples.

## Stage 5 / Strings and Char Pointers

지도 23–27에서 문자열 미션을 엽니다. 23–26은 A/B/C를 고르고 재생합니다. 26의 Try 8-byte buffer로 공간 부족 예제를 볼 수 있습니다. 27은 기본 BOMB를 Backspace로 지우고 암호를 입력한 뒤 Enter 또는 Verify를 누르세요. 최대 8개의 ASCII 문자를 지원합니다. Reset은 미션을 처음부터 다시 시작합니다.

Open levels 23–27 on the map. Choose A/B/C in levels 23–26. In 26, try the 8-byte buffer after playback. In 27, use Backspace to edit BOMB, type up to eight ASCII characters, then press Enter or Verify. Reset restarts the lesson.

## Stage 6 / Linked Lists

지도 28–33에서 순회, 삽입, 삭제, 탐색, 뒤집기, 인접 쌍 교환을 확인합니다. A/B/C를 고른 뒤 Step/Back으로 head와 연결선을 따라가세요. N0 등의 표시는 배열 인덱스가 아닌 노드 ID입니다. 이중 연결 미션에서 파랑은 next, 분홍은 prev입니다. 노드 값은 그대로 두고 연결만 변경합니다. C 소스에는 실제 malloc/free 기반 리스트 함수와 그 테스트도 포함되어 있습니다.

Open levels 28–33 for traversal, insertion, deletion, search, reversal and adjacent pair swaps. Answer A/B/C, then follow head and the arrows using Step/Back. Node IDs are not array indices. Blue is next; pink is prev. The lesson changes links, not data. Actual C heap-based list functions and tests are included alongside the snapshot viewer.

## Stage 7 / Memory Allocation

34–37은 할당/해제, 동적 배열, 누수와 dangling pointer 미션입니다. 36–37은 답을 고른 후 Original과 Run fix로 비교합니다. 34에는 malloc 실패, 35에는 잘못된 delete, 37에는 alias 실험 버튼이 있습니다. 위험한 메모리 접근은 안전한 모델 안에서 차단합니다. 앱 구현 언어는 C이며 C++ 예제는 학습용 코드입니다.

Levels 34–37 cover allocation, arrays, leaks and dangling pointers. Compare Original and Run fix in 36–37. Extra experiments show malloc failure, incorrect delete and aliases. Unsafe accesses are blocked in a safe model. The app remains C, with C++ examples displayed for learning.

## Stage 8 / Vector and Iterator

38–40을 구현했습니다. 38은 push_back과 size/capacity, 39는 erase 반환 반복자를 보여줍니다. 40에서는 Scan next → 99 찾기 → it = erase(it) → 끝까지 Scan → push_back(40) → Verify circuit 순서로 직접 조작합니다. 오른쪽 예제는 C++이고 앱은 C11입니다. capacity 숫자는 학습용 모델의 값입니다.

All 40 levels are playable. Level 40 is interactive: scan to 99, erase safely, scan to end, append 40, then verify. Level 39 includes original, fixed and last-element examples. The app is C11 with C++ reference snippets. Capacity values are illustrative.
