/*
 * bomb_variants.c - random bombs with the same rule, and hints
 * bomb_variants.c - 같은 규칙의 무작위 폭탄과 힌트
 */
#include "bomb_variants.h"

#include "array_bridge.h"
#include "bomb_modules.h"

#include <string.h>

BombKind bomb_kind_for_level(int level)
{
    return level >= 0 && level < SOO_LEVEL_COUNT ? (BombKind)level : BOMB_L1;
}

BombKind bomb_kind_for_module(int module)
{
    return module >= 0 && module < CUBE_MODULE_COUNT
               ? (BombKind)(BOMB_INVERT + module)
               : BOMB_INVERT;
}

/* Same small generator as the Mailroom: identical bombs on every platform.
 * 우편실과 같은 작은 생성기: 모든 플랫폼에서 같은 폭탄. */
static unsigned next(unsigned *s)
{
    *s = *s * 1103515245u + 12345u;
    return (*s >> 16) & 0x7fffu;
}

static int range(unsigned *s, int lo, int hi)
{
    return lo + (int)(next(s) % (unsigned)(hi - lo + 1));
}

/* Card patterns, weighted toward edge cases: random, all FAULT, all SAFE,
 * alternating. / 카드 무늬, 경계 상황 위주: 무작위, 모두 FAULT, 모두 SAFE,
 * 번갈아. */
static void fill(int *cells, int n, unsigned *s)
{
    int pattern = (int)(next(s) % 6);
    int start = (int)(next(s) % 2);
    for (int k = 0; k < n; ++k) {
        switch (pattern) {
        case 1: cells[k] = 1; break;
        case 2: cells[k] = 0; break;
        case 3: cells[k] = (k + start) % 2; break;
        default: cells[k] = (int)(next(s) % 2); break;
        }
    }
}

static void grid_size(unsigned *s, int rmin, int rmax, int cmin, int cmax,
                      int *rows, int *cols)
{
    do {
        *rows = range(s, rmin, rmax);
        *cols = range(s, cmin, cmax);
    } while (*rows * *cols > SOO_MAX_CELLS);
}

void bomb_variant_make(BombVariant *v, BombKind kind, unsigned seed)
{
    const SooLevel *base = kind <= BOMB_L6 ? &SOO_LEVELS[kind]
                                           : &CUBE_MODULES[kind - BOMB_INVERT];
    memset(v, 0, sizeof *v);
    v->kind = kind;
    v->seed = seed;
    SooLevel *l = &v->level;
    l->title = base->title;
    l->concept = base->concept;
    l->objective = base->objective;

    unsigned s = seed * 2654435761u + (unsigned)kind * 7919u + 17u;
    int *in = l->initial, *goal = l->goal;
    l->rows = 1;

    switch (kind) {
    case BOMB_L1:              /* set cards[2] to 0, leave the rest alone */
        l->cols = range(&s, 3, 8);
        fill(in, l->cols, &s);
        in[2] = 1;
        memcpy(goal, in, sizeof l->goal);
        goal[2] = 0;
        l->budget = 20;
        break;
    case BOMB_L2:              /* every FAULT becomes SAFE */
    case BOMB_L3:              /* the same, scanning right to left */
        l->cols = range(&s, 3, 10);
        fill(in, l->cols, &s);
        l->budget = 5 * l->cols;
        break;
    case BOMB_L4:              /* every FAULT in the grid becomes SAFE */
        /* grids keep 2+ rows: one row would make it cards[], not grid[][]
           격자는 2줄 이상: 한 줄이면 grid[][]가 아니라 cards[]가 된다 */
        grid_size(&s, 2, 4, 2, 6, &l->rows, &l->cols);
        fill(in, l->rows * l->cols, &s);
        l->budget = 4 * l->rows * l->cols + 2;
        break;
    case BOMB_L5:              /* only the diagonal grid[i][i] is repaired */
        l->rows = l->cols = range(&s, 2, 4);
        fill(in, l->rows * l->cols, &s);
        memcpy(goal, in, sizeof l->goal);
        for (int i = 0; i < l->rows; ++i) goal[i * l->cols + i] = 0;
        l->budget = 4 * l->rows;
        break;
    case BOMB_L6:              /* every FAULT to SAFE within 2 ops per card */
        l->cols = range(&s, 6, 12);
        fill(in, l->cols, &s);
        l->budget = 2 * l->cols;
        break;
    case BOMB_INVERT:
        l->cols = range(&s, 3, 9);
        fill(in, l->cols, &s);
        for (int i = 0; i < l->cols; ++i) goal[i] = 1 - in[i];
        l->budget = 2 * l->cols;
        break;
    case BOMB_EVENS:
        l->cols = range(&s, 3, 9);
        fill(in, l->cols, &s);
        for (int i = 0; i < l->cols; ++i) goal[i] = i % 2 == 0;
        l->budget = 2 * l->cols;
        break;
    case BOMB_MIRROR:
        l->cols = range(&s, 3, 9);           /* odd sizes too / 홀수 크기도 */
        fill(in, l->cols, &s);
        memcpy(goal, in, sizeof l->goal);
        for (int i = 0; i < l->cols / 2; ++i) goal[l->cols - 1 - i] = in[i];
        l->budget = 2 * l->cols;
        break;
    case BOMB_CHECKER:
        grid_size(&s, 2, 4, 2, 6, &l->rows, &l->cols);
        fill(in, l->rows * l->cols, &s);
        for (int i = 0; i < l->rows; ++i)
            for (int j = 0; j < l->cols; ++j)
                goal[i * l->cols + j] = (i + j) % 2;
        l->budget = 2 * l->rows * l->cols;
        break;
    case BOMB_BORDER:
        grid_size(&s, 3, 4, 3, 6, &l->rows, &l->cols);
        fill(in, l->rows * l->cols, &s);
        for (int i = 0; i < l->rows; ++i)
            for (int j = 0; j < l->cols; ++j)
                goal[i * l->cols + j] = i == 0 || j == 0 || i == l->rows - 1 ||
                                        j == l->cols - 1;
        l->budget = 2 * l->rows * l->cols;
        break;
    default:
        break;
    }
    /* "every FAULT becomes SAFE" bombs leave goal at all 0
       "모든 FAULT를 SAFE로" 폭탄은 goal이 모두 0 */

    if (kind == BOMB_L3) array_mark_reverse(l);
}

