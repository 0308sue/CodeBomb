/*
 * soo_lessons.c - hosts Sumin's lesson screens at 1920 x 1200
 * soo_lessons.c - 수민의 수업 화면을 1920 x 1200으로 실행한다
 *
 * How it works / 동작 방식:
 *   1. Scaled versions of the drawing functions her views use are defined
 *      below (text, rounded rectangles, lines, circles, triangles).
 *      수민의 화면이 쓰는 그리기 함수(글자, 둥근 사각형, 선, 원, 삼각형)의
 *      배율 버전을 아래에 정의한다.
 *   2. Object-like macros point her calls at them, then main_SOO.c is
 *      included with its main() renamed, so her Workshop, Button, Wrapped,
 *      DrawIntro, DrawStage2 and the stage 3-8 views compile unchanged.
 *      객체형 매크로로 수민의 호출을 그 함수들로 돌린 뒤, main()의 이름을
 *      바꿔 main_SOO.c를 포함한다. 그래서 Workshop, Button, Wrapped,
 *      DrawIntro, DrawStage2와 3-8단계 화면이 수정 없이 컴파일된다.
 *   3. Her main() can't be called, so the per-lesson keyboard handling from
 *      it is repeated in soo_lessons_update() (same keys, same timings).
 *      수민의 main()은 호출할 수 없으므로 그 안의 수업별 키보드 처리를
 *      soo_lessons_update()에 똑같이 옮겼다 (같은 키, 같은 시간).
 *
 * If a new stage is added to her map, add its route to open_route(),
 * update_lesson() and draw_lesson() below.
 * 수민의 지도에 새 단계가 생기면 아래 open_route(), update_lesson(),
 * draw_lesson()에 그 경로를 추가한다.
 */
#include "soo_lessons.h"

#include "raylib.h"
#include "ui.h"

#include <math.h>
#include <string.h>

/* ========================================================================
 * Scaled drawing (her 1280 x 800 -> our 1920 x 1200)
 * 배율 그리기 (수민의 1280 x 800 -> 우리 1920 x 1200)
 * ====================================================================== */

#define HOST_SCALE 1.5f

/* Round her font size x 1.5 to a multiple of 10: 18 and below -> 20,
 * 19-25 -> 30, 26-32 -> 40.
 * 수민의 글자 크기 x 1.5를 10의 배수로 맞춘다: 18 이하 -> 20, 19-25 -> 30,
 * 26-32 -> 40. */
static int host_font(int size)
{
    int s = (int)((size * HOST_SCALE + 1.6f) / 10.0f) * 10;
    return s < 20 ? 20 : s;
}

static float hs(float v) { return v * HOST_SCALE; }

static Rectangle host_rect(Rectangle r)
{
    return (Rectangle){ hs(r.x), hs(r.y), hs(r.width), hs(r.height) };
}

static Vector2 host_vec(Vector2 v) { return (Vector2){ hs(v.x), hs(v.y) }; }

/* Text keeps her line spacing (size + 2) for multi-line code, and is
 * centred on the line she intended when the rounded size differs.
 * 여러 줄 코드는 수민의 줄 간격(size + 2)을 지키고, 맞춘 크기가 다르면
 * 수민이 의도한 줄의 가운데에 놓는다. */
static void host_DrawText(const char *text, int x, int y, int size, Color c)
{
    int s = host_font(size);
    float pitch = hs((float)size + 2.0f);
    float yy = hs((float)y) + (hs((float)size) - (float)s) * 0.5f;
    char line[256];
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if (n > sizeof line - 1) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        DrawText(line, (int)roundf(hs((float)x)), (int)roundf(yy), s, c);
        yy += pitch;
        if (!end) break;
        p = end + 1;
    }
}

static int host_MeasureText(const char *text, int size)
{
    return (int)roundf((float)MeasureText(text, host_font(size)) / HOST_SCALE);
}

static void host_DrawRectangleRounded(Rectangle r, float roundness, int segments,
                                      Color c)
{
    DrawRectangleRounded(host_rect(r), roundness, segments, c);
}

