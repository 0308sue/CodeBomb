#include "array_core_SOO.h"
#include "pointer_SOO.h"
#include <assert.h>
#include <stdio.h>
/* 한글: 창 없이 실제 로직을 실행하여 검증한다.
 * English: Exercise actual engine logic without a graphics window. */
static SooState Run(int level,SooScenario scenario)
{
    SooEvent events[SOO_MAX_EVENTS]; SooState s;
    SooReset(&s,&SOO_LEVELS[level]);
    int count=SooBuildSample(s.level,scenario,events,SOO_MAX_EVENTS);
    assert(count>0);
    for(int k=0;k<count && s.status<SOO_SUCCESS;k++) SooApplyEvent(&s,events[k]);
    return s;
}
int main(void)
{
    for(int level=0;level<SOO_LEVEL_COUNT;level++) {
        SooState good=Run(level,SOO_CORRECT); assert(good.status==SOO_SUCCESS && SooGoalReached(&good));
        SooState missing=Run(level,SOO_MISSING); assert(missing.status==SOO_FAILURE);
        SooState bounds=Run(level,SOO_BOUNDS); assert(bounds.status==SOO_FAILURE);
    }
    /* 한글: 카드가 모두 고쳐져도 인덱스 8을 읽으려 하면 실패한다.
     * English: Even with all cards repaired, attempting index 8 must fail. */
    SooState original=Run(1,SOO_BOUNDS), fixed=Run(1,SOO_CORRECT);
    assert(original.status==SOO_FAILURE && original.i==8 && SooGoalReached(&original));
    assert(fixed.status==SOO_SUCCESS && SooGoalReached(&fixed));
    assert(original.operations==13 && fixed.operations==13);
    SooState slow=Run(5,SOO_SLOW); assert(slow.status==SOO_FAILURE && slow.operations==20);
    SooState fast=Run(5,SOO_CORRECT); assert(fast.operations==16);
    SooState s; SooReset(&s,&SOO_LEVELS[0]);
    /* 한글: 실패 후 변경 차단과 예산 초과 시 쓰기 차단을 확인한다.
     * English: Ensure failure latches and over-budget writes cannot change memory. */
    assert(!SooApplyEvent(&s,(SooEvent){SOO_WRITE,0,-1,0,0,0}));
    assert(!SooApplyEvent(&s,(SooEvent){SOO_WRITE,0,2,0,0,0})); assert(s.cells[2]==1);
    SooReset(&s,&SOO_LEVELS[0]); s.operations=s.level->budget;
    assert(!SooApplyEvent(&s,(SooEvent){SOO_WRITE,0,2,0,0,0})); assert(s.cells[2]==1);
    SooReset(&s,&SOO_LEVELS[3]);
    assert(!SooApplyEvent(&s,(SooEvent){SOO_READ,3,0,0,0,0}));
    SooReset(&s,&SOO_LEVELS[0]); assert(!SooApplyEvent(&s,(SooEvent){SOO_END,0,0,0,0,0}));
    SooEvent tiny[1]; assert(SooBuildSample(&SOO_LEVELS[3],SOO_CORRECT,tiny,1)==-1);
    assert(SooBuildSample(&SOO_LEVELS[0],SOO_CORRECT,NULL,0)==-1);
    /* 한글: 임시값 보관, 중간 상태, 최종 교환을 각각 확인한다.
     * English: Check the saved value, intermediate state and final swap. */
    SooSwapFrame f=SooSwapAt(1); assert(f.a==3 && f.b==7);
    f=SooSwapAt(2); assert(f.a==3 && f.b==7 && f.temp==3);
    f=SooSwapAt(3); assert(f.a==7 && f.b==7 && f.temp==3);
    f=SooSwapAt(4); assert(f.a==7 && f.b==3 && f.temp==3);
    f=SooSwapAt(2); assert(f.a==3 && f.b==7 && f.temp==3);
    assert(!SooSwapCorrect(0) && SooSwapCorrect(1) && !SooSwapCorrect(2));
    puts("PASS: six levels, incomplete goals, bounds, operation budget and buffer safety.");
    return 0;
}