/* ---- hints / 힌트 ------------------------------------------------------- */

static const char *const HINTS[BOMB_KIND_COUNT][BOMB_HINTS] = {
    [BOMB_L1] = {
        "Indexes start at 0, so cards[2] is the third card.",
        "One assignment is enough: the array name, the index in [ ], then = "
        "and the new value.",
        "cards[2] = 0; is the whole answer. Leave the other cards alone: on a "
        "random bomb they must stay as they are.",
    },
    [BOMB_L2] = {
        "Visit every index from 0 up to N - 1 with a for loop.",
        "Inside the loop, if the card is a FAULT (1), make it SAFE (0).",
        "Use N, not 8: random bombs have a different number of cards.",
    },
    [BOMB_L3] = {
        "Start i at the last index, N - 1, and count down to 0.",
        "The condition is i >= 0 and the update is i--.",
        "Every READ must be exactly one card to the left of the one before, "
        "so look at each card once, from right to left.",
    },
    [BOMB_L4] = {
        "Use two loops: the outer one over rows (i < ROWS), the inner one over "
        "columns (j < COLS).",
        "grid[i][j] is row i, column j. Repair it the same way as level 2.",
        "Use ROWS and COLS, not 3 and 4: random bombs change both.",
    },
    [BOMB_L5] = {
        "The diagonal is where the row equals the column: grid[0][0], "
        "grid[1][1], grid[2][2] ...",
        "So one loop is enough: i from 0 while i < ROWS, using grid[i][i].",
        "Leave every other card unchanged, and don't visit the whole grid: "
        "the budget only has room for the diagonal.",
    },
    [BOMB_L6] = {
        "The budget is 2 operations per card. A read plus a write on every "
        "card already uses all of it.",
        "So read each card at most once and scan the row only one time.",
        "Writing 0 without reading costs only 1 op per card.",
    },
    [BOMB_INVERT] = {
        "Every card changes: a 0 becomes 1 and a 1 becomes 0.",
        "Arithmetic does it without an if: 1 - 0 is 1, and 1 - 1 is 0.",
        "In a loop over all N cards: cards[i] = 1 - cards[i];",
    },
    [BOMB_EVENS] = {
        "An index is even when i % 2 == 0: dividing by 2 leaves nothing over.",
        "A comparison is already 1 or 0 in C, so cards[i] = (i % 2 == 0); "
        "writes 1 at even indexes and 0 at odd ones.",
        "You never need to read the cards: their old values don't matter.",
    },
    [BOMB_MIRROR] = {
        "Pair the cards from the outside in: [0] with [N-1], [1] with [N-2], "
        "[2] with [N-3] ...",
        "The partner of cards[i] is cards[N - 1 - i]. Copy the LEFT card onto "
        "its partner on the right.",
        "Loop only over the left half: i < N / 2. With an odd N the middle "
        "card has no partner and stays as it is.",
    },
    [BOMB_CHECKER] = {
        "(i + j) % 2 is 0 where row + column is even and 1 where it is odd, "
        "like the squares of a chessboard.",
        "Loop over every row and column and write (i + j) % 2 straight into "
        "grid[i][j]. No need to read.",
        "Use ROWS and COLS: a random bomb can be 2 x 5 or 4 x 3.",
    },
    [BOMB_BORDER] = {
        "A card is on the border when it is in the first or last row, or the "
        "first or last column.",
        "As a condition: i == 0 || i == ROWS - 1 || j == 0 || j == COLS - 1. "
        "It is 1 on the border and 0 inside.",
        "Write that condition straight into grid[i][j] for every card.",
    },
};

const char *bomb_hint(BombKind kind, int n)
{
    if (kind < 0 || kind >= BOMB_KIND_COUNT || n < 0 || n >= BOMB_HINTS)
        return "";
    return HINTS[kind][n];
}
