/*
 * test_variants.c - random bombs: real answers pass, card-by-card cheats fail
 * test_variants.c - 무작위 폭탄: 진짜 정답은 통과, 카드별 꼼수는 실패
 *
 * For every bomb kind: random bombs fit Sumin's engine, the working answer
 * defuses 300 of them, and a "cheat" that only sets the cards of the bomb
 * on screen (cards[3] = 0; cards[5] = 0; ...) defuses the bomb on screen
 * but fails on random ones.
 * 모든 폭탄 종류에 대해: 무작위 폭탄이 수민의 엔진에 맞고, 정답은 300개를
 * 해체하며, 화면 폭탄의 카드만 바꾸는 "꼼수"는 화면 폭탄은 해체하지만
 * 무작위 폭탄에서는 실패한다.
 */
#include "test_util.h"

#include "array_bridge.h"
#include "bomb_modules.h"
#include "bomb_variants.h"

#include <stdlib.h>
#include <string.h>

static const SooLevel *base_level(BombKind k)
{
    return k <= BOMB_L6 ? &SOO_LEVELS[k] : &CUBE_MODULES[k - BOMB_INVERT];
}

static const char *answer(BombKind k)
{
    return k <= BOMB_L6 ? array_example_solution((int)k)
                        : cube_example_solution((int)(k - BOMB_INVERT));
}

/* Write the shown bomb's answer card by card. / 화면 폭탄의 답을 카드별로. */
static void make_cheat(const SooLevel *l, char *buf, size_t n)
{
    size_t used = (size_t)snprintf(buf, n, "void run(void) {\n");
    for (int r = 0; r < l->rows; ++r)
        for (int c = 0; c < l->cols; ++c) {
            int k = r * l->cols + c;
            if (l->initial[k] == l->goal[k]) continue;
            if (l->rows == 1)
                used += (size_t)snprintf(buf + used, n - used,
                                         "    cards[%d] = %d;\n", c, l->goal[k]);
            else
                used += (size_t)snprintf(buf + used, n - used,
                                         "    grid[%d][%d] = %d;\n", r, c,
                                         l->goal[k]);
        }
    snprintf(buf + used, n - used, "}\n");
}

/* Other correct ways to write each answer (written the way students do).
 * Each one defuses the bomb on screen, so it must defuse every random bomb
 * too: a random bomb may never be stricter than the bomb on screen.
 * 각 답을 쓰는 다른 올바른 방법 (학생들이 쓰는 방식). 모두 화면 폭탄을
 * 해체하므로 모든 무작위 폭탄도 해체해야 한다: 무작위 폭탄은 화면 폭탄보다
 * 엄격하면 안 된다. */
#define SWEEP_1D "void run(void) {\n" \
                 "    for (int i =0; i<N; i++)\n" \
                 "    {\n" \
                 "        if (cards[i] == 1)\n" \
                 "        {\n" \
                 "            cards[i] = 0;\n" \
                 "        }\n" \
                 "    }\n" \
                 "}\n"
#define ZERO_1D "void run(void) {\n" \
                "    for (int i = 0; i < N; i++)\n" \
                "        cards[i] = 0;\n" \
                "}\n"
#define ZERO_2D "void run(void) {\n" \
                "    for (int i = 0; i < ROWS; i++)\n" \
                "        for (int j = 0; j < COLS; j++)\n" \
                "            grid[i][j] = 0;\n" \
                "}\n"

