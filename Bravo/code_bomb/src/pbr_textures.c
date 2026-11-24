/*
 * pbr_textures.c - procedural PBR materials (concrete, cardboard, paint...)
 * pbr_textures.c - 절차적 PBR 재질 (콘크리트, 골판지, 도장 금속 등)
 *
 * Each material is painted in code from tileable noise: a height field
 * (turned into the normal map and the occlusion), an albedo, a metallic
 * mask and a roughness map, the same channels a texture artist would
 * export from a PBR tool.
 * 각 재질은 이어지는 노이즈로 코드에서 그린다: 높이 맵 (노멀 맵과 차폐로
 * 변환), 알베도, 금속성 마스크, 거칠기 맵. 텍스처 아티스트가 PBR 도구에서
 * 내보내는 것과 같은 채널이다.
 */
#include "pbr.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- noise / 노이즈 ----------------------------------------------------- */

static unsigned hash2(int x, int y, unsigned seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u +
                 seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float rnd2(int x, int y, unsigned seed)
{
    return (float)(hash2(x, y, seed) & 0xffffffu) / 16777215.0f;
}

static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static int wrapi(int a, int p) { return ((a % p) + p) % p; }

/* Value noise that repeats every px by py cells, so textures tile.
 * px x py 칸마다 반복되는 값 노이즈. 그래서 텍스처가 이어진다. */
static float vnoise(float x, float y, int px, int py, unsigned seed)
{
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = smooth(x - (float)x0), fy = smooth(y - (float)y0);
    int xa = wrapi(x0, px), xb = wrapi(x0 + 1, px);
    int ya = wrapi(y0, py), yb = wrapi(y0 + 1, py);
    float a = rnd2(xa, ya, seed), b = rnd2(xb, ya, seed);
    float c = rnd2(xa, yb, seed), d = rnd2(xb, yb, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

/* Fractal noise on the unit square, fx by fy base cells.
 * 단위 정사각형 위의 프랙탈 노이즈 (기본 칸 fx x fy). */
static float fbm(float u, float v, int fx, int fy, int oct, unsigned seed)
{
    float sum = 0, amp = 0.5f, norm = 0;
    for (int o = 0; o < oct; o++) {
        sum += amp * vnoise(u * fx, v * fy, fx, fy, seed + (unsigned)o * 17u);
        norm += amp;
        amp *= 0.5f;
        fx *= 2;
        fy *= 2;
    }
    return sum / norm;
}

static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

static float sstep(float a, float b, float x)
{
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* ---- canvas / 캔버스 ---------------------------------------------------- */

typedef struct {
    int    n;
    float *r, *g, *b;           /* albedo, sRGB 0-1 */
    float *h;                   /* height / 높이 */
    float *metal, *rough;
    float  normal_strength;
} Canvas;

static void canvas_init(Canvas *c, int n)
{
    size_t k = (size_t)n * (size_t)n;
    c->n = n;
    c->r = calloc(k, sizeof(float));
    c->g = calloc(k, sizeof(float));
    c->b = calloc(k, sizeof(float));
    c->h = calloc(k, sizeof(float));
    c->metal = calloc(k, sizeof(float));
    c->rough = calloc(k, sizeof(float));
    c->normal_strength = 2.0f;
    if (!c->r || !c->g || !c->b || !c->h || !c->metal || !c->rough) abort();
}

static void canvas_free(Canvas *c)
{
    free(c->r); free(c->g); free(c->b); free(c->h); free(c->metal);
    free(c->rough);
}

static void set_rgb(Canvas *c, int i, float r, float g, float b)
{
    c->r[i] = clampf(r, 0, 1);
    c->g[i] = clampf(g, 0, 1);
    c->b[i] = clampf(b, 0, 1);
}

static unsigned char u8(float x) { return (unsigned char)(clampf(x, 0, 1) * 255.0f + 0.5f); }

static Texture2D upload(Color *px, int n)
{
    Image img = { px, n, n, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(img);
    GenTextureMipmaps(&t);
    /* anisotropic filtering keeps the floor sharp at low angles
       비등방성 필터는 낮은 각도에서도 바닥을 선명하게 한다 */
    SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

/* Height -> normal map (Sobel) and occlusion (height below its blurred
 * neighbourhood). / 높이 -> 노멀 맵 (Sobel), 차폐 (주변 평균보다 낮은 곳). */
static PbrTextures canvas_finish(Canvas *c)
{
    int n = c->n;
    size_t k = (size_t)n * (size_t)n;
    Color *alb = malloc(k * sizeof(Color));
    Color *mra = malloc(k * sizeof(Color));
    Color *nor = malloc(k * sizeof(Color));
    if (!alb || !mra || !nor) abort();
#define H(x, y) c->h[wrapi(y, n) * n + wrapi(x, n)]
    float s = c->normal_strength * (float)n / 256.0f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float dx = (H(x + 1, y - 1) + 2 * H(x + 1, y) + H(x + 1, y + 1)) -
                       (H(x - 1, y - 1) + 2 * H(x - 1, y) + H(x - 1, y + 1));
            float dy = (H(x - 1, y + 1) + 2 * H(x, y + 1) + H(x + 1, y + 1)) -
                       (H(x - 1, y - 1) + 2 * H(x, y - 1) + H(x + 1, y - 1));
            /* green points up in the image (towards row 0)
               초록은 이미지 위쪽(0행)을 향한다 */
            float nx = -dx * s * 0.25f, ny = dy * s * 0.25f, nz = 1.0f;
            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            nor[i] = (Color){ u8(nx / len * 0.5f + 0.5f), u8(ny / len * 0.5f + 0.5f),
                              u8(nz / len * 0.5f + 0.5f), 255 };
            float avg = 0;
            for (int oy = -3; oy <= 3; oy += 3)
                for (int ox = -3; ox <= 3; ox += 3) avg += H(x + ox, y + oy);
            avg /= 9.0f;
            float ao = clampf(1.0f - (avg - c->h[i]) * 2.5f, 0.35f, 1.0f);
            alb[i] = (Color){ u8(c->r[i]), u8(c->g[i]), u8(c->b[i]), 255 };
            mra[i] = (Color){ u8(c->metal[i]), u8(c->rough[i]), u8(ao), 255 };
        }
#undef H
    PbrTextures t = { upload(alb, n), upload(mra, n), upload(nor, n) };
    free(alb);
    free(mra);
    free(nor);
    canvas_free(c);
    return t;
}

/* Thin scratches: random segments; returns 0..1 coverage at (u, v).
 * 가는 긁힘: 무작위 선분. (u, v)에서의 덮임 0..1을 돌려준다. */
static float scratches(float u, float v, int count, float width, unsigned seed)
{
    float best = 0;
    for (int k = 0; k < count; k++) {
        float ax = rnd2(k, 1, seed), ay = rnd2(k, 2, seed);
        float ang = rnd2(k, 3, seed) * 6.2832f;
        float len = 0.04f + rnd2(k, 4, seed) * 0.16f;
        float bx = ax + cosf(ang) * len, by = ay + sinf(ang) * len;
        /* nearest distance, also across the tile edge / 타일 경계 너머도 */
        for (int ox = -1; ox <= 1; ox++)
            for (int oy = -1; oy <= 1; oy++) {
                float px = u + (float)ox, py = v + (float)oy;
                float vx = bx - ax, vy = by - ay;
                float t = clampf(((px - ax) * vx + (py - ay) * vy) /
                                     (vx * vx + vy * vy), 0, 1);
                float dx = px - (ax + vx * t), dy = py - (ay + vy * t);
                float d = sqrtf(dx * dx + dy * dy);
                float c = 1.0f - sstep(width * 0.3f, width, d);
                if (c > best) best = c;
            }
    }
    return best;
}

/* ---- materials / 재질 --------------------------------------------------- */

static PbrTextures make_concrete(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 1.6f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float low = fbm(u, v, 4, 4, 4, 11);
            float mid = fbm(u, v, 16, 16, 4, 23);
            float fine = vnoise(u * 160, v * 160, 160, 160, 37);
            float base = 0.50f + (low - 0.5f) * 0.18f + (fine - 0.5f) * 0.06f;
            float r = base, g = base * 0.985f, b = base * 0.95f;
            float stain = sstep(0.60f, 0.74f, fbm(u, v, 3, 3, 4, 51));
            r *= 1.0f - 0.22f * stain;
            g *= 1.0f - 0.24f * stain;
            b *= 1.0f - 0.28f * stain;
            float speck = rnd2(x, y, 61);
            if (speck > 0.992f) { r *= 1.25f; g *= 1.25f; b *= 1.22f; }
            else if (speck < 0.006f) { r *= 0.7f; g *= 0.7f; b *= 0.7f; }
            float h = mid * 0.35f + fine * 0.12f;
            /* saw-cut joint along the tile edges / 타일 가장자리의 줄눈 */
            float ex = fminf((float)x, (float)(n - 1 - x));
            float ey = fminf((float)y, (float)(n - 1 - y));
            float e = fminf(ex, ey);
            if (e < 2.5f) {
                h -= 0.6f * (1.0f - e / 2.5f);
                r *= 0.62f; g *= 0.62f; b *= 0.62f;
            }
            if (rnd2(x / 3, y / 3, 71) < 0.004f) h -= 0.3f;        /* pits */
            set_rgb(&c, i, r, g, b);
            c.h[i] = h;
            float polish = sstep(0.55f, 0.7f, low);
            c.rough[i] = clampf(0.86f - polish * 0.28f + (fine - 0.5f) * 0.1f +
                                    stain * 0.06f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_cardboard(int n, bool damaged)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 1.2f;
    unsigned sd = damaged ? 900u : 100u;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float fib = fbm(u, v, 4, 32, 3, sd + 1);
            float grain = vnoise(u * 200, v * 50, 200, 50, sd + 2);
            float blot = fbm(u, v, 3, 3, 4, sd + 3);
            float k = 0.92f + (fib - 0.5f) * 0.22f + (grain - 0.5f) * 0.08f +
                      (blot - 0.5f) * 0.12f;
            float r = 0.66f * k, g = 0.48f * k, b = 0.30f * k;
            /* flutes inside the board show as faint ridges / 골판지 골 */
            float flute = 0.5f + 0.5f * sinf(v * 6.2832f * 26.0f);
            float h = flute * 0.10f + fib * 0.25f + grain * 0.05f;
            /* darker, worn edges / 어둡고 닳은 가장자리 */
            float e = fminf(fminf(u, 1 - u), fminf(v, 1 - v));
            float edge = 1.0f - sstep(0.0f, 0.07f, e);
            r *= 1.0f - 0.18f * edge;
            g *= 1.0f - 0.2f * edge;
            b *= 1.0f - 0.2f * edge;
            float scuff = sstep(0.68f, 0.8f, fbm(u, v, 8, 8, 3, sd + 4));
            r += scuff * 0.06f; g += scuff * 0.05f; b += scuff * 0.03f;
            float rough = 0.86f + (grain - 0.5f) * 0.08f;
            if (damaged) {
                /* water stains with darker rims / 테두리가 진한 물 얼룩 */
                float w = fbm(u + 0.3f, v, 3, 3, 5, sd + 5);
                float wet = sstep(0.55f, 0.62f, w);
                float rim = sstep(0.55f, 0.57f, w) - sstep(0.57f, 0.6f, w);
                r *= 1.0f - 0.32f * wet - 0.2f * rim;
                g *= 1.0f - 0.36f * wet - 0.22f * rim;
                b *= 1.0f - 0.38f * wet - 0.2f * rim;
                rough -= wet * 0.15f;
                /* crush creases / 찌그러진 주름 */
                for (int q = 0; q < 4; q++) {
                    float cx = rnd2(q, 7, sd), cy = rnd2(q, 8, sd);
                    float ang = rnd2(q, 9, sd) * 3.1416f;
                    float d = fabsf((u - cx) * sinf(ang) - (v - cy) * cosf(ang));
                    float along = fabsf((u - cx) * cosf(ang) + (v - cy) * sinf(ang));
                    float crease = (1.0f - sstep(0.0f, 0.012f, d)) *
                                   (1.0f - sstep(0.15f, 0.3f, along));
                    h -= crease * 0.6f;
                    r *= 1.0f - crease * 0.3f;
                    g *= 1.0f - crease * 0.32f;
                    b *= 1.0f - crease * 0.3f;
                }
                /* a torn corner shows the pale fluting / 찢긴 모서리 */
                float torn = 1.0f - sstep(0.12f, 0.16f + 0.03f * grain,
                                          sqrtf(u * u + (1 - v) * (1 - v)));
                if (torn > 0) {
                    float t2 = 0.5f + 0.5f * sinf(u * 6.2832f * 30.0f);
                    r = r * (1 - torn) + (0.8f - t2 * 0.12f) * torn;
                    g = g * (1 - torn) + (0.66f - t2 * 0.12f) * torn;
                    b = b * (1 - torn) + (0.46f - t2 * 0.1f) * torn;
                    h += torn * t2 * 0.4f - torn * 0.3f;
                }
            }
            set_rgb(&c, i, r, g, b);
            c.h[i] = h;
            c.rough[i] = clampf(rough, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_paint(int n, float pr, float pg, float pb, unsigned sd)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 1.0f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float peel = vnoise(u * 48, v * 48, 48, 48, sd + 1);
            float dirt = sstep(0.55f, 0.85f, fbm(u, v, 4, 4, 4, sd + 2));
            float var = fbm(u, v, 8, 8, 3, sd + 3);
            float sc = scratches(u, v, 10, 0.004f, sd + 4) * 0.8f;
            float k = 0.95f + (var - 0.5f) * 0.1f;
            float r = pr * k, g = pg * k, b = pb * k;
            r *= 1.0f - dirt * 0.35f;
            g *= 1.0f - dirt * 0.38f;
            b *= 1.0f - dirt * 0.4f;
            float rough = 0.38f + (var - 0.5f) * 0.12f + dirt * 0.3f;
            float metal = 0.0f;
            float h = peel * 0.06f;
            if (sc > 0) {          /* bare steel in the scratch / 긁힌 곳의 강철 */
                r = r * (1 - sc) + 0.58f * sc;
                g = g * (1 - sc) + 0.58f * sc;
                b = b * (1 - sc) + 0.6f * sc;
                metal = sc;
                rough = rough * (1 - sc) + 0.32f * sc;
                h -= sc * 0.25f;
            }
            set_rgb(&c, i, r, g, b);
            c.h[i] = h;
            c.metal[i] = metal;
            c.rough[i] = clampf(rough, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_steel(int n, bool dark)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 0.6f;
    unsigned sd = dark ? 300u : 200u;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float streak = vnoise(u * 4, v * 220, 4, 220, sd + 1);
            float smudge = fbm(u, v, 4, 4, 4, sd + 2);
            float base = dark ? 0.26f : 0.64f;
            float k = base + (streak - 0.5f) * 0.08f + (smudge - 0.5f) * 0.06f;
            float rough = (dark ? 0.42f : 0.26f) + streak * 0.14f +
                          sstep(0.6f, 0.8f, smudge) * 0.2f;
            if (dark) {
                float oil = sstep(0.62f, 0.7f, fbm(u, v, 3, 3, 4, sd + 3));
                k *= 1.0f - oil * 0.4f;
                rough -= oil * 0.15f;
            }
            set_rgb(&c, i, k, k * 1.01f, k * 1.03f);
            c.h[i] = streak * 0.15f;
            c.metal[i] = 1.0f;
            c.rough[i] = clampf(rough, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_belt(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 2.5f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            /* raised cleats across the belt / 벨트를 가로지르는 돌기 */
            float f = fmodf(v * 8.0f, 1.0f);
            float cleat = sstep(0.0f, 0.06f, f) * (1.0f - sstep(0.16f, 0.22f, f));
            float grit = vnoise(u * 128, v * 128, 128, 128, 401);
            float wear = fbm(u, v, 2, 8, 3, 402);
            float k = 0.07f + grit * 0.025f + cleat * 0.02f +
                      sstep(0.5f, 0.8f, wear) * 0.03f;
            float dust = sstep(0.7f, 0.9f, fbm(u, v, 8, 8, 3, 403)) * 0.05f;
            set_rgb(&c, i, k + dust, k + dust * 0.95f, k + dust * 0.9f);
            c.h[i] = cleat * 0.5f + grit * 0.05f;
            c.rough[i] = clampf(0.78f - cleat * 0.15f + grit * 0.08f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_simple(int n, float r0, float g0, float b0, float rough,
                               float metal, float bump, unsigned sd)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = bump;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float fine = vnoise(u * 96, v * 96, 96, 96, sd + 1);
            float var = fbm(u, v, 4, 4, 3, sd + 2);
            float k = 0.94f + (var - 0.5f) * 0.12f + (fine - 0.5f) * 0.04f;
            set_rgb(&c, i, r0 * k, g0 * k, b0 * k);
            c.h[i] = fine * 0.2f + var * 0.1f;
            c.metal[i] = metal;
            c.rough[i] = clampf(rough + (var - 0.5f) * 0.12f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_hazard(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 1.0f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float f = fmodf((u + v) * 4.0f, 1.0f);
            float yellow = sstep(0.0f, 0.02f, f) * (1.0f - sstep(0.5f, 0.52f, f));
            float sc = scratches(u, v, 30, 0.007f, 501);
            float dirt = sstep(0.55f, 0.85f, fbm(u, v, 4, 4, 4, 502));
            float r = 0.04f + yellow * 0.9f, g = 0.04f + yellow * 0.68f,
                  b = 0.04f + yellow * 0.04f;
            r *= 1 - dirt * 0.4f; g *= 1 - dirt * 0.4f; b *= 1 - dirt * 0.4f;
            r = r * (1 - sc) + 0.55f * sc;
            g = g * (1 - sc) + 0.55f * sc;
            b = b * (1 - sc) + 0.57f * sc;
            set_rgb(&c, i, r, g, b);
            c.h[i] = -sc * 0.3f;
            c.metal[i] = sc;
            c.rough[i] = clampf(0.45f + dirt * 0.3f - sc * 0.1f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_wall(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 3.0f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            /* corrugated sheet: ridges run up the wall / 골함석: 세로 골 */
            float ridge = 0.5f + 0.5f * sinf(u * 6.2832f * 12.0f);
            float spangle = vnoise(u * 40, v * 40, 40, 40, 601);
            float streaks = fbm(u, v, 24, 2, 3, 602);
            float grime = sstep(0.3f, 1.0f, v) * 0.5f + streaks * 0.3f;
            float k = 0.56f + (spangle - 0.5f) * 0.06f - grime * 0.18f;
            set_rgb(&c, i, k * 0.95f, k, k * 1.06f);
            c.h[i] = ridge * 0.8f;
            c.metal[i] = 0.75f;
            c.rough[i] = clampf(0.42f + grime * 0.3f + (spangle - 0.5f) * 0.1f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_wood(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 1.5f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float warp = fbm(u, v, 2, 8, 3, 701);
            float ring = 0.5f + 0.5f * sinf((v * 28.0f + warp * 5.0f) * 3.1416f);
            float fine = vnoise(u * 160, v * 20, 160, 20, 702);
            float k = 0.85f + ring * 0.15f + (fine - 0.5f) * 0.1f;
            float dirt = sstep(0.55f, 0.8f, fbm(u, v, 4, 4, 3, 703));
            set_rgb(&c, i, 0.58f * k * (1 - dirt * 0.3f), 0.43f * k * (1 - dirt * 0.32f),
                    0.27f * k * (1 - dirt * 0.3f));
            c.h[i] = ring * 0.2f + fine * 0.15f;
            c.rough[i] = clampf(0.78f + (fine - 0.5f) * 0.1f, 0, 1);
        }
    return canvas_finish(&c);
}

static PbrTextures make_tape(int n)
{
    Canvas c;
    canvas_init(&c, n);
    c.normal_strength = 0.8f;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int i = y * n + x;
            float u = (float)x / n, v = (float)y / n;
            float wr = vnoise(u * 6, v * 64, 6, 64, 801);
            float k = 0.95f + (wr - 0.5f) * 0.1f;
            set_rgb(&c, i, 0.80f * k, 0.66f * k, 0.44f * k);
            c.h[i] = wr * 0.3f;
            c.rough[i] = clampf(0.28f + wr * 0.12f, 0, 1);
        }
    return canvas_finish(&c);
}

void pbr_textures_make(PbrTextures out[MAT_COUNT], int size)
{
    int big = size, small = size / 2 < 64 ? 64 : size / 2;
    out[MAT_CONCRETE] = make_concrete(big);
    out[MAT_CARDBOARD] = make_cardboard(big, false);
    out[MAT_CARDBOARD_DAMAGED] = make_cardboard(big, true);
    out[MAT_PAINT_YELLOW] = make_paint(small, 0.95f, 0.66f, 0.06f, 1000);
    out[MAT_PAINT_GREEN] = make_paint(small, 0.18f, 0.48f, 0.32f, 1100);
    out[MAT_PAINT_RED] = make_paint(small, 0.72f, 0.12f, 0.10f, 1200);
    out[MAT_PAINT_BLUE] = make_paint(small, 0.16f, 0.30f, 0.56f, 1300);
    out[MAT_STEEL] = make_steel(small, false);
    out[MAT_DARK_STEEL] = make_steel(small, true);
    out[MAT_RUBBER_BELT] = make_belt(small);
    out[MAT_RUBBER] = make_simple(small, 0.06f, 0.06f, 0.065f, 0.8f, 0.0f, 0.8f, 1400);
    out[MAT_HAZARD] = make_hazard(small);
    out[MAT_WALL] = make_wall(big);
    out[MAT_WOOD] = make_wood(small);
    out[MAT_PLASTIC] = make_simple(small, 0.82f, 0.82f, 0.8f, 0.42f, 0.0f, 0.5f, 1500);
    out[MAT_PAPER] = make_simple(small, 0.93f, 0.91f, 0.86f, 0.9f, 0.0f, 0.4f, 1600);
    out[MAT_TAPE] = make_tape(small);
    out[MAT_GLASS_DARK] = make_simple(small, 0.03f, 0.035f, 0.04f, 0.12f, 0.0f, 0.1f, 1700);
}

void pbr_textures_free(PbrTextures t[MAT_COUNT])
{
    for (int i = 0; i < MAT_COUNT; i++) {
        UnloadTexture(t[i].albedo);
        UnloadTexture(t[i].mra);
        UnloadTexture(t[i].normal);
    }
    memset(t, 0, sizeof(PbrTextures) * MAT_COUNT);
}

PbrTextures pbr_decal_textures(Image art, const PbrTextures *paint)
{
    PbrTextures t = *paint;           /* MRA and normal are borrowed
                                         MRA와 노멀은 빌려 쓴다 */
    t.albedo = LoadTextureFromImage(art);
    GenTextureMipmaps(&t.albedo);
    SetTextureFilter(t.albedo, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(t.albedo, TEXTURE_WRAP_CLAMP);
    return t;
}

void pbr_decal_free(PbrTextures *t)
{
    UnloadTexture(t->albedo);
    memset(t, 0, sizeof *t);
}
