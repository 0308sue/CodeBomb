/*
 * screen_pointers.h - Sumin's pointer swap lesson at 1920 x 1200
 * screen_pointers.h - 1920 x 1200으로 옮긴 수민의 포인터 교환 수업
 *
 * The lesson logic is pointer_SOO.c, used unchanged.
 * 수업 로직은 pointer_SOO.c를 수정 없이 사용한다.
 */
#ifndef SCREEN_POINTERS_H
#define SCREEN_POINTERS_H

#include <stdbool.h>

typedef struct {
    int   choice, step;
    bool  playing;
    float elapsed;
} PointersScreen;

void pointers_init(PointersScreen *v);
void pointers_update(PointersScreen *v, float dt);
void pointers_draw(PointersScreen *v);
void pointers_leave(PointersScreen *v);

/* Screenshot scripting: answer choice, then go to step.
 * 스크린샷용: 답을 고르고 해당 단계로 이동. */
void pointers_qa(PointersScreen *v, int choice, int step);

#endif
