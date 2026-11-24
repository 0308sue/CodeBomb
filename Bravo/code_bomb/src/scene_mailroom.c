/*
 * scene_mailroom.c - the Mailroom in 3D, drawn with physically based
 * materials: concrete floor, rubber conveyor belts, cardboard parcels,
 * painted steel machines and a worker robot.
 * scene_mailroom.c - 물리 기반 재질로 그린 3D 우편실: 콘크리트 바닥, 고무
 * 컨베이어 벨트, 골판지 상자, 도장된 강철 기계, 작업 로봇.
 *
 * What the robot does for each call / 호출마다 로봇이 하는 일:
 *   take()        drive to the INBOX, lift the front parcel onto the stack in
 *                 its arms (nothing is thrown away: the number is now in
 *                 your variables).
 *                 입력함으로 가서 맨 앞 상자를 팔의 더미에 올린다.
 *   put(n)        get parcel n ready where it stands (keep a parcel stamped
 *                 n and recycle the rest; merge parcels that add up to n,
 *                 with a pulse and a glow; restamp; or a new parcel pops
 *                 up), then drive to the OUTBOX and set it down.
 *                 그 자리에서 n 상자를 준비한 뒤 (n 상자만 남기고 나머지는
 *                 재활용, 합이 n이면 합치기, 다시 찍기, 새 상자) 출력함에
 *                 내려놓는다.
 *   shred(n)      level 3/11: the same, then into the shredder.
 *                 레벨 3/11: 같은 준비 뒤 파쇄기로.
 *   put_odd(n)    level 5: onto the second (ODD) belt.
 *                 레벨 5: 두 번째 (ODD) 벨트로.
 *   put(n), 8     level 8: a changed number is made in the stamp press.
 *                 레벨 8: 바뀐 숫자는 스탬프 프레스에서 찍는다.
 *   shelf[i] = n  level 10/11: the parcel goes into slot i of the back
 *                 shelf; reading shelf[i] brings a copy back to the robot.
 *                 레벨 10/11: 상자가 뒤 선반 i칸에 들어가고, shelf[i]를
 *                 읽으면 복사본이 로봇에게 돌아온다.
 *
 * The robot works through a short queue of tasks per call (walk, prepare,
 * hand over), and busy() stays true until the queue is empty.
 * 로봇은 호출마다 짧은 작업 대기열 (걷기, 준비, 건네기)을 처리하며, 대기열이
 * 빌 때까지 busy()가 true다.
 *
 * Rendering: pbr.c draws the scene twice (shadow map, then the camera) into
 * a render texture twice the view's size, which draw() scales down
 * (supersampling, for smooth edges). Number stickers and floor stencils are
 * real 3D decals, so anything in front of them hides them.
 * 그리기: pbr.c가 장면을 두 번 (그림자 맵, 카메라) 뷰의 두 배 크기 렌더
 * 텍스처에 그리고, draw()가 이를 줄여 그린다 (매끄러운 가장자리를 위한
 * 슈퍼샘플링). 숫자 스티커와 바닥 글자는 실제 3D 데칼이라 앞의 물체가
 * 가린다.
 */
#include "scene.h"

#include "pbr.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARCELS   64
#define MAX_PARTS     192
#define MAX_TASKS     8
#define TEXT_CACHE    48       /* number textures kept / 보관하는 숫자 텍스처 */
#define HAND_MAX      4        /* parcels the robot can stack / 들 수 있는 상자 */
#define SSAA          2        /* render at 2x, scale down / 2배로 그려 줄임 */

#define LANE_Z        1.6f
#define INBOX_X      -4.2f
#define OUTBOX_X      4.2f
#define ROBOT_IN_X   -2.9f
#define ROBOT_OUT_X   2.9f
#define MACHINE_X     1.6f     /* shredder, press, ODD belt / 파쇄기 등 */
#define MACHINE_Z    (LANE_Z - 2.2f)
#define STAND_X      (MACHINE_X - 1.1f)  /* the robot works beside the machine,
                                            so the camera can see it
                                            로봇은 기계 옆에서 일해 카메라가
                                            기계를 볼 수 있다 */
#define SLOT_GAP      1.15f
#define BELT_TOP      0.42f
#define BELT_LEN      8.0f
#define BOX           0.7f
#define ROBOT_SPEED   5.0f
#define WORK_TIME     0.25f
#define HOLD_Y        2.45f
#define FLY_TIME      0.45f
#define MERGE_TIME    0.6f
#define STAMP_TIME    0.45f
#define SORT_TIME     0.4f
#define PRESS_TIME    1.2f
#define SHELF_Z      -2.25f
#define SHELF_SLOT    0.82f
#define SHELF_TOP     1.05f
#define ODD_FRONT_Z   0.35f
#define BIN_POS      ((Vector3){ -1.6f, 0.0f, LANE_Z - 2.3f })

typedef enum {
    P_DEAD, P_INBOX, P_HELD, P_OUTBOX, P_ODD, P_SHELF, P_MERGING, P_FLY,
    P_PRESSED, P_SHREDDING
} ParcelState;

typedef struct {
    ParcelState state;
    ParcelState land;         /* state after a flight / 비행 후 상태 */
    int32_t     value;
    Vector3     pos, target, from;
    float       t, dur, arc;
    float       scale;
    float       squash;       /* press impact / 프레스 충격 */
    float       pulse;        /* 1 after a merge or stamp, fades to 0
                                 합치거나 찍은 뒤 1, 0으로 줄어듦 */
    float       glow;
    float       yaw;
} Parcel;

typedef enum { PART_SPARK, PART_SHRED, PART_DUST } PartKind;

typedef struct {
    PartKind kind;
    Vector3  p, v;
    float    life, max, size, spin;
    Vector3  color;           /* emissive for sparks / 불꽃의 발광 */
} Particle;

typedef enum { PREP_NONE, PREP_SORT, PREP_MERGE, PREP_STAMP, PREP_NEW } PrepKind;

typedef enum {
    T_WALK, T_PREP, T_TAKE, T_PUT, T_PUT_ODD, T_SHRED, T_SHELF_SET,
    T_SHELF_GET, T_PRESS, T_WAIT
} TaskKind;

typedef struct {
    TaskKind kind;
    float    x;               /* T_WALK target / 목표 */
    int32_t  value;
    int      index;           /* shelf slot / 선반 칸 */
} Task;

/* Everything reset() clears. / reset()이 지우는 모든 것. */
typedef struct {
    bool   shredder, odd_belt, press, shelf;     /* this level's machines
                                                   이 레벨의 기계 */
    Parcel parcels[MAX_PARCELS];
    int    inbox_q[MAX_PARCELS];  /* front first / 맨 앞부터 */
    int    inbox_len;
    int    outbox_q[MAX_PARCELS]; /* newest first / 최신부터 */
    int    outbox_len;
    int    odd_q[MAX_PARCELS];
    int    odd_len;
    int    shelf_slot[LEVEL_SHELF_SIZE];   /* parcel index or -1 */
    int    hand[HAND_MAX];        /* bottom first / 아래부터 */
    int    hand_n;

    Task   tasks[MAX_TASKS];
    int    task_n, task_i;
    bool   task_started;
    float  task_t, task_len;

    float    robot_x, arm_lift;
    PrepKind prep;
    int32_t  prep_value;
    int      press_parcel;
    bool     slammed;
    float    ram;                 /* 0 up, 1 down / 0 위, 1 아래 */
    float    shred_shake, bin_bump, shelf_flash[LEVEL_SHELF_SIZE];

    Particle parts[MAX_PARTS];
    char     popup[48];
    float    popup_t;
    float    time;
} World;

typedef struct {
    int       value;
    bool      used;
    Texture2D tex;
} NumberTex;

typedef struct {
    World     w;
    Camera3D  cam;
    bool      assets;
    PbrTextures mat[MAT_COUNT];
    PbrTextures sign_inbox, sign_outbox, sign_even, sign_odd, sign_shred,
                sign_press, sign_recycle, sign_mailroom;
    PbrTextures shelf_digit[LEVEL_SHELF_SIZE];
    NumberTex numbers[TEXT_CACHE];
    int       numbers_next;
    Texture2D halo;
    RenderTexture2D rt;           /* supersampled view / 슈퍼샘플링 뷰 */
} Mailroom;

/* ---- helpers / 도우미 --------------------------------------------------- */

static Vector3 v3(float x, float y, float z) { return (Vector3){ x, y, z }; }