static const char *const ALTERNATIVES[BOMB_KIND_COUNT][4] = {
    [BOMB_L1] = { SWEEP_1D, ZERO_1D,
                  "void run(void) {\n    cards[2] = 0;\n}\n", NULL },
    [BOMB_L2] = { SWEEP_1D, ZERO_1D,
                  "void run(void) {\n"
                  "    int i = 0;\n"
                  "    while (i < N) {\n"
                  "        if (cards[i]) cards[i] = 0;\n"
                  "        i++;\n"
                  "    }\n"
                  "}\n", NULL },
    [BOMB_L3] = { "void run(void) {\n"
                  "    for (int i = N - 1; i >= 0; i--)\n"
                  "        if (cards[i] == 1) cards[i] = 0;\n"
                  "}\n", NULL, NULL, NULL },
    [BOMB_L4] = { ZERO_2D,
                  "void run(void) {\n"
                  "    for (int i = 0; i < ROWS; i++)\n"
                  "        for (int j = 0; j < COLS; j++)\n"
                  "            if (grid[i][j] == 1) grid[i][j] = 0;\n"
                  "}\n", NULL, NULL },
    [BOMB_L5] = { "void run(void) {\n"
                  "    for (int i = 0; i < ROWS; i++)\n"
                  "        grid[i][i] = 0;\n"
                  "}\n", NULL, NULL, NULL },
    [BOMB_L6] = { SWEEP_1D, ZERO_1D, NULL, NULL },
    [BOMB_INVERT] = { "void run(void) {\n"
                      "    for (int i = 0; i < N; i++) {\n"
                      "        if (cards[i] == 1) cards[i] = 0;\n"
                      "        else cards[i] = 1;\n"
                      "    }\n"
                      "}\n",
                      "void run(void) {\n"
                      "    for (int i = 0; i < N; i++)\n"
                      "        cards[i] = !cards[i];\n"
                      "}\n", NULL, NULL },
    [BOMB_EVENS] = { "void run(void) {\n"
                     "    for (int i = 0; i < N; i++) {\n"
                     "        if (i % 2 == 0) cards[i] = 1;\n"
                     "        else cards[i] = 0;\n"
                     "    }\n"
                     "}\n",
                     "void run(void) {\n"
                     "    for (int i = 0; i < N; i++)\n"
                     "        cards[i] = 1 - i % 2;\n"
                     "}\n", NULL, NULL },
    [BOMB_MIRROR] = { "void run(void) {\n"
                      "    for (int i = 0; i < N; i++)\n"
                      "        cards[N - 1 - i] = cards[i];\n"
                      "}\n",
                      "void run(void) {\n"
                      "    int left = 0;\n"
                      "    int right = N - 1;\n"
                      "    while (left < right) {\n"
                      "        cards[right] = cards[left];\n"
                      "        left++;\n"
                      "        right--;\n"
                      "    }\n"
                      "}\n", NULL, NULL },
    [BOMB_CHECKER] = { "void run(void) {\n"
                       "    for (int i = 0; i < ROWS; i++)\n"
                       "        for (int j = 0; j < COLS; j++) {\n"
                       "            if ((i + j) % 2 == 0) grid[i][j] = 0;\n"
                       "            else grid[i][j] = 1;\n"
                       "        }\n"
                       "}\n", NULL, NULL, NULL },
    [BOMB_BORDER] = { "void run(void) {\n"
                      "    for (int i = 0; i < ROWS; i++)\n"
                      "        for (int j = 0; j < COLS; j++) {\n"
                      "            if (i == 0 || i == ROWS - 1 || j == 0 ||\n"
                      "                j == COLS - 1)\n"
                      "                grid[i][j] = 1;\n"
                      "            else\n"
                      "                grid[i][j] = 0;\n"
                      "        }\n"
                      "}\n", NULL, NULL, NULL },
};

