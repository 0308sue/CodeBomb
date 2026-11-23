/*
 * scene_mailroom.c - Level 1 "Mailroom": inbox conveyor, worker robot and
 * outbox conveyor in 3D.
 * scene_mailroom.c - 레벨 1 "우편실": 입력 컨베이어, 작업 로봇, 출력
 * 컨베이어로 이루어진 3D 장면.
 *
 *   take()  The robot drives to the inbox and lifts the front parcel. If it
 *           was already holding one, that value is lost: the old parcel
 *           drops and vanishes.
 *           로봇이 입력함으로 가서 맨 앞 상자를 든다. 이미 들고 있던 상자가
 *           있으면 그 값은 사라진다: 이전 상자가 떨어져 없어진다.
 *   put(n)  The robot drives to the outbox and sets down a parcel stamped n.
 *           With empty hands it stamps a fresh parcel: in C, values are
 *           copies, not physical objects.
 *           로봇이 출력함으로 가서 n이 찍힌 상자를 내려놓는다. 손이 비어
 *           있으면 새 상자에 도장을 찍는다: C에서 값은 물건이 아니라 복사본이다.
 */
#include "scene.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARCELS  64
#define LANE_Z       1.6f
#define INBOX_X     -4.2f
#define OUTBOX_X     4.2f
#define ROBOT_IN_X  -2.9f
#define ROBOT_OUT_X  2.9f
#define SLOT_GAP     1.15f
#define BELT_TOP     0.35f
#define BELT_LEN     8.0f
#define BOX          0.7f
#define ROBOT_SPEED  5.0f
#define WORK_TIME    0.3f
#define HOLD_Y       2.45f

typedef enum { P_DEAD, P_INBOX, P_HELD, P_OUTBOX, P_DISCARD } ParcelState;

typedef struct {
    ParcelState state;
    int32_t     value;
    Vector3     pos;
    Vector3     target;
    float       scale;
} Parcel;

typedef enum { PHASE_IDLE, PHASE_WALK, PHASE_WORK } Phase;

typedef struct {
    Parcel parcels[MAX_PARCELS];
    int    inbox_q[MAX_PARCELS];  /* parcel indices, front first
                                     상자 번호, 맨 앞부터 */
    int    inbox_len;
    int    outbox_q[MAX_PARCELS]; /* parcel indices, newest first
                                     상자 번호, 최신부터 */
    int    outbox_len;
    int    held;                  /* parcel index or -1 / 상자 번호 또는 -1 */

    float  robot_x;
    float  robot_target_x;
    float  arm_lift;              /* 0 = arms down, 1 = up / 0 = 내림, 1 = 올림 */
    Phase  phase;
    float  work_timer;
    LevelEvent pending;

    float    time;
    Camera3D cam;
} Mailroom;

/* ---- helpers / 도우미 --------------------------------------------------- */

static Vector3 v3(float x, float y, float z) { return (Vector3){ x, y, z }; }

static Vector3 lerp3(Vector3 a, Vector3 b, float t)
{
    return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t);
}

static float approach(float cur, float target, float step)
{
    if (cur < target) return (cur + step > target) ? target : cur + step;
    return (cur - step < target) ? target : cur - step;
}

static Color shade(Color c, float f)
{
    return (Color){ (unsigned char)(c.r * f), (unsigned char)(c.g * f),
                    (unsigned char)(c.b * f), c.a };
}

static Color parcel_color(int32_t value)
{
    static const Color palette[] = {
        { 214, 150, 92, 255 }, { 196, 134, 80, 255 },
        { 226, 172, 112, 255 }, { 186, 122, 72, 255 },
    };
    return palette[(unsigned)value % 4u];
}

static Vector3 inbox_slot(int k)
{
    return v3(INBOX_X, BELT_TOP + BOX * 0.5f, LANE_Z - k * SLOT_GAP);
}

static Vector3 outbox_slot(int k)
{
    return v3(OUTBOX_X, BELT_TOP + BOX * 0.5f, LANE_Z - k * SLOT_GAP);
}

static Vector3 hold_pos(const Mailroom *m)
{
    return v3(m->robot_x, HOLD_Y, LANE_Z);
}

