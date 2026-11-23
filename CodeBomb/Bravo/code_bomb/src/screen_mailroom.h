/*
 * screen_mailroom.h - level 1 "Mailroom": code editor + 3D scene + VM
 * screen_mailroom.h - 레벨 1 "우편실": 코드 에디터 + 3D 장면 + VM
 */
#ifndef SCREEN_MAILROOM_H
#define SCREEN_MAILROOM_H

#include "editor_ui.h"

#include <stdbool.h>

typedef struct MailroomScreen MailroomScreen;

MailroomScreen *mailroom_create(const EditorStyle *style);
void            mailroom_destroy(MailroomScreen *m);
void            mailroom_update(MailroomScreen *m, float dt);

/* Draw the 3D scene into its own render texture. Call outside any other
 * BeginTextureMode (render textures can't be nested).
 * 3D 장면을 자체 렌더 텍스처에 그린다. 다른 BeginTextureMode 밖에서
 * 호출해야 한다 (렌더 텍스처는 중첩할 수 없다). */
void mailroom_render_view(MailroomScreen *m);
void mailroom_draw(MailroomScreen *m);
bool mailroom_typing(const MailroomScreen *m);
void mailroom_leave(MailroomScreen *m);

/* Screenshots: load example `demo` (0..3) and run `steps` instructions.
 * 스크린샷용: 예제 demo(0..3)를 불러오고 steps개 명령을 실행. */
void mailroom_qa(MailroomScreen *m, int demo, int steps);

#endif