static void host_DrawRectangleRoundedLinesEx(Rectangle r, float roundness,
                                             int segments, float thick, Color c)
{
    DrawRectangleRoundedLinesEx(host_rect(r), roundness, segments, hs(thick), c);
}

static void host_DrawRectangle(int x, int y, int w, int h, Color c)
{
    DrawRectangleRec(host_rect((Rectangle){ (float)x, (float)y, (float)w,
                                            (float)h }), c);
}

static void host_DrawCircle(int x, int y, float r, Color c)
{
    DrawCircleV((Vector2){ hs((float)x), hs((float)y) }, hs(r), c);
}

static void host_DrawLine(int x1, int y1, int x2, int y2, Color c)
{
    DrawLineEx((Vector2){ hs((float)x1), hs((float)y1) },
               (Vector2){ hs((float)x2), hs((float)y2) }, HOST_SCALE, c);
}

static void host_DrawLineEx(Vector2 a, Vector2 b, float thick, Color c)
{
    DrawLineEx(host_vec(a), host_vec(b), hs(thick), c);
}

static void host_DrawTriangle(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(host_vec(a), host_vec(b), host_vec(c), col);
}

/* ========================================================================
 * Sumin's code, compiled unchanged / 수민의 코드 (수정 없이 컴파일)
 * ====================================================================== */

#define DrawText                    host_DrawText
#define MeasureText                 host_MeasureText
#define DrawRectangleRounded        host_DrawRectangleRounded
#define DrawRectangleRoundedLinesEx host_DrawRectangleRoundedLinesEx
#define DrawRectangle               host_DrawRectangle
#define DrawCircle                  host_DrawCircle
#define DrawLine                    host_DrawLine
#define DrawLineEx                  host_DrawLineEx
#define DrawTriangle                host_DrawTriangle
#define main                        soo_standalone_main   /* never called */

/* Her standalone screens (DrawApp, DrawRoadmap, DrawPointer, ...) are not
 * used here: Code Bomb has its own versions of those.
 * 수민의 단독 앱 화면(DrawApp, DrawRoadmap, DrawPointer 등)은 여기서 쓰지
 * 않는다: Code Bomb에 자체 버전이 있다. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "main_SOO.c"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#undef main
#undef DrawText
#undef MeasureText
#undef DrawRectangleRounded
#undef DrawRectangleRoundedLinesEx
#undef DrawRectangle
#undef DrawCircle
#undef DrawLine
#undef DrawLineEx
#undef DrawTriangle

/* ========================================================================
 * Lesson state (one of each, like her main()) / 수업 상태
 * ====================================================================== */

static int            host_level;          /* map level 1-40, 0 = none */
static int            host_route;          /* SOO_ROUTES value          */
static IntroView      host_intro;
static PointerLesson  host_pointer;
static ArrayLesson    host_array;
static GridLesson     host_grid;
static StringLesson   host_string;
static ListLesson     host_list;
static MemoryLesson   host_memory;
static VectorLesson   host_vector;
static VectorBossView host_boss;

static int stage_of(int route) { return route / 10; }   /* 11-16 -> 1 ... */

static bool open_route(int route)
{
    int k = route % 10 - 1;
    switch (stage_of(route)) {
    case 1: IntroLoad(&host_intro, k); break;
    case 2: PointerLessonLoad(&host_pointer, k); break;
    case 3: ArrayLessonLoad(&host_array, k); break;
    case 4: GridLessonLoad(&host_grid, k); break;
    case 5: StringLoad(&host_string, k); break;
    case 6: ListLoad(&host_list, k); break;
    case 7: MemoryLoad(&host_memory, k); break;
    case 8:
        VectorLoad(&host_vector, k, route == 82 ? 1 : 0);
        BossReset(&host_boss);
        break;
    default: return false;
    }
    return true;
}

bool soo_lessons_open(int level)
{
    if (level < 1 || level > 40 || SOO_ROUTES[level] == 0) return false;
    if (!open_route(SOO_ROUTES[level])) return false;
    host_level = level;
    host_route = SOO_ROUTES[level];
    return true;
}

int soo_lessons_level(void) { return host_level; }

