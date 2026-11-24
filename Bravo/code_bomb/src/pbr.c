/*
 * pbr.c - shaders, meshes and the two render passes (see pbr.h)
 * pbr.c - 셰이더, 메시, 두 렌더 단계 (pbr.h 참고)
 *
 * The shading follows the usual real-time PBR recipe (Cook-Torrance with a
 * GGX distribution, Smith visibility and Schlick Fresnel; Karis' analytic
 * approximation for the ambient specular term). The shaders are GLSL 330,
 * which Windows, Linux and macOS (OpenGL 3.3 core) all accept.
 * 셰이딩은 흔히 쓰는 실시간 PBR 방식을 따른다 (GGX 분포, Smith 가시성,
 * Schlick 프레넬을 쓰는 Cook-Torrance, 주변 반사광은 Karis의 근사식).
 * 셰이더는 GLSL 330이라 Windows, Linux, macOS (OpenGL 3.3 core)에서 모두
 * 동작한다.
 */
#include "pbr.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PBR_MAPS 12     /* raylib's MAX_MATERIAL_MAPS (config.h) / raylib 기본값 */

/* ---- shaders / 셰이더 --------------------------------------------------- */

static const char *PBR_VS =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "in vec3 vertexNormal;\n"
    "in vec4 vertexTangent;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "uniform mat4 matNormal;\n"
    "uniform mat4 lightVP;\n"
    "uniform vec2 uvScale;\n"
    "uniform vec2 uvOffset;\n"
    "out vec3 fragPos;\n"
    "out vec2 fragUV;\n"
    "out vec3 fragN;\n"
    "out vec3 fragT;\n"
    "out float fragW;\n"
    "out vec4 fragLight;\n"
    "void main() {\n"
    "    vec4 wp = matModel * vec4(vertexPosition, 1.0);\n"
    "    fragPos = wp.xyz;\n"
    "    fragUV = vertexTexCoord * uvScale + uvOffset;\n"
    "    fragN = normalize(mat3(matNormal) * vertexNormal);\n"
    "    fragT = mat3(matModel) * vertexTangent.xyz;\n"
    "    fragW = vertexTangent.w < 0.0 ? -1.0 : 1.0;\n"
    "    fragLight = lightVP * wp;\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

/* Split in parts: C99 only promises 4095-character string literals.
 * C99는 4095자 문자열 리터럴만 보장하므로 나누어 둔다. */
