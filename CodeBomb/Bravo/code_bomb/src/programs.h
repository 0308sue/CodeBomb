/*
 * programs.h - example programs for level 1
 * programs.h - 레벨 1용 예제 프로그램
 *
 * Each example has C source (loaded into the editor) and a hand-written
 * bytecode builder. The tests check that the compiler produces exactly the
 * same bytecode as the hand-written version.
 * 각 예제는 C 소스(에디터에 불러옴)와 손으로 작성한 바이트코드 빌더를
 * 갖는다. 테스트는 컴파일러가 손으로 쓴 것과 똑같은 바이트코드를 만드는지
 * 확인한다.
 */
#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "level.h"
#include "vm.h"

typedef struct {
    const char *name;        /* button label / 버튼 이름 */
    const char *source;      /* C source / C 소스 */
    int         statements;  /* expected size score / 기대 크기 점수 */
    void (*build)(Program *p);
    const char *about;       /* one line for the Examples menu
                                Examples 메뉴에 보일 한 줄 설명 */
} DemoProgram;

int                demo_count(void);
const DemoProgram *demo_get(int index);

/* Build the hand-written bytecode (natives registered first).
 * 손으로 작성한 바이트코드를 만든다 (네이티브를 먼저 등록). */
bool demo_build(const DemoProgram *demo, const LevelDef *level, Program *out);

#endif