void run_variant_tests(void)
{
    Program *prog = malloc(sizeof *prog);
    VM *vm = malloc(sizeof *vm);
    ArrayTrace *t = malloc(sizeof *t);
    BombVariant *v = malloc(sizeof *v);
    if (!prog || !vm || !t || !v) {
        CHECK(false, "out of memory");
        free(prog), free(vm), free(t), free(v);
        return;
    }
    char why[256], what[512], cheat[2048];
    for (int k = 0; k < BOMB_KIND_COUNT; ++k) {
        BombKind kind = (BombKind)k;
        const SooLevel *base = base_level(kind);
        const char *src = answer(kind);
        bool shapes = true, all_pass = true, varied = false, odd = false;
        int first_cols = -1;
        for (unsigned seed = 1; seed <= 300; ++seed) {
            bomb_variant_make(v, kind, seed);
            const SooLevel *l = &v->level;
            if (l->rows * l->cols > SOO_MAX_CELLS || l->cols < 2 ||
                l->budget <= 0 || (base->rows > 1) != (l->rows > 1))
                shapes = false;
            if (first_cols < 0) first_cols = l->cols;
            if (l->cols != first_cols) varied = true;
            if (l->cols % 2) odd = true;
            if (!array_check_source(src, (int)strlen(src), l, prog, vm, t, why,
                                    sizeof why)) {
                if (all_pass)
                    printf("  %s seed %u: %s\n", base->title, seed, why);
                all_pass = false;
            }
            array_unmark_reverse(l);
        }
        /* the rule reproduces the shown bomb's goal exactly
           규칙이 화면 폭탄의 목표를 정확히 만든다 */
        int goal[SOO_MAX_CELLS];
        bomb_rule_goal(kind, base->rows, base->cols, base->initial, goal);
        bool same = true;
        for (int c = 0; c < base->rows * base->cols; ++c)
            if (goal[c] != base->goal[c]) same = false;
        snprintf(what, sizeof what, "%s: the rule gives the shown bomb's goal",
                 base->title);
        CHECK(same, what);

        /* every correct way of writing it defuses every random bomb
           올바른 모든 작성 방법이 모든 무작위 폭탄을 해체한다 */
        for (int a = 0; a < 4 && ALTERNATIVES[k][a]; ++a) {
            const char *alt = ALTERNATIVES[k][a];
            bool shown = array_check_source(alt, (int)strlen(alt), base, prog,
                                            vm, t, why, sizeof why);
            snprintf(what, sizeof what,
                     "%s: alternative answer %d defuses the shown bomb (%s)",
                     base->title, a + 1, shown ? "ok" : why);
            CHECK(shown, what);
            bool fair = true;
            for (unsigned seed = 1; seed <= 300 && fair; ++seed) {
                bomb_variant_make(v, kind, seed);
                if (!array_check_source(alt, (int)strlen(alt), &v->level, prog,
                                        vm, t, why, sizeof why)) {
                    fair = false;
                    printf("  %s alternative %d, random bomb %u: %s\n",
                           base->title, a + 1, seed, why);
                }
                array_unmark_reverse(&v->level);
            }
            snprintf(what, sizeof what,
                     "%s: alternative answer %d defuses 300 random bombs",
                     base->title, a + 1);
            CHECK(fair, what);
        }

        snprintf(what, sizeof what, "%s: random bombs fit the engine",
                 base->title);
        CHECK(shapes, what);
        snprintf(what, sizeof what, "%s: the size changes between bombs",
                 base->title);
        CHECK(varied, what);
        if (kind == BOMB_MIRROR) CHECK(odd, "Mirror: some bombs have odd N");
        snprintf(what, sizeof what, "%s: the answer defuses 300 random bombs",
                 base->title);
        CHECK(all_pass, what);

        /* the card-by-card cheat / 카드별 꼼수 */
        make_cheat(base, cheat, sizeof cheat);
        bool shown = array_check_source(cheat, (int)strlen(cheat), base, prog,
                                        vm, t, why, sizeof why);
        snprintf(what, sizeof what, "%s: card-by-card code defuses the shown "
                                    "bomb", base->title);
        CHECK(shown, what);
        if (kind == BOMB_L1) continue;   /* cards[2] = 0 IS the rule there
                                            여기서는 cards[2] = 0이 규칙 */
        bool caught = false;
        for (unsigned seed = 1; seed <= 10 && !caught; ++seed) {
            bomb_variant_make(v, kind, seed);
            if (!array_check_source(cheat, (int)strlen(cheat), &v->level, prog,
                                    vm, t, why, sizeof why))
                caught = true;
            array_unmark_reverse(&v->level);
        }
        snprintf(what, sizeof what, "%s: card-by-card code fails within 10 "
                                    "random bombs", base->title);
        CHECK(caught, what);
    }

    /* level 3's reverse rule also holds on random bombs
       레벨 3의 역방향 규칙은 무작위 폭탄에서도 유지된다 */
    bomb_variant_make(v, BOMB_L3, 5);
    const char *forward = array_example_solution(1);
    CHECK(!array_check_source(forward, (int)strlen(forward), &v->level, prog,
                              vm, t, why, sizeof why) &&
              strstr(why, "Reverse") != NULL,
          "random level 3 bombs still require the right-to-left scan");
    array_unmark_reverse(&v->level);

    for (int k = 0; k < BOMB_KIND_COUNT; ++k)
        for (int h = 0; h < BOMB_HINTS; ++h)
            CHECK(strlen(bomb_hint((BombKind)k, h)) > 20, "every bomb has hints");
    free(prog), free(vm), free(t), free(v);
}