static const char *const PBR_FS[] = {
    "#version 330\n"
    "in vec3 fragPos;\n"
    "in vec2 fragUV;\n"
    "in vec3 fragN;\n"
    "in vec3 fragT;\n"
    "in float fragW;\n"
    "in vec4 fragLight;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;   // albedo\n"
    "uniform sampler2D texture1;   // metallic, roughness, occlusion\n"
    "uniform sampler2D texture2;   // normal map\n"
    "uniform sampler2D shadowMap;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 emissive;\n"
    "uniform vec3 sunDir;\n"
    "uniform vec3 sunColor;\n"
    "uniform vec3 pointPos[4];\n"
    "uniform vec3 pointColor[4];\n"
    "uniform vec3 skyColor;\n"
    "uniform vec3 groundColor;\n"
    "uniform float exposure;\n"
    "uniform vec3 fogColor;\n"
    "uniform float fogDensity;\n"
    "uniform float shadowTexel;\n"
    "uniform int useShadow;\n"
    "const float PI = 3.14159265;\n"
    "\n"
    "float D_GGX(float NoH, float a) {\n"
    "    float a2 = a * a;\n"
    "    float d = NoH * NoH * (a2 - 1.0) + 1.0;\n"
    "    return a2 / (PI * d * d);\n"
    "}\n"
    "float V_Smith(float NoV, float NoL, float r) {\n"
    "    float k = (r + 1.0) * (r + 1.0) / 8.0;\n"
    "    float gv = NoV / (NoV * (1.0 - k) + k);\n"
    "    float gl = NoL / (NoL * (1.0 - k) + k);\n"
    "    return gv * gl / max(4.0 * NoV * NoL, 1e-4);\n"
    "}\n"
    "vec3 F_Schlick(float c, vec3 F0) {\n"
    "    return F0 + (1.0 - F0) * pow(1.0 - c, 5.0);\n"
    "}\n"
    "// The warehouse as seen in reflections: bright skylight strips above,\n"
    "// a dim wall band and a warm concrete floor below.\n"
    "vec3 environment(vec3 r, float rough) {\n"
    "    float up = clamp(r.y, -1.0, 1.0);\n"
    "    vec3 base = mix(groundColor, skyColor, smoothstep(-0.3, 0.5, up));\n"
    "    float strips = smoothstep(0.55, 0.9, abs(sin(r.z * 5.0 + r.x * 0.7)))\n"
    "                 * smoothstep(0.35, 0.8, up);\n"
    "    vec3 sharp = base + skyColor * strips * 2.5;\n"
    "    return mix(sharp, base * 1.1, clamp(rough * 1.4, 0.0, 1.0));\n"
    "}\n"
    "// Karis' fit of the split-sum BRDF lookup.\n"
    "vec3 envBRDF(vec3 F0, float rough, float NoV) {\n"
    "    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);\n"
    "    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);\n"
    "    vec4 r = rough * c0 + c1;\n"
    "    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;\n"
    "    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;\n"
    "    return F0 * AB.x + AB.y;\n"
    "}\n"
    "float shadowAt(vec3 N) {\n"
    "    if (useShadow == 0) return 1.0;\n"
    "    vec3 p = fragLight.xyz / fragLight.w * 0.5 + 0.5;\n"
    "    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0)\n"
    "        return 1.0;\n"
    "    float bias = max(0.004 * (1.0 - dot(N, -sunDir)), 0.0012);\n"
    "    float lit = 0.0;\n"
    "    for (int x = -2; x <= 2; x++)\n"
    "        for (int y = -2; y <= 2; y++) {\n"
    "            float d = texture(shadowMap, p.xy + vec2(x, y) * shadowTexel).r;\n"
    "            lit += (p.z - bias > d) ? 0.0 : 1.0;\n"
    "        }\n"
    "    return lit / 25.0;\n"
    "}\n",
    "vec3 aces(vec3 x) {\n"
    "    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),\n"
    "                 0.0, 1.0);\n"
    "}\n"
    "vec3 lightOne(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo,\n"
    "              float metal, float rough, vec3 F0) {\n"
    "    vec3 H = normalize(V + L);\n"
    "    float NoL = max(dot(N, L), 0.0);\n"
    "    if (NoL <= 0.0) return vec3(0.0);\n"
    "    float NoV = max(dot(N, V), 1e-4);\n"
    "    float NoH = max(dot(N, H), 0.0);\n"
    "    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);\n"
    "    vec3 spec = D_GGX(NoH, rough * rough) * V_Smith(NoV, NoL, rough) * F;\n"
    "    vec3 kd = (1.0 - F) * (1.0 - metal);\n"
    "    return (kd * albedo / PI + spec) * radiance * NoL;\n"
    "}\n"
    "void main() {\n"
    "    vec4 base = texture(texture0, fragUV) * colDiffuse;\n"
    "    if (base.a < 0.02) discard;\n"
    "    vec3 albedo = pow(base.rgb, vec3(2.2));\n"
    "    vec3 mra = texture(texture1, fragUV).rgb;\n"
    "    float metal = mra.r;\n"
    "    float rough = clamp(mra.g, 0.05, 1.0);\n"
    "    float ao = mra.b;\n"
    "    vec3 Ng = normalize(fragN);\n"
    "    vec3 T = fragT - Ng * dot(Ng, fragT);\n"
    "    vec3 N = Ng;\n"
    "    if (dot(T, T) > 1e-6) {\n"
    "        T = normalize(T);\n"
    "        vec3 B = cross(Ng, T) * fragW;\n"
    "        vec3 nt = texture(texture2, fragUV).xyz * 2.0 - 1.0;\n"
    "        N = normalize(mat3(T, B, Ng) * nt);\n"
    "    }\n"
    "    vec3 V = normalize(viewPos - fragPos);\n"
    "    vec3 F0 = mix(vec3(0.04), albedo, metal);\n"
    "    vec3 color = lightOne(-sunDir, sunColor * shadowAt(Ng), N, V, albedo,\n"
    "                          metal, rough, F0);\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "        vec3 d = pointPos[i] - fragPos;\n"
    "        float dist2 = max(dot(d, d), 0.01);\n"
    "        color += lightOne(d * inversesqrt(dist2), pointColor[i] / dist2, N,\n"
    "                          V, albedo, metal, rough, F0);\n"
    "    }\n"
    "    float NoV = max(dot(N, V), 1e-4);\n"
    "    vec3 irradiance = mix(groundColor, skyColor, N.y * 0.5 + 0.5);\n"
    "    vec3 Fa = F_Schlick(NoV, F0);\n"
    "    vec3 diffuseAmb = (1.0 - Fa) * (1.0 - metal) * albedo * irradiance;\n"
    "    vec3 R = reflect(-V, N);\n"
    "    vec3 specAmb = environment(R, rough) * envBRDF(F0, rough, NoV);\n"
    "    color += (diffuseAmb + specAmb) * ao;\n"
    "    color += emissive;\n"
    "    float dist = length(viewPos - fragPos);\n"
    "    color = mix(fogColor, color, exp(-dist * fogDensity));\n"
    "    color = aces(color * exposure);\n"
    "    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), base.a);\n"
    "}\n"
};

