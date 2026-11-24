/*
 * scene.h - interface every level's visual scene implements
 * scene.h - 모든 레벨의 시각 장면이 구현하는 인터페이스
 *
 * The host forwards LevelEvents (take, put, ...) to the scene, waits while
 * busy() is true (the animation is playing), then resumes the VM.
 * draw() renders into the currently bound render target, so scenes never
 * need to know the screen layout.
 * 호스트는 LevelEvent(take, put 등)를 장면에 전달하고, busy()가 true인 동안
 * (애니메이션 재생 중) 기다린 뒤 VM을 재개한다. draw()는 현재 연결된 렌더
 * 타깃에 그리므로 장면은 화면 배치를 알 필요가 없다.
 */
#ifndef SCENE_H
#define SCENE_H

#include "level.h"

typedef struct Scene {
    void *state;
    void (*reset)(void *state, const LevelRun *run);
    void (*on_event)(void *state, LevelEvent ev);
    void (*update)(void *state, float dt);   /* dt already speed-scaled
                                                dt는 이미 속도 배율이 적용됨 */
    bool (*busy)(const void *state);
    /* Optional: offscreen passes (shadows, supersampling) that must run
     * outside any BeginTextureMode. Called just before draw().
     * 선택: BeginTextureMode 밖에서 해야 하는 화면 밖 단계 (그림자,
     * 슈퍼샘플링). draw() 바로 전에 호출된다. */
    void (*prerender)(void *state, int width, int height);
    void (*draw)(void *state, int width, int height);
    void (*destroy)(void *state);
} Scene;

Scene scene_mailroom_create(void);

#endif
