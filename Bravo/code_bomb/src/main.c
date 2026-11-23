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
 *          --shot h6.png help - 6 1        (field manual 6, page 2)
 *   codebomb --selftest      cube click picking, all 40 lessons and every
 *                            page of the field manual
 *                            큐브 클릭 선택, 40개 수업, 매뉴얼 모든 페이지
 */
#include "editor_ui.h"
#include "help.h"
#include "level.h"
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
    bool            quitting;    /* "Quit Code Bomb?" is showing
                                    종료 확인 창이 떠 있음 */
    bool            quit;
} Game;

static void leave_screen(Game *g)
{
    if (g->screen == SCREEN_MAILROOM) mailroom_leave(g->mailroom);
    if (g->screen == SCREEN_BOMB) arrays_leave(g->arrays);
    if (g->screen == SCREEN_LESSON) soo_lessons_leave();
}

/* Opening a screen for the first time also opens its field manual.
 * 화면을 처음 열면 그 화면의 현장 매뉴얼도 열린다. */
static void go(Game *g, ScreenId to)
{
    if (g->screen != to) leave_screen(g);
    g->screen = to;
    if (to == SCREEN_BOMB) help_open_once(HELP_BOMB);
    if (to == SCREEN_MAILROOM)
        help_open_once((HelpTopic)mailroom_help_topic(g->mailroom));
    if (to == SCREEN_LESSON)
        help_open_once(help_topic_for_level(soo_lessons_level()));
}

/* Tabs in the top-right, the Exit button, and the screen's Help button.
 * Missions also returns from a lesson to the map.
 * 오른쪽 위 탭, Exit 버튼, 화면의 Help 버튼. Missions는 수업에서 지도로
 * 돌아가는 역할도 한다. */
static void draw_tabs(Game *g)
{
    static const char *const names[3] = { "Missions", "Bomb", "Mailroom" };
    static const ScreenId targets[3] = { SCREEN_MAP, SCREEN_BOMB, SCREEN_MAILROOM };
    for (int n = 0; n < 3; n++) {
        Rectangle r = { 1134 + n * 192, 30, 180, 52 };
        bool selected = g->screen == targets[n] ||
                        (n == 0 && g->screen == SCREEN_LESSON);
        if (ui_button(r, names[n], true, selected)) go(g, targets[n]);
    }
    if (ui_button((Rectangle){ 1134 + 3 * 192, 30, 180, 52 }, "Exit", true,
                  false))
        g->quitting = true;

    Rectangle help = { 972, 30, 150, 52 };
    if (g->screen == SCREEN_BOMB) help_button(help, HELP_BOMB);
    if (g->screen == SCREEN_MAILROOM)
        help_button(help, (HelpTopic)mailroom_help_topic(g->mailroom));
    if (g->screen == SCREEN_LESSON)
        help_button(help, help_topic_for_level(soo_lessons_level()));
}

/* "Quit Code Bomb?" pop-up. Enter quits, Esc cancels.
 * "Quit Code Bomb?" 팝업. Enter는 종료, Esc는 취소. */
