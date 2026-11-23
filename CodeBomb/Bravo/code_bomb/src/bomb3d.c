/*
 * bomb3d.c - 3D bomb on a wooden table
 * bomb3d.c - 나무 탁자 위의 3D 폭탄
 *
 * Everything is drawn with simple shaded boxes: each face gets a brightness
 * from a fixed light direction, so the scene looks lit without shaders
 * (works the same on Windows, macOS and raylib's software renderer).
 * 모든 것을 음영 있는 상자로 그린다: 각 면은 고정된 빛 방향에 따라 밝기를
 * 받으므로 셰이더 없이도 조명을 받은 것처럼 보인다 (Windows, macOS, raylib
 * 소프트웨어 렌더러에서 똑같이 동작).
 *
 * Text on the bomb (card values, displays) is drawn in 2D at the projected
 * position of its 3D point, so it is always upright and sharp, at font sizes
 * that are multiples of 10.
 * 폭탄 위의 글자(카드 값, 표시창)는 3D 점을 투영한 위치에 2D로 그린다. 그래서
 * 항상 바로 서 있고 선명하며, 글자 크기는 10의 배수다.
 */
#include "bomb3d.h"

#include "ui.h"

#include "rlgl.h"
/* Static inline so debug builds (-O0) link without the raylib library's
 * copies. / 디버그 빌드(-O0)에서도 링크되도록 static inline으로 쓴다. */
#define RAYMATH_STATIC_INLINE
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- sizes (world units) / 크기 (월드 단위) ------------------------------ */
#define TILE       1.0f      /* card size / 카드 크기          */
#define PITCH      1.18f     /* card + gap / 카드 + 간격        */
#define TILE_H     0.26f     /* card height / 카드 높이         */
#define CUBE_S     6.2f      /* cube side / 큐브 한 변          */
#define CUBE_HOVER 0.9f      /* cube floats above the table / 큐브가 떠 있는 높이 */
#define FLAT_FEET  0.12f
#define FLAT_H     0.85f
#define FOVY       32.0f
#define MAX_LABELS 360
#define MAX_PARTS  900
#define FACES      6

/* Overlay space the screen keeps free at the top and bottom of the panel.
 * 화면이 패널 위와 아래에 비워 두는 오버레이 공간. */
/* Plates on a face are stacked, never coplanar (coplanar plates flicker,
 * "z-fighting"). / 면 위의 판은 겹쳐 쌓고 같은 높이에 두지 않는다 (같은 높이의
 * 판은 깜빡인다, "z-fighting"). */
#define LAYER      0.03f     /* thickness of one layer / 한 층 두께   */
#define CLIP_NEAR  1.0f      /* camera clip planes / 카메라 클립 거리 */
#define CLIP_FAR   250.0f
#define FIT_TOP    86.0f
#define FIT_BOTTOM 112.0f
#define FIT_SIDE   28.0f

typedef struct {
    Vector3 pos;
    char    text[32];
    int     size;
    Color   color;
} Label;

typedef enum { PART_SOLID, PART_FIRE, PART_SMOKE } PartKind;

typedef struct {
    Vector3  p, v;
    float    life, max_life, size, grow;
    Color    c;
    PartKind kind;   /* solid: falls and bounces; fire and smoke rise
                        solid: 떨어져 튐; fire, smoke: 위로 올라감 */
} Particle;

/* A face of the bomb: origin at its centre, u = right, v = down, n = out.
 * 폭탄의 한 면: 원점은 중심, u = 오른쪽, v = 아래, n = 바깥. */
typedef struct {
    Vector3 o, u, v, n;
} Face;

/* Model transform: rotation q, then translation t. / 모델 변환. */
typedef struct {
    Quaternion q;
    Vector3    t;
} Xform;

struct BombView {
    RenderTexture2D tex;
    int             w, h;
    Camera3D        cam;
    Vector3         cam_target;
    Vector3         cam_dir;
    float           cam_dist;
    int             layout;          /* signature of the fitted layout
                                        카메라를 맞춘 배치 표시 */

    Quaternion      q, q_goal;       /* cube orientation / 큐브 방향 */
    int             selected;

    bool            pressing, dragging;
    Vector2         press_pos, last_mouse;

    float           press[BOMB_MAX_MODULES][SOO_MAX_CELLS];
    float           flash[BOMB_MAX_MODULES][SOO_MAX_CELLS];
    bool            spark[BOMB_MAX_MODULES][SOO_MAX_CELLS];  /* one-shot
                                                                한 번만 */
    int             prev_cells[BOMB_MAX_MODULES][SOO_MAX_CELLS];
    SooStatus       prev_status[BOMB_MAX_MODULES];
    const SooLevel *prev_level[BOMB_MAX_MODULES];
    bool            have_prev;

    float           time, shake, red_flash, green_flash;
    bool            boomed;          /* explosion started / 폭발 시작됨 */
    float           boom_t;          /* seconds since it / 폭발 후 시간 */
    float           white_flash;
    Vector3         boom_at;
    Particle        parts[MAX_PARTS];
    Label           labels[MAX_LABELS];
    int             nlabels;
};

/* Cube faces in local space. Modules 0-4 sit on faces 0-4; face 5 is the
 * top display. / 로컬 공간의 큐브 면. 모듈 0-4는 면 0-4, 면 5는 윗면 표시창. */
static const Face CUBE_FACES[FACES] = {
    { { 0, 0, 1 }, { 1, 0, 0 },  { 0, -1, 0 }, { 0, 0, 1 } },    /* front  */
    { { 1, 0, 0 }, { 0, 0, -1 }, { 0, -1, 0 }, { 1, 0, 0 } },    /* right  */
    { { 0, 0, -1 }, { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, -1 } },  /* back   */
    { { -1, 0, 0 }, { 0, 0, 1 }, { 0, -1, 0 }, { -1, 0, 0 } },   /* left   */
    { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, -1, 0 } },   /* bottom */
    { { 0, 1, 0 }, { 1, 0, 0 },  { 0, 0, 1 },  { 0, 1, 0 } },    /* top    */
};

static const Color COL_CASING  = { 58, 62, 72, 255 };
static const Color COL_TOP     = { 76, 80, 92, 255 };
static const Color COL_RECESS  = { 30, 32, 40, 255 };
static const Color COL_FRAME   = { 96, 100, 112, 255 };
static const Color COL_LCD     = { 26, 8, 10, 255 };
static const Color COL_LCD_ON  = { 255, 72, 60, 255 };
static const Color COL_LCD_DIM = { 70, 22, 22, 255 };
static const Color COL_ROOM    = { 20, 14, 12, 255 };

/* ========================================================================
 * Small math and colour helpers / 작은 수학, 색 도우미
 * ====================================================================== */

static Vector3 v3(float x, float y, float z) { return (Vector3){ x, y, z }; }

static Vector3 xf_point(const Xform *x, Vector3 p)
{
    return Vector3Add(Vector3RotateByQuaternion(p, x->q), x->t);
}

static Vector3 xf_dir(const Xform *x, Vector3 d)
{
    return Vector3RotateByQuaternion(d, x->q);
}

static Vector3 vabs(Vector3 a) { return v3(fabsf(a.x), fabsf(a.y), fabsf(a.z)); }

static Color shade(Color c, float f)
{
    if (f < 0) f = 0;
    float r = c.r * f, g = c.g * f, b = c.b * f;
    return (Color){ (unsigned char)(r > 255 ? 255 : r),
                    (unsigned char)(g > 255 ? 255 : g),
                    (unsigned char)(b > 255 ? 255 : b), c.a };
}

static Color mix(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t),
                    (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t),
                    (unsigned char)(a.a + (b.a - a.a) * t) };
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* Brightness of a face whose outward normal (world) is n.
 * 바깥 법선(월드)이 n인 면의 밝기. */
