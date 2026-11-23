/*
 * scene_mailroom.c - the Mailroom in 3D: inbox conveyor, worker robot,
 * outbox conveyor and a recycling bin.
 * scene_mailroom.c - 3D 우편실: 입력 컨베이어, 작업 로봇, 출력 컨베이어,
 * 재활용 통.
 *
 *   take()  The robot drives to the inbox, lifts the front parcel and adds
 *           it to the stack in its arms (its number is now in your
 *           variables, so nothing is thrown away).
 *           로봇이 입력함으로 가서 맨 앞 상자를 들고 팔의 상자 더미에
 *           올린다 (숫자는 이제 변수에 있으므로 아무것도 버리지 않는다).
 *   put(n)  Before walking, the robot gets parcel n ready where it stands:
 *             - holding a parcel stamped n: it keeps that one, and the
 *               others go into the recycling bin;
 *             - holding parcels that add up to n: they slide together and
 *               merge into one, which pulses and glows ("10 + 7 = 17");
 *             - otherwise its parcel is restamped n, or a new parcel pops
 *               into its hands.
 *           Then it drives to the outbox and sets the parcel down.
 *           걷기 전에 로봇은 그 자리에서 n 상자를 준비한다:
 *             - n이 찍힌 상자를 들고 있으면 그것만 남기고 나머지는 재활용
 *               통으로 보낸다;
 *             - 더해서 n이 되는 상자들을 들고 있으면 하나로 합쳐지며
 *               번쩍이고 빛난다 ("10 + 7 = 17");
 *             - 그 밖에는 상자에 n을 다시 찍거나, 새 상자가 손에 나타난다.
 *           그다음 출력함으로 가서 상자를 내려놓는다.
 */
#include "scene.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARCELS  64
#define MAX_SPARKS   96
#define HAND_MAX     4        /* parcels the robot can stack / 들 수 있는 상자 */
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
#define MERGE_TIME   0.6f     /* boxes slide together / 상자가 합쳐지는 시간 */
#define STAMP_TIME   0.45f    /* restamp or new box / 다시 찍기, 새 상자 */
#define SORT_TIME    0.4f     /* extras fly to the bin / 남는 상자를 버리는 시간 */
#define BIN_POS      ((Vector3){ 0.0f, 0.0f, LANE_Z - 2.3f })

typedef enum {
    P_DEAD, P_INBOX, P_HELD, P_OUTBOX, P_MERGING, P_RECYCLE
} ParcelState;

typedef struct {
    ParcelState state;
    int32_t     value;
    Vector3     pos;
    Vector3     target;
    Vector3     from;         /* start of the arc to the bin / 통까지 곡선 시작 */
    float       t;            /* arc progress 0-1 / 곡선 진행 */
    float       scale;
    float       pulse;        /* 1 after a merge or stamp, fades to 0
                                 합치거나 찍은 뒤 1, 0으로 줄어듦 */
    float       glow;
} Parcel;

typedef struct {
    Vector3 p, v;
    float   life;
    Color   c;
} Spark;

typedef enum { PHASE_IDLE, PHASE_PREP, PHASE_WALK, PHASE_WORK } Phase;
typedef enum { PREP_NONE, PREP_SORT, PREP_MERGE, PREP_STAMP, PREP_NEW } PrepKind;

typedef struct {
    Parcel parcels[MAX_PARCELS];
    int    inbox_q[MAX_PARCELS];  /* parcel indices, front first
                                     상자 번호, 맨 앞부터 */
    int    inbox_len;
    int    outbox_q[MAX_PARCELS]; /* parcel indices, newest first
                                     상자 번호, 최신부터 */
    int    outbox_len;
    int    hand[HAND_MAX];        /* parcels in the robot's arms, bottom first
                                     로봇 팔의 상자, 아래부터 */
    int    hand_n;

    float  robot_x;
    float  robot_target_x;
    float  arm_lift;              /* 0 = arms down, 1 = up / 0 = 내림, 1 = 올림 */
    Phase  phase;
    float  timer;
    PrepKind prep;
    int32_t  prep_value;          /* the number being put / 내보낼 숫자 */
    LevelEvent pending;

    Spark  sparks[MAX_SPARKS];
    char   popup[48];             /* "10 + 7 = 17" above the robot */
    float  popup_t;
    float  bin_bump;

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

static Color mix(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t),
                    (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), a.a };
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

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

