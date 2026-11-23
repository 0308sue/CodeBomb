/*
 * test_levels.c - every Mailroom level: answers pass, mistakes fail
 * test_levels.c - 모든 우편실 레벨: 정답은 통과, 실수는 실패
 *
 * For each built level: the starter compiles, the hidden reference answer
 * passes Submit on 50 random inboxes and meets the level's par, every
 * example in its Examples menu fails, and the inboxes fit the scene.
 * 만들어진 레벨마다: 시작 코드가 컴파일되고, 숨겨진 정답이 무작위 입력함 50개로
 * Submit을 통과하며 기준(par)을 만족하고, Examples 메뉴의 모든 예제는
 * 실패하며, 입력함이 장면에 들어맞는다.
 */
#include "test_util.h"

#include "compiler.h"
#include "level.h"

#include <stdlib.h>
#include <string.h>

#define SCENE_MAX 9      /* parcels that fit on a belt / 벨트에 들어가는 소포 */

static bool build(const LevelDef *l, const char *src, Program *p,
                  CompileResult *r)
{
    prog_init(p);
    l->register_natives(p);
    return compile_program(src, (int)strlen(src), p, r);
}

void run_level_tests(void)
{
    Program *p = malloc(sizeof *p);
    if (!p) {
        CHECK(false, "out of memory");
        return;
    }
    int built = 0;
    for (int slot = 0; slot < LEVEL_SLOTS; ++slot) {
        const LevelDef *l = LEVEL_LIST[slot];
        if (!l) continue;
        built++;
        char what[700];
        CompileResult r;

        snprintf(what, sizeof what, "level %d number matches its slot",
                 l->number);
        CHECK(l->number == slot + 1, what);

        snprintf(what, sizeof what, "level %d starter compiles", l->number);
        CHECK(build(l, l->starter, p, &r), what);

        bool ok = build(l, l->reference, p, &r);
        snprintf(what, sizeof what, "level %d reference compiles: %s",
                 l->number, ok ? "" : r.message);
        CHECK(ok, what);
        SubmitResult s;
        level_submit(l, p, 50, &s);
        snprintf(what, sizeof what, "level %d reference passes 50 tests: %s",
                 l->number, s.message);
        CHECK(s.passed, what);
        /* par is judged on the same 5 tests as the game's Submit
           기준은 게임의 Submit과 같은 5개 테스트로 판단한다 */
        SubmitResult s5;
        level_submit(l, p, 5, &s5);
        snprintf(what, sizeof what,
                 "level %d reference meets par (size %d <= %d, speed %.1f <= "
                 "%d)",
                 l->number, r.statements, l->par_size, s5.avg_steps,
                 l->par_speed);
        CHECK(r.statements <= l->par_size && s5.avg_steps <= l->par_speed,
              what);
        printf("  level %d %-14s size %d  speed %.1f\n", l->number, l->title,
               r.statements, s5.avg_steps);

        for (int e = 0; e < l->example_count; ++e) {
            const LevelExample *ex = &l->examples[e];
            bool compiled = build(l, ex->source, p, &r);
            snprintf(what, sizeof what, "level %d example '%s' compiles: %s",
                     l->number, ex->name, compiled ? "" : r.message);
            CHECK(compiled, what);
            level_submit(l, p, 5, &s);
            snprintf(what, sizeof what, "level %d example '%s' fails Submit",
                     l->number, ex->name);
            CHECK(!s.passed, what);
        }

        bool fits = true;
        for (unsigned seed = 0; seed < 200; ++seed) {
            LevelRun run;
            level_run_init(&run, l, seed, true);
            if (run.inbox_len < 1 || run.inbox_len > SCENE_MAX ||
                run.expected_len > SCENE_MAX || run.expected_len < 1)
                fits = false;
        }
        snprintf(what, sizeof what,
                 "level %d inboxes and outboxes hold 1-%d parcels", l->number,
                 SCENE_MAX);
        CHECK(fits, what);
    }
    CHECK(built >= 6, "at least six Mailroom levels are built");
    free(p);
}
