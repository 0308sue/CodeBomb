#include "stage7_SOO.h"
#include <assert.h>
#include <stdio.h>

/* 한글: 이 테스트는 위험한 동작을 실제로 실행하지 않는다. 차단 여부를 검사한다.
 * English: Tests verify blocked operations without executing native undefined behaviour. */
int main(void)
{
    for(int level=0;level<4;level++) {
        MemoryLesson v;
        assert(MemoryLoad(&v,level));
        MemoryNext(&v);assert(v.step==0);
        assert(MemoryChoose(&v,MEMORY_TASKS[level].answer));assert(!MemoryChoose(&v,0));
        assert(MemoryBuild(&v,0));
        while(v.step+1<v.n) MemoryNext(&v);
        const MemoryFrame *end=&v.frames[v.step];
        assert(end->passed && end->done && !end->live && !end->pointer && !end->failed);
        int step=v.step;MemoryBack(&v);assert(v.step==step-1 && !v.playing);
        MemoryNext(&v);assert(v.step==step);
        assert(v.n<MEMORY_FRAMES);
        if(level==1) {
            const MemoryFrame *filled=&v.frames[6];
            assert(filled->live && filled->initialized==31u);
            for(int i=0;i<5;i++) assert(filled->values[i]==(i+1)*10);
        }
    }
    MemoryLesson wrongRelease;MemoryLoad(&wrongRelease,1);MemoryBuild(&wrongRelease,2);
    assert(wrongRelease.frames[wrongRelease.n-1].failed && wrongRelease.frames[wrongRelease.n-1].live);
    MemoryLesson leak;MemoryLoad(&leak,2);MemoryBuild(&leak,1);
    assert(leak.frames[leak.n-1].failed && leak.frames[leak.n-1].live && leak.frames[leak.n-1].size==8);
    MemoryLesson dangling;MemoryLoad(&dangling,3);MemoryBuild(&dangling,1);
    assert(dangling.frames[dangling.n-1].failed && !dangling.frames[dangling.n-1].live);
    MemoryBuild(&dangling,2);
    const MemoryFrame *alias=&dangling.frames[dangling.n-1];
    assert(alias->failed && !alias->pointer && alias->alias && !alias->live);
    MemoryLoad(&dangling,0);MemoryBuild(&dangling,2);
    assert(dangling.frames[dangling.n-1].passed && dangling.frames[dangling.n-1].allocationFailed);

    MemoryFrame f;MemoryReset(&f);
    assert(!MemoryApply(&f,(MemoryEvent){.kind=MEM_READ}));
    MemoryReset(&f);assert(MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_NEW_ARRAY,0,5,false}));
    assert(!MemoryApply(&f,(MemoryEvent){MEM_FREE,MEMORY_NEW_SINGLE,0,0,false}));
    assert(f.live); /* 한글: 잘못된 해제는 실제 메모리를 바꾸지 않는다. English: Wrong release is not applied. */
    MemoryReset(&f);MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_MALLOC,0,4,false});
    assert(!MemoryApply(&f,(MemoryEvent){MEM_READ,MEMORY_MALLOC,0,0,false}));
    MemoryReset(&f);MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_MALLOC,0,4,false});
    assert(!MemoryApply(&f,(MemoryEvent){MEM_WRITE,MEMORY_MALLOC,4,100,false}));
    MemoryReset(&f);MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_MALLOC,0,4,false});
    assert(!MemoryApply(&f,(MemoryEvent){MEM_WRITE,MEMORY_MALLOC,-1,100,false}));
    MemoryReset(&f);MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_MALLOC,0,4,false});
    assert(MemoryApply(&f,(MemoryEvent){MEM_FREE,MEMORY_MALLOC,0,0,false}));
    assert(!MemoryApply(&f,(MemoryEvent){MEM_FREE,MEMORY_MALLOC,0,0,false}));
    MemoryReset(&f);MemoryApply(&f,(MemoryEvent){MEM_ALLOC,MEMORY_NEW_ARRAY,0,8,false});
    MemoryApply(&f,(MemoryEvent){.kind=MEM_NULL});
    assert(!MemoryApply(&f,(MemoryEvent){.kind=MEM_END}) && f.live);
    MemoryReset(&f);assert(MemoryApply(&f,(MemoryEvent){.kind=MEM_FREE}));
    MemoryApply(&f,(MemoryEvent){.kind=MEM_ALLOC_FAIL});
    assert(!MemoryApply(&f,(MemoryEvent){.kind=MEM_READ}));
    assert(!MemoryLoad(&leak,4) && !MemoryBuild(&leak,3));
    puts("PASS: Stage 7 safe/failing traces, leaks, aliases, families, NULL, bounds, uninitialized reads and double release.");
    return 0;
}
