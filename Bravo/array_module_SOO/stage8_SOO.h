#ifndef STAGE8_SOO_H
#define STAGE8_SOO_H
#include <stdbool.h>
#define VECTOR_LIMIT 8
#define VECTOR_FRAMES 16

typedef enum { VECTOR_READY,VECTOR_PUSH,VECTOR_ERASE,ITER_MOVE,ITER_READ,VECTOR_CHECK,VECTOR_ERROR } VectorEvent;
/* 한글: 실제 STL 반복자가 아닌 유효성 모델이다. 같은 위치라도 무효 반복자는 읽을 수 없다.
 * English: Model validity explicitly; an invalid iterator cannot be read even at an occupied slot. */
typedef struct {
    int data[VECTOR_LIMIT],size,capacity,it,active,readValue;
    unsigned scanned;
    bool valid,failed,reallocated,found,removed,pushed,done,passed;
    VectorEvent event;
    const char *line,*note;
} VectorFrame;
typedef struct { int level,step,n,answer,variant; bool playing;float timer;VectorFrame frames[VECTOR_FRAMES]; } VectorLesson;
typedef enum { BOSS_SCAN,BOSS_ERASE,BOSS_PUSH,BOSS_CHECK } BossAction;
void VectorReset(VectorFrame *s,const int *values,int size,int capacity);
bool VectorIterator(VectorFrame *s,int index);
bool VectorRead(VectorFrame *s);
bool VectorAdvance(VectorFrame *s);
bool VectorPush(VectorFrame *s,int value);
bool VectorErase(VectorFrame *s,int index,bool takeReturnedIterator);
bool VectorBoss(VectorFrame *s,BossAction action);
bool VectorLoad(VectorLesson *v,int level,int variant);
bool VectorChoose(VectorLesson *v,int choice);
void VectorNext(VectorLesson *v);
void VectorBack(VectorLesson *v);
const char *VectorCode(int level,int variant);
const char *VectorEventName(VectorEvent event);
#endif
