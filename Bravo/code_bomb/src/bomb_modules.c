/*
 * bomb_modules.c - cube bomb modules (each one teaches a different idea)
 * bomb_modules.c - 큐브 폭탄 모듈 (모듈마다 다른 개념을 가르친다)
 *
 * Face layout on the cube / 큐브의 면 배치:
 *   front = Invert, right = Even slots, back = Mirror, left = Checkerboard,
 *   bottom = Border, top = display (no code).
 */
#include "bomb_modules.h"

const SooLevel CUBE_MODULES[CUBE_MODULE_COUNT] = {
    {
        /* Arithmetic on values: cards[i] = 1 - cards[i]
         * 값에 대한 산술 */
        "C1  Invert", "Arithmetic on values",
        "Flip every card: each 0 becomes 1 and each 1 becomes 0.",
        1, 6,
        { 1, 0, 0, 1, 1, 0 },
        { 0, 1, 1, 0, 0, 1 },
        12,
    },
    {
        /* Modulo on the index: cards[i] = (i % 2 == 0)
         * 인덱스에 대한 나머지 연산 */
        "C2  Even slots", "The % operator on indices",
        "Cards at even indexes (0, 2, 4 ...) must be 1, and cards at odd "
        "indexes (1, 3, 5 ...) must be 0.",
        1, 6,
        { 0, 0, 1, 1, 0, 1 },
        { 1, 0, 1, 0, 1, 0 },
        12,
    },
    {
        /* Index arithmetic: cards[N - 1 - i] = cards[i]
         * 인덱스 계산 */
        "C3  Mirror", "Index arithmetic",
        "Make the row a mirror image. Leave the left half as it is and copy "
        "each left card onto its partner the same distance from the right "
        "end: [0] onto [N-1], [1] onto [N-2] ... For example 1 1 0 | 0 0 1 "
        "becomes 1 1 0 | 0 1 1.",
        1, 6,
        { 1, 1, 0, 0, 0, 1 },
        { 1, 1, 0, 0, 1, 1 },
        12,
    },
    {
        /* 2D indices and %: grid[i][j] = (i + j) % 2
         * 2D 인덱스와 % */
        "C4  Checkerboard", "2D indices and %",
        "Paint a checkerboard: every card must become (i + j) % 2, so "
        "grid[0][0] is 0, its neighbours are 1, and so on.",
        4, 4,
        { 1, 1, 0, 0,
          0, 0, 1, 1,
          1, 0, 0, 1,
          0, 1, 1, 0 },
        { 0, 1, 0, 1,
          1, 0, 1, 0,
          0, 1, 0, 1,
          1, 0, 1, 0 },
        32,
    },
    {
        /* Conditions on indices: edges are 1, the inside is 0
         * 인덱스에 대한 조건 */
        "C5  Border", "Conditions with || on indices",
        "Every card on the outside edge must be 1, and every card inside "
        "must be 0.",
        4, 4,
        { 1, 0, 1, 1,
          0, 1, 0, 1,
          1, 1, 0, 0,
          1, 1, 0, 1 },
        { 1, 1, 1, 1,
          1, 0, 0, 1,
          1, 0, 0, 1,
          1, 1, 1, 1 },
        32,
    },
};

static const char *const NAMES[CUBE_MODULE_COUNT] = {
    "Invert", "Evens", "Mirror", "Checker", "Border"
};

static const char *const SOLUTIONS[CUBE_MODULE_COUNT] = {
    "void run(void) {\n"
    "    for (int i = 0; i < N; i++)\n"
    "        cards[i] = 1 - cards[i];\n"
    "}\n",

    "void run(void) {\n"
    "    for (int i = 0; i < N; i++)\n"
    "        cards[i] = (i % 2 == 0);\n"
    "}\n",

    "void run(void) {\n"
    "    for (int i = 0; i < N / 2; i++)\n"
    "        cards[N - 1 - i] = cards[i];\n"
    "}\n",

    "void run(void) {\n"
    "    for (int i = 0; i < ROWS; i++)\n"
    "        for (int j = 0; j < COLS; j++)\n"
    "            grid[i][j] = (i + j) % 2;\n"
    "}\n",

    "void run(void) {\n"
    "    for (int i = 0; i < ROWS; i++)\n"
    "        for (int j = 0; j < COLS; j++)\n"
    "            grid[i][j] = i == 0 || j == 0 || i == ROWS - 1 ||\n"
    "                         j == COLS - 1;\n"
    "}\n",
};

const char *cube_module_name(int module)
{
    if (module < 0 || module >= CUBE_MODULE_COUNT) return "";
    return NAMES[module];
}

const char *cube_example_solution(int module)
{
    if (module < 0 || module >= CUBE_MODULE_COUNT) return "";
    return SOLUTIONS[module];
}
