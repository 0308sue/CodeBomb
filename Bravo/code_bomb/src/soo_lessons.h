/*
 * soo_lessons.h - Sumin's 40 mission lessons running inside Code Bomb
 * soo_lessons.h - Code Bomb 안에서 실행되는 수민의 40개 미션 수업
 *
 * Sumin's lesson screens (array_module_SOO/stage*_view_SOO.h and the views
 * in main_SOO.c) are compiled unchanged. soo_lessons.c redirects their few
 * drawing calls through a scaler, so her 1280 x 800 layout is drawn at
 * exactly 1.5x on our 1920 x 1200 canvas, with every font size rounded to
 * 20, 30 or 40 (multiples of 10, so the pixel font stays sharp).
 * When Sumin updates her lessons, the game picks up the changes as they are.
 *
 * 수민의 수업 화면(array_module_SOO/stage*_view_SOO.h와 main_SOO.c의 화면)을
 * 수정 없이 컴파일한다. soo_lessons.c는 그 몇 가지 그리기 호출을 배율 변환기로
 * 보내서 수민의 1280 x 800 배치를 우리 1920 x 1200 화면에 정확히 1.5배로
 * 그리고, 모든 글자 크기를 20, 30, 40(10의 배수, 픽셀 폰트가 선명함)으로
 * 맞춘다. 수민이 수업을 고치면 게임에 그대로 반영된다.
 *
 * Levels and routes come from her roadmap_SOO.h (SOO_CHAPTERS, SOO_ROUTES).
 * 레벨과 경로는 수민의 roadmap_SOO.h(SOO_CHAPTERS, SOO_ROUTES)에서 온다.
 */
#ifndef SOO_LESSONS_H
#define SOO_LESSONS_H

#include <stdbool.h>

/* What the lesson asked the game to do after drawing.
 * 그린 뒤 수업이 게임에 요청한 동작. */
typedef enum {
    LESSON_NONE,
    LESSON_OPEN_BUG     /* Sumin's "Find the bug" button on Stage 1
                           Stage 1의 수민의 "Find the bug" 버튼 */
} LessonAction;

/* Open map level 1-40 (uses her SOO_ROUTES). Returns false if not ready.
 * 지도 레벨 1-40을 연다 (수민의 SOO_ROUTES 사용). 준비 안 됐으면 false. */
bool         soo_lessons_open(int level);
int          soo_lessons_level(void);           /* open level, 0 = none */
void         soo_lessons_update(float dt);      /* her keys and timers */
LessonAction soo_lessons_draw(void);            /* draws the whole canvas */
void         soo_lessons_leave(void);           /* stop playback */

/* Screenshots and self-test: open a level, pick the correct answer and
 * jump to the last step (like her --stageN-smoke options).
 * 스크린샷과 자체 검사: 레벨을 열고 정답을 고른 뒤 마지막 단계로 이동
 * (수민의 --stageN-smoke 옵션과 같음). */
bool         soo_lessons_qa(int level);

#endif
