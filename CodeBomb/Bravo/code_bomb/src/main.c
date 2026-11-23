/*
 * main.c - Code Bomb / Circuit Pals: window, scaling and screens
 * main.c - Code Bomb / Circuit Pals: 창, 화면 배율, 화면 전환
 *
 * Screens / 화면:
 *   Missions  - the mission map (hub) and Sumin's 40 lessons
 *               미션 지도(중심 화면)와 수민의 40개 수업
 *   Bomb      - write C to defuse bombs 1-6 and the cube (3D bomb)
 *               C를 써서 폭탄 1-6과 큐브를 해체 (3D 폭탄)
 *   Mailroom  - the tutorial level with the 3D robot
 *               3D 로봇이 있는 튜토리얼 레벨
 *
 * Every screen draws on a 1920 x 1200 canvas (a render texture). The canvas
 * is scaled to the window with letterboxing, and the mouse is converted
 * back (Sumin's approach, so it behaves the same on macOS).
 * 모든 화면은 1920 x 1200 캔버스(렌더 텍스처)에 그린다. 캔버스를 비율을
 * 유지하며 창에 맞추고 마우스 좌표는 거꾸로 변환한다 (수민의 방식이라
 * macOS에서도 같게 동작한다).
 *
 * On a monitor of at least 1920 x 1200 the game starts in borderless full
 * screen, so the canvas is shown 1:1. F11 switches to a window.
 * 1920 x 1200 이상의 모니터에서는 테두리 없는 전체 화면으로 시작하여
 * 캔버스가 1:1로 보인다. F11로 창 모드로 바꾼다.
 *
 * QA / 품질 확인:
 *   codebomb --shot FILE.png SCREEN [MODE] [LEVEL] [STEPS] [VARIANT]
 *     SCREEN = map | lesson | arrays | mailroom
 *     e.g. --shot map.png map
 *          --shot l28.png lesson - 28          (lesson 28, answered)
 *          --shot b4.png arrays code 4 14 0    (bomb 4, 14 steps)
 *          --shot cube.png arrays code 7 6 22  (cube, 2 modules solved)
 *          --shot mail.png mailroom - 1 9 0
 *   codebomb --selftest      cube click picking + all 40 lessons
 *                            큐브 클릭 선택 + 40개 수업 전부 검사
 */
#include "editor_ui.h"
#include "screen_arrays.h"
#include "screen_mailroom.h"
#include "screen_missions.h"
#include "soo_lessons.h"
#include "ui.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_FONT_SIZE 20     /* multiple of 10 / 10의 배수 */

typedef enum { SCREEN_MAP, SCREEN_LESSON, SCREEN_BOMB, SCREEN_MAILROOM }
    ScreenId;

typedef struct {
    ScreenId        screen;
    MailroomScreen *mailroom;
    ArraysScreen   *arrays;
} Game;

static void leave_screen(Game *g)
{
    if (g->screen == SCREEN_MAILROOM) mailroom_leave(g->mailroom);
    if (g->screen == SCREEN_BOMB) arrays_leave(g->arrays);
    if (g->screen == SCREEN_LESSON) soo_lessons_leave();
}

static void go(Game *g, ScreenId to)
{
    if (g->screen != to) leave_screen(g);
    g->screen = to;
}

/* Tabs in the top-right. Missions also returns from a lesson to the map.
 * 오른쪽 위 탭. Missions는 수업에서 지도로 돌아가는 역할도 한다. */
static void draw_tabs(Game *g)
{
    static const char *const names[3] = { "Missions", "Bomb", "Mailroom" };
    static const ScreenId targets[3] = { SCREEN_MAP, SCREEN_BOMB, SCREEN_MAILROOM };
    for (int n = 0; n < 3; n++) {
        Rectangle r = { 1312 + n * 192, 30, 180, 52 };
        bool selected = g->screen == targets[n] ||
                        (n == 0 && g->screen == SCREEN_LESSON);
        if (ui_button(r, names[n], true, selected)) go(g, targets[n]);
    }
}

/* Optional monospace font in assets/, otherwise raylib's pixel font.
 * assets/의 고정폭 폰트(선택 사항). 없으면 raylib 픽셀 폰트. */