static const char *DEPTH_VS =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "uniform mat4 mvp;\n"
    "void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";

static const char *DEPTH_FS =
    "#version 330\n"
    "out vec4 finalColor;\n"
    "void main() { finalColor = vec4(1.0); }\n";

/* ---- state / 상태 ------------------------------------------------------- */

typedef struct {
    bool            ready;
    bool            active;           /* real PBR shaders / 실제 PBR 셰이더 */
    bool            shadow_pass;
    Shader          shader;
    Shader          depth;
    Material        mat;              /* reused for every draw / 재사용 */
    MaterialMap     maps[PBR_MAPS];
    Material        fallback;
    RenderTexture2D shadow;
    int             shadow_size;
    Matrix          light_vp;
    PbrMeshes       meshes;

    Vector3 sun_dir, sun_color;
    Vector3 point_pos[PBR_MAX_LIGHTS], point_color[PBR_MAX_LIGHTS];
    Vector3 sky, ground, fog;
    float   exposure, fog_density;
    double  saved_near, saved_far;

    int loc_view, loc_light_vp, loc_shadow, loc_emissive, loc_sun_dir,
        loc_sun_color, loc_point_pos, loc_point_color, loc_sky, loc_ground,
        loc_exposure, loc_fog, loc_fog_density, loc_texel, loc_use_shadow,
        loc_uv_scale, loc_uv_offset;
} Pbr;

static Pbr P;

bool pbr_active(void) { return P.active; }
bool pbr_in_shadow_pass(void) { return P.shadow_pass; }
const PbrMeshes *pbr_meshes(void) { return &P.meshes; }

/* ---- meshes / 메시 ------------------------------------------------------ */

/* Growing triangle list; every triangle gets its own tangent.
 * 늘어나는 삼각형 목록. 삼각형마다 접선을 따로 계산한다. */
typedef struct {
    float *v, *n, *t, *uv;
    int    count, cap;
} Builder;

static void b_push(Builder *b, Vector3 p, Vector3 n, Vector2 uv, Vector4 t)
{
    if (b->count == b->cap) {
        int cap = b->cap ? b->cap * 2 : 256;
        b->v = realloc(b->v, sizeof(float) * 3 * (size_t)cap);
        b->n = realloc(b->n, sizeof(float) * 3 * (size_t)cap);
        b->t = realloc(b->t, sizeof(float) * 4 * (size_t)cap);
        b->uv = realloc(b->uv, sizeof(float) * 2 * (size_t)cap);
        if (!b->v || !b->n || !b->t || !b->uv) abort();
        b->cap = cap;
    }
    int i = b->count++;
    b->v[i * 3] = p.x; b->v[i * 3 + 1] = p.y; b->v[i * 3 + 2] = p.z;
    b->n[i * 3] = n.x; b->n[i * 3 + 1] = n.y; b->n[i * 3 + 2] = n.z;
    b->t[i * 4] = t.x; b->t[i * 4 + 1] = t.y; b->t[i * 4 + 2] = t.z;
    b->t[i * 4 + 3] = t.w;
    b->uv[i * 2] = uv.x; b->uv[i * 2 + 1] = uv.y;
}

/* One triangle facing `out` (winding fixed automatically). Normals are
 * given per vertex (smooth) or all equal (flat). The tangent follows +u and
 * the bitangent "up in the image" (-v), matching the normal maps.
 * `out` 쪽을 향하는 삼각형 하나 (감는 방향은 자동으로 맞춘다). 접선은 +u,
 * 종접선은 "이미지 위쪽"(-v)을 따라 노멀 맵과 맞춘다. */
