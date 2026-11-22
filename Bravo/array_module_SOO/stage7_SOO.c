#include "stage7_SOO.h"
#include <string.h>

const MemoryTask MEMORY_TASKS[4]={
 {"Allocate the Memory","Allocate four int slots, use them, then release them.","What must be checked before using ptr?",{"ptr != NULL","ptr == 4","No check"},"malloc can fail. Check the returned pointer before use.",0},
 {"Build the Memory Array","Fill a dynamic array with 10, 20, 30, 40, 50.","What matches new int[5]?",{"free(arr)","delete arr","delete[] arr"},"An array created by new[] must be released with delete[].",2},
 {"Memory Leak Detector","Find and fix the missing memory cleanup.","What releases data created by new int[8]?",{"data = nullptr","delete[] data","Return early"},"Clearing a pointer does not free its allocation. Release it first.",1},
 {"Dangling Pointer","Read before release. Never read through a dangling pointer.","Which order avoids use-after-free?",{"Delete / read","Null / read","Read / delete"},"Read while the allocation is live, then delete it and clear the pointer.",2}
};
/* 한글: 모델만 초기화한다. 실제 힙 할당이나 해제된 주소를 저장하지 않는다.
 * English: Reset the teaching model; it contains no actual heap pointers. */
void MemoryReset(MemoryFrame *f)
{
    memset(f,0,sizeof *f);f->active=-1;
    f->line="Ready";f->note="Choose an answer to start the sample.";
}
static bool MemoryFail(MemoryFrame *f,const char *note)
{
    f->failed=true;f->event=MEM_ERROR;f->note=note;return false;
}
/* 한글: 오류는 실제로 수행하지 않고 검사 단계에서 막는다.
 * English: Reject invalid operations without ever performing unsafe native accesses. */
bool MemoryApply(MemoryFrame *f,MemoryEvent e)
{
    if(f->failed || f->done) return false;
    f->event=e.kind;f->active=-1;
    bool target=e.alias?f->alias:f->pointer;
    switch(e.kind) {
        case MEM_ALLOC:
            if(f->everAllocated || e.value<1 || e.value>MEMORY_BLOCKS)
                return MemoryFail(f,"Invalid model allocation size or duplicate allocation.");
            f->everAllocated=true;f->live=true;f->pointer=true;f->size=e.value;f->family=e.family;
            f->note="Allocation succeeded. Slots exist, but their values are not initialized.";return true;
        case MEM_ALLOC_FAIL:
            f->pointer=false;f->allocationFailed=true;
            f->note="Allocation failed. ptr is NULL. The safe sample skips all access.";return true;
        case MEM_WRITE:case MEM_READ:
            if(!target) return MemoryFail(f,"NULL pointer access blocked. No memory access occurred.");
            if(!f->live) return MemoryFail(f,"Use-after-free blocked. This pointer still refers to released memory.");
            if(e.index<0 || e.index>=f->size) return MemoryFail(f,"Out-of-bounds access blocked.");
            f->active=e.index;
            if(e.kind==MEM_WRITE) {
                f->values[e.index]=e.value;f->initialized|=1u<<e.index;
                f->note="Write while the allocation is live.";
            } else {
                if(!(f->initialized&(1u<<e.index))) return MemoryFail(f,"Uninitialized read blocked. Write a value first.");
                f->readValue=f->values[e.index];f->note="Read while the allocation is live.";
            }
            return true;
        case MEM_FREE:
            /* 한글: 널 포인터 해제는 C/C++ 모두 아무 일도 하지 않는다.
             * English: Deallocation of a null pointer is a no-op. */
            if(!target) {f->note="Null pointer release is a no-op.";return true;}
            if(!f->live) return MemoryFail(f,"Double release blocked. This allocation is already freed.");
            if(e.family!=f->family) return MemoryFail(f,"Wrong release family. Match malloc/free, new/delete, new[]/delete[].");
            f->live=false;f->note="Memory released. Existing pointer copies are now dangling, not automatically NULL.";return true;
        case MEM_NULL:
            if(e.alias) f->alias=false;else f->pointer=false;
            f->note="One pointer is cleared. This does not release memory or clear other aliases.";return true;
        case MEM_ALIAS:
            f->alias=f->pointer;f->note="alias copies the same target as ptr. It is a separate pointer variable.";return true;
        case MEM_END:
            f->done=true;
            if(f->live) return MemoryFail(f,"Leak detected: allocated slots remain after the sample function ends.");
            if(f->pointer || f->alias) return MemoryFail(f,"A dangling pointer remains. The lesson requires clearing it.");
            f->passed=true;
            f->note=f->allocationFailed?"Safe exit: the failed allocation was never dereferenced.":"Sample passed: memory was released and sample pointers were cleared.";return true;
        default:return MemoryFail(f,"Unsupported memory event.");
    }
}
static bool Append(MemoryLesson *v,MemoryFrame *f,MemoryEvent e,const char *line)
{
    if(v->n>=MEMORY_FRAMES) return false;
    f->line=line;MemoryApply(f,e);v->frames[v->n++]=*f;return true;
}
/* 한글: variant 0=안전 예제, 1=원본 오류, 2=별칭/실패 실험이다.
 * English: Variant 0 is safe, 1 is the broken original, 2 is an alias/failure experiment. */