static Font load_code_font(bool *owned)
{
    char paths[2][512];
    snprintf(paths[0], sizeof paths[0], "assets/mono.ttf");
    snprintf(paths[1], sizeof paths[1], "%sassets/mono.ttf",
             GetApplicationDirectory());
    for (int i = 0; i < 2; ++i) {
        if (!FileExists(paths[i])) continue;
        Font f = LoadFontEx(paths[i], CODE_FONT_SIZE * 2, NULL, 0);
        if (f.texture.id != 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            *owned = true;
            return f;
        }
    }
    *owned = false;
    return GetFontDefault();
}

/* Size the window for this monitor. / 이 모니터에 맞게 창 크기를 정한다.
 * A fitted window is set first, so F11 always returns to a window that fits
 * the screen. Monitors of at least 1920 x 1200 then switch to borderless
 * full screen for a sharp 1:1 canvas.
 * 먼저 화면에 맞는 창 크기를 정해 두므로 F11을 누르면 항상 화면에 맞는 창으로
 * 돌아간다. 1920 x 1200 이상의 모니터는 그다음 테두리 없는 전체 화면으로
 * 바꿔 선명한 1:1 화면을 쓴다. */
static void fit_window(void)
{
    int monitor = GetCurrentMonitor();
    int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    if (mw <= 0 || mh <= 0) return;
    int width = mw * 90 / 100, height = mh * 88 / 100;
    if (width > height * UI_W / UI_H) width = height * UI_W / UI_H;
    if (width < 960) width = 960;
    height = width * UI_H / UI_W;
    SetWindowSize(width, height);
    SetWindowPosition((mw - width) / 2, (mh - height) / 2);
    if (mw >= UI_W && mh >= UI_H) ToggleBorderlessWindowed();
}

static int arg_int(int argc, char **argv, int i, int fallback)
{
    return argc > i ? atoi(argv[i]) : fallback;
}

/* --selftest: cube picking, then every lesson opened, answered and drawn.
 * --selftest: 큐브 선택 검사 후 모든 수업을 열고 답하고 그린다. */
static int self_test(Game *g, RenderTexture2D canvas)
{
    puts("Cube bomb click picking:");
    int failures = arrays_self_test(g->arrays);
    puts("Sumin's lessons (open, answer, play to the end, draw):");
    int lessons = 0;
    for (int level = 1; level <= 40; level++) {
        if (!soo_lessons_open(level)) continue;
        for (int pass = 0; pass < 2; pass++) {
            if (pass == 1 && !soo_lessons_qa(level)) failures++;
            for (int f = 0; f < 3; f++) {
                soo_lessons_update(1.0f / 60.0f);
                BeginTextureMode(canvas);
                soo_lessons_draw();
                EndTextureMode();
            }
        }
        lessons++;
    }
    printf("  %d lessons drawn\n", lessons);
    if (lessons != 40) failures++;
    return failures;
}