static void b_tri(Builder *b, Vector3 p0, Vector3 p1, Vector3 p2, Vector2 u0,
                  Vector2 u1, Vector2 u2, Vector3 n0, Vector3 n1, Vector3 n2,
                  Vector3 out)
{
    Vector3 e1 = Vector3Subtract(p1, p0), e2 = Vector3Subtract(p2, p0);
    if (Vector3DotProduct(Vector3CrossProduct(e1, e2), out) < 0) {
        Vector3 tp = p1; p1 = p2; p2 = tp;
        Vector2 tu = u1; u1 = u2; u2 = tu;
        Vector3 tn = n1; n1 = n2; n2 = tn;
        e1 = Vector3Subtract(p1, p0);
        e2 = Vector3Subtract(p2, p0);
    }
    float s1 = u1.x - u0.x, t1 = u1.y - u0.y;
    float s2 = u2.x - u0.x, t2 = u2.y - u0.y;
    float det = s1 * t2 - s2 * t1;
    Vector3 T = { 1, 0, 0 }, Bimg = { 0, 1, 0 };
    if (fabsf(det) > 1e-8f) {
        float r = 1.0f / det;
        T = Vector3Scale(Vector3Subtract(Vector3Scale(e1, t2),
                                         Vector3Scale(e2, t1)), r);
        Vector3 dv = Vector3Scale(Vector3Subtract(Vector3Scale(e2, s1),
                                                  Vector3Scale(e1, s2)), r);
        Bimg = Vector3Negate(dv);               /* image up / 이미지 위쪽 */
    }
    Vector3 ns[3] = { n0, n1, n2 };
    Vector3 ps[3] = { p0, p1, p2 };
    Vector2 us[3] = { u0, u1, u2 };
    for (int k = 0; k < 3; k++) {
        Vector3 n = ns[k];
        Vector3 t = Vector3Subtract(T, Vector3Scale(n, Vector3DotProduct(n, T)));
        if (Vector3Length(t) < 1e-6f) t = (Vector3){ 1, 0, 0 };
        t = Vector3Normalize(t);
        float w = Vector3DotProduct(Vector3CrossProduct(n, t), Bimg) < 0 ? -1.0f
                                                                         : 1.0f;
        b_push(b, ps[k], n, us[k], (Vector4){ t.x, t.y, t.z, w });
    }
}

static void b_flat_tri(Builder *b, Vector3 p0, Vector3 p1, Vector3 p2,
                       Vector2 u0, Vector2 u1, Vector2 u2, Vector3 n)
{
    b_tri(b, p0, p1, p2, u0, u1, u2, n, n, n, n);
}

static void b_flat_quad(Builder *b, Vector3 p0, Vector3 p1, Vector3 p2,
                        Vector3 p3, Vector2 u0, Vector2 u1, Vector2 u2,
                        Vector2 u3, Vector3 n)
{
    b_flat_tri(b, p0, p1, p2, u0, u1, u2, n);
    b_flat_tri(b, p0, p2, p3, u0, u2, u3, n);
}

static Mesh b_upload(Builder *b)
{
    Mesh m = { 0 };
    m.vertexCount = b->count;
    m.triangleCount = b->count / 3;
    size_t n = (size_t)b->count;
    m.vertices = MemAlloc((unsigned)(n * 3 * sizeof(float)));
    m.normals = MemAlloc((unsigned)(n * 3 * sizeof(float)));
    m.tangents = MemAlloc((unsigned)(n * 4 * sizeof(float)));
    m.texcoords = MemAlloc((unsigned)(n * 2 * sizeof(float)));
    memcpy(m.vertices, b->v, n * 3 * sizeof(float));
    memcpy(m.normals, b->n, n * 3 * sizeof(float));
    memcpy(m.tangents, b->t, n * 4 * sizeof(float));
    memcpy(m.texcoords, b->uv, n * 2 * sizeof(float));
    free(b->v); free(b->n); free(b->t); free(b->uv);
    memset(b, 0, sizeof *b);
    UploadMesh(&m, false);
    return m;
}

/* Face frames: normal, +u and image-up axes (see b_tri).
 * 면 좌표계: 법선, +u 축, 이미지 위쪽 축 (b_tri 참고). */
static const Vector3 FACE_N[6] = { { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 },
                                   { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } };
static const Vector3 FACE_U[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, -1 },
                                   { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 } };
static const Vector3 FACE_UP[6] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 },
                                    { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };

/* Point on face f at (a, c) in [-1, 1] of the inset rectangle, and its UV.
 * 면 f의 안쪽 사각형에서 (a, c) ∈ [-1, 1] 위치의 점과 UV. */
static Vector3 face_point(int f, float a, float c, float half, float inset)
{
    Vector3 p = Vector3Scale(FACE_N[f], half);
    p = Vector3Add(p, Vector3Scale(FACE_U[f], a * inset));
    return Vector3Add(p, Vector3Scale(FACE_UP[f], c * inset));
}

static Vector2 face_uv(float a, float c, float span)
{
    return (Vector2){ 0.5f + a * span * 0.5f, 0.5f - c * span * 0.5f };
}