static float light(Vector3 n)
{
    static const Vector3 L = { -0.36f, 0.86f, 0.36f };   /* normalised */
    float d = Vector3DotProduct(n, L);
    return 0.46f + 0.54f * (d > 0 ? d : 0) + 0.06f * (n.z > 0 ? n.z : 0);
}

/* ========================================================================
 * Geometry / 도형
 * ====================================================================== */

/* Box centred at c (local) with half sizes h along the local axes.
 * 로컬 중심 c, 로컬 축 방향 반 크기 h인 상자. */
static void draw_box(const Xform *x, Vector3 c, Vector3 h, Color col)
{
    static const float SIGNS[6][3] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 },
        { 0, 0, -1 }
    };
    rlBegin(RL_QUADS);
    for (int f = 0; f < 6; f++) {
        Vector3 n = v3(SIGNS[f][0], SIGNS[f][1], SIGNS[f][2]);
        /* two in-plane axes for this face / 이 면의 두 평면 축 */
        Vector3 a = fabsf(n.x) > 0 ? v3(0, 1, 0) : v3(1, 0, 0);
        Vector3 b = Vector3CrossProduct(n, a);
        Vector3 fc = Vector3Add(c, v3(n.x * h.x, n.y * h.y, n.z * h.z));
        Vector3 ha = v3(a.x * h.x, a.y * h.y, a.z * h.z);
        Vector3 hb = v3(b.x * h.x, b.y * h.y, b.z * h.z);
        Color s = shade(col, light(xf_dir(x, n)));
        rlColor4ub(s.r, s.g, s.b, s.a);
        Vector3 p[4] = {
            Vector3Add(Vector3Subtract(fc, ha), Vector3Negate(hb)),
            Vector3Add(Vector3Add(fc, ha), Vector3Negate(hb)),
            Vector3Add(Vector3Add(fc, ha), hb),
            Vector3Add(Vector3Subtract(fc, ha), hb),
        };
        for (int k = 0; k < 4; k++) {
            Vector3 w = xf_point(x, p[k]);
            rlVertex3f(w.x, w.y, w.z);
        }
    }
    rlEnd();
}

/* A thin plate lying on face f, centred at (fx, fy) in face coordinates.
 * 면 f 위에 놓인 얇은 판. 면 좌표 (fx, fy)가 중심. */
static void face_plate(const Xform *x, const Face *f, float fx, float fy,
                       float w, float hgt, float lift, float thick, Color col)
{
    Vector3 c = Vector3Add(Vector3Add(f->o, Vector3Scale(f->u, fx)),
                           Vector3Add(Vector3Scale(f->v, fy),
                                      Vector3Scale(f->n, lift + thick * 0.5f)));
    Vector3 h = Vector3Add(Vector3Add(vabs(Vector3Scale(f->u, w * 0.5f)),
                                      vabs(Vector3Scale(f->v, hgt * 0.5f))),
                           vabs(Vector3Scale(f->n, thick * 0.5f)));
    draw_box(x, c, h, col);
}

static Vector3 face_point(const Xform *x, const Face *f, float fx, float fy,
                          float lift)
{
    Vector3 p = Vector3Add(Vector3Add(f->o, Vector3Scale(f->u, fx)),
                           Vector3Add(Vector3Scale(f->v, fy),
                                      Vector3Scale(f->n, lift)));
    return xf_point(x, p);
}

/* Is face f (with transform x) turned toward the camera?
 * 면 f가 카메라 쪽을 향하는가? */
static float facing(const BombView *v, const Xform *x, const Face *f)
{
    Vector3 n = xf_dir(x, f->n);
    Vector3 c = xf_point(x, f->o);
    Vector3 to_cam = Vector3Normalize(Vector3Subtract(v->cam.position, c));
    return Vector3DotProduct(n, to_cam);
}

static void add_label(BombView *v, Vector3 pos, const char *text, int size,
                      Color color)
{
    if (v->nlabels >= MAX_LABELS) return;
    Label *l = &v->labels[v->nlabels++];
    l->pos = pos;
    snprintf(l->text, sizeof l->text, "%s", text);
    l->size = size;
    l->color = color;
}

/* Projected length in pixels of a world segment / 월드 선분의 화면 길이 */
static float screen_len(const BombView *v, Vector3 a, Vector3 b)
{
    Vector2 pa = GetWorldToScreenEx(a, v->cam, v->w, v->h);
    Vector2 pb = GetWorldToScreenEx(b, v->cam, v->w, v->h);
    return Vector2Distance(pa, pb);
}

/* ========================================================================
 * Effects / 효과
 * ====================================================================== */

static void spawn_sparks(BombView *v, Vector3 at, Vector3 up, Color c, int n,
                         float speed)
{
    for (int k = 0; k < MAX_PARTS && n > 0; k++) {
        Particle *p = &v->parts[k];
        if (p->life > 0) continue;
        Vector3 dir = Vector3Normalize(v3(frand() * 2 - 1, frand() * 2 - 1,
                                          frand() * 2 - 1));
        dir = Vector3Normalize(Vector3Add(dir, Vector3Scale(up, 1.6f)));
        p->p = at;
        p->v = Vector3Scale(dir, speed * (0.5f + frand()));
        p->max_life = p->life = 0.45f + frand() * 0.45f;
        p->size = 0.06f + frand() * 0.07f;
        p->grow = 0;
        p->kind = PART_SOLID;
        p->c = frand() < 0.3f ? (Color){ 255, 240, 200, 255 } : c;
        n--;
    }
}

static Particle *free_particle(BombView *v)
{
    for (int k = 0; k < MAX_PARTS; k++)
        if (v->parts[k].life <= 0) return &v->parts[k];
    return NULL;
}

static Vector3 rand_dir(void)
{
    return Vector3Normalize(v3(frand() * 2 - 1, frand() * 2 - 1,
                               frand() * 2 - 1));
}

/* The explosion: debris, fire, smoke and sparks from the cube's centre.
 * 폭발: 큐브 중심에서 잔해, 불, 연기, 불꽃이 나온다. */
static void spawn_explosion(BombView *v, Vector3 at)
{
    static const Color DEBRIS[4] = { { 58, 62, 72, 255 }, { 96, 100, 112, 255 },
                                     { 30, 30, 34, 255 }, { 76, 80, 92, 255 } };
    static const Color FIRE[3] = { { 255, 230, 140, 255 }, { 255, 150, 50, 255 },
                                   { 230, 70, 30, 255 } };
    for (int n = 0; n < 110; n++) {           /* casing pieces / 몸체 조각 */
        Particle *p = free_particle(v);
        if (!p) return;
        Vector3 d = rand_dir();
        d.y = fabsf(d.y) * 0.9f + 0.3f;
        p->p = Vector3Add(at, Vector3Scale(d, 1.2f));
        p->v = Vector3Scale(Vector3Normalize(d), 5.0f + frand() * 10.0f);
        p->max_life = p->life = 4.0f + frand() * 3.0f;
        p->size = 0.14f + frand() * 0.34f;
        p->grow = 0;
        p->kind = PART_SOLID;
        p->c = DEBRIS[n % 4];
    }
    for (int n = 0; n < 170; n++) {           /* fire / 불 */
        Particle *p = free_particle(v);
        if (!p) return;
        Vector3 d = rand_dir();
        d.y = fabsf(d.y) * 0.7f + 0.15f;
        p->p = Vector3Add(at, Vector3Scale(d, frand() * 1.5f));
        p->v = Vector3Scale(Vector3Normalize(d), 3.0f + frand() * 8.0f);
        p->max_life = p->life = 0.5f + frand() * 0.8f;
        p->size = 0.45f + frand() * 0.6f;
        p->grow = 1.4f;
        p->kind = PART_FIRE;
        p->c = FIRE[n % 3];
    }
    for (int n = 0; n < 90; n++) {            /* smoke / 연기 */
        Particle *p = free_particle(v);
        if (!p) return;
        Vector3 d = rand_dir();
        d.y = fabsf(d.y) * 0.5f + 0.4f;
        p->p = Vector3Add(at, Vector3Scale(d, frand() * 2.0f));
        p->v = Vector3Scale(Vector3Normalize(d), 1.0f + frand() * 3.0f);
        p->max_life = p->life = 2.2f + frand() * 1.8f;
        p->size = 0.6f + frand() * 0.6f;
        p->grow = 0.7f;
        p->kind = PART_SMOKE;
        unsigned char g = (unsigned char)(40 + frand() * 40);
        p->c = (Color){ g, g, (unsigned char)(g + 6), 255 };
    }
    spawn_sparks(v, at, v3(0, 1, 0), (Color){ 255, 200, 90, 255 }, 140, 13.0f);
}

