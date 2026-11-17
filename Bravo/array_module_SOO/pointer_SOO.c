#include "pointer_SOO.h"

/* 한글: 처음부터 요청한 줄까지 다시 실행한다. 따라서 Back도 같은 결과를 낸다.
 * English: Replay from the starting values so Back restores the exact earlier state. */
SooSwapFrame SooSwapAt(int step)
{
    SooSwapFrame f = {3, 7, 0};
    /* 한글: &는 주소를 얻고, *는 그 주소에 있는 값을 읽거나 쓴다.
     * English: & takes an address; * reads or writes the value at that address. */
    int *p = &f.a;
    int *q = &f.b;
    if (step >= 2) f.temp = *p;
    if (step >= 3) *p = *q;
    if (step >= 4) *q = f.temp;
    return f;
}

/* 한글: 보기 번호는 0부터 시작한다. B는 두 값이 바뀐 결과다.
 * English: Choice indices start at zero. B represents the swapped values. */
bool SooSwapCorrect(int choice)
{
    return choice == 1;
}