static Mesh gen_box(float bevel)
{
    Builder b = { 0 };
    const float h = 0.5f, in = h - bevel;
    const float span = in / h;
    for (int f = 0; f < 6; f++) {
        Vector3 p[4] = { face_point(f, -1, -1, h, in), face_point(f, 1, -1, h, in),
                         face_point(f, 1, 1, h, in), face_point(f, -1, 1, h, in) };
        b_flat_quad(&b, p[0], p[1], p[2], p[3], face_uv(-1, -1, span),
                    face_uv(1, -1, span), face_uv(1, 1, span),
                    face_uv(-1, 1, span), FACE_N[f]);
    }
    if (bevel <= 0.0f) return b_upload(&b);

    /* chamfer strips between neighbouring faces / 이웃한 면 사이의 띠 */
    for (int a = 0; a < 3; a++)
        for (int c = a + 1; c < 3; c++)
            for (int sa = -1; sa <= 1; sa += 2)
                for (int sc = -1; sc <= 1; sc += 2) {
                    int e = 3 - a - c;
                    float pa[3], pb[3];
                    Vector3 q[4];
                    for (int k = 0; k < 2; k++) {
                        float se = k ? in : -in;
                        pa[a] = sa * h; pa[c] = sc * in; pa[e] = se;
                        pb[a] = sa * in; pb[c] = sc * h; pb[e] = se;
                        q[k] = (Vector3){ pa[0], pa[1], pa[2] };
                        q[3 - k] = (Vector3){ pb[0], pb[1], pb[2] };
                    }
                    float nn[3] = { 0, 0, 0 };
                    nn[a] = (float)sa; nn[c] = (float)sc;
                    Vector3 n = Vector3Normalize((Vector3){ nn[0], nn[1], nn[2] });
                    Vector2 uv0 = { 0.0f, 0.0f }, uv1 = { 1.0f, 0.0f };
                    Vector2 uv2 = { 1.0f, 0.03f }, uv3 = { 0.0f, 0.03f };
                    b_flat_quad(&b, q[0], q[1], q[2], q[3], uv0, uv1, uv2, uv3, n);
                }
    /* corner triangles / 모서리 삼각형 */
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2) {
                Vector3 p0 = { sx * h, sy * in, sz * in };
                Vector3 p1 = { sx * in, sy * h, sz * in };
                Vector3 p2 = { sx * in, sy * in, sz * h };
                Vector3 n = Vector3Normalize((Vector3){ (float)sx, (float)sy,
                                                        (float)sz });
                b_flat_tri(&b, p0, p1, p2, (Vector2){ 0, 0 },
                           (Vector2){ 0.03f, 0 }, (Vector2){ 0, 0.03f }, n);
            }
    return b_upload(&b);
}

static Mesh gen_cylinder(int seg)
{
    Builder b = { 0 };
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
        Vector3 n0 = { sinf(a0), 0, cosf(a0) }, n1 = { sinf(a1), 0, cosf(a1) };
        Vector3 b0 = n0, b1 = n1;
        Vector3 t0 = { n0.x, 1, n0.z }, t1 = { n1.x, 1, n1.z };
        float u0 = (float)i / seg * 2.0f, u1 = (float)(i + 1) / seg * 2.0f;
        Vector3 mid = Vector3Normalize(Vector3Add(n0, n1));
        b_tri(&b, b0, b1, t1, (Vector2){ u0, 1 }, (Vector2){ u1, 1 },
              (Vector2){ u1, 0 }, n0, n1, n1, mid);
        b_tri(&b, b0, t1, t0, (Vector2){ u0, 1 }, (Vector2){ u1, 0 },
              (Vector2){ u0, 0 }, n0, n1, n0, mid);
        /* caps / 뚜껑 */
        Vector3 up = { 0, 1, 0 }, dn = { 0, -1, 0 };
        b_flat_tri(&b, (Vector3){ 0, 1, 0 }, t0, t1, (Vector2){ 0.5f, 0.5f },
                   (Vector2){ 0.5f + n0.x * 0.5f, 0.5f + n0.z * 0.5f },
                   (Vector2){ 0.5f + n1.x * 0.5f, 0.5f + n1.z * 0.5f }, up);
        b_flat_tri(&b, (Vector3){ 0, 0, 0 }, b1, b0, (Vector2){ 0.5f, 0.5f },
                   (Vector2){ 0.5f + n1.x * 0.5f, 0.5f - n1.z * 0.5f },
                   (Vector2){ 0.5f + n0.x * 0.5f, 0.5f - n0.z * 0.5f }, dn);
    }
    return b_upload(&b);
}

static Mesh gen_sphere(int rings, int seg)
{
    Builder b = { 0 };
    for (int r = 0; r < rings; r++) {
        float v0 = (float)r / rings, v1 = (float)(r + 1) / rings;
        float p0 = PI * v0, p1 = PI * v1;
        for (int s = 0; s < seg; s++) {
            float u0 = (float)s / seg, u1 = (float)(s + 1) / seg;
            float a0 = 2 * PI * u0, a1 = 2 * PI * u1;
#define SP(p, a) ((Vector3){ sinf(p) * sinf(a), cosf(p), sinf(p) * cosf(a) })
            Vector3 q00 = SP(p0, a0), q01 = SP(p0, a1);
            Vector3 q10 = SP(p1, a0), q11 = SP(p1, a1);
#undef SP
            Vector3 mid = Vector3Normalize(Vector3Add(Vector3Add(q00, q11),
                                                      Vector3Add(q01, q10)));
            if (r > 0)
                b_tri(&b, q00, q10, q11, (Vector2){ u0, v0 }, (Vector2){ u0, v1 },
                      (Vector2){ u1, v1 }, q00, q10, q11, mid);
            if (r < rings - 1)
                b_tri(&b, q00, q11, q01, (Vector2){ u0, v0 }, (Vector2){ u1, v1 },
                      (Vector2){ u1, v0 }, q00, q11, q01, mid);
        }
    }
    return b_upload(&b);
}