static int spawn(Mailroom *m, int32_t value, Vector3 pos)
{
    for (int i = 0; i < MAX_PARCELS; ++i) {
        if (m->parcels[i].state != P_DEAD) continue;
        Parcel *p = &m->parcels[i];
        p->state = P_INBOX;
        p->value = value;
        p->pos = p->target = pos;
        p->scale = 1.0f;
        return i;
    }
    return -1;
}

static void layout_queues(Mailroom *m)
{
    for (int k = 0; k < m->inbox_len; ++k)
        m->parcels[m->inbox_q[k]].target = inbox_slot(k);
    for (int k = 0; k < m->outbox_len; ++k)
        m->parcels[m->outbox_q[k]].target = outbox_slot(k);
}

/* ---- scene callbacks / 장면 콜백 ---------------------------------------- */

static void mailroom_reset(void *state, const LevelRun *run)
{
    Mailroom *m = state;
    Camera3D cam = m->cam;
    memset(m, 0, sizeof *m);
    m->cam = cam;
    m->held = -1;

    for (int i = run->inbox_pos; i < run->inbox_len; ++i) {
        int idx = spawn(m, run->inbox[i], inbox_slot(m->inbox_len));
        if (idx >= 0) m->inbox_q[m->inbox_len++] = idx;
    }
}

static void mailroom_on_event(void *state, LevelEvent ev)
{
    Mailroom *m = state;
    m->pending = ev;
    m->phase = PHASE_WALK;
    m->robot_target_x = (ev.type == LEVEL_EVENT_TAKE) ? ROBOT_IN_X
                                                      : ROBOT_OUT_X;
}

/* Runs when the robot reaches the belt.
 * 로봇이 컨베이어에 도착하면 실행된다. */
static void perform_action(Mailroom *m)
{
    LevelEvent ev = m->pending;

    if (ev.type == LEVEL_EVENT_TAKE) {
        if (m->held >= 0) {
            /* The overwritten value is lost / 덮어쓴 값은 사라진다 */
            Parcel *old = &m->parcels[m->held];
            old->state = P_DISCARD;
            old->target = v3(m->robot_x + 0.9f, BOX * 0.5f, LANE_Z + 1.3f);
            m->held = -1;
        }
        if (m->inbox_len > 0) {
            int idx = m->inbox_q[0];
            memmove(m->inbox_q, m->inbox_q + 1,
                    sizeof(int) * (size_t)(m->inbox_len - 1));
            m->inbox_len--;
            m->parcels[idx].state = P_HELD;
            m->held = idx;
        }
    } else if (ev.type == LEVEL_EVENT_PUT) {
        int idx = m->held;
        if (idx < 0) idx = spawn(m, ev.value, hold_pos(m));
        if (idx >= 0 && m->outbox_len < MAX_PARCELS) {
            Parcel *p = &m->parcels[idx];
            p->value = ev.value;
            p->state = P_OUTBOX;
            memmove(m->outbox_q + 1, m->outbox_q,
                    sizeof(int) * (size_t)m->outbox_len);
            m->outbox_q[0] = idx;
            m->outbox_len++;
        }
        m->held = -1;
    }
    layout_queues(m);
}

static void mailroom_update(void *state, float dt)
{
    Mailroom *m = state;
    m->time += dt;

    if (m->phase == PHASE_WALK) {
        m->robot_x = approach(m->robot_x, m->robot_target_x, ROBOT_SPEED * dt);
        if (fabsf(m->robot_x - m->robot_target_x) < 0.001f) {
            perform_action(m);
            m->phase = PHASE_WORK;
            m->work_timer = WORK_TIME;
        }
    } else if (m->phase == PHASE_WORK) {
        m->work_timer -= dt;
        if (m->work_timer <= 0.0f) m->phase = PHASE_IDLE;
    }

    /* Frame-rate independent smoothing / 프레임률과 무관한 부드러운 이동 */
    float k = 1.0f - expf(-12.0f * dt);
    float target_lift = (m->held >= 0) ? 1.0f : 0.0f;
    m->arm_lift += (target_lift - m->arm_lift) * k;

    for (int i = 0; i < MAX_PARCELS; ++i) {
        Parcel *p = &m->parcels[i];
        switch (p->state) {
        case P_DEAD:
            break;
        case P_HELD:
            p->target = hold_pos(m);
            p->pos = lerp3(p->pos, p->target, 1.0f - expf(-20.0f * dt));
            break;
        case P_DISCARD:
            p->pos = lerp3(p->pos, p->target, k);
            p->scale -= dt * 1.4f;
            if (p->scale <= 0.0f) p->state = P_DEAD;
            break;
        case P_INBOX:
        case P_OUTBOX:
            p->pos = lerp3(p->pos, p->target, k);
            break;
        }
    }
}

