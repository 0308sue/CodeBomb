#ifndef POINTER_SOO_H
#define POINTER_SOO_H

#include <stdbool.h>

/* 한글: 각 단계는 값만 저장한다. 실제 주소는 프레임 사이에 보관하지 않는다.
 * English: Store values for each step; never keep local addresses between frames. */
enum { SOO_SWAP_LAST = 4 };
typedef struct { int a, b, temp; } SooSwapFrame;
SooSwapFrame SooSwapAt(int step);
bool SooSwapCorrect(int choice);

#endif
