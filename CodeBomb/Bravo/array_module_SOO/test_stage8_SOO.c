#include "stage8_SOO.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 한글: 표준의 무효화 규칙과 끝 반복자 규칙을 실제 위험한 접근 없이 검사한다.
 * English: Check invalidation and end-iterator rules without executing undefined behaviour. */
int main(void)
{
    VectorLesson v;assert(VectorLoad(&v,0,0));
    VectorNext(&v);assert(v.step==0);
    assert(VectorChoose(&v,1) && !VectorChoose(&v,0));
    while(v.step+1<v.n) VectorNext(&v);
    VectorFrame end=v.frames[v.step];
    assert(end.passed && end.size==6 && end.capacity==6 && !end.valid);
    assert(memcmp(end.data,(int[]){10,20,30,40,50,60},6*sizeof(int))==0);
    int step=v.step;VectorBack(&v);assert(v.step==step-1);VectorNext(&v);assert(v.step==step);
    assert(VectorLoad(&v,1,1));assert(v.frames[v.n-1].failed);
    assert(VectorLoad(&v,1,0));end=v.frames[v.n-1];assert(end.passed && end.readValue==20 && end.valid);
    assert(VectorLoad(&v,1,2));end=v.frames[v.n-1];assert(end.failed && end.valid && end.it==end.size);

    VectorFrame s;
    VectorReset(&s,(int[]){10,20,30},3,6);VectorIterator(&s,0);
    assert(VectorPush(&s,40) && !s.reallocated && s.valid && s.capacity==6);
    assert(VectorRead(&s) && s.readValue==10);
    VectorIterator(&s,s.size);assert(VectorPush(&s,50) && !s.valid);
    VectorReset(&s,(int[]){10,20,30},3,3);VectorIterator(&s,0);
    assert(VectorPush(&s,40) && s.reallocated && !s.valid);
    assert(!VectorRead(&s));
    VectorReset(&s,(int[]){10,99,20,30},4,4);VectorIterator(&s,0);
    assert(VectorErase(&s,1,false) && s.valid && s.it==0);
    VectorReset(&s,(int[]){10,99,20,30},4,4);VectorIterator(&s,2);
    assert(VectorErase(&s,1,false) && !s.valid);
    VectorReset(&s,(int[]){10},1,1);VectorIterator(&s,0);
    assert(VectorErase(&s,0,true) && s.valid && s.size==0 && s.it==0);
    assert(!VectorRead(&s));
    VectorReset(&s,NULL,0,0);assert(VectorIterator(&s,0));assert(!VectorAdvance(&s));
    VectorReset(&s,NULL,0,0);assert(!VectorErase(&s,0,true));
    VectorReset(&s,NULL,0,0);assert(VectorPush(&s,10) && s.capacity==1);
    VectorReset(&s,(int[]){1,2,3,4,5,6,7,8},8,8);assert(!VectorPush(&s,9));

    /* 한글: 최종 미션에서 찾기 → 안전한 삭제 → 스캔 완료 → 추가 → 검증을 확인한다.
     * English: Exercise the complete player-driven final mission in order. */
    VectorReset(&s,(int[]){10,99,20,30},4,4);VectorIterator(&s,0);
    assert(VectorBoss(&s,BOSS_SCAN) && s.it==1);
    assert(VectorBoss(&s,BOSS_SCAN) && s.found);
    assert(VectorBoss(&s,BOSS_ERASE) && s.removed && s.it==1 && s.data[1]==20);
    assert(VectorBoss(&s,BOSS_SCAN));assert(VectorBoss(&s,BOSS_SCAN) && s.it==s.size);
    assert(VectorBoss(&s,BOSS_PUSH) && !s.valid);
    assert(VectorBoss(&s,BOSS_CHECK) && s.passed && s.done);
    assert(memcmp(s.data,(int[]){10,20,30,40},4*sizeof(int))==0);
    VectorReset(&s,(int[]){10,99,20,30},4,4);VectorIterator(&s,0);
    assert(!VectorBoss(&s,BOSS_CHECK) && !s.passed);
    VectorReset(&s,(int[]){10,99,20,30},4,4);VectorIterator(&s,0);
    assert(!VectorBoss(&s,BOSS_ERASE));
    assert(!VectorLoad(&v,3,0) && !VectorLoad(&v,0,3));
    puts("PASS: Stage 8 growth, erase invalidation, end, size/capacity and complete final mission.");
    return 0;
}
