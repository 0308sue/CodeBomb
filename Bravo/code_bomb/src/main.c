/*
 * main.c - Code Bomb / Circuit Pals: window, scaling and module tabs
 * main.c - Code Bomb / Circuit Pals: 창, 화면 배율, 모듈 탭
 *
 * Every screen draws on a 1920 x 1200 canvas (a render texture). The canvas
 * is scaled to the window with letterboxing, and the mouse is converted
 * back (Sumin's approach, unchanged, so it behaves the same on macOS).
 * 모든 화면은 1920 x 1200 캔버스(렌더 텍스처)에 그린다. 캔버스를 비율을
 * 유지하며 창에 맞추고 마우스 좌표는 거꾸로 변환한다 (수민의 방식 그대로,
 * 그래서 macOS에서도 같게 동작한다).
 *
 * On a monitor of at least 1920 x 1200 the game starts in borderless full
 * screen, so the canvas is shown 1:1 and the pixel font is perfectly sharp.
 * F11 switches between full screen and a window.
 * 1920 x 1200 이상의 모니터에서는 테두리 없는 전체 화면으로 시작하므로
 * 캔버스가 1:1로 보이고 픽셀 폰트가 아주 선명하다. F11로 전체 화면과 창을
 * 전환한다.
 *
 * QA screenshots / QA 스크린샷:
 *   codebomb --shot FILE.png SCREEN [MODE] [LEVEL] [STEPS] [VARIANT]
 *   e.g. --shot arrays.png arrays code 4 20 0
 *        --shot cube.png arrays code 7 6 22      (cube, 2 modules solved)
 *   codebomb --selftest      checks cube click picking / 큐브 클릭 선택 검사
 *        --shot ptr.png pointers - 0 3 1
 *        --shot mail.png mailroom - 0 9 0
 */
#include "editor_ui.h"
#include "screen_arrays.h"
#include "screen_mailroom.h"
#include "screen_pointers.h"
#include "ui.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_FONT_SIZE 20     /* multiple of 10 / 10의 배수 */

typedef enum { SCREEN_MAILROOM, SCREEN_ARRAYS, SCREEN_POINTERS, SCREEN_COUNT }
    ScreenId;

typedef struct {
    ScreenId        screen;
    MailroomScreen *mailroom;
    ArraysScreen   *arrays;
    PointersScreen  pointers;
} Game;

/* Module tabs in the top-right of the header. Later these become the
 * modules on the 3D bomb.
 * 머리 막대 오른쪽 위의 모듈 탭. 나중에 3D 폭탄의 모듈이 된다. */
static void draw_tabs(Game *g)
{
    static const char *const names[SCREEN_COUNT] = { "Mailroom", "Arrays",
                                                     "Pointers" };
    for (int n = 0; n < SCREEN_COUNT; n++) {
        Rectangle r = { 1312 + n * 192, 30, 180, 52 };
        if (ui_button(r, names[n], true, g->screen == (ScreenId)n) &&
            g->screen != (ScreenId)n) {
            if (g->screen == SCREEN_MAILROOM) mailroom_leave(g->mailroom);
            if (g->screen == SCREEN_ARRAYS) arrays_leave(g->arrays);
            if (g->screen == SCREEN_POINTERS) pointers_leave(&g->pointers);
            g->screen = (ScreenId)n;
        }
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
    g.screen = SCREEN_ARRAYS;
    g.mailroom = mailroom_create(&style);
    g.arrays = arrays_create(&style);
    pointers_init(&g.pointers);
    if (!g.mailroom || !g.arrays) {
        fputs("Out of memory\n", stderr);
        CloseWindow();
        return EXIT_FAILURE;
    }

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0) {
        puts("Cube bomb click picking:");
        int failures = arrays_self_test(g.arrays);
        puts(failures ? "SELFTEST FAILED" : "SELFTEST PASSED");
        arrays_destroy(g.arrays);
        mailroom_destroy(g.mailroom);
        UnloadRenderTexture(canvas);
        CloseWindow();
        return failures ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    if (shot) {
        const char *screen = argv[3];
        const char *mode = argc > 4 ? argv[4] : "code";
        int level = arg_int(argc, argv, 5, 1) - 1;
        int steps = arg_int(argc, argv, 6, 0);
        int variant = arg_int(argc, argv, 7, 0);
        if (strcmp(screen, "mailroom") == 0) {
            g.screen = SCREEN_MAILROOM;
            mailroom_qa(g.mailroom, level, steps);
        } else if (strcmp(screen, "pointers") == 0) {
            g.screen = SCREEN_POINTERS;
            pointers_qa(&g.pointers, variant, steps);
        } else {
            arrays_qa(g.arrays, mode, level, steps, variant);
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

        /* At exactly 1:1 use point sampling for crisp pixels; otherwise
         * smooth scaling (Sumin's bilinear filter).
         * 정확히 1:1이면 선명한 픽셀을 위해 점 샘플링, 아니면 부드러운
         * 확대 (수민의 bilinear 필터). */
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
        case SCREEN_ARRAYS:   arrays_update(g.arrays, dt); break;
        default:              pointers_update(&g.pointers, dt); break;
        }

        if (g.screen == SCREEN_MAILROOM) mailroom_render_view(g.mailroom);
        if (g.screen == SCREEN_ARRAYS) arrays_render_view(g.arrays);

        BeginTextureMode(canvas);
        switch (g.screen) {
        case SCREEN_MAILROOM: mailroom_draw(g.mailroom); break;
        case SCREEN_ARRAYS:   arrays_draw(g.arrays); break;
        default:              pointers_draw(&g.pointers); break;
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

    arrays_destroy(g.arrays);
    mailroom_destroy(g.mailroom);
    if (font_owned) UnloadFont(code_font);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
