/*
 * bomb3d.h - the 3D bomb on a wooden table (left panel of the Arrays screen)
 * bomb3d.h - 나무 탁자 위의 3D 폭탄 (배열 화면의 왼쪽 패널)
 *
 * Two shapes / 두 가지 모양:
 *   flat  - levels 1-6: a flat rectangular bomb with one challenge (the
 *           cards) on its top face, plus a display and a status light.
 *           레벨 1-6: 윗면에 도전 과제(카드) 하나가 있는 납작한 직사각형 폭탄.
 *   cube  - harder levels: a 6-sided cube with a coding module on five faces
 *           and the display on top. Drag to rotate it, click a face to work
 *           on that module.
 *           더 어려운 레벨: 다섯 면에 코딩 모듈이 있고 윗면에 표시창이 있는
 *           6면 큐브. 끌어서 돌리고, 면을 클릭하면 그 모듈을 작업한다.
 *
 * The view only draws what the screen gives it (SooState per module); all
 * game logic stays in screen_arrays.c and Sumin's engine.
 * 이 화면은 받은 것(모듈별 SooState)만 그린다. 게임 로직은 모두
 * screen_arrays.c와 수민의 엔진에 있다.
 *
 * Effects: the accessed card lifts and glows, a write flashes the card and
 * throws sparks, a failure shakes the bomb with a red flash, success turns
 * the lights green. When the Cube bomb explodes: a white flash, a fireball,
 * a shock ring on the table, fire, smoke and flying pieces of the casing.
 * 효과: 접근한 카드는 올라오며 빛나고, 쓰기는 카드를 번쩍이며 불꽃을 튀기고,
 * 실패하면 폭탄이 흔들리며 붉게 번쩍이고, 성공하면 불빛이 초록색이 된다.
 * 큐브 폭탄이 터지면: 하얀 섬광, 불덩이, 탁자 위의 충격파 고리, 불, 연기,
 * 날아가는 몸체 조각.
 */
#ifndef BOMB3D_H
#define BOMB3D_H

#include "array_core_SOO.h"

#include "raylib.h"

#include <stdbool.h>

#define BOMB_MAX_MODULES 5
#define BOMB_FACE_TOP    5      /* cube face that holds the display
                                   표시창이 있는 큐브 면 */

typedef struct {
    const SooState *state;      /* cards to draw / 그릴 카드 */
    bool            solved;     /* green module light / 초록 모듈 불빛 */
} BombModule;

typedef struct {
    bool       cube;            /* false = flat bomb with modules[0] on top
                                   false = modules[0]이 윗면에 있는 납작한 폭탄 */
    int        count;           /* number of modules (1 for flat)
                                   모듈 수 (납작한 폭탄은 1) */
    BombModule modules[BOMB_MAX_MODULES];
    int        selected;        /* module being worked on / 작업 중인 모듈 */
    bool       running;         /* blink the status light / 상태 불빛 깜박임 */
    char       lcd[24];         /* big red display, e.g. "08/40"
                                   큰 빨간 표시창, 예: "08/40" */
    char       lcd_caption[12]; /* small word in front of it, e.g. "OPS" */
    char       lcd_small[24];   /* second display, e.g. "i 3  j --"
                                   두 번째 표시창 */
    bool       lcd_alert;       /* blink the big display (time running out)
                                   큰 표시창 깜박임 (시간이 얼마 남지 않음) */
    bool       exploded;        /* cube only: play the explosion, then show
                                   the wreck / 큐브 전용: 폭발 후 잔해 표시 */
} BombScene;

typedef struct BombView BombView;

BombView *bomb_view_create(int width, int height);
void      bomb_view_destroy(BombView *v);

/* Forget animations and refit the camera (call on level change).
 * 애니메이션을 지우고 카메라를 다시 맞춘다 (레벨이 바뀔 때 호출). */
void bomb_view_reset(BombView *v);

/* Turn the cube so module `module` faces the player.
 * module 모듈이 플레이어를 향하도록 큐브를 돌린다. */
void bomb_view_select(BombView *v, int module, bool animate);

/* Animate and handle the mouse. mouse is in panel pixels. Returns the
 * module the player clicked on the cube, or -1.
 * 애니메이션과 마우스 처리. mouse는 패널 픽셀 좌표. 큐브에서 클릭한 모듈을
 * 돌려주고, 없으면 -1. */
int  bomb_view_update(BombView *v, const BombScene *s, float dt, Vector2 mouse,
                      bool mouse_inside);

/* Draw into the view's render texture. Call outside any other
 * BeginTextureMode (they can't be nested).
 * 뷰의 렌더 텍스처에 그린다. 다른 BeginTextureMode 밖에서 호출해야 한다. */
void bomb_view_render(BombView *v, const BombScene *s);

Texture2D bomb_view_texture(const BombView *v);

/* Checks cube click picking for every module; returns the failure count.
 * 모든 모듈에 대해 큐브 클릭 선택을 검사하고 실패 수를 돌려준다. */
int bomb_view_self_test(BombView *v, const BombScene *cube_scene);

#endif
