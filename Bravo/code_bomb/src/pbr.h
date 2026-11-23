/*
 * pbr.h - a small physically based renderer for the 3D scenes
 * pbr.h - 3D 장면을 위한 작은 물리 기반 렌더러 (PBR)
 *
 * Materials use the metallic-roughness model: an albedo texture, an "MRA"
 * texture (red = metallic, green = roughness, blue = ambient occlusion)
 * and a tangent-space normal map. Every texture is generated in code at
 * start-up (pbr_textures.c), so the game ships no image files.
 * 재질은 metallic-roughness 모델을 쓴다: 알베도 텍스처, "MRA" 텍스처
 * (빨강 = 금속성, 초록 = 거칠기, 파랑 = 주변광 차폐), 접선 공간 노멀 맵.
 * 모든 텍스처는 시작할 때 코드로 만든다 (pbr_textures.c). 그래서 게임에
 * 이미지 파일이 필요 없다.
 *
 * Lighting: one shadow-casting key light (the skylights), up to four warm
 * point lights, a sky/floor ambient and a simple reflected environment for
 * metals, with ACES tone mapping. The scene is drawn twice: once into the
 * shadow map (pbr_begin_shadow) and once for the camera (pbr_begin_scene).
 * 조명: 그림자를 만드는 주 조명 하나 (천창), 따뜻한 점광원 최대 네 개,
 * 하늘/바닥 주변광, 금속을 위한 간단한 반사 환경, ACES 톤 매핑. 장면은 두 번
 * 그린다: 그림자 맵에 한 번 (pbr_begin_shadow), 카메라용으로 한 번
 * (pbr_begin_scene).
 *
 * Fallback: without OpenGL 3.3 shaders (for example raylib's software
 * renderer) the same calls draw the albedo textures unlit, so the scene
 * still works everywhere.
 * 대체 경로: OpenGL 3.3 셰이더가 없으면 (예: raylib 소프트웨어 렌더러) 같은
 * 호출이 조명 없이 알베도 텍스처만 그린다. 그래서 장면은 어디서나 동작한다.
 */
#ifndef PBR_H
#define PBR_H

#include "raylib.h"

#include <stdbool.h>

#define PBR_MAX_LIGHTS 4

typedef struct {
    Texture2D albedo;   /* sRGB colour, alpha for decals / sRGB 색, 데칼은 알파 */
    Texture2D mra;      /* R metallic, G roughness, B occlusion
                           R 금속성, G 거칠기, B 차폐 */
    Texture2D normal;   /* tangent space / 접선 공간 */
} PbrTextures;

/* Per-draw settings. / 그릴 때마다의 설정. */
typedef struct {
    const PbrTextures *tex;
    Color   tint;           /* multiplies the albedo / 알베도에 곱함 */
    Vector3 emissive;       /* linear light added on top (glow)
                               위에 더하는 선형 빛 (발광) */
    Vector2 uv_scale;       /* 0,0 means 1,1 / 0,0이면 1,1 */
    Vector2 uv_offset;      /* scrolling belts / 움직이는 벨트 */
    bool    no_shadow;      /* skip in the shadow pass / 그림자 단계에서 제외 */
} PbrDraw;

/* Unit meshes made in code, with tangents. / 코드로 만든 단위 메시 (접선 포함) */
typedef struct {
    Mesh box;          /* 1 x 1 x 1, centred                      */
    Mesh bevel_box;    /* the same with chamfered edges / 모서리 깎음 */
    Mesh cylinder;     /* radius 1, height 1, base at y = 0      */
    Mesh sphere;       /* radius 1                               */
    Mesh quad;         /* 1 x 1 facing +z (labels, signs)        */
    Mesh floor_quad;   /* 1 x 1 facing +y                        */
} PbrMeshes;

bool pbr_init(int shadow_size);     /* true: real PBR, false: fallback
                                       true: 실제 PBR, false: 대체 경로 */
void pbr_shutdown(void);
bool pbr_active(void);
const PbrMeshes *pbr_meshes(void);

/* Lights for the next frame. / 다음 프레임의 조명. */
void pbr_set_sun(Vector3 dir, Vector3 color);           /* dir points down
                                                           아래를 향하는 방향 */
void pbr_set_point(int i, Vector3 pos, Vector3 color);  /* color 0 = off */
void pbr_set_ambient(Vector3 sky, Vector3 ground, float exposure);
void pbr_set_fog(Vector3 color, float density);

/* Shadow pass: draw every caster between begin and end.
 * 그림자 단계: begin과 end 사이에 그림자를 만드는 물체를 모두 그린다. */
void pbr_begin_shadow(Vector3 center, float radius);
void pbr_end_shadow(void);

/* Camera pass: call inside BeginMode3D(cam). / 카메라 단계: BeginMode3D 안. */
void pbr_begin_scene(Camera3D cam);
void pbr_end_scene(void);
bool pbr_in_shadow_pass(void);

void pbr_draw(const Mesh *mesh, const PbrDraw *d, Matrix transform);

/* Helpers: box centred at c with size s, turned by yaw (radians) about y.
 * 도우미: 중심 c, 크기 s, y축으로 yaw(라디안)만큼 돌린 상자. */
void pbr_box(const PbrDraw *d, Vector3 c, Vector3 s, float yaw);
void pbr_bevel_box(const PbrDraw *d, Vector3 c, Vector3 s, float yaw);
void pbr_cylinder(const PbrDraw *d, Vector3 base, float radius, float height);
void pbr_sphere(const PbrDraw *d, Vector3 c, float radius);

/* Procedural material sets (pbr_textures.c). / 절차적 재질 (pbr_textures.c) */
typedef enum {
    MAT_CONCRETE, MAT_CARDBOARD, MAT_CARDBOARD_DAMAGED, MAT_PAINT_YELLOW,
    MAT_PAINT_GREEN, MAT_PAINT_RED, MAT_PAINT_BLUE, MAT_STEEL, MAT_DARK_STEEL,
    MAT_RUBBER_BELT, MAT_RUBBER, MAT_HAZARD, MAT_WALL, MAT_WOOD, MAT_PLASTIC,
    MAT_PAPER, MAT_TAPE, MAT_GLASS_DARK, MAT_COUNT
} PbrMaterialId;

void pbr_textures_make(PbrTextures out[MAT_COUNT], int size);
void pbr_textures_free(PbrTextures t[MAT_COUNT]);

/* A decal: alpha texture with flat paint MRA/normal (signs, stencils).
 * 데칼: 알파 텍스처 + 평평한 도료 MRA/노멀 (표지판, 바닥 글자). */
PbrTextures pbr_decal_textures(Image alpha_art, const PbrTextures *paint);
void pbr_decal_free(PbrTextures *t);

#endif
