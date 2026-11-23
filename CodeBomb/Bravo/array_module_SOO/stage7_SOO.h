#ifndef STAGE7_SOO_H
#define STAGE7_SOO_H
#include <stdbool.h>

#define MEMORY_BLOCKS 16
#define MEMORY_FRAMES 24
/* 한글: C 프로그램 내부에서 C++ 할당 종류도 표식으로 구분한다.
 * English: The C application represents C++ allocation families with tags. */
typedef enum { MEMORY_MALLOC, MEMORY_NEW_ARRAY, MEMORY_NEW_SINGLE } MemoryFamily;
typedef enum { MEM_READY, MEM_ALLOC, MEM_ALLOC_FAIL, MEM_FREE, MEM_READ, MEM_WRITE,
               MEM_NULL, MEM_ALIAS, MEM_END, MEM_ERROR } MemoryKind;
typedef struct { MemoryKind kind; MemoryFamily family; int index,value; bool alias; } MemoryEvent;
typedef struct {
    int values[MEMORY_BLOCKS],size,active,readValue;
    unsigned initialized;
    bool everAllocated,live,pointer,alias,failed,done,passed,allocationFailed;
    MemoryFamily family;
    MemoryKind event;
    const char *line,*note;
} MemoryFrame;
typedef struct {
    int level,step,n,answer,variant;
    bool playing;float timer;
    MemoryFrame frames[MEMORY_FRAMES];
} MemoryLesson;
typedef struct { const char *title,*goal,*question,*choices[3],*reason; int answer; } MemoryTask;
extern const MemoryTask MEMORY_TASKS[4];
void MemoryReset(MemoryFrame *frame);
bool MemoryApply(MemoryFrame *frame,MemoryEvent event);
bool MemoryBuild(MemoryLesson *lesson,int variant);
bool MemoryLoad(MemoryLesson *lesson,int level);
bool MemoryChoose(MemoryLesson *lesson,int answer);
void MemoryNext(MemoryLesson *lesson);
void MemoryBack(MemoryLesson *lesson);
const char *MemoryCode(int level,int variant);
const char *MemoryEventName(MemoryKind kind);
#endif