int main(int argc, char **argv)
{
    bool shot = argc > 2 && strcmp(argv[1], "--shot") == 0;
    bool selftest = argc > 1 && strcmp(argv[1], "--selftest") == 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(UI_W, UI_H, "Code Bomb | Circuit Pals (C + raylib)");
    if (!IsWindowReady()) {
        fputs("Could not open the raylib window. Run from a desktop session.\n",
              stderr);
        return EXIT_FAILURE;
    }
    SetExitKey(KEY_NULL);                    /* Esc belongs to the editor
                                                Esc는 에디터가 사용 */
    SetWindowMinSize(960, 600);
    if (!shot && !selftest) fit_window();
    SetTargetFPS(60);

    RenderTexture2D canvas = LoadRenderTexture(UI_W, UI_H);
    bool font_owned = false;
    Font code_font = load_code_font(&font_owned);
    EditorStyle style = editor_style_make(code_font, (float)CODE_FONT_SIZE);

    Game g;
    memset(&g, 0, sizeof g);
    g.screen = SCREEN_MAP;
    g.mailroom = mailroom_create(&style);
    g.arrays = arrays_create(&style);
    if (!g.mailroom || !g.arrays) {
        fputs("Out of memory\n", stderr);
        CloseWindow();
        return EXIT_FAILURE;
    }

    int exit_code = 0;
    if (selftest) {
        int failures = self_test(&g, canvas);
        puts(failures ? "SELFTEST FAILED" : "SELFTEST PASSED");
        exit_code = failures ? EXIT_FAILURE : EXIT_SUCCESS;
        goto done;
    }

    if (shot) {
        const char *screen = argv[3];
        const char *mode = argc > 4 ? argv[4] : "code";
        int level = arg_int(argc, argv, 5, 1);
        int steps = arg_int(argc, argv, 6, 0);
        int variant = arg_int(argc, argv, 7, 0);
        if (strcmp(screen, "map") == 0) {
            g.screen = SCREEN_MAP;
        } else if (strcmp(screen, "lesson") == 0) {
            g.screen = SCREEN_LESSON;
            soo_lessons_qa(level);
        } else if (strcmp(screen, "mailroom") == 0) {
            g.screen = SCREEN_MAILROOM;
            mailroom_qa(g.mailroom, level - 1, steps);
        } else {
            g.screen = SCREEN_BOMB;
            arrays_qa(g.arrays, mode, level - 1, steps, variant);
            arrays_qa_settle(g.arrays, 1.5f);
        }
    }

    int frames = 0;
    float filter_scale = -1.0f;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (sw <= 0 || sh <= 0) continue;

        /* Fit the canvas, keep its shape, centre it on whole pixels.
         * 캔버스를 비율 그대로 맞추고 정수 픽셀 위치에 가운데 정렬한다. */
        float scale = fminf((float)sw / UI_W, (float)sh / UI_H);
        float ox = floorf((sw - UI_W * scale) / 2);
        float oy = floorf((sh - UI_H * scale) / 2);
        ui_set_transform(scale, ox, oy);
        if (scale != filter_scale) {
            SetTextureFilter(canvas.texture, scale == 1.0f
                                                 ? TEXTURE_FILTER_POINT
                                                 : TEXTURE_FILTER_BILINEAR);
            filter_scale = scale;
        }

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        switch (g.screen) {
        case SCREEN_MAILROOM: mailroom_update(g.mailroom, dt); break;
        case SCREEN_BOMB:     arrays_update(g.arrays, dt); break;
        case SCREEN_LESSON:   soo_lessons_update(dt); break;
        default:              break;
        }

        if (g.screen == SCREEN_MAILROOM) mailroom_render_view(g.mailroom);
        if (g.screen == SCREEN_BOMB) arrays_render_view(g.arrays);

        BeginTextureMode(canvas);
        switch (g.screen) {
        case SCREEN_MAILROOM:
            mailroom_draw(g.mailroom);
            break;
        case SCREEN_BOMB:
            arrays_draw(g.arrays);
            break;
        case SCREEN_LESSON:
            if (soo_lessons_draw() == LESSON_OPEN_BUG) {
                go(&g, SCREEN_BOMB);
                arrays_open(g.arrays, 1, true);
            }
            break;
        default: {
            bool done_flags[MISSION_BOMBS];
            arrays_defused(g.arrays, done_flags);
            MissionPick pick = missions_draw(done_flags);
            if (pick.kind == PICK_LESSON && soo_lessons_open(pick.level)) {
                go(&g, SCREEN_LESSON);
            } else if (pick.kind == PICK_BOMB) {
                go(&g, SCREEN_BOMB);
                arrays_open(g.arrays, pick.level, false);
            } else if (pick.kind == PICK_MAILROOM) {
                go(&g, SCREEN_MAILROOM);
            }
            break;
        }
        }
        draw_tabs(&g);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(UI_LETTERBOX);
        DrawTexturePro(canvas.texture, (Rectangle){ 0, 0, UI_W, -UI_H },
                       (Rectangle){ ox, oy, UI_W * scale, UI_H * scale },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();

        if (shot && ++frames == 3) {
            TakeScreenshot(argv[2]);
            break;
        }
    }

done:
    arrays_destroy(g.arrays);
    mailroom_destroy(g.mailroom);
    if (font_owned) UnloadFont(code_font);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return exit_code;
}