static bool mailroom_busy(const void *state)
{
    const Mailroom *m = state;
    return m->phase != PHASE_IDLE;
}

/* ---- drawing / 그리기 --------------------------------------------------- */

static void draw_conveyor(float x, float time, float dir)
{
    float zc = LANE_Z + 0.6f - BELT_LEN * 0.5f;
    float zmin = zc - BELT_LEN * 0.5f;
    Color base = { 58, 62, 70, 255 };

    DrawCube(v3(x, BELT_TOP * 0.5f, zc), 1.2f, BELT_TOP, BELT_LEN, base);
    DrawCubeWires(v3(x, BELT_TOP * 0.5f, zc), 1.2f, BELT_TOP, BELT_LEN,
                  shade(base, 0.6f));

    /* Moving slats show the belt direction / 움직이는 판이 벨트 방향을 보여 줌 */
    float phase = fmodf(time * 0.6f * dir + 1000.0f, 0.5f);
    for (int i = 0; i < 16; ++i) {
        float z = zmin + fmodf(i * 0.5f + phase, BELT_LEN);
        DrawCube(v3(x, BELT_TOP + 0.004f, z), 1.0f, 0.01f, 0.07f,
                 (Color){ 40, 43, 50, 255 });
    }

    Color rail = { 230, 180, 40, 255 };
    DrawCube(v3(x - 0.63f, BELT_TOP + 0.06f, zc), 0.06f, 0.14f, BELT_LEN, rail);
    DrawCube(v3(x + 0.63f, BELT_TOP + 0.06f, zc), 0.06f, 0.14f, BELT_LEN, rail);
}

static void draw_parcel(const Parcel *p)
{
    float s = BOX * p->scale;
    Color c = parcel_color(p->value);
    DrawCube(p->pos, s, s, s, c);
    DrawCubeWires(p->pos, s, s, s, shade(c, 0.55f));
    /* Packing tape / 포장 테이프 */
    DrawCube(v3(p->pos.x, p->pos.y + s * 0.5f + 0.004f, p->pos.z),
             s * 0.22f, 0.012f, s * 1.01f, (Color){ 235, 215, 170, 255 });
}

static void draw_robot(const Mailroom *m)
{
    float x = m->robot_x, z = LANE_Z;
    float bob = (m->phase == PHASE_WALK) ? sinf(m->time * 18.0f) * 0.04f : 0.0f;
    Color body = { 242, 169, 0, 255 };
    Color dark = { 38, 40, 48, 255 };
    Color head = { 205, 210, 218, 255 };

    DrawCylinder(v3(x, 0.0f, z), 0.55f, 0.62f, 0.22f, 24, dark);

    DrawCube(v3(x, 0.82f + bob, z), 0.95f, 1.0f, 0.72f, body);
    DrawCubeWires(v3(x, 0.82f + bob, z), 0.95f, 1.0f, 0.72f, shade(body, 0.6f));
    DrawCube(v3(x, 0.9f + bob, z + 0.365f), 0.5f, 0.32f, 0.02f, dark);

    DrawCube(v3(x, 1.58f + bob, z), 0.7f, 0.48f, 0.58f, head);
    DrawCubeWires(v3(x, 1.58f + bob, z), 0.7f, 0.48f, 0.58f, shade(head, 0.6f));
    DrawCube(v3(x, 1.6f + bob, z + 0.295f), 0.52f, 0.2f, 0.02f, dark);

    Color eye = (m->phase == PHASE_WORK) ? (Color){ 90, 230, 120, 255 }
                                         : (Color){ 80, 200, 255, 255 };
    DrawSphere(v3(x - 0.13f, 1.6f + bob, z + 0.31f), 0.055f, eye);
    DrawSphere(v3(x + 0.13f, 1.6f + bob, z + 0.31f), 0.055f, eye);

    for (int side = -1; side <= 1; side += 2) {
        float ax = x + side * 0.6f;
        float ay = 0.75f + m->arm_lift * 0.95f + bob;
        DrawCube(v3(ax, ay, z), 0.16f, 0.75f, 0.16f, shade(body, 0.8f));
        float hand_y = ay + (m->arm_lift > 0.5f ? 0.42f : -0.42f);
        DrawSphere(v3(ax, hand_y, z), 0.1f, dark);
    }
}