static void draw_quit(Game *g)
{
    DrawRectangle(0, 0, UI_W, UI_H, (Color){ 8, 5, 18, 190 });
    Rectangle card = { UI_W / 2 - 340, UI_H / 2 - 150, 680, 300 };
    DrawRectangleRounded(card, 0.1f, 8, UI_PANEL);
    DrawRectangleRoundedLinesEx(card, 0.1f, 8, 3.0f, UI_ACCENT);
    const char *t = "Quit Code Bomb?";
    DrawText(t, UI_W / 2 - MeasureText(t, FS_TITLE) / 2, (int)card.y + 46,
             FS_TITLE, UI_INK);
    const char *s = "Progress in this session is not saved.";
    DrawText(s, UI_W / 2 - MeasureText(s, FS_SMALL) / 2, (int)card.y + 110,
             FS_SMALL, UI_MUTED);
    if (ui_button((Rectangle){ card.x + 70, card.y + 190, 250, 64 }, "Quit",
                  true, true))
        g->quit = true;
    if (ui_button((Rectangle){ card.x + card.width - 320, card.y + 190, 250,
                               64 },
                  "Cancel", true, false))
        g->quitting = false;
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
/* One frame of the Bomb screen as the main loop runs it while a pop-up is
 * open: only the clock ticks, then the 3D view is drawn.
 * 팝업이 열려 있을 때 메인 루프가 실행하는 Bomb 화면 한 프레임: 시계만 가고
 * 3D 화면을 그린다. */
static void bomb_frame_under_popup(Game *g)
{
    arrays_tick(g->arrays, 1.0f / 60.0f);
    arrays_render_view(g->arrays);
}

static int self_test(Game *g, RenderTexture2D canvas)
{
    /* Regression: the first click on the Bomb tab opens its Help page
     * before the Bomb screen has ever updated.
     * 회귀 검사: Bomb 탭을 처음 누르면 Bomb 화면이 한 번도 업데이트되기 전에
     * 도움말이 열린다. */
    puts("First visit to the Bomb screen with Help open:");
    go(g, SCREEN_BOMB);
    int failures = help_active() ? 0 : 1;
    for (int f = 0; f < 3; f++) bomb_frame_under_popup(g);
    puts("  drawn without crashing ok");
    puts("First visit to the Mailroom with Help open:");
    go(g, SCREEN_MAILROOM);
    if (!help_active()) failures++;
    for (int f = 0; f < 3; f++) mailroom_render_view(g->mailroom);
    puts("  drawn without crashing ok");
    leave_screen(g);
    g->screen = SCREEN_MAP;

    failures += arrays_self_test(g->arrays);
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
    puts("Mailroom levels (open, run the answer, draw):");
    int levels = 0;
    for (int slot = 0; slot < LEVEL_SLOTS; slot++) {
        if (!mailroom_select_level(g->mailroom, slot)) continue;
        help_close();
        mailroom_qa(g->mailroom, 9, 400);
        mailroom_render_view(g->mailroom);
        BeginTextureMode(canvas);
        mailroom_draw(g->mailroom);
        EndTextureMode();
        levels++;
    }
    mailroom_select_level(g->mailroom, 0);
    help_close();
    printf("  %d levels drawn\n", levels);
    if (levels < 6) failures++;
    puts("Field manual (every page of every topic):");
    int pages = 0;
    for (int t = 0; t < HELP_TOPIC_COUNT; t++) {
        help_open((HelpTopic)t);
        for (int p = 0; p < help_page_count((HelpTopic)t); p++) {
            help_set_page(p);
            BeginTextureMode(canvas);
            help_draw();
            EndTextureMode();
            pages++;
        }
        if (help_page_count((HelpTopic)t) < 2) failures++;
    }
    printf("  %d pages drawn\n", pages);
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
        } else if (strcmp(screen, "help") == 0) {
            g.screen = SCREEN_MAP;
            help_open((HelpTopic)(level - 1));
            help_set_page(steps);
        } else if (strcmp(screen, "quit") == 0) {
            g.screen = SCREEN_MAP;
            g.quitting = true;
        } else if (strcmp(screen, "lesson") == 0) {
            g.screen = SCREEN_LESSON;
            soo_lessons_qa(level);
        } else if (strcmp(screen, "mailroom") == 0) {
            g.screen = SCREEN_MAILROOM;
            /* variant = Mailroom level 1-11 / variant = 우편실 레벨 1-11 */
            if (variant > 1) {
                mailroom_select_level(g.mailroom, variant - 1);
                help_close();                   /* keep the shot clean */
            }
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
        /* A pop-up takes the keyboard; the screen underneath only keeps
         * its clock. / 팝업이 키보드를 가져가고, 아래 화면은 시계만 간다. */
        bool popup = help_active() || g.quitting;
        if (help_active()) {
            help_update();
        } else if (g.quitting) {
            if (IsKeyPressed(KEY_ESCAPE)) g.quitting = false;
            if (IsKeyPressed(KEY_ENTER)) g.quit = true;
            while (GetCharPressed() > 0) {
            }
        } else {
            switch (g.screen) {
            case SCREEN_MAILROOM: mailroom_update(g.mailroom, dt); break;
            case SCREEN_BOMB:     arrays_update(g.arrays, dt); break;
            case SCREEN_LESSON:   soo_lessons_update(dt); break;
            default:              break;
            }
        }
        if (popup && g.screen == SCREEN_BOMB) arrays_tick(g.arrays, dt);

        if (g.screen == SCREEN_MAILROOM) mailroom_render_view(g.mailroom);
        if (g.screen == SCREEN_BOMB) arrays_render_view(g.arrays);

        BeginTextureMode(canvas);
        ui_block_input(popup);
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
        ui_block_input(false);
        if (help_active()) help_draw();
        else if (g.quitting) draw_quit(&g);
        EndTextureMode();
        if (g.quit) break;

        BeginDrawing();
        ClearBackground(UI_LETTERBOX);
        ui_draw_view(canvas.texture, (Rectangle){ 0, 0, UI_W, -UI_H },
                     (Rectangle){ ox, oy, UI_W * scale, UI_H * scale });
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