void soo_lessons_leave(void)
{
    host_intro.playing = host_pointer.playing = host_array.playing = false;
    host_grid.playing = host_string.playing = host_list.playing = false;
    host_memory.playing = host_vector.playing = false;
}

/* ---- keyboard and timers, as in her main() / 수민의 main()과 같은 키 ---- */

void soo_lessons_update(float dt)
{
    switch (stage_of(host_route)) {
    case 8: {
        VectorLesson *v = &host_vector;
        if (v->level == 2) {
            if (IsKeyPressed(KEY_R)) BossReset(&host_boss);
            if (IsKeyPressed(KEY_LEFT) && host_boss.step > 0) host_boss.step--;
        } else {
            if (IsKeyPressed(KEY_R)) VectorLoad(v, v->level, v->level == 1 ? 1 : 0);
            if (v->answer < 0)
                for (int k = 0; k < 3; k++)
                    if (IsKeyPressed(KEY_A + k)) VectorChoose(v, k);
            bool more = v->answer >= 0 && v->step + 1 < v->n;
            if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
            if (more && IsKeyPressed(KEY_RIGHT)) {
                v->playing = false;
                VectorNext(v);
            }
            if (IsKeyPressed(KEY_LEFT)) VectorBack(v);
            if (v->playing && v->step + 1 < v->n) {
                v->timer += dt;
                if (v->timer >= 0.85f) {
                    v->timer = 0;
                    VectorNext(v);
                }
            }
        }
        break;
    }
    case 7: {
        MemoryLesson *v = &host_memory;
        if (IsKeyPressed(KEY_R)) MemoryLoad(v, v->level);
        if (v->answer < 0)
            for (int k = 0; k < 3; k++)
                if (IsKeyPressed(KEY_A + k)) MemoryChoose(v, k);
        bool more = v->answer >= 0 && v->step + 1 < v->n;
        if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
        if (more && IsKeyPressed(KEY_RIGHT)) {
            v->playing = false;
            MemoryNext(v);
        }
        if (IsKeyPressed(KEY_LEFT)) MemoryBack(v);
        if (v->playing && v->step + 1 < v->n) {
            v->timer += dt;
            if (v->timer >= 0.85f) {
                v->timer = 0;
                MemoryNext(v);
            }
        }
        break;
    }
    case 6: {
        ListLesson *v = &host_list;
        if (IsKeyPressed(KEY_R)) ListLoad(v, v->level);
        if (v->answer < 0)
            for (int k = 0; k < 3; k++)
                if (IsKeyPressed(KEY_A + k)) ListChoose(v, k);
        bool more = v->answer >= 0 && v->step + 1 < v->n;
        if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
        if (more && IsKeyPressed(KEY_RIGHT)) {
            v->playing = false;
            ListAdvance(v);
        }
        if (IsKeyPressed(KEY_LEFT)) ListBack(v);
        if (v->playing && v->step + 1 < v->n) {
            v->timer += dt;
            if (v->timer >= 0.85f) {
                v->timer = 0;
                ListAdvance(v);
            }
        }
        break;
    }
    case 5: {
        StringLesson *v = &host_string;
        if (v->level == 4 && v->answer < 0) {
            /* password box: type up to 8 ASCII characters
               암호 입력: ASCII 문자 최대 8개 */
            char previous[STRING_CAP];
            strcpy(previous, v->input);
            int ch;
            while ((ch = GetCharPressed()) > 0) {
                size_t len = strlen(v->input);
                if (ch >= 32 && ch <= 126 && len < 8) {
                    v->input[len] = (char)ch;
                    v->input[len + 1] = 0;
                }
            }
            size_t len = strlen(v->input);
            if (IsKeyPressed(KEY_BACKSPACE) && len) v->input[len - 1] = 0;
            if (strcmp(previous, v->input) != 0) StringBuild(v);
            if (IsKeyPressed(KEY_ENTER)) {
                StringBuild(v);
                v->answer = 0;
                v->playing = true;
            }
        } else {
            while (GetCharPressed() > 0) {
            }
            if (IsKeyPressed(KEY_R)) StringLoad(v, v->level);
            if (v->answer < 0)
                for (int k = 0; k < 3; k++)
                    if (IsKeyPressed(KEY_A + k)) {
                        v->answer = k;
                        v->playing = true;
                    }
        }
        bool more = v->answer >= 0 && v->step + 1 < v->n;
        if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
        if (more && IsKeyPressed(KEY_RIGHT)) {
            v->playing = false;
            v->step++;
        }
        if (v->step > 0 && IsKeyPressed(KEY_LEFT)) {
            v->playing = false;
            v->step--;
        }
        if (v->playing && v->step + 1 < v->n) {
            v->timer += dt;
            if (v->timer >= 0.65f) {
                v->timer = 0;
                v->step++;
            }
        }
        if (v->step + 1 == v->n) v->playing = false;
        break;
    }
    case 4: {
        GridLesson *v = &host_grid;
        if (IsKeyPressed(KEY_R)) GridLessonLoad(v, v->level);
        if (v->answer < 0)
            for (int k = 0; k < 3; k++)
                if (IsKeyPressed(KEY_A + k)) {
                    v->answer = k;
                    v->playing = true;
                }
        bool more = v->answer >= 0 && v->step + 1 < v->n;
        if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
        if (more && IsKeyPressed(KEY_RIGHT)) {
            v->playing = false;
            v->step++;
        }
        if (v->step > 0 && IsKeyPressed(KEY_LEFT)) {
            v->playing = false;
            v->step--;
        }
        if (v->playing && v->step + 1 < v->n) {
            v->timer += dt;
            if (v->timer >= 0.65f) {
                v->timer = 0;
                v->step++;
            }
        }
        if (v->step + 1 == v->n) v->playing = false;
        break;
    }
    case 3: {
        ArrayLesson *v = &host_array;
        if (IsKeyPressed(KEY_R)) ArrayLessonLoad(v, v->level);
        if (v->level != 5) {
            if (v->answer < 0)
                for (int k = 0; k < 3; k++)
                    if (IsKeyPressed(KEY_A + k)) {
                        v->answer = k;
                        v->playing = true;
                    }
            bool more = v->answer >= 0 && v->step + 1 < v->n;
            if (more && IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
            if (more && IsKeyPressed(KEY_RIGHT)) {
                v->playing = false;
                v->step++;
            }
            if (v->step > 0 && IsKeyPressed(KEY_LEFT)) {
                v->playing = false;
                v->step--;
            }
            if (v->playing && v->step + 1 < v->n) {
                v->timer += dt;
                if (v->timer >= 0.85f) {
                    v->timer = 0;
                    v->step++;
                }
            }
            if (v->step + 1 == v->n) v->playing = false;
        }
        break;
    }
    case 2: {
        PointerLesson *v = &host_pointer;
        if (IsKeyPressed(KEY_R)) PointerLessonLoad(v, v->mission);
        if (v->answer < 0)
            for (int k = 0; k < 3; k++)
                if (IsKeyPressed(KEY_A + k)) {
                    v->answer = k;
                    v->playing = true;
                }
        if (v->answer >= 0 && v->step + 1 < v->count) {
            if (IsKeyPressed(KEY_SPACE)) v->playing = !v->playing;
            if (IsKeyPressed(KEY_RIGHT)) {
                v->playing = false;
                v->step++;
            }
        }
        if (IsKeyPressed(KEY_LEFT) && v->step > 0) {
            v->playing = false;
            v->step--;
        }
        if (v->playing && v->step + 1 < v->count) {
            v->timer += dt;
            if (v->timer >= 0.85f) {
                v->timer = 0;
                v->step++;
            }
        }
        if (v->step + 1 == v->count) v->playing = false;
        break;
    }
    case 1: {
        IntroView *v = &host_intro;
        if (IsKeyPressed(KEY_R)) IntroLoad(v, v->level);
        if (IsKeyPressed(KEY_A)) IntroChoose(v, 0);
        if (IsKeyPressed(KEY_B)) IntroChoose(v, 1);
        if (IsKeyPressed(KEY_C)) IntroChoose(v, 2);
        if (IsKeyPressed(KEY_SPACE) && v->answer >= 0 && v->step < v->count)
            v->playing = !v->playing;
        if (IsKeyPressed(KEY_RIGHT)) {
            v->playing = false;
            IntroNext(v);
        }
        if (IsKeyPressed(KEY_LEFT) && v->step > 0) {
            v->playing = false;
            v->step--;
        }
        if (v->playing) {
            v->timer += dt;
            if (v->timer >= 0.65f) {
                v->timer = 0;
                IntroNext(v);
            }
        }
        break;
    }
    default:
        break;
    }
}

LessonAction soo_lessons_draw(void)
{
    /* Her views take the mouse in her 1280 x 800 coordinates.
     * 수민의 화면은 1280 x 800 좌표의 마우스를 받는다. */
    Vector2 m = ui_mouse();
    Vector2 mouse = { m.x / HOST_SCALE, m.y / HOST_SCALE };
    LessonAction action = LESSON_NONE;

    switch (stage_of(host_route)) {
    case 1:
        DrawIntro(&host_intro, mouse);
        /* her "Find the bug" link from Stage 1, at her position
           수민의 위치에 있는 Stage 1의 "Find the bug" 연결 */
        if (Button((Rectangle){ 1060, 58, 185, 42 }, "Find the bug", mouse, true,
                   false)) {
            host_intro.playing = false;
            action = LESSON_OPEN_BUG;
        }
        break;
    case 2: DrawStage2(&host_pointer, mouse); break;
    case 3: DrawStage3(&host_array, mouse); break;
    case 4: DrawStage4(&host_grid, mouse); break;
    case 5: DrawStage5(&host_string, mouse); break;
    case 6: DrawStage6(&host_list, mouse); break;
    case 7: DrawStage7(&host_memory, mouse); break;
    case 8: DrawStage8(&host_vector, &host_boss, mouse); break;
    default: ui_background(); break;
    }
    return action;
}

/* Same set-ups as her --stageN-smoke options. / 수민의 smoke 옵션과 같은 설정. */
bool soo_lessons_qa(int level)
{
    if (!soo_lessons_open(level)) return false;
    switch (stage_of(host_route)) {
    case 8:
        if (host_vector.level == 2) {
            BossAct(&host_boss, BOSS_SCAN);
            BossAct(&host_boss, BOSS_SCAN);
            BossAct(&host_boss, BOSS_ERASE);
            BossAct(&host_boss, BOSS_SCAN);
            BossAct(&host_boss, BOSS_SCAN);
            BossAct(&host_boss, BOSS_PUSH);
            BossAct(&host_boss, BOSS_CHECK);
        } else {
            VectorChoose(&host_vector, host_vector.level == 0 ? 1 : 2);
            host_vector.step = host_vector.n - 1;
            host_vector.playing = false;
        }
        break;
    case 7:
        MemoryChoose(&host_memory, MEMORY_TASKS[host_memory.level].answer);
        host_memory.step = host_memory.n - 1;
        host_memory.playing = false;
        break;
    case 6:
        ListChoose(&host_list, LIST_TASKS[host_list.level].answer);
        host_list.step = host_list.n - 1;
        host_list.playing = false;
        break;
    case 5:
        host_string.answer = STRING_TASKS[host_string.level].answer;
        host_string.step = host_string.n - 1;
        break;
    case 4:
        host_grid.answer = GRID_TASKS[host_grid.level].answer;
        host_grid.step = host_grid.n - 1;
        break;
    case 3:
        if (host_array.level == 5) {
            ArrayPick(&host_array, 0);
            ArrayPick(&host_array, 4);
        } else {
            host_array.answer = ARRAY_TASKS[host_array.level].answer;
            host_array.step = host_array.n - 1;
        }
        break;
    case 2:
        host_pointer.answer = POINTER_TASKS[host_pointer.mission].answer;
        host_pointer.step = host_pointer.count - 1;
        break;
    case 1:
        host_intro.answer = SOO_INTRO[host_intro.level].answer;
        host_intro.step = host_intro.count;
        break;
    default:
        break;
    }
    return true;
}