bool MemoryBuild(MemoryLesson *v,int variant)
{
    if(!v || v->level<0 || v->level>3 || variant<0 || variant>2) return false;
    v->variant=variant;v->n=1;v->step=0;v->timer=0;v->playing=false;
    MemoryFrame f;MemoryReset(&f);v->frames[0]=f;
    MemoryFamily family=v->level==0?MEMORY_MALLOC:v->level==3?MEMORY_NEW_SINGLE:MEMORY_NEW_ARRAY;
    int size=(int[]){4,5,8,1}[v->level];
    #define EVENT(K,F,I,V,A,L) do { if(!Append(v,&f,(MemoryEvent){K,F,I,V,A},L)) return false; if(f.failed) return true; } while(0)
    if(v->level==0 && variant==2) {
        EVENT(MEM_ALLOC_FAIL,family,0,0,false,"ptr = malloc(...) -> NULL");
        EVENT(MEM_END,family,0,0,false,"if (ptr != NULL) is false; skip access");return true;
    }
    EVENT(MEM_ALLOC,family,0,size,false,v->level==0?"ptr = malloc(4 * sizeof *ptr)":v->level==3?"ptr = new int":v->level==2?"data = new int[8]":"arr = new int[5]");
    if(v->level==3 && variant==2) EVENT(MEM_ALIAS,family,0,0,false,"alias = ptr");
    int writes=v->level==1?5:1;
    for(int k=0;k<writes;k++) {
        EVENT(MEM_WRITE,family,k,v->level==1?(k+1)*10:100,false,v->level==1?"arr[i] = (i + 1) * 10":v->level==2?"data[0] = 100":"*ptr = 100");
    }
    if(v->level==2 && variant==1) {
        EVENT(MEM_END,family,0,0,false,"return; // cleanup is missing");return true;
    }
    if(v->level==3 && variant==0) EVENT(MEM_READ,family,0,0,false,"read(*ptr); // before delete");
    if(v->level==1 && variant==2) {
        EVENT(MEM_FREE,MEMORY_NEW_SINGLE,0,0,false,"delete arr; // WRONG: array needs delete[]");return true;
    }
    EVENT(MEM_FREE,family,0,0,false,v->level==0?"free(ptr)":family==MEMORY_NEW_ARRAY?(v->level==2?"delete[] data":"delete[] arr"):"delete ptr");
    if(v->level==3 && variant==1) {
        EVENT(MEM_READ,family,0,0,false,"read(*ptr); // after delete: INVALID");return true;
    }
    EVENT(MEM_NULL,family,0,0,false,v->level==0?"ptr = NULL":v->level==1?"arr = nullptr":v->level==2?"data = nullptr":"ptr = nullptr");
    if(v->level==3 && variant==2) {
        EVENT(MEM_READ,family,0,0,true,"read(*alias); // ptr is null, alias is dangling");return true;
    }
    EVENT(MEM_END,family,0,0,false,"End of sample function");
    #undef EVENT
    return true;
}
bool MemoryLoad(MemoryLesson *v,int level)
{
    if(!v || level<0 || level>3) return false;
    *v=(MemoryLesson){.level=level,.answer=-1};return MemoryBuild(v,level>=2?1:0);
}
bool MemoryChoose(MemoryLesson *v,int answer)
{
    if(!v || v->answer>=0 || answer<0 || answer>2) return false;
    v->answer=answer;v->playing=true;v->timer=0;return true;
}
void MemoryNext(MemoryLesson *v)
{
    if(v->answer>=0 && v->step+1<v->n) v->step++;
    if(v->step+1>=v->n) v->playing=false;
}
void MemoryBack(MemoryLesson *v)
{
    v->playing=false;v->timer=0;if(v->step>0) v->step--;
}
const char *MemoryEventName(MemoryKind e)
{
    switch(e) {
        case MEM_ALLOC:return "MEM_ALLOC";case MEM_ALLOC_FAIL:return "ALLOCATION FAILED";
        case MEM_FREE:return "MEM_FREE";case MEM_READ:return "MEM_READ";case MEM_WRITE:return "MEM_WRITE";
        case MEM_NULL:return "CLEAR POINTER";case MEM_ALIAS:return "COPY POINTER";
        case MEM_END:return "END";case MEM_ERROR:return "MEM_ERROR";default:return "READY";
    }
}
const char *MemoryCode(int level,int variant)
{
    if(level==0) return "// C\nint *ptr = malloc(4 * sizeof *ptr);\nif (ptr != NULL) {\n    *ptr = 100;\n    free(ptr);\n    ptr = NULL;\n}";
    if(level==1 && variant==2) return "// C++ / wrong release example\nint *arr = new int[5];\nfor (int i = 0; i < 5; i++)\n    arr[i] = (i + 1) * 10;\ndelete arr; // WRONG: use delete[] arr";
    if(level==1) return "// C++ learning example\nint *arr = new int[5];\nfor (int i = 0; i < 5; i++)\n    arr[i] = (i + 1) * 10;\ndelete[] arr;\narr = nullptr;\n// new[] pairs with delete[], not delete.";
    if(level==2) return variant==0?
        "// C++ / fixed sample\nvoid process() {\n    int *data = new int[8];\n    data[0] = 100;\n    delete[] data;\n    data = nullptr;\n}":
        "// C++ / missing cleanup\nvoid process() {\n    int *data = new int[8];\n    data[0] = 100;\n    // Missing delete[] data;\n}";
    if(variant==2) return "// C++ / alias experiment\nint *ptr = new int;\n*ptr = 100;\nint *alias = ptr;\ndelete ptr;\nptr = nullptr;\nread(*alias); // INVALID: alias is dangling";
    return variant==0?"// C++ / fixed sample\nint *ptr = new int;\n*ptr = 100;\nread(*ptr); // valid here\ndelete ptr;\nptr = nullptr;":
        "// C++ / original sample\nint *ptr = new int;\n*ptr = 100;\ndelete ptr;\nread(*ptr); // INVALID use-after-free";
}