static Mesh gen_quad(bool floor)
{
    Builder b = { 0 };
    if (floor) {
        b_flat_quad(&b, (Vector3){ -0.5f, 0, 0.5f }, (Vector3){ 0.5f, 0, 0.5f },
                    (Vector3){ 0.5f, 0, -0.5f }, (Vector3){ -0.5f, 0, -0.5f },
                    (Vector2){ 0, 1 }, (Vector2){ 1, 1 }, (Vector2){ 1, 0 },
                    (Vector2){ 0, 0 }, (Vector3){ 0, 1, 0 });
    } else {
        b_flat_quad(&b, (Vector3){ -0.5f, -0.5f, 0 }, (Vector3){ 0.5f, -0.5f, 0 },
                    (Vector3){ 0.5f, 0.5f, 0 }, (Vector3){ -0.5f, 0.5f, 0 },
                    (Vector2){ 0, 1 }, (Vector2){ 1, 1 }, (Vector2){ 1, 0 },
                    (Vector2){ 0, 0 }, (Vector3){ 0, 0, 1 });
    }
    return b_upload(&b);
}

/* ---- set-up / 준비 ------------------------------------------------------ */

static RenderTexture2D load_shadow_target(int size)
{
    RenderTexture2D t = { 0 };
    t.id = rlLoadFramebuffer();
    if (t.id == 0) return t;
    rlEnableFramebuffer(t.id);
    /* A colour target too: some drivers want one for a complete FBO.
     * 색 타깃도 붙인다: 어떤 드라이버는 완전한 FBO에 이것이 필요하다. */
    t.texture.id = rlLoadTexture(NULL, size, size,
                                 PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    t.texture.width = t.texture.height = size;
    t.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    t.texture.mipmaps = 1;
    t.depth.id = rlLoadTextureDepth(size, size, false);
    t.depth.width = t.depth.height = size;
    t.depth.format = 19;            /* DEPTH_COMPONENT_24BIT */
    t.depth.mipmaps = 1;
    rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    bool ok = rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    if (!ok) {
        rlUnloadFramebuffer(t.id);
        rlUnloadTexture(t.texture.id);
        rlUnloadTexture(t.depth.id);
        memset(&t, 0, sizeof t);
    }
    return t;
}

static int loc(const char *name) { return GetShaderLocation(P.shader, name); }

bool pbr_init(int shadow_size)
{
    if (P.ready) return P.active;
    memset(&P, 0, sizeof P);
    P.ready = true;
    P.meshes.box = gen_box(0.0f);
    P.meshes.bevel_box = gen_box(0.045f);
    P.meshes.cylinder = gen_cylinder(28);
    P.meshes.sphere = gen_sphere(12, 20);
    P.meshes.quad = gen_quad(false);
    P.meshes.floor_quad = gen_quad(true);
    P.fallback = LoadMaterialDefault();

    int v = rlGetVersion();
    bool gl33 = v == RL_OPENGL_33 || v == RL_OPENGL_43;
    if (gl33) {
        static char fs[8192];
        fs[0] = '\0';
        for (size_t i = 0; i < sizeof PBR_FS / sizeof PBR_FS[0]; i++)
            strncat(fs, PBR_FS[i], sizeof fs - strlen(fs) - 1);
        P.shader = LoadShaderFromMemory(PBR_VS, fs);
        P.depth = LoadShaderFromMemory(DEPTH_VS, DEPTH_FS);
        gl33 = P.shader.id != rlGetShaderIdDefault() &&
               P.depth.id != rlGetShaderIdDefault() && P.shader.id > 0 &&
               P.depth.id > 0;
    }
    if (gl33) {
        P.shadow_size = shadow_size;
        P.shadow = load_shadow_target(shadow_size);
        P.shader.locs[SHADER_LOC_MAP_METALNESS] = loc("texture1");
        P.shader.locs[SHADER_LOC_MAP_NORMAL] = loc("texture2");
        P.loc_view = loc("viewPos");
        P.loc_light_vp = loc("lightVP");
        P.loc_shadow = loc("shadowMap");
        P.loc_emissive = loc("emissive");
        P.loc_sun_dir = loc("sunDir");
        P.loc_sun_color = loc("sunColor");
        P.loc_point_pos = loc("pointPos");
        P.loc_point_color = loc("pointColor");
        P.loc_sky = loc("skyColor");
        P.loc_ground = loc("groundColor");
        P.loc_exposure = loc("exposure");
        P.loc_fog = loc("fogColor");
        P.loc_fog_density = loc("fogDensity");
        P.loc_texel = loc("shadowTexel");
        P.loc_use_shadow = loc("useShadow");
        P.loc_uv_scale = loc("uvScale");
        P.loc_uv_offset = loc("uvOffset");
        P.mat.shader = P.shader;
        P.mat.maps = P.maps;
        P.active = true;
    } else {
        if (P.shader.id > 0 && P.shader.id != rlGetShaderIdDefault())
            UnloadShader(P.shader);
        if (P.depth.id > 0 && P.depth.id != rlGetShaderIdDefault())
            UnloadShader(P.depth);
        memset(&P.shader, 0, sizeof P.shader);
        memset(&P.depth, 0, sizeof P.depth);
    }
    P.sun_dir = Vector3Normalize((Vector3){ -0.35f, -1.0f, -0.45f });
    P.sun_color = (Vector3){ 3.0f, 2.9f, 2.7f };
    P.sky = (Vector3){ 0.35f, 0.40f, 0.48f };
    P.ground = (Vector3){ 0.16f, 0.14f, 0.12f };
    P.exposure = 1.0f;
    return P.active;
}

void pbr_shutdown(void)
{
    if (!P.ready) return;
    UnloadMesh(P.meshes.box);
    UnloadMesh(P.meshes.bevel_box);
    UnloadMesh(P.meshes.cylinder);
    UnloadMesh(P.meshes.sphere);
    UnloadMesh(P.meshes.quad);
    UnloadMesh(P.meshes.floor_quad);
    if (P.active) {
        UnloadShader(P.shader);
        UnloadShader(P.depth);
        if (P.shadow.id) {
            rlUnloadFramebuffer(P.shadow.id);
            rlUnloadTexture(P.shadow.texture.id);
            rlUnloadTexture(P.shadow.depth.id);
        }
    }
    /* only the maps array is ours; the default shader stays
       maps 배열만 우리 것이다. 기본 셰이더는 그대로 둔다 */
    MemFree(P.fallback.maps);
    memset(&P, 0, sizeof P);
}

/* ---- lights / 조명 ------------------------------------------------------ */

void pbr_set_sun(Vector3 dir, Vector3 color)
{
    P.sun_dir = Vector3Normalize(dir);
    P.sun_color = color;
}

void pbr_set_point(int i, Vector3 pos, Vector3 color)
{
    if (i < 0 || i >= PBR_MAX_LIGHTS) return;
    P.point_pos[i] = pos;
    P.point_color[i] = color;
}

void pbr_set_ambient(Vector3 sky, Vector3 ground, float exposure)
{
    P.sky = sky;
    P.ground = ground;
    P.exposure = exposure;
}

void pbr_set_fog(Vector3 color, float density)
{
    P.fog = color;
    P.fog_density = density;
}

/* ---- passes / 단계 ------------------------------------------------------ */

void pbr_begin_shadow(Vector3 center, float radius)
{
    if (!P.active || P.shadow.id == 0) return;
    Camera3D lc = { 0 };
    lc.target = center;
    lc.position = Vector3Subtract(center, Vector3Scale(P.sun_dir, radius * 2.0f));
    lc.up = (Vector3){ 0, 0, -1 };
    if (fabsf(P.sun_dir.y) < 0.9f) lc.up = (Vector3){ 0, 1, 0 };
    lc.fovy = radius * 2.0f;
    lc.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(P.shadow);
    ClearBackground(WHITE);
    /* a tight depth range keeps the shadow bias small and exact
       좁은 깊이 범위가 그림자 바이어스를 작고 정확하게 한다 */
    P.saved_near = rlGetCullDistanceNear();
    P.saved_far = rlGetCullDistanceFar();
    rlSetClipPlanes(0.5, radius * 4.0);
    BeginMode3D(lc);
    P.light_vp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    P.shadow_pass = true;
}

void pbr_end_shadow(void)
{
    if (!P.shadow_pass) return;
    EndMode3D();
    EndTextureMode();
    rlSetClipPlanes(P.saved_near, P.saved_far);
    P.shadow_pass = false;
}

void pbr_begin_scene(Camera3D cam)
{
    if (!P.active) return;
    Shader s = P.shader;
    SetShaderValue(s, P.loc_view, &cam.position, SHADER_UNIFORM_VEC3);
    SetShaderValueMatrix(s, P.loc_light_vp, P.light_vp);
    SetShaderValue(s, P.loc_sun_dir, &P.sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, P.loc_sun_color, &P.sun_color, SHADER_UNIFORM_VEC3);
    SetShaderValueV(s, P.loc_point_pos, P.point_pos, SHADER_UNIFORM_VEC3,
                    PBR_MAX_LIGHTS);
    SetShaderValueV(s, P.loc_point_color, P.point_color, SHADER_UNIFORM_VEC3,
                    PBR_MAX_LIGHTS);
    SetShaderValue(s, P.loc_sky, &P.sky, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, P.loc_ground, &P.ground, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, P.loc_exposure, &P.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, P.loc_fog, &P.fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, P.loc_fog_density, &P.fog_density, SHADER_UNIFORM_FLOAT);
    float texel = P.shadow_size > 0 ? 1.0f / (float)P.shadow_size : 0.0f;
    SetShaderValue(s, P.loc_texel, &texel, SHADER_UNIFORM_FLOAT);
    int use = P.shadow.id != 0;
    SetShaderValue(s, P.loc_use_shadow, &use, SHADER_UNIFORM_INT);
    if (use) {
        int slot = 10;          /* above the material maps / 재질 맵 위 */
        rlActiveTextureSlot(slot);
        rlEnableTexture(P.shadow.depth.id);
        rlActiveTextureSlot(0);
        SetShaderValue(s, P.loc_shadow, &slot, SHADER_UNIFORM_INT);
    }
}

void pbr_end_scene(void)
{
    if (!P.active || P.shadow.id == 0) return;
    rlActiveTextureSlot(10);
    rlDisableTexture();
    rlActiveTextureSlot(0);
}

/* ---- drawing / 그리기 --------------------------------------------------- */

void pbr_draw(const Mesh *mesh, const PbrDraw *d, Matrix transform)
{
    if (P.shadow_pass) {
        if (d->no_shadow) return;
        Material m = P.fallback;
        m.shader = P.depth;
        DrawMesh(*mesh, m, transform);
        return;
    }
    if (!P.active) {
        Material m = P.fallback;
        m.maps[MATERIAL_MAP_ALBEDO].texture = d->tex->albedo;
        Color c = d->tint;
        /* a little glow even without shaders / 셰이더 없이도 약간의 발광 */
        float e = fmaxf(d->emissive.x, fmaxf(d->emissive.y, d->emissive.z));
        if (e > 0.0f) {
            float k = fminf(1.0f, e * 0.5f);
            c.r = (unsigned char)(c.r + (255 - c.r) * k);
            c.g = (unsigned char)(c.g + (255 - c.g) * k);
            c.b = (unsigned char)(c.b + (230 - c.b > 0 ? (230 - c.b) * k : 0));
        }
        m.maps[MATERIAL_MAP_ALBEDO].color = c;
        DrawMesh(*mesh, m, transform);
        return;
    }
    P.maps[MATERIAL_MAP_ALBEDO].texture = d->tex->albedo;
    P.maps[MATERIAL_MAP_ALBEDO].color = d->tint;
    P.maps[MATERIAL_MAP_METALNESS].texture = d->tex->mra;
    P.maps[MATERIAL_MAP_NORMAL].texture = d->tex->normal;
    Vector2 scale = d->uv_scale;
    if (scale.x == 0.0f && scale.y == 0.0f) scale = (Vector2){ 1, 1 };
    SetShaderValue(P.shader, P.loc_uv_scale, &scale, SHADER_UNIFORM_VEC2);
    SetShaderValue(P.shader, P.loc_uv_offset, &d->uv_offset,
                   SHADER_UNIFORM_VEC2);
    SetShaderValue(P.shader, P.loc_emissive, &d->emissive, SHADER_UNIFORM_VEC3);
    DrawMesh(*mesh, P.mat, transform);
}

static Matrix trs(Vector3 c, Vector3 s, float yaw)
{
    Matrix m = MatrixScale(s.x, s.y, s.z);
    if (yaw != 0.0f) m = MatrixMultiply(m, MatrixRotateY(yaw));
    return MatrixMultiply(m, MatrixTranslate(c.x, c.y, c.z));
}

void pbr_box(const PbrDraw *d, Vector3 c, Vector3 s, float yaw)
{
    pbr_draw(&P.meshes.box, d, trs(c, s, yaw));
}

void pbr_bevel_box(const PbrDraw *d, Vector3 c, Vector3 s, float yaw)
{
    pbr_draw(&P.meshes.bevel_box, d, trs(c, s, yaw));
}

void pbr_cylinder(const PbrDraw *d, Vector3 base, float radius, float height)
{
    pbr_draw(&P.meshes.cylinder, d,
             trs(base, (Vector3){ radius, height, radius }, 0.0f));
}

void pbr_sphere(const PbrDraw *d, Vector3 c, float radius)
{
    pbr_draw(&P.meshes.sphere, d,
             trs(c, (Vector3){ radius, radius, radius }, 0.0f));
}
