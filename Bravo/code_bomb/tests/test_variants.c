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