static float approach(float cur, float target, float step)
{
    if (cur < target) return (cur + step > target) ? target : cur + step;
    return (cur - step < target) ? target : cur - step;
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

static float ease(float t)
{
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

static Vector3 inbox_slot(int k)
{
    return v3(INBOX_X, BELT_TOP + BOX * 0.5f, LANE_Z - k * SLOT_GAP);
}

static Vector3 outbox_slot(int k)
{
    return v3(OUTBOX_X, BELT_TOP + BOX * 0.5f, LANE_Z - k * SLOT_GAP);
}

static Vector3 odd_slot(int k)
{
    return v3(MACHINE_X, BELT_TOP + BOX * 0.5f, ODD_FRONT_Z - k * SLOT_GAP);
}

static float shelf_x(int i)
{
    return (i - (LEVEL_SHELF_SIZE - 1) * 0.5f) * SHELF_SLOT;
}

static Vector3 shelf_pos(int i)
{
    return v3(shelf_x(i), SHELF_TOP + BOX * 0.5f * 0.85f, SHELF_Z);
}

static float robot_x_for_slot(int i)
{
    float x = shelf_x(i);
    return x < ROBOT_IN_X ? ROBOT_IN_X : x > ROBOT_OUT_X ? ROBOT_OUT_X : x;
}

/* k-th parcel of the stack in the robot's arms / 팔에 든 더미의 k번째 상자 */
static Vector3 hold_pos(const World *w, int k)
{
    return v3(w->robot_x, HOLD_Y + k * BOX * 0.95f, LANE_Z);
}

static Vector3 press_bed(void)
{
    return v3(MACHINE_X, 0.55f + BOX * 0.5f, MACHINE_Z);
}

static int spawn(World *w, int32_t value, Vector3 pos, ParcelState st)
{
    for (int i = 0; i < MAX_PARCELS; ++i) {
        if (w->parcels[i].state != P_DEAD) continue;
        Parcel *p = &w->parcels[i];
        memset(p, 0, sizeof *p);
        p->state = st;
        p->value = value;
        p->pos = p->target = pos;
        p->scale = 1.0f;
        /* damaged parcels sit a little crooked / 손상된 상자는 비뚤다 */
        p->yaw = value < 0 ? (frand() - 0.5f) * 0.35f : (frand() - 0.5f) * 0.06f;
        return i;
    }
    return -1;
}

static void layout_queues(World *w)
{
    for (int k = 0; k < w->inbox_len; ++k)
        w->parcels[w->inbox_q[k]].target = inbox_slot(k);
    for (int k = 0; k < w->outbox_len; ++k)
        w->parcels[w->outbox_q[k]].target = outbox_slot(k);
    for (int k = 0; k < w->odd_len; ++k)
        w->parcels[w->odd_q[k]].target = odd_slot(k);
}

static Particle *new_part(World *w)
{
    for (int k = 0; k < MAX_PARTS; ++k)
        if (w->parts[k].life <= 0) return &w->parts[k];
    return NULL;
}

static void sparks_at(World *w, Vector3 at, Vector3 color, int n)
{
    for (; n > 0; n--) {
        Particle *s = new_part(w);
        if (!s) return;
        float a = frand() * 6.2832f;
        s->kind = PART_SPARK;
        s->p = at;
        s->v = v3(cosf(a) * (1.5f + frand() * 2.0f), 1.5f + frand() * 3.0f,
                  sinf(a) * (1.5f + frand() * 2.0f));
        s->life = s->max = 0.5f + frand() * 0.4f;
        s->size = 0.05f + frand() * 0.04f;
        s->color = frand() < 0.4f ? v3(6.0f, 5.6f, 4.4f) : color;
    }
}

static void shreds_at(World *w, Vector3 at, int n)
{
    for (; n > 0; n--) {
        Particle *s = new_part(w);
        if (!s) return;
        s->kind = PART_SHRED;
        s->p = v3(at.x + (frand() - 0.5f) * 0.5f, at.y, at.z);
        s->v = v3((frand() - 0.5f) * 1.2f, 0.4f + frand() * 1.0f,
                  1.2f + frand() * 1.0f);
        s->life = s->max = 0.9f + frand() * 0.6f;
        s->size = 0.08f + frand() * 0.05f;
        s->spin = (frand() - 0.5f) * 14.0f;
    }
}

static void dust_at(World *w, Vector3 at, int n)
{
    for (; n > 0; n--) {
        Particle *s = new_part(w);
        if (!s) return;
        float a = frand() * 6.2832f;
        s->kind = PART_DUST;
        s->p = v3(at.x + cosf(a) * 0.4f, at.y, at.z + sinf(a) * 0.4f);
        s->v = v3(cosf(a) * 0.8f, 0.3f + frand() * 0.4f, sinf(a) * 0.8f);
        s->life = s->max = 0.5f + frand() * 0.3f;
        s->size = 0.05f + frand() * 0.04f;
    }
}

static void set_popup(World *w, const char *text)
{
    snprintf(w->popup, sizeof w->popup, "%s", text);
    w->popup_t = 1.4f;
}

static void fly(Parcel *p, Vector3 to, ParcelState land, float dur, float arc)
{
    p->state = P_FLY;
    p->land = land;
    p->from = p->pos;
    p->target = to;
    p->t = 0;
    p->dur = dur;
    p->arc = arc;
}

/* Send a parcel flying into the recycling bin. / 상자를 재활용 통으로 던진다. */
static void recycle(World *w, int idx)
{
    fly(&w->parcels[idx], v3(BIN_POS.x, 0.9f, BIN_POS.z), P_DEAD, 0.55f, 1.6f);
}

static void hand_remove_top_to(World *w, int k)
{
    memmove(w->hand + k, w->hand + k + 1, sizeof(int) * (size_t)(HAND_MAX - 1 - k));
    w->hand_n--;
}

static bool hand_has(const World *w, int32_t v)
{
    for (int k = 0; k < w->hand_n; ++k)
        if (w->parcels[w->hand[k]].value == v) return true;
    return false;
}

/* ---- tasks / 작업 ------------------------------------------------------- */

static void push(World *w, TaskKind kind, float x, int32_t value, int index)
{
    if (w->task_n >= MAX_TASKS) return;
    w->tasks[w->task_n++] = (Task){ kind, x, value, index };
}

/* Get parcel v into hand[0] (see the file comment). Returns how long the
 * animation takes. / 상자 v를 hand[0]에 준비한다. 애니메이션 시간을 돌려준다. */
static float plan_prep(World *w, int32_t v)
{
    w->prep_value = v;
    int match = -1;
    for (int k = w->hand_n - 1; k >= 0 && match < 0; --k)
        if (w->parcels[w->hand[k]].value == v) match = k;

    if (match >= 0) {
        int keep = w->hand[match];
        for (int k = 0; k < w->hand_n; ++k)
            if (k != match) recycle(w, w->hand[k]);
        w->prep = w->hand_n > 1 ? PREP_SORT : PREP_NONE;
        w->hand[0] = keep;
        w->hand_n = 1;
        return w->prep == PREP_SORT ? SORT_TIME : 0.0f;
    }
    if (w->hand_n >= 2) {
        int32_t sum = 0;
        char text[48] = "";
        for (int k = 0; k < w->hand_n; ++k) {
            int32_t val = w->parcels[w->hand[k]].value;
            sum += val;
            size_t used = strlen(text);
            snprintf(text + used, sizeof text - used, "%s%d", k ? " + " : "",
                     (int)val);
            if (k > 0) w->parcels[w->hand[k]].state = P_MERGING;
        }
        if (sum == v) {
            size_t used = strlen(text);
            snprintf(text + used, sizeof text - used, " = %d", (int)v);
        } else {
            snprintf(text, sizeof text, "%d boxes -> %d", w->hand_n, (int)v);
        }
        set_popup(w, text);
        w->prep = PREP_MERGE;
        return MERGE_TIME;
    }
    if (w->hand_n == 1) {
        char text[48];
        snprintf(text, sizeof text, "%d -> %d",
                 (int)w->parcels[w->hand[0]].value, (int)v);
        set_popup(w, text);
        w->prep = PREP_STAMP;
        return STAMP_TIME;
    }
    int idx = spawn(w, v, hold_pos(w, 0), P_HELD);
    if (idx < 0) {
        w->prep = PREP_NONE;
        return 0.0f;
    }
    w->parcels[idx].scale = 0.05f;
    w->parcels[idx].glow = 1.0f;
    w->hand[0] = idx;
    w->hand_n = 1;
    char text[48];
    snprintf(text, sizeof text, "new %d", (int)v);
    set_popup(w, text);
    w->prep = PREP_NEW;
    return STAMP_TIME;
}

static void finish_prep(World *w)
{
    if (w->hand_n == 0) return;
    Parcel *keeper = &w->parcels[w->hand[0]];
    if (w->prep == PREP_MERGE) {
        for (int k = 1; k < w->hand_n; ++k)
            w->parcels[w->hand[k]].state = P_DEAD;
        w->hand_n = 1;
    }
    if (w->prep == PREP_MERGE || w->prep == PREP_STAMP || w->prep == PREP_NEW) {
        keeper->value = w->prep_value;
        keeper->pulse = 1.0f;
        keeper->glow = 1.0f;
        keeper->scale = 1.0f;
        sparks_at(w, keeper->pos, v3(5.0f, 3.4f, 0.8f),
                  w->prep == PREP_MERGE ? 36 : 20);
    }
    w->prep = PREP_NONE;
}

/* Take the parcel in hand[0] out of the arms and send it to `to`.
 * hand[0]의 상자를 팔에서 꺼내 `to`로 보낸다. */
static int hand_off(World *w, Vector3 to, ParcelState land, float arc)
{
    if (w->hand_n == 0) return -1;
    int idx = w->hand[0];
    hand_remove_top_to(w, 0);
    fly(&w->parcels[idx], to, land, FLY_TIME, arc);
    return idx;
}

static void start_task(World *w, Task *t)
{
    w->task_t = 0;
    w->task_len = 0;
    switch (t->kind) {
    case T_WALK:
        break;
    case T_PREP:
        w->task_len = plan_prep(w, t->value);
        break;
    case T_TAKE:
        if (w->hand_n == HAND_MAX) {       /* arms full: oldest to the bin
                                              팔이 가득: 가장 오래된 것을 통에 */
            recycle(w, w->hand[0]);
            hand_remove_top_to(w, 0);
        }
        if (w->inbox_len > 0) {
            int idx = w->inbox_q[0];
            memmove(w->inbox_q, w->inbox_q + 1,
                    sizeof(int) * (size_t)(w->inbox_len - 1));
            w->inbox_len--;
            w->parcels[idx].state = P_HELD;
            w->hand[w->hand_n++] = idx;
        }
        w->task_len = WORK_TIME;
        break;
    case T_PUT: {
        int idx = hand_off(w, outbox_slot(0), P_OUTBOX, 0.5f);
        if (idx >= 0 && w->outbox_len < MAX_PARCELS) {
            memmove(w->outbox_q + 1, w->outbox_q,
                    sizeof(int) * (size_t)w->outbox_len);
            w->outbox_q[0] = idx;
            w->outbox_len++;
            layout_queues(w);
        }
        if (w->odd_belt) {
            char text[48];
            snprintf(text, sizeof text, "%d %% 2 = 0: EVEN", (int)t->value);
            set_popup(w, text);
        }
        w->task_len = FLY_TIME;
        break;
    }
    case T_PUT_ODD: {
        int idx = hand_off(w, odd_slot(0), P_ODD, 0.9f);
        if (idx >= 0 && w->odd_len < MAX_PARCELS) {
            memmove(w->odd_q + 1, w->odd_q, sizeof(int) * (size_t)w->odd_len);
            w->odd_q[0] = idx;
            w->odd_len++;
            layout_queues(w);
        }
        char text[48];
        snprintf(text, sizeof text, "%d %% 2 = 1: ODD", (int)t->value);
        set_popup(w, text);
        w->task_len = FLY_TIME;
        break;
    }
    case T_SHRED: {
        hand_off(w, v3(MACHINE_X, 1.25f, MACHINE_Z), P_SHREDDING, 1.0f);
        char text[48];
        snprintf(text, sizeof text, "%d < 0: shred", (int)t->value);
        set_popup(w, text);
        w->task_len = FLY_TIME + 0.45f;
        break;
    }
    case T_SHELF_SET: {
        int i = t->index;
        if (w->shelf_slot[i] >= 0) {       /* the old parcel goes / 이전 상자 */
            recycle(w, w->shelf_slot[i]);
            w->shelf_slot[i] = -1;
        }
        int idx = hand_off(w, shelf_pos(i), P_SHELF, 1.2f);
        w->shelf_slot[i] = idx;
        w->shelf_flash[i] = 1.0f;
        char text[48];
        snprintf(text, sizeof text, "shelf[%d] = %d", i, (int)t->value);
        set_popup(w, text);
        w->task_len = FLY_TIME + 0.05f;
        break;
    }
    case T_SHELF_GET: {
        int i = t->index;
        if (w->hand_n == HAND_MAX) {
            recycle(w, w->hand[0]);
            hand_remove_top_to(w, 0);
        }
        Vector3 from = shelf_pos(i);
        int idx = spawn(w, t->value, from, P_HELD);
        if (idx >= 0) {
            w->parcels[idx].glow = 0.8f;
            w->hand[w->hand_n++] = idx;
            fly(&w->parcels[idx], hold_pos(w, w->hand_n - 1), P_HELD, FLY_TIME,
                1.2f);
        }
        w->shelf_flash[i] = 1.0f;
        char text[48];
        snprintf(text, sizeof text, "shelf[%d] is %d", i, (int)t->value);
        set_popup(w, text);
        w->task_len = FLY_TIME + 0.05f;
        break;
    }
    case T_PRESS: {
        /* one parcel goes under the ram; extras are recycled
           상자 하나가 램 아래로 가고 나머지는 재활용 */
        while (w->hand_n > 1) {
            recycle(w, w->hand[w->hand_n - 1]);
            w->hand_n--;
        }
        if (w->hand_n == 0) {
            int idx = spawn(w, 0, hold_pos(w, 0), P_HELD);
            if (idx >= 0) {
                w->hand[0] = idx;
                w->hand_n = 1;
            }
        }
        w->press_parcel = w->hand_n ? w->hand[0] : -1;
        w->slammed = false;
        if (w->press_parcel >= 0) {
            hand_remove_top_to(w, 0);
            char text[48];
            snprintf(text, sizeof text, "%d -> press -> %d",
                     (int)w->parcels[w->press_parcel].value, (int)t->value);
            set_popup(w, text);
            fly(&w->parcels[w->press_parcel], press_bed(), P_PRESSED, 0.3f,
                0.8f);
        }
        w->task_len = PRESS_TIME;
        break;
    }
    case T_WAIT:
        w->task_len = WORK_TIME;
        break;
    }
}

/* Returns true when the task is finished. / 작업이 끝나면 true. */
static bool run_task(World *w, Task *t, float dt)
{
    if (!w->task_started) {
        start_task(w, t);
        w->task_started = true;
    }
    w->task_t += dt;
    switch (t->kind) {
    case T_WALK:
        w->robot_x = approach(w->robot_x, t->x, ROBOT_SPEED * dt);
        return fabsf(w->robot_x - t->x) < 0.001f;
    case T_PREP:
        if (w->task_t >= w->task_len) {
            finish_prep(w);
            return true;
        }
        return false;
    case T_PRESS: {
        float u = w->task_t / PRESS_TIME;
        /* ram: down 0.3-0.45, hold, up 0.55-0.75 / 램: 내려감, 멈춤, 올라감 */
        if (u < 0.3f) w->ram = 0;
        else if (u < 0.42f) w->ram = ease((u - 0.3f) / 0.12f);
        else if (u < 0.55f) w->ram = 1;
        else w->ram = 1.0f - ease((u - 0.55f) / 0.2f);
        Parcel *p = w->press_parcel >= 0 ? &w->parcels[w->press_parcel] : NULL;
        if (p && !w->slammed && u >= 0.42f) {          /* the slam / 쾅 */
            w->slammed = true;
            p->value = t->value;
            p->squash = 1.0f;
            p->pulse = 1.0f;
            p->glow = 1.0f;
            sparks_at(w, v3(p->pos.x, p->pos.y + 0.3f, p->pos.z + 0.3f),
                      v3(5.0f, 3.0f, 0.6f), 30);
            dust_at(w, v3(MACHINE_X, 0.6f, MACHINE_Z), 10);
        }
        if (p && u >= 0.78f && p->state == P_PRESSED) {
            if (w->hand_n < HAND_MAX) {
                w->hand[w->hand_n++] = w->press_parcel;
                fly(p, hold_pos(w, w->hand_n - 1), P_HELD, 0.25f, 0.6f);
            }
        }
        if (u >= 1.0f) {
            w->ram = 0;
            return true;
        }
        return false;
    }
    default:
        return w->task_t >= w->task_len;
    }
}

/* ---- scene callbacks / 장면 콜백 ---------------------------------------- */

static void mailroom_reset(void *state, const LevelRun *run)
{
    Mailroom *m = state;
    World *w = &m->w;
    memset(w, 0, sizeof *w);
    const LevelDef *d = run->def;
    w->shelf = d->has_shelf;
    w->shredder = d->side_name && strcmp(d->side_name, "shredder") == 0;
    w->odd_belt = d->side_name && !w->shredder;
    w->press = d->needs_helper;
    w->robot_x = 0.0f;
    w->press_parcel = -1;
    for (int i = 0; i < LEVEL_SHELF_SIZE; ++i) w->shelf_slot[i] = -1;

    for (int i = run->inbox_pos; i < run->inbox_len; ++i) {
        int idx = spawn(w, run->inbox[i], inbox_slot(w->inbox_len), P_INBOX);
        if (idx >= 0) w->inbox_q[w->inbox_len++] = idx;
    }
    /* parcels already delivered (screenshots of a run in progress)
       이미 배달된 상자 (진행 중인 실행의 스크린샷) */
    for (int i = run->outbox_len - 1; i >= 0; --i) {
        int idx = spawn(w, run->outbox[i], outbox_slot(w->outbox_len), P_OUTBOX);
        if (idx >= 0) w->outbox_q[w->outbox_len++] = idx;
    }
    if (w->odd_belt)
        for (int i = run->side_len - 1; i >= 0; --i) {
            int idx = spawn(w, run->side[i], odd_slot(w->odd_len), P_ODD);
            if (idx >= 0) w->odd_q[w->odd_len++] = idx;
        }
    if (w->shelf)
        for (int i = 0; i < LEVEL_SHELF_SIZE; ++i)
            if (run->shelf_used[i]) {
                int idx = spawn(w, run->shelf[i], shelf_pos(i), P_SHELF);
                w->shelf_slot[i] = idx;
                if (idx >= 0) w->parcels[idx].scale = 0.85f;
            }
}

static void mailroom_on_event(void *state, LevelEvent ev)
{
    World *w = &((Mailroom *)state)->w;
    w->task_n = w->task_i = 0;
    w->task_started = false;
    int32_t v = ev.value;
    switch (ev.type) {
    case LEVEL_EVENT_TAKE:
        push(w, T_WALK, ROBOT_IN_X, 0, 0);
        push(w, T_TAKE, 0, 0, 0);
        break;
    case LEVEL_EVENT_PUT:
        if (w->press && !hand_has(w, v)) {
            push(w, T_WALK, STAND_X, 0, 0);
            push(w, T_PRESS, 0, v, 0);
        } else {
            push(w, T_PREP, 0, v, 0);
        }
        push(w, T_WALK, ROBOT_OUT_X, 0, 0);
        push(w, T_PUT, 0, v, 0);
        break;
    case LEVEL_EVENT_PUT_ODD:
        push(w, T_PREP, 0, v, 0);
        push(w, T_WALK, STAND_X, 0, 0);
        push(w, T_PUT_ODD, 0, v, 0);
        break;
    case LEVEL_EVENT_SHRED:
        push(w, T_PREP, 0, v, 0);
        push(w, T_WALK, STAND_X, 0, 0);
        push(w, T_SHRED, 0, v, 0);
        break;
    case LEVEL_EVENT_SHELF_SET:
        push(w, T_PREP, 0, v, 0);
        push(w, T_WALK, robot_x_for_slot(ev.index), 0, 0);
        push(w, T_SHELF_SET, 0, v, ev.index);
        break;
    case LEVEL_EVENT_SHELF_GET:
        push(w, T_WALK, robot_x_for_slot(ev.index), 0, 0);
        push(w, T_SHELF_GET, 0, v, ev.index);
        break;
    default:
        return;
    }
    push(w, T_WAIT, 0, 0, 0);
}

static void update_parcel(World *w, int i, float dt, float k)
{
    Parcel *p = &w->parcels[i];
    p->pulse = fmaxf(0.0f, p->pulse - dt * 1.8f);
    p->glow = fmaxf(0.0f, p->glow - dt * 1.2f);
    p->squash = fmaxf(0.0f, p->squash - dt * 2.5f);
    const Task *task = w->task_i < w->task_n ? &w->tasks[w->task_i] : NULL;
    bool prepping = task && task->kind == T_PREP && w->task_started;
    switch (p->state) {
    case P_DEAD:
        break;
    case P_HELD: {
        int slot = 0;
        for (int h = 0; h < w->hand_n; ++h)
            if (w->hand[h] == i) slot = h;
        p->target = hold_pos(w, slot);
        p->pos = Vector3Lerp(p->pos, p->target, 1.0f - expf(-20.0f * dt));
        if (prepping && w->prep == PREP_NEW) {
            float t = w->task_len > 0 ? w->task_t / w->task_len : 1.0f;
            p->scale = 1.0f + 0.3f * sinf(t * PI) - (1.0f - t) * 0.9f;
            if (p->scale < 0.05f) p->scale = 0.05f;
        } else if (prepping && w->prep == PREP_STAMP && slot == 0) {
            p->scale = 1.0f + 0.08f * sinf(w->time * 40.0f);       /* wobble */
        } else {
            p->scale = approach(p->scale, 1.0f, dt * 4.0f);
        }
        break;
    }
    case P_MERGING:
        p->target = hold_pos(w, 0);
        p->pos = Vector3Lerp(p->pos, p->target, 1.0f - expf(-9.0f * dt));
        p->scale = fmaxf(0.3f, p->scale - dt * 0.9f);
        p->glow = 1.0f;
        break;
    case P_FLY: {
        p->t += dt / p->dur;
        float t = fminf(p->t, 1.0f);
        Vector3 to = p->target;
        if (p->land == P_HELD) {           /* chase the moving hands
                                              움직이는 손을 따라감 */
            int slot = 0;
            for (int h = 0; h < w->hand_n; ++h)
                if (w->hand[h] == i) slot = h;
            to = hold_pos(w, slot);
        }
        p->pos = Vector3Lerp(p->from, to, ease(t));
        p->pos.y += sinf(t * PI) * p->arc;
        if (p->land == P_DEAD) p->scale = 1.0f - 0.5f * t;
        if (p->t >= 1.0f) {
            p->pos = to;
            p->state = p->land;
            if (p->land == P_DEAD) {
                w->bin_bump = 1.0f;
            } else if (p->land == P_SHREDDING) {
                p->t = 0;
                w->shred_shake = 1.0f;
            } else if (p->land == P_OUTBOX || p->land == P_ODD) {
                dust_at(w, v3(p->pos.x, BELT_TOP + 0.02f, p->pos.z), 6);
            } else if (p->land == P_SHELF) {
                p->scale = 0.85f;
            }
        }
        break;
    }
    case P_SHREDDING:
        /* sinks into the cutters; strips fly out of the front chute
           칼날 속으로 가라앉고, 앞 배출구로 종이 조각이 날아나온다 */
        p->t += dt / 0.45f;
        p->pos.y = 1.25f - p->t * 0.9f;
        p->scale = fmaxf(0.1f, 1.0f - p->t * 0.6f);
        if (frand() < 0.6f)
            shreds_at(w, v3(MACHINE_X, 0.45f, MACHINE_Z + 0.5f), 2);
        if (p->t >= 1.0f) p->state = P_DEAD;
        break;
    case P_PRESSED:
        p->pos = Vector3Lerp(p->pos, press_bed(), 1.0f - expf(-20.0f * dt));
        break;
    case P_SHELF:
    case P_INBOX:
    case P_OUTBOX:
    case P_ODD:
        p->pos = Vector3Lerp(p->pos, p->target, k);
        break;
    }
}

static void mailroom_update(void *state, float dt)
{
    World *w = &((Mailroom *)state)->w;
    w->time += dt;

    float tdt = dt;
    while (w->task_i < w->task_n) {
        if (!run_task(w, &w->tasks[w->task_i], tdt)) break;
        w->task_i++;
        w->task_started = false;
        tdt = 0.0f;              /* the next task starts with no time used
                                    다음 작업은 시간을 쓰지 않고 시작한다 */
    }

    float k = 1.0f - expf(-12.0f * dt);
    float target_lift = w->hand_n > 0 ? 1.0f : 0.0f;
    w->arm_lift += (target_lift - w->arm_lift) * k;
    w->popup_t = fmaxf(0.0f, w->popup_t - dt);
    w->bin_bump = fmaxf(0.0f, w->bin_bump - dt * 3.0f);
    w->shred_shake = fmaxf(0.0f, w->shred_shake - dt * 1.6f);
    for (int i = 0; i < LEVEL_SHELF_SIZE; ++i)
        w->shelf_flash[i] = fmaxf(0.0f, w->shelf_flash[i] - dt * 1.5f);

    for (int i = 0; i < MAX_PARCELS; ++i) update_parcel(w, i, dt, k);

    for (int i = 0; i < MAX_PARTS; ++i) {
        Particle *s = &w->parts[i];
        if (s->life <= 0) continue;
        s->life -= dt;
        float g = s->kind == PART_SPARK ? 9.0f : s->kind == PART_SHRED ? 3.0f
                                                                        : -0.5f;
        s->v.y -= g * dt;
        if (s->kind != PART_SPARK) {
            s->v.x *= 1.0f - 1.5f * dt;
            s->v.z *= 1.0f - 1.5f * dt;
        }
        s->p = Vector3Add(s->p, Vector3Scale(s->v, dt));
        if (s->p.y < 0.01f) {
            s->p.y = 0.01f;
            s->v = v3(0, 0, 0);
        }
    }
}

static bool mailroom_busy(const void *state)
{
    const World *w = &((const Mailroom *)state)->w;
    return w->task_i < w->task_n;
}

/* ---- assets / 자원 ------------------------------------------------------ */

/* White stencil letters on a transparent image (tinted when drawn).
 * 투명한 이미지 위의 흰 글자 (그릴 때 색을 입힌다). */
static Image stencil(const char *text, int size, int pad, int bold)
{
    int tw = MeasureText(text, size);
    Image img = GenImageColor(tw + pad * 2 + bold, size + pad * 2 + bold, BLANK);
    for (int dy = 0; dy <= bold; dy++)        /* bold: thicker strokes
                                                 굵게: 획을 두껍게 */
        for (int dx = 0; dx <= bold; dx++)
            ImageDrawText(&img, text, pad + dx, pad + dy, size, WHITE);
    return img;
}

static PbrTextures make_sign(Mailroom *m, const char *text, int size,
                             PbrMaterialId paint, int bold)
{
    Image img = stencil(text, size, size / 5, bold);
    PbrTextures t = pbr_decal_textures(img, &m->mat[paint]);
    UnloadImage(img);
    return t;
}

/* Number texture: dark ink, transparent paper (the label is drawn below).
 * 숫자 텍스처: 어두운 잉크, 투명한 배경 (아래에 라벨을 그린다). */
static Texture2D number_tex(Mailroom *m, int value)
{
    for (int i = 0; i < TEXT_CACHE; ++i)
        if (m->numbers[i].used && m->numbers[i].value == value)
            return m->numbers[i].tex;
    NumberTex *n = &m->numbers[m->numbers_next];
    m->numbers_next = (m->numbers_next + 1) % TEXT_CACHE;
    if (n->used) UnloadTexture(n->tex);
    Image img = stencil(TextFormat("%d", value), 60, 4, 2);
    n->tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&n->tex);
    SetTextureFilter(n->tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(n->tex, TEXTURE_WRAP_CLAMP);
    n->value = value;
    n->used = true;
    return n->tex;
}

static void load_assets(Mailroom *m)
{
    bool pbr = pbr_init(2048);
    pbr_textures_make(m->mat, pbr ? 512 : 128);
    m->sign_inbox = make_sign(m, "INBOX", 60, MAT_PAINT_YELLOW, 4);
    m->sign_outbox = make_sign(m, "OUTBOX", 60, MAT_PAINT_YELLOW, 4);
    m->sign_even = make_sign(m, "EVEN", 60, MAT_PAINT_YELLOW, 4);
    m->sign_odd = make_sign(m, "ODD", 60, MAT_PAINT_YELLOW, 4);
    m->sign_shred = make_sign(m, "SHRED", 40, MAT_PAINT_YELLOW, 2);
    m->sign_press = make_sign(m, "PRESS", 40, MAT_PAINT_YELLOW, 2);
    m->sign_recycle = make_sign(m, "RECYCLE", 40, MAT_PLASTIC, 2);
    m->sign_mailroom = make_sign(m, "MAILROOM 7", 60, MAT_PAINT_YELLOW, 2);
    for (int i = 0; i < LEVEL_SHELF_SIZE; ++i)
        m->shelf_digit[i] = make_sign(m, TextFormat("%d", i), 40,
                                      MAT_PAINT_YELLOW, 2);
    /* soft round halo for glows / 발광용 부드러운 원형 빛 */
    Image halo = GenImageGradientRadial(128, 128, 0.0f,
                                        (Color){ 255, 255, 255, 255 },
                                        (Color){ 255, 255, 255, 0 });
    m->halo = LoadTextureFromImage(halo);
    UnloadImage(halo);
    SetTextureFilter(m->halo, TEXTURE_FILTER_BILINEAR);
    m->assets = true;
}

static void free_assets(Mailroom *m)
{
    if (!m->assets) return;
    PbrTextures *signs[] = { &m->sign_inbox, &m->sign_outbox, &m->sign_even,
                             &m->sign_odd, &m->sign_shred, &m->sign_press,
                             &m->sign_recycle, &m->sign_mailroom };
    for (size_t i = 0; i < sizeof signs / sizeof signs[0]; ++i)
        pbr_decal_free(signs[i]);
    for (int i = 0; i < LEVEL_SHELF_SIZE; ++i) pbr_decal_free(&m->shelf_digit[i]);
    for (int i = 0; i < TEXT_CACHE; ++i)
        if (m->numbers[i].used) UnloadTexture(m->numbers[i].tex);
    UnloadTexture(m->halo);
    pbr_textures_free(m->mat);
    if (m->rt.id) UnloadRenderTexture(m->rt);
    pbr_shutdown();
    m->assets = false;
}

/* ---- drawing helpers / 그리기 도우미 ------------------------------------ */

static PbrDraw mat(const Mailroom *m, PbrMaterialId id)
{
    PbrDraw d = { 0 };
    d.tex = &m->mat[id];
    d.tint = WHITE;
    return d;
}

static PbrDraw decal(const PbrTextures *t, Color tint)
{
    PbrDraw d = { 0 };
    d.tex = t;
    d.tint = tint;
    d.no_shadow = true;
    return d;
}

/* Cylinder lying along x. / x축으로 누운 원기둥. */
static void cyl_x(const PbrDraw *d, Vector3 c, float r, float len)
{
    Matrix t = MatrixScale(r, len, r);
    t = MatrixMultiply(t, MatrixTranslate(0, -len * 0.5f, 0));
    t = MatrixMultiply(t, MatrixRotateZ(PI / 2));
    t = MatrixMultiply(t, MatrixTranslate(c.x, c.y, c.z));
    pbr_draw(&pbr_meshes()->cylinder, d, t);
}

/* Quad facing +z (yaw turns it), w x h. / +z를 향한 사각형. */
static void quad(const PbrDraw *d, Vector3 c, float w, float h, float yaw)
{
    Matrix t = MatrixScale(w, h, 1);
    if (yaw != 0) t = MatrixMultiply(t, MatrixRotateY(yaw));
    t = MatrixMultiply(t, MatrixTranslate(c.x, c.y, c.z));
    pbr_draw(&pbr_meshes()->quad, d, t);
}

/* Flat on the floor, text reading towards the camera. / 바닥에 눕힌 글자. */
static void floor_decal(const PbrTextures *t, Vector3 c, float w, Color tint)
{
    float h = w * (float)t->albedo.height / (float)t->albedo.width;
    PbrDraw d = decal(t, tint);
    Matrix tr = MatrixScale(w, 1, h);
    tr = MatrixMultiply(tr, MatrixTranslate(c.x, c.y, c.z));
    pbr_draw(&pbr_meshes()->floor_quad, &d, tr);
}

static void sign_on_wall(const PbrTextures *t, Vector3 c, float w, Color tint)
{
    float h = w * (float)t->albedo.height / (float)t->albedo.width;
    PbrDraw d = decal(t, tint);
    quad(&d, c, w, h, 0);
}

/* ---- the factory / 공장 ------------------------------------------------- */

static void draw_room(const Mailroom *m)
{
    PbrDraw floor = mat(m, MAT_CONCRETE);
    floor.uv_scale = (Vector2){ 6, 4.8f };
    pbr_draw(&pbr_meshes()->floor_quad, &floor,
             MatrixMultiply(MatrixScale(30, 1, 24), MatrixTranslate(0, 0, -2)));

    /* back wall: corrugated sheet over a painted kick band
       뒷벽: 도장된 아랫단 위의 골함석 */
    PbrDraw wall = mat(m, MAT_WALL);
    wall.uv_scale = (Vector2){ 8, 1 };
    pbr_box(&wall, v3(0, 3.6f, -7.2f), v3(30, 6.0f, 0.2f), 0);
    PbrDraw band = mat(m, MAT_HAZARD);
    band.uv_scale = (Vector2){ 30, 0.6f };
    pbr_box(&band, v3(0, 0.3f, -7.05f), v3(30, 0.6f, 0.12f), 0);
    PbrDraw beam = mat(m, MAT_PAINT_BLUE);
    for (int i = -3; i <= 2; i++)
        pbr_box(&beam, v3(i * 4.0f + 2.0f, 3.3f, -7.0f), v3(0.3f, 6.6f, 0.3f), 0);
    /* the department sign: a dark plate with painted letters
       부서 표지판: 어두운 판에 칠한 글자 */
    PbrDraw plate = mat(m, MAT_DARK_STEEL);
    pbr_bevel_box(&plate, v3(0, 3.5f, -7.05f), v3(3.6f, 0.75f, 0.06f), 0);
    sign_on_wall(&m->sign_mailroom, v3(0, 3.5f, -7.015f), 3.3f,
                 (Color){ 255, 214, 90, 255 });

    /* yellow safety lines round the robot's lane / 로봇 통로의 노란 안전선 */
    PbrDraw paint = mat(m, MAT_PAINT_YELLOW);
    paint.no_shadow = true;
    paint.uv_scale = (Vector2){ 6, 0.1f };
    float zf = LANE_Z + 0.85f, zb = LANE_Z - 0.85f;
    pbr_box(&paint, v3(0, 0.004f, zf), v3(6.6f, 0.008f, 0.08f), 0);
    pbr_box(&paint, v3(0, 0.004f, zb), v3(6.6f, 0.008f, 0.08f), 0);

    /* pallets of stock in the corners / 구석의 재고 팔레트 */
    PbrDraw wood = mat(m, MAT_WOOD);
    PbrDraw card = mat(m, MAT_CARDBOARD);
    for (int side = -1; side <= 1; side += 2) {
        float px = side * 7.2f, pz = -5.6f;
        for (int k = -1; k <= 1; k++)
            pbr_box(&wood, v3(px, 0.07f, pz + k * 0.5f), v3(1.3f, 0.14f, 0.16f), 0);
        pbr_box(&wood, v3(px, 0.16f, pz), v3(1.3f, 0.04f, 1.2f), 0);
        for (int l = 0; l < 3; l++)
            for (int c = 0; c < 2; c++) {
                if (l == 2 && c == (side > 0)) continue;
                card.tint = (Color){ (unsigned char)(235 - l * 10), 225, 215, 255 };
                pbr_bevel_box(&card, v3(px - 0.3f + c * 0.6f, 0.47f + l * 0.6f, pz),
                              v3(0.58f, 0.58f, 1.0f), (l + c) * 0.03f);
            }
    }
    /* fire extinguisher by the wall / 벽 옆 소화기 */
    PbrDraw red = mat(m, MAT_PAINT_RED);
    pbr_cylinder(&red, v3(-5.4f, 0, -6.7f), 0.16f, 0.62f);
    PbrDraw black = mat(m, MAT_RUBBER);
    pbr_cylinder(&black, v3(-5.4f, 0.62f, -6.7f), 0.06f, 0.12f);
}

/* A conveyor belt running along z, front end at z_front.
 * z축으로 놓인 컨베이어 벨트, 앞 끝은 z_front. */
static void draw_conveyor(const Mailroom *m, float x, float z_front, float len,
                          float speed)
{
    float zc = z_front - len * 0.5f;
    PbrDraw steel = mat(m, MAT_DARK_STEEL);
    PbrDraw bright = mat(m, MAT_STEEL);
    PbrDraw yellow = mat(m, MAT_PAINT_YELLOW);
    PbrDraw belt = mat(m, MAT_RUBBER_BELT);

    for (int side = -1; side <= 1; side += 2)       /* side beams / 옆 보 */
        pbr_box(&steel, v3(x + side * 0.56f, 0.3f, zc), v3(0.08f, 0.2f, len), 0);
    for (float z = z_front - 0.4f; z > z_front - len; z -= 1.8f)
        for (int side = -1; side <= 1; side += 2)   /* legs / 다리 */
            pbr_box(&bright, v3(x + side * 0.5f, 0.12f, z), v3(0.07f, 0.24f, 0.07f), 0);
    cyl_x(&bright, v3(x, BELT_TOP - 0.06f, z_front - 0.06f), 0.07f, 1.08f);
    cyl_x(&bright, v3(x, BELT_TOP - 0.06f, z_front - len + 0.06f), 0.07f, 1.08f);
    /* the belt scrolls (texture offset) / 벨트가 움직인다 (텍스처 이동) */
    belt.uv_scale = (Vector2){ 1, len };
    belt.uv_offset = (Vector2){ 0, -fmodf(m->w.time * speed, 1.0f) };
    pbr_box(&belt, v3(x, BELT_TOP - 0.02f, zc), v3(1.0f, 0.04f, len - 0.06f), 0);
    for (int side = -1; side <= 1; side += 2)       /* guide rails / 안내 레일 */
        pbr_bevel_box(&yellow, v3(x + side * 0.6f, BELT_TOP + 0.08f, zc),
                      v3(0.05f, 0.1f, len), 0);
}

static void draw_bin(const Mailroom *m)
{
    float squash = 1.0f + 0.12f * m->w.bin_bump;
    PbrDraw green = mat(m, MAT_PLASTIC);
    green.tint = (Color){ 70, 160, 110, 255 };
    float r = 0.55f * squash, h = 0.85f / squash;
    pbr_cylinder(&green, BIN_POS, r, h);
    PbrDraw rim = mat(m, MAT_PLASTIC);
    rim.tint = (Color){ 40, 90, 60, 255 };
    pbr_cylinder(&rim, v3(BIN_POS.x, h - 0.02f, BIN_POS.z), r + 0.04f, 0.07f);
    PbrDraw hole = mat(m, MAT_RUBBER);
    pbr_cylinder(&hole, v3(BIN_POS.x, h - 0.01f, BIN_POS.z), r - 0.05f, 0.03f);
    float w = 0.95f;
    float sh = w * (float)m->sign_recycle.albedo.height / (float)m->sign_recycle.albedo.width;
    PbrDraw sign = decal(&m->sign_recycle, (Color){ 240, 250, 240, 255 });
    quad(&sign, v3(BIN_POS.x, h * 0.6f, BIN_POS.z + r + 0.02f), w, sh, 0);
}

static void draw_shredder(const Mailroom *m)
{
    const World *w = &m->w;
    float shake = w->shred_shake * sinf(w->time * 70.0f) * 0.025f;
    Vector3 c = v3(MACHINE_X + shake, 0, MACHINE_Z);
    PbrDraw body = mat(m, MAT_DARK_STEEL);
    PbrDraw stripe = mat(m, MAT_HAZARD);
    PbrDraw steel = mat(m, MAT_STEEL);
    PbrDraw dark = mat(m, MAT_RUBBER);
    pbr_bevel_box(&body, v3(c.x, 0.5f, c.z), v3(1.1f, 1.0f, 0.9f), 0);
    stripe.uv_scale = (Vector2){ 2, 0.3f };
    pbr_box(&stripe, v3(c.x, 1.04f, c.z), v3(1.16f, 0.1f, 0.96f), 0);
    pbr_box(&dark, v3(c.x, 1.1f, c.z), v3(0.8f, 0.02f, 0.25f), 0);   /* slot */
    /* front chute and tray of shreds / 앞 배출구와 조각 받침 */
    pbr_box(&steel, v3(c.x, 0.32f, c.z + 0.52f), v3(0.7f, 0.06f, 0.2f), 0);
    pbr_box(&steel, v3(c.x, 0.06f, c.z + 0.75f), v3(0.8f, 0.12f, 0.4f), 0);
    PbrDraw lamp = mat(m, MAT_PLASTIC);
    lamp.tint = (Color){ 255, 120, 80, 255 };
    float on = w->shred_shake > 0 ? 0.5f + 0.5f * sinf(w->time * 30.0f) : 0.15f;
    lamp.emissive = v3(4.0f * on, 1.2f * on, 0.3f * on);
    pbr_cylinder(&lamp, v3(c.x + 0.42f, 1.09f, c.z - 0.32f), 0.07f, 0.12f);
    float sw = 0.9f;
    float sh = sw * (float)m->sign_shred.albedo.height / (float)m->sign_shred.albedo.width;
    PbrDraw sign = decal(&m->sign_shred, (Color){ 255, 214, 90, 255 });
    quad(&sign, v3(c.x, 0.7f, c.z + 0.452f), sw, sh, 0);
}

static void draw_press(const Mailroom *m)
{
    float ram = m->w.ram;
    Vector3 c = v3(MACHINE_X, 0, MACHINE_Z);
    PbrDraw body = mat(m, MAT_DARK_STEEL);
    PbrDraw steel = mat(m, MAT_STEEL);
    PbrDraw yellow = mat(m, MAT_PAINT_YELLOW);
    PbrDraw stripe = mat(m, MAT_HAZARD);
    pbr_bevel_box(&body, v3(c.x, 0.25f, c.z), v3(1.3f, 0.5f, 1.0f), 0);
    stripe.uv_scale = (Vector2){ 2, 1.5f };
    pbr_box(&stripe, v3(c.x, 0.52f, c.z), v3(1.0f, 0.04f, 0.8f), 0);   /* bed */
    for (int side = -1; side <= 1; side += 2)
        pbr_cylinder(&steel, v3(c.x + side * 0.55f, 0.5f, c.z - 0.3f), 0.07f, 2.0f);
    pbr_bevel_box(&yellow, v3(c.x, 2.6f, c.z - 0.1f), v3(1.35f, 0.4f, 0.75f), 0);
    /* hydraulic ram / 유압 램 */
    float head_y = 1.95f - ram * (1.95f - (0.55f + BOX * 0.92f + 0.12f));
    pbr_cylinder(&steel, v3(c.x, head_y, c.z), 0.1f, 2.45f - head_y);
    pbr_bevel_box(&body, v3(c.x, head_y, c.z), v3(0.85f, 0.24f, 0.75f), 0);
    float sw = 0.9f;
    float sh = sw * (float)m->sign_press.albedo.height / (float)m->sign_press.albedo.width;
    PbrDraw sign = decal(&m->sign_press, (Color){ 40, 30, 10, 255 });
    quad(&sign, v3(c.x, 2.6f, c.z + 0.28f), sw, sh, 0);
}

static void draw_shelf(const Mailroom *m)
{
    PbrDraw frame = mat(m, MAT_PAINT_BLUE);
    PbrDraw wood = mat(m, MAT_WOOD);
    float half = LEVEL_SHELF_SIZE * SHELF_SLOT * 0.5f;
    for (int k = 0; k <= LEVEL_SHELF_SIZE; k += 2)
        for (int d = -1; d <= 1; d += 2)
            pbr_box(&frame, v3(-half + k * SHELF_SLOT, 1.0f, SHELF_Z + d * 0.36f),
                    v3(0.07f, 2.0f, 0.07f), 0);
    wood.uv_scale = (Vector2){ 6, 1 };
    pbr_box(&wood, v3(0, SHELF_TOP - 0.03f, SHELF_Z), v3(half * 2 + 0.1f, 0.06f, 0.8f), 0);
    pbr_box(&wood, v3(0, 0.15f, SHELF_Z), v3(half * 2 + 0.1f, 0.06f, 0.8f), 0);
    pbr_box(&frame, v3(0, SHELF_TOP - 0.09f, SHELF_Z + 0.4f), v3(half * 2 + 0.1f, 0.1f, 0.04f), 0);
    for (int i = 0; i < LEVEL_SHELF_SIZE; ++i) {
        float f = m->w.shelf_flash[i];
        Color tint = { 255, 214, 90, 255 };
        PbrDraw dg = decal(&m->shelf_digit[i], tint);
        dg.emissive = v3(3.0f * f, 2.2f * f, 0.5f * f);
        float w = 0.16f;
        float h = w * (float)m->shelf_digit[i].albedo.height /
                  (float)m->shelf_digit[i].albedo.width;
        quad(&dg, v3(shelf_x(i), SHELF_TOP - 0.09f, SHELF_Z + 0.424f), w, h, 0);
    }
}

static void draw_parcel(Mailroom *m, const Parcel *p)
{
    float pop = 1.0f + 0.35f * p->pulse * cosf((1.0f - p->pulse) * 14.0f);
    float s = BOX * p->scale * pop;
    if (s < 0.01f) return;
    bool damaged = p->value < 0;
    float sy = s * (damaged ? 0.86f : 1.0f) * (1.0f - 0.25f * p->squash);
    float sxz = s * (1.0f + 0.12f * p->squash);
    Vector3 c = p->pos;
    c.y -= (s - sy) * 0.5f;
    PbrDraw card = mat(m, damaged ? MAT_CARDBOARD_DAMAGED : MAT_CARDBOARD);
    static const Color shades[4] = { { 255, 250, 240, 255 }, { 240, 228, 214, 255 },
                                     { 255, 240, 222, 255 }, { 232, 222, 210, 255 } };
    card.tint = shades[(unsigned)(p->value < 0 ? -p->value : p->value) % 4u];
    card.emissive = v3(1.6f * p->glow, 1.1f * p->glow, 0.35f * p->glow);
    pbr_bevel_box(&card, c, v3(sxz, sy, sxz), p->yaw);

    /* packing tape over the top and down the front and back
       윗면을 지나 앞뒤로 내려오는 포장 테이프 */
    PbrDraw tape = mat(m, MAT_TAPE);
    tape.emissive = card.emissive;
    pbr_box(&tape, v3(c.x, c.y + sy * 0.5f + 0.003f, c.z), v3(sxz * 0.24f, 0.008f, sxz * 1.004f), p->yaw);

    if (p->state == P_MERGING || p->scale < 0.4f) return;
    /* the label and its number face the camera (+z)
       라벨과 숫자는 카메라 쪽 (+z)을 향한다 */
    Texture2D tex = number_tex(m, (int)p->value);
    float h = s * 0.36f;
    float w = h * (float)tex.width / (float)tex.height;
    if (w > sxz * 0.82f) {
        h *= sxz * 0.82f / w;
        w = sxz * 0.82f;
    }
    float zf = 0.5f * sxz;
    Vector3 front = v3(c.x + sinf(p->yaw) * zf, c.y - sy * 0.06f,
                       c.z + cosf(p->yaw) * zf);
    PbrDraw label = mat(m, MAT_PAPER);
    label.no_shadow = true;
    label.emissive = v3(1.2f * p->glow, 0.9f * p->glow, 0.3f * p->glow);
    Vector3 lp = v3(front.x + sinf(p->yaw) * 0.004f, front.y,
                    front.z + cosf(p->yaw) * 0.004f);
    quad(&label, lp, w + sxz * 0.1f, h + sxz * 0.08f, p->yaw);
    PbrTextures ink = m->mat[MAT_PAPER];
    ink.albedo = tex;
    PbrDraw num = decal(&ink, damaged ? (Color){ 190, 30, 30, 255 }
                                      : (Color){ 25, 25, 30, 255 });
    Vector3 np = v3(front.x + sinf(p->yaw) * 0.008f, front.y,
                    front.z + cosf(p->yaw) * 0.008f);
    quad(&num, np, w, h, p->yaw);
}

static void draw_robot(const Mailroom *m)
{
    const World *w = &m->w;
    float x = w->robot_x, z = LANE_Z;
    bool walking = w->task_i < w->task_n &&
                   w->tasks[w->task_i].kind == T_WALK && w->task_started;
    bool working = w->task_i < w->task_n && !walking;
    float bob = walking ? sinf(w->time * 18.0f) * 0.04f : 0.0f;
    PbrDraw body = mat(m, MAT_PAINT_YELLOW);
    PbrDraw steel = mat(m, MAT_STEEL);
    PbrDraw dark = mat(m, MAT_DARK_STEEL);
    PbrDraw rubber = mat(m, MAT_RUBBER);
    PbrDraw glass = mat(m, MAT_GLASS_DARK);
    PbrDraw stripe = mat(m, MAT_HAZARD);

    pbr_cylinder(&rubber, v3(x, 0.0f, z), 0.58f, 0.2f);
    pbr_cylinder(&steel, v3(x, 0.2f, z), 0.5f, 0.06f);
    pbr_bevel_box(&body, v3(x, 0.82f + bob, z), v3(0.95f, 1.0f, 0.72f), 0);
    stripe.uv_scale = (Vector2){ 1.5f, 0.25f };
    pbr_box(&stripe, v3(x, 0.4f + bob, z), v3(0.97f, 0.12f, 0.74f), 0);
    /* chest screen / 가슴 화면 */
    pbr_box(&glass, v3(x, 0.92f + bob, z + 0.36f), v3(0.52f, 0.32f, 0.02f), 0);
    Color sc = working ? (Color){ 90, 230, 120, 255 } : (Color){ 80, 200, 255, 255 };
    PbrDraw bar = mat(m, MAT_PLASTIC);
    bar.tint = sc;
    bar.no_shadow = true;
    float level = 0.5f + 0.5f * sinf(w->time * (working ? 9.0f : 2.0f));
    bar.emissive = v3(sc.r / 255.0f * 2.5f, sc.g / 255.0f * 2.5f, sc.b / 255.0f * 2.5f);
    pbr_box(&bar, v3(x - 0.12f + level * 0.08f, 0.92f + bob, z + 0.372f),
            v3(0.14f + level * 0.18f, 0.05f, 0.006f), 0);

    pbr_cylinder(&steel, v3(x, 1.32f + bob, z), 0.14f, 0.12f);      /* neck */
    PbrDraw head = mat(m, MAT_STEEL);
    pbr_bevel_box(&head, v3(x, 1.6f + bob, z), v3(0.7f, 0.46f, 0.58f), 0);
    pbr_box(&glass, v3(x, 1.61f + bob, z + 0.292f), v3(0.54f, 0.22f, 0.02f), 0);
    PbrDraw eye = mat(m, MAT_PLASTIC);
    eye.tint = sc;
    eye.no_shadow = true;
    eye.emissive = v3(sc.r / 255.0f * 4.0f, sc.g / 255.0f * 4.0f, sc.b / 255.0f * 4.0f);
    pbr_sphere(&eye, v3(x - 0.13f, 1.61f + bob, z + 0.3f), 0.05f);
    pbr_sphere(&eye, v3(x + 0.13f, 1.61f + bob, z + 0.3f), 0.05f);
    pbr_cylinder(&steel, v3(x + 0.2f, 1.83f + bob, z), 0.015f, 0.25f);  /* antenna */
    PbrDraw tip = mat(m, MAT_PLASTIC);
    tip.tint = (Color){ 255, 70, 60, 255 };
    float blink = fmodf(w->time, 1.2f) < 0.15f ? 5.0f : 0.4f;
    tip.emissive = v3(blink, blink * 0.2f, blink * 0.15f);
    pbr_sphere(&tip, v3(x + 0.2f, 2.1f + bob, z), 0.045f);

    for (int side = -1; side <= 1; side += 2) {
        float ax = x + side * 0.6f;
        float ay = 0.75f + w->arm_lift * 0.95f + bob;
        pbr_sphere(&dark, v3(ax, 1.15f + bob, z), 0.11f);           /* shoulder */
        pbr_bevel_box(&body, v3(ax, ay, z), v3(0.16f, 0.75f, 0.16f), 0);
        float hand_y = ay + (w->arm_lift > 0.5f ? 0.42f : -0.42f);
        pbr_sphere(&rubber, v3(ax, hand_y, z), 0.1f);
    }
}

static void draw_particles(const Mailroom *m)
{
    const World *w = &m->w;
    PbrDraw paper = mat(m, MAT_PAPER);
    paper.no_shadow = true;
    PbrDraw spark = mat(m, MAT_PLASTIC);
    spark.no_shadow = true;
    for (int i = 0; i < MAX_PARTS; ++i) {
        const Particle *s = &w->parts[i];
        if (s->life <= 0) continue;
        float a = s->life / s->max;
        if (s->kind == PART_SPARK) {
            spark.emissive = Vector3Scale(s->color, a);
            spark.tint = (Color){ 255, 220, 150, 255 };
            pbr_box(&spark, s->p, v3(s->size, s->size, s->size), 0);
        } else if (s->kind == PART_SHRED) {
            paper.tint = (Color){ 215, 180, 130, 255 };
            pbr_box(&paper, s->p, v3(s->size * 0.3f, 0.01f, s->size),
                    s->spin * (s->max - s->life));
        } else {
            paper.tint = (Color){ 180, 170, 160, 255 };
            float k = s->size * (1.5f - a);
            pbr_box(&paper, s->p, v3(k, k, k), 0);
        }
    }
}

/* Everything that casts shadows is drawn in both passes.
 * 그림자를 만드는 모든 것은 두 단계 모두에서 그린다. */
static void draw_world(Mailroom *m)
{
    World *w = &m->w;
    draw_room(m);
    draw_conveyor(m, INBOX_X, LANE_Z + 0.6f, BELT_LEN, 0.6f);
    draw_conveyor(m, OUTBOX_X, LANE_Z + 0.6f, BELT_LEN, -0.6f);
    if (w->odd_belt) draw_conveyor(m, MACHINE_X, ODD_FRONT_Z + 0.55f, 6.0f, -0.6f);
    draw_bin(m);
    if (w->shredder) draw_shredder(m);
    if (w->press) draw_press(m);
    if (w->shelf) draw_shelf(m);
    for (int i = 0; i < MAX_PARCELS; ++i)
        if (w->parcels[i].state != P_DEAD) draw_parcel(m, &w->parcels[i]);
    draw_robot(m);
    draw_particles(m);

    if (!pbr_in_shadow_pass()) {
        /* floor stencils in front of each belt / 각 벨트 앞의 바닥 글자 */
        Color paint = { 250, 196, 40, 255 };
        floor_decal(&m->sign_inbox, v3(INBOX_X, 0.006f, LANE_Z + 1.35f), 1.3f, paint);
        floor_decal(w->odd_belt ? &m->sign_even : &m->sign_outbox,
                    v3(OUTBOX_X, 0.006f, LANE_Z + 1.35f), w->odd_belt ? 1.1f : 1.5f,
                    paint);
        if (w->odd_belt)
            floor_decal(&m->sign_odd, v3(MACHINE_X, 0.006f, LANE_Z + 1.35f), 0.8f,
                        paint);
    }
}

/* Additive halos over glowing parcels and sparks (a cheap bloom).
 * 빛나는 상자와 불꽃 위의 가산 혼합 빛 (값싼 블룸). */
static void draw_halos(const Mailroom *m)
{
    const World *w = &m->w;
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (int i = 0; i < MAX_PARCELS; ++i) {
        const Parcel *p = &w->parcels[i];
        if (p->state == P_DEAD || p->glow <= 0.02f) continue;
        float s = BOX * p->scale * (2.4f + 0.6f * (1.0f - p->glow));
        DrawBillboard(m->cam, m->halo, p->pos, s,
                      (Color){ 255, 190, 90, (unsigned char)(170 * p->glow) });
    }
    for (int i = 0; i < MAX_PARTS; ++i) {
        const Particle *s = &w->parts[i];
        if (s->life <= 0 || s->kind != PART_SPARK) continue;
        float a = s->life / s->max;
        DrawBillboard(m->cam, m->halo, s->p, 0.35f,
                      (Color){ 255, 200, 110, (unsigned char)(150 * a) });
    }
    rlEnableDepthMask();
    EndBlendMode();
}

static void setup_lights(const World *w)
{
    pbr_set_sun(v3(-0.38f, -1.0f, -0.42f), v3(3.0f, 2.9f, 2.7f));
    pbr_set_ambient(v3(0.26f, 0.30f, 0.38f), v3(0.11f, 0.095f, 0.08f), 0.95f);
    pbr_set_fog(v3(0.05f, 0.055f, 0.07f), 0.012f);
    /* warm work lamps over the lane / 통로 위의 따뜻한 작업등 */
    pbr_set_point(0, v3(-4.0f, 4.2f, 2.2f), v3(22, 17, 11));
    pbr_set_point(1, v3(0.0f, 4.4f, 1.6f), v3(18, 14, 9));
    pbr_set_point(2, v3(4.0f, 4.2f, 2.2f), v3(22, 17, 11));
    /* the shredder's warning lamp lights the floor while it runs
       파쇄기의 경고등은 작동하는 동안 바닥을 비춘다 */
    float s = w->shredder ? w->shred_shake : 0.0f;
    pbr_set_point(3, v3(MACHINE_X + 0.42f, 1.4f, MACHINE_Z),
                  v3(6.0f * s, 1.8f * s, 0.4f * s));
}

static void mailroom_prerender(void *state, int width, int height)
{
    Mailroom *m = state;
    World *w = &m->w;
    if (!m->assets) load_assets(m);
    int rw = width * SSAA, rh = height * SSAA;
    if (m->rt.id == 0 || m->rt.texture.width != rw || m->rt.texture.height != rh) {
        if (m->rt.id) UnloadRenderTexture(m->rt);
        m->rt = LoadRenderTexture(rw, rh);
        SetTextureFilter(m->rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    /* make new number textures before any pass starts
       어떤 단계보다 먼저 새 숫자 텍스처를 만든다 */
    for (int i = 0; i < MAX_PARCELS; ++i)
        if (w->parcels[i].state != P_DEAD) number_tex(m, (int)w->parcels[i].value);

    setup_lights(w);
    pbr_begin_shadow(v3(0, 0, -1.5f), 8.5f);
    draw_world(m);
    pbr_end_shadow();

    BeginTextureMode(m->rt);
    ClearBackground((Color){ 12, 13, 17, 255 });
    BeginMode3D(m->cam);
    pbr_begin_scene(m->cam);
    draw_world(m);
    pbr_end_scene();
    draw_halos(m);
    EndMode3D();
    EndTextureMode();
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
    DrawRectangleRounded((Rectangle){ (float)x - 8, (float)y - 5, (float)tw + 16,
                                      (float)size + 10 }, 0.3f, 6, bg);
    DrawText(text, x, y, size, fg);
}

static void mailroom_draw(void *state, int w, int h)
{
    Mailroom *m = state;
    World *wd = &m->w;
    ClearBackground(BLACK);
    if (m->rt.id == 0) return;      /* prerender() has not run yet
                                       prerender()가 아직 실행되지 않음 */
    Rectangle src = { 0, 0, (float)m->rt.texture.width, -(float)m->rt.texture.height };
    DrawTexturePro(m->rt.texture, src, (Rectangle){ 0, 0, (float)w, (float)h },
                   (Vector2){ 0, 0 }, 0, WHITE);

    if (wd->popup_t > 0) {
        /* the sum rises above the robot / 합계가 로봇 위로 떠오른다 */
        float rise = (1.4f - wd->popup_t) * 0.6f;
        unsigned char a = (unsigned char)(255 * fminf(1.0f, wd->popup_t * 2.0f));
        draw_label(m->cam,
                   v3(wd->robot_x, HOLD_Y + wd->hand_n * BOX + 0.8f + rise, LANE_Z),
                   wd->popup, 30, (Color){ 40, 30, 10, (unsigned char)(a * 0.8f) },
                   (Color){ 255, 220, 100, a }, w, h);
    }
}

static void mailroom_destroy(void *state)
{
    Mailroom *m = state;
    if (!m) return;
    free_assets(m);
    free(m);
}

Scene scene_mailroom_create(void)
{
    Mailroom *m = calloc(1, sizeof *m);
    if (m) {
        m->cam.position = v3(0.0f, 6.9f, 9.9f);
        m->cam.target = v3(0.0f, 0.9f, -0.9f);
        m->cam.up = v3(0.0f, 1.0f, 0.0f);
        m->cam.fovy = 41.0f;
        m->cam.projection = CAMERA_PERSPECTIVE;
        m->w.press_parcel = -1;
        for (int i = 0; i < LEVEL_SHELF_SIZE; ++i) m->w.shelf_slot[i] = -1;
    }
    Scene s = {
        .state = m,
        .reset = mailroom_reset,
        .on_event = mailroom_on_event,
        .update = mailroom_update,
        .busy = mailroom_busy,
        .prerender = mailroom_prerender,
        .draw = mailroom_draw,
        .destroy = mailroom_destroy,
    };
    return s;
}
