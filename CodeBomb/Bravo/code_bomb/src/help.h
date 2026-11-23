/*
 * help.h - the field manual: short C explanations before each mission
 * help.h - 현장 매뉴얼: 각 미션 전에 보는 짧은 C 설명
 *
 * Like the info pages in "The Farmer Was Replaced", every new concept gets
 * a few pages: a plain explanation, a picture of what happens in memory,
 * a small code example and one tip. The manual opens by itself the first
 * time a chapter, the Bomb screen or the Mailroom is entered, and any time
 * from a Help button.
 * "The Farmer Was Replaced"의 정보 페이지처럼, 새 개념마다 몇 쪽이 있다:
 * 쉬운 설명, 메모리에서 일어나는 일의 그림, 짧은 코드 예제, 팁 하나.
 * 매뉴얼은 챕터, Bomb 화면, Mailroom에 처음 들어갈 때 저절로 열리고,
 * Help 버튼으로 언제든 열 수 있다.
 *
 * The manual is a pop-up: while it is open the screen underneath is drawn
 * but gets no input (see ui_block_input).
 * 매뉴얼은 팝업이다: 열려 있는 동안 아래 화면은 그려지지만 입력을 받지
 * 않는다 (ui_block_input 참고).
 */
#ifndef HELP_H
#define HELP_H

#include "raylib.h"

#include <stdbool.h>

typedef enum {
    HELP_CHAPTER_1,      /* chapters 1-8 follow Sumin's roadmap order
                            챕터 1-8은 수민의 로드맵 순서 */
    HELP_CHAPTER_2,
    HELP_CHAPTER_3,
    HELP_CHAPTER_4,
    HELP_CHAPTER_5,
    HELP_CHAPTER_6,
    HELP_CHAPTER_7,
    HELP_CHAPTER_8,
    HELP_BOMB,           /* writing code for bombs, the cube challenge */
    HELP_MAILROOM,
    HELP_TOPIC_COUNT
} HelpTopic;

void help_open(HelpTopic topic);
void help_open_once(HelpTopic topic);   /* only the first time / 처음만 */
bool help_active(void);

/* Keyboard (arrows, Esc, Enter). Call each frame while active.
 * 키보드 (방향키, Esc, Enter). 열려 있으면 매 프레임 호출. */
void help_update(void);

/* Draw the pop-up over the canvas and handle its buttons. Call last.
 * 캔버스 위에 팝업을 그리고 버튼을 처리한다. 마지막에 호출. */
void help_draw(void);

/* A "? Help" button that opens topic. / topic을 여는 "? Help" 버튼. */
bool help_button(Rectangle r, HelpTopic topic);

/* Chapter of a map level 1-40. / 지도 레벨 1-40의 챕터. */
HelpTopic help_topic_for_level(int level);

/* Chapter colour shared with the mission map. / 미션 지도와 같은 챕터 색. */
Color help_chapter_color(int chapter);

/* Screenshots and self-test. / 스크린샷과 자체 검사. */
int  help_page_count(HelpTopic topic);
void help_set_page(int page);

#endif