/* Project a 3D point to the screen and draw a 2D label there.
 * 3D 점을 화면에 투영하고 그 위치에 2D 라벨을 그린다. */
static void draw_label(Camera3D cam, Vector3 world, const char *text,
                       int size, Color bg, Color fg, int w, int h)
{
    Vector2 s = GetWorldToScreenEx(world, cam, w, h);
    int tw = MeasureText(text, size);
    int x = (int)s.x - tw / 2;
    int y = (int)s.y - size / 2;
    DrawRectangle(x - 5, y - 3, tw + 10, size + 6, bg);
    DrawText(text, x, y, size, fg);
}

static void mailroom_draw(void *state, int w, int h)
{
    Mailroom *m = state;
    ClearBackground((Color){ 30, 32, 38, 255 });

    BeginMode3D(m->cam);
    DrawPlane(v3(0.0f, -0.02f, -1.5f), (Vector2){ 18.0f, 14.0f },
              (Color){ 50, 54, 62, 255 });
    DrawGrid(18, 1.0f);
    DrawCube(v3(0.0f, -0.005f, LANE_Z), 6.6f, 0.01f, 1.4f,
             (Color){ 72, 76, 86, 255 });

    draw_conveyor(INBOX_X, m->time, 1.0f);
    draw_conveyor(OUTBOX_X, m->time, -1.0f);

    for (int i = 0; i < MAX_PARCELS; ++i)
        if (m->parcels[i].state != P_DEAD) draw_parcel(&m->parcels[i]);
    draw_robot(m);
    EndMode3D();

    Color sign_bg = { 20, 22, 28, 220 };
    draw_label(m->cam, v3(INBOX_X, 0.05f, LANE_Z + 1.2f), "INBOX", 20,
               sign_bg, (Color){ 120, 200, 255, 255 }, w, h);
    draw_label(m->cam, v3(OUTBOX_X, 0.05f, LANE_Z + 1.2f), "OUTBOX", 20,
               sign_bg, (Color){ 130, 230, 140, 255 }, w, h);

    for (int i = 0; i < MAX_PARCELS; ++i) {
        const Parcel *p = &m->parcels[i];
        if (p->state == P_DEAD || p->scale < 0.4f) continue;
        Vector3 face = v3(p->pos.x, p->pos.y, p->pos.z + BOX * p->scale * 0.5f);
        draw_label(m->cam, face, TextFormat("%d", (int)p->value), 20,
                   (Color){ 250, 245, 230, 235 }, (Color){ 30, 30, 30, 255 },
                   w, h);
    }
}

static void mailroom_destroy(void *state) { free(state); }

Scene scene_mailroom_create(void)
{
    Mailroom *m = calloc(1, sizeof *m);
    if (m) {
        m->cam.position = v3(0.0f, 8.0f, 10.5f);
        m->cam.target = v3(0.0f, 0.6f, -1.0f);
        m->cam.up = v3(0.0f, 1.0f, 0.0f);
        m->cam.fovy = 42.0f;
        m->cam.projection = CAMERA_PERSPECTIVE;
        m->held = -1;
    }
    Scene s = {
        .state = m,
        .reset = mailroom_reset,
        .on_event = mailroom_on_event,
        .update = mailroom_update,
        .busy = mailroom_busy,
        .draw = mailroom_draw,
        .destroy = mailroom_destroy,
    };
    return s;
}
