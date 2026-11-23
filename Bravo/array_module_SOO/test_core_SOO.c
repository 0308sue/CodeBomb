#define SOO_STAGE1_HEADLESS
#include "stage1_SOO.h"
#include "stage2_SOO.h"
#include "stage3_SOO.h"
#include "stage4_SOO.h"
#include "stage5_SOO.h"
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
    /* 한글: 여섯 미션의 최종 결과와 조작을 확인한다.
     * English: Verify all six outcomes and the initial playback lock. */
    const int writes[]={5,0,2,4,4,9};
    for(int level=0;level<6;level++) {
        IntroView v;
        IntroLoad(&v,level);
        IntroNext(&v);
        assert(v.step==0);
        IntroChoose(&v,SOO_INTRO[level].answer);
        while(v.step<v.count) IntroNext(&v);
        IntroState state=IntroAt(&v);
        assert(state.done && state.passed && state.writes==writes[level]);
        if(level==1) assert(state.returned && state.result==2);
        if(level==3) assert(state.cells[2]==8 && state.cells[6]==8);
        if(level==4) assert(state.cells[0]==5 && state.cells[2]==3 && state.cells[4]==1);
    }
    /* 한글: 마지막 결과와 중간 포인터 위치 및 누적값을 확인한다.
     * English: Check final results, intermediate pointer targets and running totals. */
    const int expected[][5]={{20,10},{'A','B'},{3,9,2,8,5},{5,4,3,2,1},{2,4,6,8}};
    for(int level=0;level<5;level++) {
        PointerLesson v;
        PointerLessonLoad(&v,level);
        PointerFrame final=v.frames[v.count-1];
        assert(memcmp(final.cells,expected[level],sizeof final.cells)==0);
        if(level==0) assert(v.frames[3].cells[0]==20 && v.frames[3].temp==10);
        if(level==2) assert(v.frames[2].left==1 && v.frames[2].total==9 && final.total==9);
        if(level==3) assert(final.left==2 && final.right==2);
        if(level==4) assert(v.frames[3].total==12 && final.total==20);
    }
    /* 한글: 순서, 유효 길이, 중복 최대값과 배터리 선택 제한을 검증한다.
     * English: Check ordering, logical length, duplicate maxima and pair selection. */
    ArrayLesson lesson;
    ArrayLessonLoad(&lesson,0);
    ArrayFrame end=lesson.frames[lesson.n-1];
    assert(end.a[end.maxAt]==9 && end.a[end.secondAt]==8);
    ArrayFrame duplicates={.a={9,9,9,8},.maxAt=-1,.secondAt=-1};
    for(int i=0;i<3;i++) ArrayRank(&duplicates,i);
    assert(duplicates.secondAt==-1);
    ArrayRank(&duplicates,3); assert(duplicates.secondAt==3);
    ArrayLessonLoad(&lesson,1); end=lesson.frames[lesson.n-1];
    assert(end.used==5 && memcmp(end.a,(int[]){10,20,99,30,40},5*sizeof(int))==0);
    ArrayLessonLoad(&lesson,2); end=lesson.frames[lesson.n-1];
    assert(end.used==4 && memcmp(end.a,(int[]){10,20,30,40},4*sizeof(int))==0);
    ArrayLessonLoad(&lesson,3); assert(lesson.frames[lesson.n-1].count==3);
    ArrayLessonLoad(&lesson,4); end=lesson.frames[lesson.n-1];
    assert(memcmp(end.a,(int[]){2,3,4,5,1},5*sizeof(int))==0);
    ArrayLessonLoad(&lesson,5);
    ArrayPick(&lesson,0); ArrayPick(&lesson,1); ArrayPick(&lesson,4);
    assert(ArrayPairSum(&lesson)==75);
    ArrayPick(&lesson,1); ArrayPick(&lesson,4);
    assert(ArrayPairSum(&lesson)==120 && lesson.pick[0]!=lesson.pick[1]);
    ArrayPick(&lesson,0); assert(ArrayPairSum(&lesson)==90 && lesson.pick[1]<0);
    /* 한글: 위치별 가로 우선, 4개 연속, 중력 순서와 특수값 제외를 검사한다.
     * English: Test per-position priority, long runs, stable gravity and sentinels. */
    GridLesson g;
    GridLessonLoad(&g,0); assert(g.frames[g.n-1].scanned==24);
    GridLessonLoad(&g,1); assert(g.frames[g.n-1].match==14u);
    GridLessonLoad(&g,2);
    assert(g.frames[g.n-1].score==6 && g.frames[g.n-1].cells[1]==-1);
    GridLessonLoad(&g,3);
    const int *gravity=g.frames[g.n-1].cells;
    assert(gravity[0]==-2 && gravity[6]==-2 && gravity[12]==-2 && gravity[18]==1);
    assert(gravity[1]==-2 && gravity[7]==-2 && gravity[13]==2 && gravity[19]==3);
    GridLessonLoad(&g,4);
    assert(g.frames[g.n-1].score==6 && g.frames[g.n-1].cells[1]==-2);
    for(int k=0;k<24;k++) assert(g.frames[g.n-1].cells[k]!=-1);
    int priority[24]; for(int k=0;k<24;k++) priority[k]=0;
    priority[0]=priority[6]=priority[12]=1;
    priority[1]=priority[2]=priority[3]=2;
    GridMatch gm=GridFind(priority); assert(gm.start==0 && gm.delta==6);
    priority[1]=priority[2]=1;
    gm=GridFind(priority); assert(gm.start==0 && gm.delta==1);
    for(int k=0;k<24;k++) priority[k]=-2;
    assert(GridFind(priority).length==0);
    for(int k=0;k<24;k++) priority[k]=0;
    priority[1]=priority[2]=priority[3]=priority[4]=4;
    assert(GridFind(priority).length==4);
    /* 한글: 널 종료, 두 문자열 순서, 빈 입력과 용량 부족을 확인한다.
     * English: Check terminators, ordering, empty input and capacity failure. */
    StringLesson sl;
    StringLoad(&sl,0); assert(sl.frames[sl.n-1].count==5);
    StringLoad(&sl,1); assert(sl.frames[sl.n-1].count==4);
    StringLoad(&sl,2);
    assert(strcmp(sl.frames[sl.n-1].second,"CODE")==0 && sl.frames[sl.n-1].initialized==31u);
    StringLoad(&sl,3);
    assert(strcmp(sl.frames[sl.n-1].second,"BOMBSAFE")==0 && sl.frames[sl.n-1].initialized==511u);
    sl.capacity=9;StringBuild(&sl);assert(!sl.frames[sl.n-1].failed);
    sl.capacity=8;StringBuild(&sl);
    assert(sl.frames[sl.n-1].failed && strcmp(sl.frames[sl.n-1].second,"BOMB")==0);
    StringLoad(&sl,4);assert(sl.frames[sl.n-1].result==0);
    strcpy(sl.input,"BONB");StringBuild(&sl);assert(sl.frames[sl.n-1].result<0);
    strcpy(sl.input,"BOLA");StringBuild(&sl);assert(sl.frames[sl.n-1].result>0);
    sl.input[0]=0;StringBuild(&sl);assert(sl.frames[sl.n-1].result>0);
    strcpy(sl.input,"BOM");StringBuild(&sl);assert(sl.frames[sl.n-1].result>0);
    assert(StringCompare("apple","banana")<0 && StringCompare("","")==0);
    assert(StringVowel('a') && StringVowel('I') && !StringVowel('p'));
    puts("PASS: all five Stage 5 missions and string edge cases.");
    puts("PASS: all five Stage 4 missions and pattern/gravity edge cases.");
    puts("PASS: all six Stage 3 missions.");
    puts("PASS: all five Stage 2 missions.");
    puts("PASS: all six Stage 1 missions.");
    puts("PASS: six levels, incomplete goals, bounds, operation budget and buffer safety.");
    return 0;
}