static void update_particles(BombView *v, float dt)
{
    for (int k = 0; k < MAX_PARTS; k++) {
        Particle *p = &v->parts[k];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->size += p->grow * dt;
        if (p->kind != PART_SOLID) {
            /* fire and smoke slow down and drift upward
               불과 연기는 느려지며 위로 떠오른다 */
            float drag = expf((p->kind == PART_FIRE ? -3.0f : -1.2f) * dt);
            p->v = Vector3Scale(p->v, drag);
            p->v.y += (p->kind == PART_FIRE ? 2.5f : 0.9f) * dt;
            p->p = Vector3Add(p->p, Vector3Scale(p->v, dt));
            continue;
        }
        p->v.y -= 9.0f * dt;
        p->p = Vector3Add(p->p, Vector3Scale(p->v, dt));
        if (p->p.y < 0.02f) {             /* bounce on the table / 탁자에서 튐 */
            p->p.y = 0.02f;
            p->v.y *= -0.35f;
            p->v.x *= 0.6f;
            p->v.z *= 0.6f;
        }
    }
}

static void draw_particles(const BombView *v)
{
    for (int k = 0; k < MAX_PARTS; k++) {
        const Particle *p = &v->parts[k];
        if (p->life <= 0 || p->kind != PART_SOLID) continue;
        float t = p->life / p->max_life;
        Color c = p->c;
        c.a = (unsigned char)(255 * fminf(1.0f, t * 3.0f));
        DrawCube(p->p, p->size, p->size, p->size, c);
    }
    /* Smoke and fire are see-through, so they don't write depth; fire adds
     * light. / 연기와 불은 반투명이라 깊이를 쓰지 않고, 불은 빛을 더한다. */
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (int k = 0; k < MAX_PARTS; k++) {
        const Particle *p = &v->parts[k];
        if (p->life <= 0 || p->kind != PART_SMOKE) continue;
        float t = p->life / p->max_life;
        Color c = p->c;
        c.a = (unsigned char)(120 * t);
        DrawSphereEx(p->p, p->size, 5, 7, c);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < MAX_PARTS; k++) {
        const Particle *p = &v->parts[k];
        if (p->life <= 0 || p->kind != PART_FIRE) continue;
        float t = p->life / p->max_life;
        Color c = mix((Color){ 120, 20, 10, 255 }, p->c, t);
        c.a = (unsigned char)(220 * t);
        DrawSphereEx(p->p, p->size, 5, 7, c);
    }
    if (v->boomed && v->boom_t < 0.9f) {
        /* fireball and shock ring / 불덩이와 충격파 고리 */
        float t = v->boom_t / 0.9f;
        float r = 1.2f + (1.0f - (1.0f - t) * (1.0f - t)) * 4.0f;
        Color c = mix((Color){ 255, 240, 180, 255 }, (Color){ 200, 60, 20, 255 },
                      t);
        c.a = (unsigned char)(230 * (1.0f - t));
        DrawSphereEx(v->boom_at, r, 10, 14, c);
        for (int k = 0; k < 4; k++)
            DrawCircle3D(v3(v->boom_at.x, 0.06f, v->boom_at.z),
                         r * 1.6f + k * 0.12f, v3(1, 0, 0), 90.0f,
                         (Color){ 255, 200, 140,
                                  (unsigned char)(200 * (1.0f - t)) });
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

/* ========================================================================
 * Layout and camera / 배치와 카메라
 * ====================================================================== */

static int layout_signature(const BombScene *s)
{
    if (s->cube) return 1;
    const SooLevel *l = s->modules[0].state->level;
    return 100 + l->rows * 32 + l->cols;
}

/* Flat bomb size for a level / 레벨에 맞는 납작한 폭탄 크기 */
static void flat_size(const SooLevel *l, float *W, float *D)
{
    float grid_w = l->cols * PITCH - (PITCH - TILE);
    float grid_d = l->rows * PITCH - (PITCH - TILE);
    *W = fmaxf(grid_w + 1.6f, 8.8f);
    *D = grid_d + (l->rows == 1 ? 2.2f : 1.4f) + 1.7f;
}

static Face flat_top_face(const SooLevel *l)
{
    float W, D;
    flat_size(l, &W, &D);
    float grid_d = l->rows * PITCH - (PITCH - TILE);
    float front = D * 0.5f - (l->rows == 1 ? 1.1f : 0.7f);
    Face f = { v3(0, FLAT_FEET + FLAT_H, front - grid_d * 0.5f),
               v3(1, 0, 0), v3(0, 0, 1), v3(0, 1, 0) };
    return f;
}

/* Rotation that turns cube face `face` toward the presentation direction
 * p, with the face's "up" kept as close to world up as possible.
 * 큐브의 face 면을 보여 주는 방향 p로 돌리는 회전. 면의 "위"는 가능한 한
 * 월드의 위에 가깝게 유지한다. */
static Quaternion present_face(int face, Vector3 p)
{
    const Face *f = &CUBE_FACES[face];
    Vector3 lr = f->u, lu = Vector3Negate(f->v), ln = f->n;
    Vector3 wn = p;
    Vector3 wu = Vector3Normalize(Vector3Subtract(
        v3(0, 1, 0), Vector3Scale(wn, Vector3DotProduct(wn, v3(0, 1, 0)))));
    Vector3 wr = Vector3CrossProduct(wu, wn);
    float W[3][3] = { { wr.x, wu.x, wn.x }, { wr.y, wu.y, wn.y },
                      { wr.z, wu.z, wn.z } };
    float L[3][3] = { { lr.x, lu.x, ln.x }, { lr.y, lu.y, ln.y },
                      { lr.z, lu.z, ln.z } };
    float R[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R[i][j] = W[i][0] * L[j][0] + W[i][1] * L[j][1] +
                      W[i][2] * L[j][2];
    Matrix m = MatrixIdentity();
    m.m0 = R[0][0]; m.m4 = R[0][1]; m.m8 = R[0][2];
    m.m1 = R[1][0]; m.m5 = R[1][1]; m.m9 = R[1][2];
    m.m2 = R[2][0]; m.m6 = R[2][1]; m.m10 = R[2][2];
    return QuaternionNormalize(QuaternionFromMatrix(m));
}

/* Direction the presented face points: toward the camera, turned a little
 * left and down so the neighbouring face and the top show as well.
 * 보여 주는 면이 가리키는 방향: 카메라 쪽에서 조금 왼쪽 아래로 돌려 옆면과
 * 윗면도 보이게 한다. */
static Vector3 presentation_dir(const BombView *v)
{
    (void)v;
    /* Turned 17 degrees to the left so the right-hand face shows, and
     * pointing a little below horizontal so the top display tips toward
     * the (raised) camera.
     * 왼쪽으로 17도 돌려 오른쪽 면이 보이게 하고, 수평보다 조금 아래를
     * 향하게 하여 윗면 표시창이 (높이 있는) 카메라 쪽으로 기울게 한다. */
    return Vector3Normalize(v3(sinf(-0.30f), -0.10f, cosf(-0.30f)));
}

static void place_camera(BombView *v, Vector3 target, float dist)
{
    v->cam.target = target;
    v->cam.position = Vector3Add(target, Vector3Scale(v->cam_dir, dist));
    v->cam.up = v3(0, 1, 0);
    v->cam.fovy = FOVY;
    v->cam.projection = CAMERA_PERSPECTIVE;
}

/* Choose the closest camera that still shows every point inside the free
 * area of the panel, and centre them there.
 * 모든 점이 패널의 빈 영역 안에 보이는 가장 가까운 카메라를 고르고, 점들을
 * 그 영역 가운데에 놓는다. */
static void fit_camera(BombView *v, const Vector3 *pts, int n, Vector3 center)
{
    float x0 = FIT_SIDE, x1 = v->w - FIT_SIDE;
    float y0 = FIT_TOP, y1 = v->h - FIT_BOTTOM;
    Vector3 target = center;
    float dist = 40.0f;
    for (int pass = 0; pass < 4; pass++) {
        float lo = 2.0f, hi = 400.0f;
        for (int it = 0; it < 40; it++) {
            float mid = (lo + hi) * 0.5f;
            place_camera(v, target, mid);
            bool inside = true;
            for (int k = 0; k < n && inside; k++) {
                Vector2 s = GetWorldToScreenEx(pts[k], v->cam, v->w, v->h);
                if (s.x < x0 || s.x > x1 || s.y < y0 || s.y > y1) inside = false;
            }
            if (inside) hi = mid;
            else lo = mid;
        }
        dist = hi;
        place_camera(v, target, dist);

        /* centre the projected box in the free area / 투영 상자를 가운데로 */
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
        for (int k = 0; k < n; k++) {
            Vector2 s = GetWorldToScreenEx(pts[k], v->cam, v->w, v->h);
            minx = fminf(minx, s.x);
            maxx = fmaxf(maxx, s.x);
            miny = fminf(miny, s.y);
            maxy = fmaxf(maxy, s.y);
        }
        float dx = (minx + maxx) * 0.5f - (x0 + x1) * 0.5f;
        float dy = (miny + maxy) * 0.5f - (y0 + y1) * 0.5f;
        float wpp = 2.0f * dist * tanf(FOVY * DEG2RAD * 0.5f) / v->h;
        Vector3 right = Vector3Normalize(Vector3CrossProduct(
            Vector3Negate(v->cam_dir), v3(0, 1, 0)));
        Vector3 up = Vector3CrossProduct(right, Vector3Negate(v->cam_dir));
        /* Screen y grows downward: content too low (dy > 0) means the
         * camera must move down. / 화면 y는 아래로 커진다: 내용이 너무 아래에
         * 있으면 (dy > 0) 카메라를 아래로 옮긴다. */
        target = Vector3Add(target, Vector3Scale(right, dx * wpp));
        target = Vector3Subtract(target, Vector3Scale(up, dy * wpp));
    }
    v->cam_target = target;
    v->cam_dist = dist;
}

static void fit_layout(BombView *v, const BombScene *s)
{
    Vector3 pts[16];
    int n = 0;
    if (!s->cube) {
        const SooLevel *l = s->modules[0].state->level;
        float W, D;
        flat_size(l, &W, &D);
        v->cam_dir = Vector3Normalize(v3(0, 1.45f, 1.0f));     /* ~55 deg */
        for (int k = 0; k < 8; k++)
            pts[n++] = v3((k & 1) ? W * 0.5f : -W * 0.5f,
                          (k & 2) ? FLAT_FEET + FLAT_H + TILE_H : 0.0f,
                          (k & 4) ? D * 0.5f : -D * 0.5f);
        fit_camera(v, pts, n, v3(0, FLAT_FEET + FLAT_H * 0.5f, 0));
    } else {
        v->cam_dir = Vector3Normalize(v3(0, 0.56f, 1.0f));     /* ~29 deg */
        Quaternion q = present_face(0, presentation_dir(v));
        Vector3 c = v3(0, CUBE_S * 0.5f + CUBE_HOVER, 0);
        float h = CUBE_S * 0.5f + 0.12f;
        for (int k = 0; k < 8; k++) {
            Vector3 p = v3((k & 1) ? h : -h, (k & 2) ? h : -h, (k & 4) ? h : -h);
            pts[n++] = Vector3Add(Vector3RotateByQuaternion(p, q), c);
        }
        fit_camera(v, pts, n, c);
    }
    place_camera(v, v->cam_target, v->cam_dist);
}

/* ========================================================================
 * Public: lifecycle and input / 공개: 생성과 입력
 * ====================================================================== */

BombView *bomb_view_create(int width, int height)
{
    BombView *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    v->w = width;
    v->h = height;
    v->tex = LoadRenderTexture(width, height);
    v->q = v->q_goal = QuaternionIdentity();
    v->layout = -1;
    return v;
}

void bomb_view_destroy(BombView *v)
{
    if (!v) return;
    if (v->tex.id != 0) UnloadRenderTexture(v->tex);
    free(v);
}

Texture2D bomb_view_texture(const BombView *v) { return v->tex.texture; }

void bomb_view_reset(BombView *v)
{
    v->layout = -1;
    v->have_prev = false;
    v->shake = v->red_flash = v->green_flash = v->white_flash = 0;
    v->boomed = false;
    memset(v->press, 0, sizeof v->press);
    memset(v->flash, 0, sizeof v->flash);
    memset(v->spark, 0, sizeof v->spark);
    for (int k = 0; k < MAX_PARTS; k++) v->parts[k].life = 0;
    v->pressing = v->dragging = false;
}

void bomb_view_select(BombView *v, int module, bool animate)
{
    if (module < 0 || module >= BOMB_MAX_MODULES) return;
    v->selected = module;
    v->q_goal = present_face(module, presentation_dir(v));
    if (!animate) v->q = v->q_goal;
}

/* Which cube face is under the mouse (-1 = none)? / 마우스 아래의 큐브 면 */
static int pick_face(const BombView *v, Vector2 mouse, Vector3 center)
{
    Ray ray = GetScreenToWorldRayEx(mouse, v->cam, v->w, v->h);
    Quaternion inv = QuaternionInvert(v->q);
    Ray local = { Vector3RotateByQuaternion(Vector3Subtract(ray.position, center),
                                            inv),
                  Vector3RotateByQuaternion(ray.direction, inv) };
    float h = CUBE_S * 0.5f;
    RayCollision hit = GetRayCollisionBox(local, (BoundingBox){ v3(-h, -h, -h),
                                                                v3(h, h, h) });
    if (!hit.hit) return -1;
    Vector3 p = hit.point, a = vabs(p);
    if (a.x >= a.y && a.x >= a.z) return p.x > 0 ? 1 : 3;
    if (a.y >= a.x && a.y >= a.z) return p.y > 0 ? 5 : 4;
    return p.z > 0 ? 0 : 2;
}

static Vector3 cube_center(const BombView *v)
{
    return v3(0, CUBE_S * 0.5f + CUBE_HOVER + sinf(v->time * 1.4f) * 0.05f, 0);
}

/* Compare each module with last frame to start flashes, sparks, the strike
 * shake and the success glow.
 * 각 모듈을 지난 프레임과 비교하여 번쩍임, 불꽃, 실패 흔들림, 성공 빛을 시작. */
static void detect_changes(BombView *v, const BombScene *s)
{
    for (int m = 0; m < s->count; m++) {
        const SooState *st = s->modules[m].state;
        const SooLevel *l = st->level;
        int cells = l->rows * l->cols;
        bool fresh = !v->have_prev || v->prev_level[m] != l;
        bool live = !s->cube || m == s->selected;
        if (!fresh && live) {
            for (int k = 0; k < cells; k++)
                if (st->cells[k] != v->prev_cells[m][k]) {
                    v->flash[m][k] = 1.0f;
                    v->spark[m][k] = true;   /* sparks need the card's 3D spot,
                                                so draw_module spawns them
                                                불꽃은 카드의 3D 위치가 필요하므로
                                                draw_module에서 만든다 */
                }
            if (st->status == SOO_FAILURE && v->prev_status[m] != SOO_FAILURE) {
                v->shake = 1.0f;
                v->red_flash = 1.0f;
            }
            if (st->status == SOO_SUCCESS && v->prev_status[m] != SOO_SUCCESS)
                v->green_flash = 1.0f;
        }
        memcpy(v->prev_cells[m], st->cells, sizeof(int) * (size_t)cells);
        v->prev_status[m] = st->status;
        v->prev_level[m] = l;
    }
    v->have_prev = true;
}

int bomb_view_update(BombView *v, const BombScene *s, float dt, Vector2 mouse,
                     bool mouse_inside)
{
    int clicked = -1;
    v->time += dt;
    if (s->count <= 0 || !s->modules[0].state) return -1;   /* empty scene */

    int sig = layout_signature(s);
    if (sig != v->layout) {
        v->layout = sig;
        fit_layout(v, s);
        if (s->cube) bomb_view_select(v, s->selected, false);
        v->have_prev = false;
    }

    if (s->cube) {
        if (mouse_inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            v->pressing = true;
            v->dragging = false;
            v->press_pos = v->last_mouse = mouse;
        }
        if (v->pressing && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = Vector2Subtract(mouse, v->last_mouse);
            if (!v->dragging && Vector2Distance(mouse, v->press_pos) > 6.0f)
                v->dragging = true;
            if (v->dragging) {
                /* drag left/right = spin, up/down = tilt toward you
                   좌우로 끌면 회전, 위아래로 끌면 앞뒤로 기울임 */
                Vector3 right = Vector3Normalize(Vector3CrossProduct(
                    v3(0, 1, 0), v->cam_dir));
                Quaternion yaw = QuaternionFromAxisAngle(v3(0, 1, 0), d.x * 0.009f);
                Quaternion pitch = QuaternionFromAxisAngle(right, d.y * 0.009f);
                v->q = QuaternionNormalize(QuaternionMultiply(
                    pitch, QuaternionMultiply(yaw, v->q)));
                v->q_goal = v->q;
            }
            v->last_mouse = mouse;
        }
        if (v->pressing && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            v->pressing = false;
            if (!v->dragging && mouse_inside) {
                int face = pick_face(v, mouse, cube_center(v));
                if (face >= 0 && face < s->count) clicked = face;
            }
            v->dragging = false;
        }
        if (!v->dragging)
            v->q = QuaternionSlerp(v->q, v->q_goal, 1.0f - expf(-7.0f * dt));
    }

    detect_changes(v, s);

    if (s->cube && s->exploded && !v->boomed) {
        v->boomed = true;
        v->boom_t = 0;
        v->boom_at = cube_center(v);
        v->shake = 1.8f;
        v->white_flash = 1.0f;
        v->pressing = v->dragging = false;
        spawn_explosion(v, v->boom_at);
    } else if (!s->exploded && v->boomed) {
        v->boomed = false;              /* restarted / 다시 시작 */
        v->shake = v->white_flash = 0;
        for (int k = 0; k < MAX_PARTS; k++) v->parts[k].life = 0;
    }
    if (v->boomed) v->boom_t += dt;

    /* card lift toward the accessed card, flash fade
       접근한 카드는 올라오고, 번쩍임은 사라짐 */
    for (int m = 0; m < s->count; m++) {
        const SooState *st = s->modules[m].state;
        int cols = st->level->cols, cells = st->level->rows * cols;
        for (int k = 0; k < cells; k++) {
            bool active = st->active_row >= 0 &&
                          k == st->active_row * cols + st->active_col &&
                          (!s->cube || m == s->selected);
            float goal = active ? 1.0f : 0.0f;
            v->press[m][k] += (goal - v->press[m][k]) * (1.0f - expf(-14.0f * dt));
            v->flash[m][k] = fmaxf(0.0f, v->flash[m][k] - dt * 2.2f);
        }
    }
    v->shake = fmaxf(0.0f, v->shake - dt * 2.2f);
    v->red_flash = fmaxf(0.0f, v->red_flash - dt * 1.6f);
    v->green_flash = fmaxf(0.0f, v->green_flash - dt * 0.8f);
    v->white_flash = fmaxf(0.0f, v->white_flash - dt * 1.4f);
    update_particles(v, dt);
    return clicked;
}

/* ========================================================================
 * Drawing / 그리기
 * ====================================================================== */

static void draw_table(void)
{
    static const Color PLANKS[5] = {
        { 150, 98, 58, 255 }, { 139, 89, 52, 255 }, { 160, 106, 64, 255 },
        { 145, 94, 55, 255 }, { 133, 85, 49, 255 }
    };
    Xform id = { QuaternionIdentity(), v3(0, 0, 0) };
    draw_box(&id, v3(0, -0.5f, 0), v3(60, 0.5f, 45), (Color){ 78, 48, 28, 255 });
    int n = 0;
    for (float z = -44.0f; z < 44.0f; z += 1.6f, n++) {
        Color c = PLANKS[(n * 7 + 3) % 5];
        draw_box(&id, v3(0, 0.006f, z), v3(60, 0.006f, 0.76f), c);
        /* wood grain: a few darker streaks per plank / 나뭇결 */
        for (int g = 0; g < 3; g++) {
            unsigned h = (unsigned)(n * 2654435761u + g * 40503u);
            float gz = z - 0.6f + (float)(h % 1000) / 1000.0f * 1.2f;
            float gx = -20.0f + (float)((h >> 10) % 4000) / 100.0f;
            float len = 3.0f + (float)((h >> 20) % 600) / 100.0f;
            draw_box(&id, v3(gx, 0.013f, gz), v3(len, 0.002f, 0.025f),
                     shade(c, 0.82f));
        }
        /* plank joints across the board / 판자 이음새 */
        float jx = -30.0f + (float)((n * 977) % 3000) / 100.0f;
        draw_box(&id, v3(jx, 0.013f, z), v3(0.03f, 0.002f, 0.76f),
                 shade(c, 0.6f));
    }
}

/* Soft shadow: stacked translucent rectangles / 부드러운 그림자 */
static void draw_shadow(float cx, float cz, float hw, float hd, float strength)
{
    Xform id = { QuaternionIdentity(), v3(0, 0, 0) };
    for (int k = 0; k < 4; k++) {
        float grow = 0.18f * k;
        Color c = { 10, 6, 4, (unsigned char)(70 * strength / (k + 1)) };
        draw_box(&id, v3(cx, 0.016f + 0.001f * k, cz),
                 v3(hw + grow, 0.0005f, hd + grow), c);
    }
}

static void lcd_text(char *ghost, const char *text, size_t n)
{
    size_t k = 0;
    for (; text[k] && k + 1 < n; k++)
        ghost[k] = (text[k] == ' ' || text[k] == '/') ? text[k] : '8';
    ghost[k] = '\0';
}

/* A red LCD with ghost segments behind the digits (like a real timer).
 * 숫자 뒤에 꺼진 칸이 비치는 빨간 LCD (실제 타이머처럼). */
static void draw_lcd(BombView *v, const Xform *x, const Face *f, float fx,
                     float fy, float w, float h, const char *caption,
                     const char *text, int size, Color on)
{
    face_plate(x, f, fx, fy, w + 0.16f, h + 0.16f, 0.0f, 0.06f, COL_RECESS);
    face_plate(x, f, fx, fy, w, h, 0.07f, LAYER, COL_LCD);
    if (facing(v, x, f) < 0.15f) return;
    float tx = caption && caption[0] ? fx + w * 0.14f : fx;
    Vector3 p = face_point(x, f, tx, fy, 0.11f);
    if (size >= 40) {               /* ghost segments on the big display only
                                       꺼진 칸은 큰 표시창에만 */
        char ghost[24];
        lcd_text(ghost, text, sizeof ghost);
        add_label(v, p, ghost, size, COL_LCD_DIM);
    }
    add_label(v, p, text, size, on);
    if (caption && caption[0])
        add_label(v, face_point(x, f, fx - w * 0.34f, fy, 0.11f), caption, 20,
                  shade(on, 0.75f));
}

static void draw_led(const Xform *x, const Face *f, float fx, float fy,
                     Color c, float radius)
{
    face_plate(x, f, fx, fy, radius * 2.6f, radius * 2.6f, 0.0f, 0.08f,
               COL_RECESS);
    DrawSphere(face_point(x, f, fx, fy, 0.08f + radius * 0.4f), radius, c);
}

/* Cards of one module on face f. avail = room on the face.
 * 면 f 위에 한 모듈의 카드를 그린다. avail = 면 위의 공간. */
static void draw_module(BombView *v, const Xform *x, const Face *f, int slot,
                        const SooState *st, float avail_w, float avail_h,
                        bool labels, bool selected, float floor_h)
{
    /* floor_h = height of the plate underneath. Layers above it:
     * recess, then the selection frame and cards on top of the recess.
     * floor_h = 아래 판의 높이. 그 위로 홈, 그리고 홈 위에 선택 테두리와 카드. */
    float recess_top = floor_h + LAYER;
    const SooLevel *l = st->level;
    float grid_w = l->cols * PITCH - (PITCH - TILE);
    float grid_d = l->rows * PITCH - (PITCH - TILE);
    float sc = fminf(1.0f, fminf(avail_w / grid_w, avail_h / grid_d));
    float pitch = PITCH * sc, tile = TILE * sc, th = TILE_H * sc;
    bool one_d = l->rows == 1;

    /* recess the cards sit in / 카드가 놓이는 홈 */
    float rec_w = grid_w * sc + 0.5f * sc, rec_h = grid_d * sc + 0.5f * sc;
    face_plate(x, f, 0, 0, rec_w, rec_h, floor_h, LAYER, COL_RECESS);
    if (selected) {
        Color a = UI_ACCENT;
        float t = 0.07f;
        float th2 = LAYER + 0.02f;       /* stands above the recess
                                            홈보다 높이 솟는다 */
        face_plate(x, f, 0, -rec_h * 0.5f - t, rec_w + 2 * t, t, floor_h, th2, a);
        face_plate(x, f, 0, rec_h * 0.5f + t, rec_w + 2 * t, t, floor_h, th2, a);
        face_plate(x, f, -rec_w * 0.5f - t, 0, t, rec_h, floor_h, th2, a);
        face_plate(x, f, rec_w * 0.5f + t, 0, t, rec_h, floor_h, th2, a);
    }

    bool show = labels && facing(v, x, f) > 0.3f;
    int value_size = 40, small = 20;
    if (show) {
        float px = screen_len(v, face_point(x, f, 0, 0, 0),
                              face_point(x, f, tile, 0, 0));
        if (px < 84.0f) value_size = 30;
        if (px < 46.0f) show = false;       /* too small to read / 너무 작음 */
    }

    for (int r = 0; r < l->rows; r++)
        for (int c = 0; c < l->cols; c++) {
            int k = r * l->cols + c;
            float fx = (c - (l->cols - 1) * 0.5f) * pitch;
            float fy = (r - (l->rows - 1) * 0.5f) * pitch;
            float lift = recess_top + 0.01f + v->press[slot][k] * 0.16f * sc;
            bool fault = st->cells[k] != 0;
            Color base = fault ? UI_FAULT : UI_SAFE;
            base = mix(base, (Color){ 255, 255, 255, 255 },
                       v->flash[slot][k] * 0.55f);
            face_plate(x, f, fx, fy, tile, tile, lift, th, shade(base, 0.86f));
            face_plate(x, f, fx, fy, tile * 0.84f, tile * 0.84f, lift + th,
                       0.012f, shade(base, 1.12f));

            bool active = st->active_row == r && st->active_col == c;
            if (active) {
                /* glowing frame that rises with the card
                   카드와 함께 올라오는 빛나는 테두리 */
                float t = 0.07f * sc, g = tile * 0.5f + 0.06f * sc;
                float fl = lift + th - 0.05f * sc;
                Color a = UI_ACCENT;
                face_plate(x, f, fx, fy - g, 2 * g + t, t, fl, 0.07f * sc, a);
                face_plate(x, f, fx, fy + g, 2 * g + t, t, fl, 0.07f * sc, a);
                face_plate(x, f, fx - g, fy, t, 2 * g, fl, 0.07f * sc, a);
                face_plate(x, f, fx + g, fy, t, 2 * g, fl, 0.07f * sc, a);
            }
            if (v->spark[slot][k]) {
                Vector3 top = face_point(x, f, fx, fy, lift + th);
                spawn_sparks(v, top, xf_dir(x, f->n),
                             fault ? (Color){ 255, 140, 170, 255 }
                                   : (Color){ 140, 255, 200, 255 },
                             16, 2.6f);
                v->spark[slot][k] = false;
            }

            if (!show) continue;
            float top = lift + th + 0.02f;
            char buf[32];
            snprintf(buf, sizeof buf, "%d", st->cells[k]);
            add_label(v, face_point(x, f, fx, fy, top), buf, value_size,
                      (Color){ 255, 255, 255, 255 });
            if (one_d) snprintf(buf, sizeof buf, "[%d]", c);
            else snprintf(buf, sizeof buf, "[%d][%d]", r, c);
            float off = one_d ? tile * 0.5f + 0.32f * sc + 0.06f : tile * 0.33f;
            add_label(v, face_point(x, f, fx, fy - off,
                                    one_d ? recess_top + 0.03f : top), buf,
                      small, one_d ? UI_INK : (Color){ 235, 235, 245, 255 });
            snprintf(buf, sizeof buf, "to %d", l->goal[k]);
            add_label(v, face_point(x, f, fx, fy + off,
                                    one_d ? recess_top + 0.03f : top), buf,
                      small, one_d ? UI_ACCENT : (Color){ 255, 236, 170, 255 });
        }
}

static Color status_led(const SooState *st, bool solved, bool running,
                        float time)
{
    if (st->status == SOO_FAILURE) return (Color){ 255, 60, 60, 255 };
    if (solved || st->status == SOO_SUCCESS) return (Color){ 70, 255, 140, 255 };
    if (running && fmodf(time, 0.6f) < 0.3f) return (Color){ 255, 190, 60, 255 };
    return (Color){ 60, 46, 40, 255 };
}

static void draw_flat(BombView *v, const BombScene *s)
{
    const SooState *st = s->modules[0].state;
    const SooLevel *l = st->level;
    float W, D;
    flat_size(l, &W, &D);
    Xform x = { QuaternionIdentity(), v3(0, 0, 0) };
    float top = FLAT_FEET + FLAT_H;

    draw_shadow(0.25f, 0.2f, W * 0.5f + 0.2f, D * 0.5f + 0.2f, 1.0f);
    /* feet, casing, top plate / 받침, 몸체, 윗판 */
    for (int k = 0; k < 4; k++)
        draw_box(&x, v3((k & 1) ? W * 0.5f - 0.5f : -W * 0.5f + 0.5f, FLAT_FEET * 0.5f,
                        (k & 2) ? D * 0.5f - 0.5f : -D * 0.5f + 0.5f),
                 v3(0.3f, FLAT_FEET * 0.5f, 0.3f), (Color){ 24, 24, 28, 255 });
    draw_box(&x, v3(0, FLAT_FEET + FLAT_H * 0.5f, 0),
             v3(W * 0.5f, FLAT_H * 0.5f, D * 0.5f), COL_CASING);
    Face lid = { v3(0, top, 0), v3(1, 0, 0), v3(0, 0, 1), v3(0, 1, 0) };
    face_plate(&x, &lid, 0, 0, W - 0.3f, D - 0.3f, 0, 0.04f, COL_TOP);

    /* hazard stripes along the front / 앞면의 경고 줄무늬 */
    Face front = { v3(0, FLAT_FEET + FLAT_H * 0.5f, D * 0.5f), v3(1, 0, 0),
                   v3(0, -1, 0), v3(0, 0, 1) };
    int stripes = (int)(W / 0.6f);
    for (int k = 0; k < stripes; k++) {
        float fx = -W * 0.5f + 0.3f + k * 0.6f;
        face_plate(&x, &front, fx, 0, 0.3f, FLAT_H * 0.4f, 0, LAYER,
                   (k & 1) ? (Color){ 30, 28, 26, 255 }
                           : (Color){ 236, 186, 40, 255 });
    }

    /* screws in the corners / 모서리 나사 */
    for (int k = 0; k < 4; k++) {
        Vector3 p = v3((k & 1) ? W * 0.5f - 0.38f : -W * 0.5f + 0.38f, top + 0.04f,
                       (k & 2) ? D * 0.5f - 0.38f : -D * 0.5f + 0.38f);
        DrawCylinder(p, 0.13f, 0.13f, 0.05f, 12, (Color){ 170, 174, 184, 255 });
    }

    /* the challenge: cards on the top face / 도전 과제: 윗면의 카드 */
    Face f = flat_top_face(l);
    draw_module(v, &x, &f, 0, st, 100.0f, 100.0f, true, false, 0.04f);

    /* display strip at the back / 뒤쪽 표시창 줄 */
    float zs = -D * 0.5f + 0.95f;
    Face strip = { v3(0, top + 0.04f, zs), v3(1, 0, 0), v3(0, 0, 1),
                   v3(0, 1, 0) };
    float left = -W * 0.5f + 0.5f;
    draw_lcd(v, &x, &strip, left + 1.9f, 0, 3.6f, 1.0f, s->lcd_caption, s->lcd,
             40, COL_LCD_ON);
    draw_lcd(v, &x, &strip, left + 5.3f, 0, 2.6f, 1.0f, NULL, s->lcd_small, 20,
             (Color){ 255, 190, 70, 255 });
    draw_led(&x, &strip, W * 0.5f - 0.9f, 0, status_led(st, s->modules[0].solved,
                                                        s->running, v->time),
             0.24f);

    /* wires leaving the right side / 오른쪽으로 나오는 전선 */
    static const Color WIRES[3] = { { 220, 50, 50, 255 }, { 60, 120, 230, 255 },
                                    { 236, 196, 50, 255 } };
    for (int k = 0; k < 3; k++) {
        float z = -D * 0.5f + 0.7f + k * 0.38f;
        Vector3 a = v3(W * 0.5f, top - 0.25f, z);
        Vector3 b = v3(W * 0.5f + 0.55f, top - 0.15f, z + 0.25f);
        Vector3 c = v3(W * 0.5f + 1.0f, 0.06f, z + 0.9f + k * 0.2f);
        DrawCylinderEx(a, b, 0.07f, 0.07f, 8, WIRES[k]);
        DrawSphere(b, 0.07f, WIRES[k]);
        DrawCylinderEx(b, c, 0.07f, 0.07f, 8, WIRES[k]);
    }
}

/* What is left after the explosion: a scorch mark and burnt pieces.
 * 폭발 뒤 남은 것: 그을음 자국과 탄 조각. */
static void draw_wreck(void)
{
    draw_shadow(0, 0, 3.6f, 3.6f, 1.0f);
    draw_shadow(0, 0, 2.2f, 2.2f, 1.0f);
    static const float P[7][5] = {   /* x, z, w, h, yaw */
        { 0.0f, 0.2f, 2.2f, 0.5f, 0.3f },   { -1.6f, 1.1f, 0.9f, 0.3f, 1.1f },
        { 1.7f, -0.6f, 1.1f, 0.35f, -0.7f }, { 0.6f, 1.9f, 0.6f, 0.25f, 0.5f },
        { -1.1f, -1.5f, 0.8f, 0.3f, 2.0f },  { 2.4f, 1.5f, 0.4f, 0.2f, 0.9f },
        { -2.5f, 0.0f, 0.5f, 0.2f, -1.2f },
    };
    for (int k = 0; k < 7; k++) {
        Xform x = { QuaternionFromAxisAngle(v3(0, 1, 0), P[k][4]),
                    v3(P[k][0], P[k][3] * 0.5f, P[k][1]) };
        draw_box(&x, v3(0, 0, 0), v3(P[k][2] * 0.5f, P[k][3] * 0.5f,
                                     P[k][2] * 0.35f),
                 k == 0 ? (Color){ 34, 32, 34, 255 } : (Color){ 46, 46, 52, 255 });
    }
}

static void draw_cube(BombView *v, const BombScene *s)
{
    if (v->boomed) {
        draw_wreck();
        return;
    }
    Vector3 c = cube_center(v);
    Xform x = { v->q, c };
    float h = CUBE_S * 0.5f;

    float strength = 1.0f - (c.y - h - CUBE_HOVER) * 2.0f;
    draw_shadow(0.3f, 0.3f, h * 0.95f, h * 0.95f, strength * 0.9f);

    draw_box(&x, v3(0, 0, 0), v3(h, h, h), COL_CASING);
    /* metal frame along the 12 edges / 12개 모서리의 금속 틀 */
    float t = 0.16f;
    for (int axis = 0; axis < 3; axis++)
        for (int k = 0; k < 4; k++) {
            float a = (k & 1) ? h : -h, b = (k & 2) ? h : -h;
            Vector3 p = axis == 0 ? v3(0, a, b) : axis == 1 ? v3(a, 0, b)
                                                            : v3(a, b, 0);
            Vector3 hs = axis == 0 ? v3(h + t, t, t) : axis == 1 ? v3(t, h + t, t)
                                                                 : v3(t, t, h + t);
            draw_box(&x, p, hs, COL_FRAME);
        }

    /* Cards may use most of the face width; height leaves room for the
     * corner number and light. / 카드는 면 너비 대부분을 쓰고, 높이는 모서리
     * 번호와 불빛 자리를 남긴다. */
    float avail_w = CUBE_S - 0.9f, avail_h = CUBE_S - 1.9f;
    for (int m = 0; m < FACES; m++) {
        Face f = CUBE_FACES[m];
        f.o = Vector3Scale(f.n, h);
        face_plate(&x, &f, 0, 0, CUBE_S - 0.5f, CUBE_S - 0.5f, 0, LAYER, COL_TOP);
        if (m < s->count) {
            const SooState *st = s->modules[m].state;
            /* Only the module being worked on gets text; the other faces
             * show card colours, which keeps the bomb uncluttered.
             * 작업 중인 모듈에만 글자를 넣고, 다른 면은 카드 색만 보여 주어
             * 폭탄을 깔끔하게 유지한다. */
            draw_module(v, &x, &f, m, st, avail_w, avail_h, m == s->selected,
                        m == s->selected, LAYER);
            draw_led(&x, &f, h - 0.62f, -h + 0.62f,
                     status_led(st, s->modules[m].solved,
                                s->running && m == s->selected, v->time),
                     0.2f);
            if (facing(v, &x, &f) > 0.3f) {
                char num[16];
                snprintf(num, sizeof num, "%d", m + 1);
                add_label(v, face_point(&x, &f, -h + 0.62f, -h + 0.62f, 0.06f),
                          num, 30, UI_MUTED);
            }
        } else if (m == BOMB_FACE_TOP) {
            /* Lay the display out facing the selected side, so it reads
             * left to right whichever module the player is looking at.
             * 표시창을 선택한 옆면 쪽으로 배치하여 어느 모듈을 보든 왼쪽에서
             * 오른쪽으로 읽히게 한다. */
            if (s->selected >= 0 && s->selected < 4) {
                f.u = CUBE_FACES[s->selected].u;
                f.v = CUBE_FACES[s->selected].n;
            }
            /* display module: big LCD and one light per module
               표시창 모듈: 큰 LCD와 모듈별 불빛 */
            Color on = COL_LCD_ON;
            if (s->lcd_alert && fmodf(v->time, 0.5f) < 0.25f)
                on = (Color){ 255, 220, 200, 255 };   /* blink / 깜박임 */
            draw_lcd(v, &x, &f, 0, 0.9f, 4.4f, 1.3f, s->lcd_caption, s->lcd, 40,
                     on);
            draw_lcd(v, &x, &f, 0, -0.75f, 3.0f, 0.8f, NULL, s->lcd_small, 20,
                     (Color){ 255, 190, 70, 255 });
            for (int k = 0; k < s->count; k++)
                draw_led(&x, &f, (k - (s->count - 1) * 0.5f) * 0.85f, -1.9f,
                         status_led(s->modules[k].state, s->modules[k].solved,
                                    false, v->time),
                         0.17f);
        }
    }
}

void bomb_view_render(BombView *v, const BombScene *s)
{
    v->nlabels = 0;
    place_camera(v, v->cam_target, v->cam_dist);
    if (v->shake > 0) {
        float a = v->shake * v->shake * 0.18f;
        Vector3 jitter = v3((frand() - 0.5f) * a, (frand() - 0.5f) * a,
                            (frand() - 0.5f) * a);
        v->cam.position = Vector3Add(v->cam.position, jitter);
        v->cam.target = Vector3Add(v->cam.target, jitter);
    }

    BeginTextureMode(v->tex);
    ClearBackground(COL_ROOM);
    /* raylib's default near plane (0.01-0.05) wastes depth precision; the
     * camera is always 10+ units away, so 1.0 is safe and stops flicker.
     * raylib 기본 근거리 평면(0.01-0.05)은 깊이 정밀도를 낭비한다. 카메라는
     * 항상 10 이상 떨어져 있으므로 1.0이 안전하고 깜빡임을 없앤다. */
    double old_near = rlGetCullDistanceNear(), old_far = rlGetCullDistanceFar();
    rlSetClipPlanes(CLIP_NEAR, CLIP_FAR);
    BeginMode3D(v->cam);
    rlDisableBackfaceCulling();
    draw_table();
    /* a scene without module state has nothing to put on the table
       모듈 상태가 없는 장면은 탁자에 올릴 것이 없다 */
    bool ready = s->count > 0;
    for (int m = 0; m < s->count && m < BOMB_MAX_MODULES; m++)
        if (!s->modules[m].state) ready = false;
    if (ready && s->cube) draw_cube(v, s);
    else if (ready) draw_flat(v, s);
    draw_particles(v);
    rlEnableBackfaceCulling();
    EndMode3D();
    rlSetClipPlanes(old_near, old_far);

    /* warm lamp light and a dark vignette / 따뜻한 조명과 어두운 가장자리 */
    /* (DrawCircleGradient's arguments differ between raylib 5.5 and 6.0,
     * so the glow is built from soft circles instead.
     * DrawCircleGradient의 인자가 raylib 5.5와 6.0에서 달라서 부드러운 원을
     * 겹쳐 만든다.) */
    for (int k = 0; k < 6; k++)
        DrawCircleV((Vector2){ v->w * 0.5f, v->h * 0.5f }, v->w * (0.62f - k * 0.08f),
                    (Color){ 255, 214, 160, 5 });
    DrawRectangleGradientV(0, 0, v->w, 120, (Color){ 0, 0, 0, 150 },
                           (Color){ 0, 0, 0, 0 });
    DrawRectangleGradientV(0, v->h - 160, v->w, 160, (Color){ 0, 0, 0, 0 },
                           (Color){ 0, 0, 0, 170 });

    for (int k = 0; k < v->nlabels; k++) {
        const Label *l = &v->labels[k];
        Vector2 p = GetWorldToScreenEx(l->pos, v->cam, v->w, v->h);
        int tw = MeasureText(l->text, l->size);
        int x = (int)roundf(p.x) - tw / 2, y = (int)roundf(p.y) - l->size / 2;
        if (l->color.r != COL_LCD_DIM.r || l->color.g != COL_LCD_DIM.g)
            DrawText(l->text, x + 2, y + 2, l->size, (Color){ 0, 0, 0, 140 });
        DrawText(l->text, x, y, l->size, l->color);
    }

    if (v->red_flash > 0)
        DrawRectangle(0, 0, v->w, v->h,
                      (Color){ 255, 40, 40, (unsigned char)(90 * v->red_flash) });
    if (v->green_flash > 0)
        DrawRectangle(0, 0, v->w, v->h,
                      (Color){ 60, 255, 160, (unsigned char)(40 * v->green_flash) });
    if (v->boomed && v->boom_t < 2.0f)
        DrawRectangle(0, 0, v->w, v->h,
                      (Color){ 255, 110, 30,
                               (unsigned char)(70 * (1.0f - v->boom_t / 2.0f)) });
    if (v->white_flash > 0)
        DrawRectangle(0, 0, v->w, v->h,
                      (Color){ 255, 250, 235, (unsigned char)(255 * v->white_flash) });
    EndTextureMode();
}

/* Self-test for the cube's click picking (run with --selftest): present
 * each module, project the centre of its face to the screen and check that
 * a click there picks the same face.
 * 큐브 클릭 선택 자체 검사 (--selftest로 실행): 모듈마다 그 면을 보여 주고,
 * 면 중심을 화면에 투영한 뒤 그 위치를 클릭하면 같은 면이 골라지는지 확인. */
int bomb_view_self_test(BombView *v, const BombScene *cube_scene)
{
    int failures = 0;
    v->layout = -1;
    bomb_view_update(v, cube_scene, 0.0f, (Vector2){ -1, -1 }, false);
    for (int m = 0; m < cube_scene->count; m++) {
        bomb_view_select(v, m, false);
        place_camera(v, v->cam_target, v->cam_dist);
        Vector3 c = cube_center(v);
        Vector3 face_c = Vector3Add(c, Vector3RotateByQuaternion(
                                           Vector3Scale(CUBE_FACES[m].n,
                                                        CUBE_S * 0.5f), v->q));
        Vector2 sp = GetWorldToScreenEx(face_c, v->cam, v->w, v->h);
        int got = pick_face(v, sp, c);
        bool on_screen = sp.x > 0 && sp.y > 0 && sp.x < v->w && sp.y < v->h;
        printf("  module %d: face centre at (%.0f, %.0f) picks face %d %s\n", m,
               sp.x, sp.y, got, got == m && on_screen ? "ok" : "FAIL");
        if (got != m || !on_screen) failures++;
    }
    return failures;
}