/* k-th parcel of the stack in the robot's arms / 팔에 든 더미의 k번째 상자 */
static Vector3 hold_pos(const Mailroom *m, int k)
{
    return v3(m->robot_x, HOLD_Y + k * BOX * 0.95f, LANE_Z);
}

static int spawn(Mailroom *m, int32_t value, Vector3 pos)
{
    for (int i = 0; i < MAX_PARCELS; ++i) {
        if (m->parcels[i].state != P_DEAD) continue;
        Parcel *p = &m->parcels[i];
        memset(p, 0, sizeof *p);
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

static void sparks_at(Mailroom *m, Vector3 at, Color c, int n)
{
    for (int k = 0; k < MAX_SPARKS && n > 0; ++k) {
        Spark *s = &m->sparks[k];
        if (s->life > 0) continue;
        float a = frand() * 6.2832f;
        s->p = at;
        s->v = v3(cosf(a) * (1.5f + frand() * 2.0f), 1.5f + frand() * 3.0f,
                  sinf(a) * (1.5f + frand() * 2.0f));
        s->life = 0.5f + frand() * 0.4f;
        s->c = frand() < 0.4f ? (Color){ 255, 250, 220, 255 } : c;
        n--;
    }
}

static void set_popup(Mailroom *m, const char *text)
{
    snprintf(m->popup, sizeof m->popup, "%s", text);
    m->popup_t = 1.4f;
}

/* Send a held parcel flying into the recycling bin.
 * 들고 있던 상자를 재활용 통으로 던진다. */
static void recycle(Mailroom *m, int idx)
{
    Parcel *p = &m->parcels[idx];
    p->state = P_RECYCLE;
    p->from = p->pos;
    p->t = 0;
}

/* ---- scene callbacks / 장면 콜백 ---------------------------------------- */

static void mailroom_reset(void *state, const LevelRun *run)
{
    Mailroom *m = state;
    Camera3D cam = m->cam;
    memset(m, 0, sizeof *m);
    m->cam = cam;

    for (int i = run->inbox_pos; i < run->inbox_len; ++i) {
        int idx = spawn(m, run->inbox[i], inbox_slot(m->inbox_len));
        if (idx >= 0) m->inbox_q[m->inbox_len++] = idx;
    }
}

/* put(n): decide how to get parcel n ready before walking.
 * put(n): 걷기 전에 n 상자를 어떻게 준비할지 정한다. */
static void plan_put(Mailroom *m, int32_t v)
{
    m->prep_value = v;
    int match = -1;
    for (int k = m->hand_n - 1; k >= 0 && match < 0; --k)
        if (m->parcels[m->hand[k]].value == v) match = k;

    if (match >= 0) {
        /* keep the matching parcel, recycle the rest
           맞는 상자만 남기고 나머지는 재활용 */
        int keep = m->hand[match];
        for (int k = 0; k < m->hand_n; ++k)
            if (k != match) recycle(m, m->hand[k]);
        m->prep = m->hand_n > 1 ? PREP_SORT : PREP_NONE;
        m->hand[0] = keep;
        m->hand_n = 1;
        return;
    }
    if (m->hand_n >= 2) {
        /* merge everything into the bottom parcel / 맨 아래 상자로 합치기 */
        int32_t sum = 0;
        char text[48] = "";
        for (int k = 0; k < m->hand_n; ++k) {
            int32_t val = m->parcels[m->hand[k]].value;
            sum += val;
            size_t used = strlen(text);
            snprintf(text + used, sizeof text - used, "%s%d", k ? " + " : "",
                     (int)val);
            if (k > 0) m->parcels[m->hand[k]].state = P_MERGING;
        }
        if (sum == v) {
            size_t used = strlen(text);
            snprintf(text + used, sizeof text - used, " = %d", (int)v);
        } else {
            snprintf(text, sizeof text, "%d boxes -> %d", m->hand_n, (int)v);
        }
        set_popup(m, text);
        m->prep = PREP_MERGE;
        return;
    }
    if (m->hand_n == 1) {
        Parcel *p = &m->parcels[m->hand[0]];
        char text[48];
        snprintf(text, sizeof text, "%d -> %d", (int)p->value, (int)v);
        set_popup(m, text);
        m->prep = PREP_STAMP;
        return;
    }
    /* empty hands: a new parcel appears / 빈손: 새 상자가 나타난다 */
    int idx = spawn(m, v, hold_pos(m, 0));
    if (idx < 0) {
        m->prep = PREP_NONE;
        return;
    }
    Parcel *p = &m->parcels[idx];
    p->state = P_HELD;
    p->scale = 0.05f;
    p->glow = 1.0f;
    m->hand[0] = idx;
    m->hand_n = 1;
    char text[48];
    snprintf(text, sizeof text, "new %d", (int)v);
    set_popup(m, text);
    m->prep = PREP_NEW;
}

/* The preparation is finished: the parcel in hand now shows n.
 * 준비 완료: 손의 상자가 이제 n을 보여 준다. */
static void finish_prep(Mailroom *m)
{
    if (m->hand_n == 0) return;
    Parcel *keeper = &m->parcels[m->hand[0]];
    if (m->prep == PREP_MERGE) {
        for (int k = 1; k < m->hand_n; ++k)
            m->parcels[m->hand[k]].state = P_DEAD;
        m->hand_n = 1;
    }
    if (m->prep == PREP_MERGE || m->prep == PREP_STAMP ||
        m->prep == PREP_NEW) {
        keeper->value = m->prep_value;
        keeper->pulse = 1.0f;
        keeper->glow = 1.0f;
        keeper->scale = 1.0f;
        sparks_at(m, keeper->pos, (Color){ 255, 210, 90, 255 },
                  m->prep == PREP_MERGE ? 36 : 20);
    }
}

static void mailroom_on_event(void *state, LevelEvent ev)
{
    Mailroom *m = state;
    m->pending = ev;
    if (ev.type == LEVEL_EVENT_PUT) {
        plan_put(m, ev.value);
        if (m->prep != PREP_NONE) {
            m->phase = PHASE_PREP;
            m->timer = m->prep == PREP_MERGE ? MERGE_TIME
                     : m->prep == PREP_SORT  ? SORT_TIME
                                             : STAMP_TIME;
            return;
        }
    }
    m->phase = PHASE_WALK;
    m->robot_target_x = (ev.type == LEVEL_EVENT_TAKE) ? ROBOT_IN_X
                                                      : ROBOT_OUT_X;
}

/* Runs when the robot reaches a belt. / 로봇이 컨베이어에 도착하면 실행. */
static void perform_action(Mailroom *m)
{
    LevelEvent ev = m->pending;

    if (ev.type == LEVEL_EVENT_TAKE) {
        if (m->hand_n == HAND_MAX) {          /* arms full: oldest to the bin
                                                 팔이 가득: 가장 오래된 것을 통에 */
            recycle(m, m->hand[0]);
            memmove(m->hand, m->hand + 1, sizeof(int) * (HAND_MAX - 1));
            m->hand_n--;
        }
        if (m->inbox_len > 0) {
            int idx = m->inbox_q[0];
            memmove(m->inbox_q, m->inbox_q + 1,
                    sizeof(int) * (size_t)(m->inbox_len - 1));
            m->inbox_len--;
            m->parcels[idx].state = P_HELD;
            m->hand[m->hand_n++] = idx;
        }
    } else if (ev.type == LEVEL_EVENT_PUT && m->hand_n > 0) {
        int idx = m->hand[0];
        if (m->outbox_len < MAX_PARCELS) {
            Parcel *p = &m->parcels[idx];
            p->state = P_OUTBOX;
            memmove(m->outbox_q + 1, m->outbox_q,
                    sizeof(int) * (size_t)m->outbox_len);
            m->outbox_q[0] = idx;
            m->outbox_len++;
        }
        memmove(m->hand, m->hand + 1, sizeof(int) * (HAND_MAX - 1));
        m->hand_n--;
    }
    layout_queues(m);
}

static void mailroom_update(void *state, float dt)
{
    Mailroom *m = state;
    m->time += dt;

    switch (m->phase) {
    case PHASE_PREP:
        m->timer -= dt;
        if (m->timer <= 0.0f) {
            finish_prep(m);
            m->phase = PHASE_WALK;
            m->robot_target_x = ROBOT_OUT_X;
        }
        break;
    case PHASE_WALK:
        m->robot_x = approach(m->robot_x, m->robot_target_x, ROBOT_SPEED * dt);
        if (fabsf(m->robot_x - m->robot_target_x) < 0.001f) {
            perform_action(m);
            m->phase = PHASE_WORK;
            m->timer = WORK_TIME;
        }
        break;
    case PHASE_WORK:
        m->timer -= dt;
        if (m->timer <= 0.0f) m->phase = PHASE_IDLE;
        break;
    default:
        break;
    }

    /* Frame-rate independent smoothing / 프레임률과 무관한 부드러운 이동 */
    float k = 1.0f - expf(-12.0f * dt);
    float target_lift = m->hand_n > 0 ? 1.0f : 0.0f;
    m->arm_lift += (target_lift - m->arm_lift) * k;
    m->popup_t = fmaxf(0.0f, m->popup_t - dt);
    m->bin_bump = fmaxf(0.0f, m->bin_bump - dt * 3.0f);

    for (int i = 0; i < MAX_PARCELS; ++i) {
        Parcel *p = &m->parcels[i];
        p->pulse = fmaxf(0.0f, p->pulse - dt * 1.8f);
        p->glow = fmaxf(0.0f, p->glow - dt * 1.2f);
        switch (p->state) {
        case P_DEAD:
            break;
        case P_HELD: {
            int slot = 0;
            for (int h = 0; h < m->hand_n; ++h)
                if (m->hand[h] == i) slot = h;
            p->target = hold_pos(m, slot);
            p->pos = lerp3(p->pos, p->target, 1.0f - expf(-20.0f * dt));
            if (m->phase == PHASE_PREP && m->prep == PREP_NEW) {
                /* pop in with a little overshoot / 살짝 튀어나오며 커짐 */
                float t = 1.0f - m->timer / STAMP_TIME;
                p->scale = 1.0f + 0.3f * sinf(t * 3.1416f) - (1.0f - t) * 0.9f;
                if (p->scale < 0.05f) p->scale = 0.05f;
            } else if (m->phase == PHASE_PREP && m->prep == PREP_STAMP &&
                       slot == 0) {
                p->scale = 1.0f + 0.08f * sinf(m->time * 40.0f);  /* wobble */
            }
            break;
        }
        case P_MERGING:
            /* slide down into the bottom parcel / 맨 아래 상자로 미끄러짐 */
            p->target = hold_pos(m, 0);
            p->pos = lerp3(p->pos, p->target, 1.0f - expf(-9.0f * dt));
            p->scale = fmaxf(0.3f, p->scale - dt * 0.9f);
            p->glow = 1.0f;
            break;
        case P_RECYCLE: {
            /* arc into the bin and shrink / 통으로 곡선을 그리며 작아짐 */
            p->t += dt / 0.55f;
            float t = fminf(p->t, 1.0f);
            Vector3 top = v3(BIN_POS.x, 0.9f, BIN_POS.z);
            p->pos = lerp3(p->from, top, t);
            p->pos.y += sinf(t * 3.1416f) * 1.6f;
            p->scale = 1.0f - 0.5f * t;
            if (p->t >= 1.0f) {
                p->state = P_DEAD;
                m->bin_bump = 1.0f;
            }
            break;
        }
        case P_INBOX:
        case P_OUTBOX:
            p->pos = lerp3(p->pos, p->target, k);
            break;
        }
    }

    for (int i = 0; i < MAX_SPARKS; ++i) {
        Spark *s = &m->sparks[i];
        if (s->life <= 0) continue;
        s->life -= dt;
        s->v.y -= 9.0f * dt;
        s->p = v3(s->p.x + s->v.x * dt, s->p.y + s->v.y * dt,
                  s->p.z + s->v.z * dt);
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

/* The recycling bin behind the robot / 로봇 뒤의 재활용 통 */
static void draw_bin(const Mailroom *m)
{
    float squash = 1.0f + 0.12f * m->bin_bump;
    Color green = { 70, 150, 110, 255 };
    DrawCylinder(BIN_POS, 0.62f * squash, 0.5f, 0.85f / squash, 16, green);
    DrawCylinderWires(BIN_POS, 0.62f * squash, 0.5f, 0.85f / squash, 16,
                      shade(green, 0.6f));
    DrawCylinder(v3(BIN_POS.x, 0.84f / squash, BIN_POS.z), 0.55f * squash,
                 0.55f * squash, 0.02f, 16, (Color){ 30, 50, 40, 255 });
}

static void draw_parcel(const Parcel *p)
{
    /* elastic pop after a merge or stamp / 합치거나 찍은 뒤 튕기는 효과 */
    float pop = 1.0f + 0.35f * p->pulse * cosf((1.0f - p->pulse) * 14.0f);
    float s = BOX * p->scale * pop;
    Color c = mix(parcel_color(p->value), (Color){ 255, 236, 150, 255 },
                  p->glow * 0.8f);
    DrawCube(p->pos, s, s, s, c);
    DrawCubeWires(p->pos, s, s, s, shade(c, 0.55f));
    /* Packing tape / 포장 테이프 */
    DrawCube(v3(p->pos.x, p->pos.y + s * 0.5f + 0.004f, p->pos.z),
             s * 0.22f, 0.012f, s * 1.01f, (Color){ 235, 215, 170, 255 });
}

static void draw_glows(const Mailroom *m)
{
    /* see-through halos are drawn last / 반투명 빛은 마지막에 그린다 */
    for (int i = 0; i < MAX_PARCELS; ++i) {
        const Parcel *p = &m->parcels[i];
        if (p->state == P_DEAD || p->glow <= 0.01f) continue;
        float s = BOX * p->scale * (1.25f + 0.25f * (1.0f - p->glow));
        DrawCube(p->pos, s, s, s,
                 (Color){ 255, 220, 120, (unsigned char)(110 * p->glow) });
    }
    for (int i = 0; i < MAX_SPARKS; ++i) {
        const Spark *s = &m->sparks[i];
        if (s->life <= 0) continue;
        Color c = s->c;
        c.a = (unsigned char)(255 * fminf(1.0f, s->life * 2.0f));
        DrawCube(s->p, 0.09f, 0.09f, 0.09f, c);
    }
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

    Color eye = (m->phase == PHASE_WORK || m->phase == PHASE_PREP)
                    ? (Color){ 90, 230, 120, 255 }
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
    draw_bin(m);

    for (int i = 0; i < MAX_PARCELS; ++i)
        if (m->parcels[i].state != P_DEAD) draw_parcel(&m->parcels[i]);
    draw_robot(m);
    draw_glows(m);
    EndMode3D();

    Color sign_bg = { 20, 22, 28, 220 };
    draw_label(m->cam, v3(INBOX_X, 0.05f, LANE_Z + 1.2f), "INBOX", 20,
               sign_bg, (Color){ 120, 200, 255, 255 }, w, h);
    draw_label(m->cam, v3(OUTBOX_X, 0.05f, LANE_Z + 1.2f), "OUTBOX", 20,
               sign_bg, (Color){ 130, 230, 140, 255 }, w, h);
    draw_label(m->cam, v3(BIN_POS.x, 1.2f, BIN_POS.z), "RECYCLE", 20, sign_bg,
               (Color){ 140, 220, 170, 255 }, w, h);

    for (int i = 0; i < MAX_PARCELS; ++i) {
        const Parcel *p = &m->parcels[i];
        if (p->state == P_DEAD || p->state == P_MERGING || p->scale < 0.4f)
            continue;
        Vector3 face = v3(p->pos.x, p->pos.y, p->pos.z + BOX * p->scale * 0.5f);
        Color bg = mix((Color){ 250, 245, 230, 235 },
                       (Color){ 255, 220, 90, 235 }, p->glow);
        draw_label(m->cam, face, TextFormat("%d", (int)p->value),
                   p->pulse > 0.3f ? 30 : 20, bg, (Color){ 30, 30, 30, 255 },
                   w, h);
    }

    if (m->popup_t > 0) {
        /* the sum rises above the robot / 합계가 로봇 위로 떠오른다 */
        float rise = (1.4f - m->popup_t) * 0.6f;
        unsigned char a = (unsigned char)(255 * fminf(1.0f, m->popup_t * 2.0f));
        draw_label(m->cam,
                   v3(m->robot_x, HOLD_Y + m->hand_n * BOX + 0.8f + rise, LANE_Z),
                   m->popup, 30, (Color){ 40, 30, 10, (unsigned char)(a * 0.8f) },
                   (Color){ 255, 220, 100, a }, w, h);
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
