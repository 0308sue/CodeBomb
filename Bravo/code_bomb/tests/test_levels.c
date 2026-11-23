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
                run.expected_len > SCENE_MAX || run.expected_len < 1 ||
                run.side_expected_len > SCENE_MAX)
                fits = false;
            if (l->side_name && seed < 3 && run.side_expected_len < 1)
                fits = false;           /* the machine has work to show
                                           기계가 보여 줄 일이 있다 */
        }
        snprintf(what, sizeof what,
                 "level %d inboxes and outboxes hold 1-%d parcels", l->number,
                 SCENE_MAX);
        CHECK(fits, what);
    }
    CHECK(built == LEVEL_SLOTS, "all eleven Mailroom levels are built");

    /* Each mistake fails for the reason its Examples line gives.
     * 각 실수는 Examples 설명에 적힌 이유로 실패한다. */
    static const struct { int level; const char *example, *says; } WHY[] = {
        { 3, "No shredder", "damaged" },
        { 3, "Zero is damaged?", "shred(0): that parcel isn't damaged" },
        { 5, "Divide, not remainder", "is even" },
        { 5, "Swapped belts", "is even" },
        { 8, "Pressed once", "wrong parcel" },
        { 8, "No function", "int press(int n)" },
        { 10, "Same order", "wrong parcel" },
        { 10, "One past the end", "shelf[" },
        { 11, "Stores the damaged", "damaged" },
    };
    for (size_t k = 0; k < sizeof WHY / sizeof WHY[0]; ++k) {
        const LevelDef *l = LEVEL_LIST[WHY[k].level - 1];
        const LevelExample *ex = NULL;
        for (int e = 0; e < l->example_count; ++e)
            if (strcmp(l->examples[e].name, WHY[k].example) == 0)
                ex = &l->examples[e];
        CompileResult r;
        SubmitResult s;
        bool found = ex && build(l, ex->source, p, &r);
        if (found) level_submit(l, p, 5, &s);
        char what[700];
        snprintf(what, sizeof what, "level %d '%s' says \"%s\": %s",
                 WHY[k].level, WHY[k].example, WHY[k].says,
                 found ? s.message : "(missing)");
        CHECK(found && strstr(s.message, WHY[k].says) != NULL, what);
    }

    /* The shelf checks every index, like the bomb's cards[].
     * 선반은 폭탄의 cards[]처럼 모든 인덱스를 검사한다. */
    static const struct { const char *src, *says; } SHELF[] = {
        { "void run(void) { shelf[8] = take(); }", "shelf[8] is outside" },
        { "void run(void) { shelf[-1] = 1; }", "shelf[-1] is outside" },
        { "void run(void) { put(shelf[3]); }", "shelf[3] is empty" },
        { "void run(void) { put(SHELF_SIZE); }", "wrong parcel" },
        { "void run(void) { int shelf = 2; }", "level's array" },
    };
    for (size_t k = 0; k < sizeof SHELF / sizeof SHELF[0]; ++k) {
        CompileResult r;
        SubmitResult s;
        char what[700];
        if (!build(&LEVEL_BACK_SHELF, SHELF[k].src, p, &r)) {
            snprintf(what, sizeof what, "shelf: %s -> %s", SHELF[k].src,
                     r.message);
            CHECK(strstr(r.message, SHELF[k].says) != NULL, what);
            continue;
        }
        level_submit(&LEVEL_BACK_SHELF, p, 1, &s);
        snprintf(what, sizeof what, "shelf: %s -> %s", SHELF[k].src, s.message);
        CHECK(!s.passed && strstr(s.message, SHELF[k].says) != NULL, what);
    }

    /* Level 8 accepts any helper name, but not a helper that is never used.
     * 레벨 8은 도우미 이름은 자유지만, 쓰지 않는 도우미는 안 된다. */
    {
        CompileResult r;
        SubmitResult s;
        const char *renamed =
            "int stamp(int x) { return x * 3 + 1; }\n"
            "void run(void) { while (has_input()) {\n"
            "  int a = stamp(take()); put(stamp(a)); } }\n";
        CHECK(build(&LEVEL_STAMP_PRESS, renamed, p, &r), "renamed helper compiles");
        level_submit(&LEVEL_STAMP_PRESS, p, 5, &s);
        CHECK(s.passed, "level 8: a helper with another name passes");
        const char *unused =
            "int press(int n) { return 3 * n + 1; }\n"
            "void run(void) { while (has_input()) {\n"
            "  int b = take(); b = 3 * b + 1; put(3 * b + 1); } }\n";
        CHECK(build(&LEVEL_STAMP_PRESS, unused, p, &r), "unused helper compiles");
        level_submit(&LEVEL_STAMP_PRESS, p, 5, &s);
        CHECK(!s.passed, "level 8: a helper that is never called fails");
    }
    free(p);
}
