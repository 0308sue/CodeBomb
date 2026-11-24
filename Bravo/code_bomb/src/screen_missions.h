/*
 * screen_missions.h - the mission map: the game's hub
 * screen_missions.h - 미션 지도: 게임의 중심 화면
 *
 * Sumin's eight chapters and 40 lessons (from her roadmap_SOO.h), plus a
 * Bomb defusal row where the player writes real C (Mailroom tutorial,
 * bombs 1-6 and the Cube bomb).
 * 수민의 8개 챕터와 40개 수업(roadmap_SOO.h), 그리고 플레이어가 실제 C를
 * 쓰는 폭탄 해체 줄 (Mailroom 튜토리얼, 폭탄 1-6, 큐브 폭탄).
 */
#ifndef SCREEN_MISSIONS_H
#define SCREEN_MISSIONS_H

#include <stdbool.h>

#define MISSION_BOMBS 7          /* bombs 1-6 and the cube / 폭탄 1-6과 큐브 */

typedef enum {
    PICK_NONE,
    PICK_LESSON,                 /* level = map level 1-40     */
    PICK_BOMB,                   /* level = bomb 0-5, 6 = cube */
    PICK_MAILROOM
} MissionPickKind;

typedef struct {
    MissionPickKind kind;
    int             level;
} MissionPick;

/* Draw the map; returns what the player clicked. bomb_done marks
 * defused bombs. / 지도를 그리고 클릭한 것을 돌려준다. */
/* mail_solved / mail_total: Mailroom progress on the first button.
 * mail_solved / mail_total: 첫 버튼에 보이는 우편실 진행 상황. */
MissionPick missions_draw(const bool bomb_done[MISSION_BOMBS], int mail_solved,
                          int mail_total);

#endif
