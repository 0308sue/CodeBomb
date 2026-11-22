#ifndef STAGE6_SOO_H
#define STAGE6_SOO_H
#include <stdbool.h>

#define LIST_NODES 6
#define LIST_FRAMES 80
/* 한글: 실제 C 리스트 자료형. prev는 이중 연결 미션에서 사용한다.
 * English: A real C list node. The doubly linked mission uses prev as well. */
typedef struct SooListNode { int data; struct SooListNode *next,*prev; } SooListNode;
SooListNode *SooListCreate(int value);
bool SooListInsertAfter(SooListNode *current,int value);
bool SooListRemoveAfter(SooListNode *current);
SooListNode *SooListFind(SooListNode *head,int value);
void SooListReverse(SooListNode **head);
void SooListSwapPairs(SooListNode **head);
void SooListFree(SooListNode **head);

/* 한글: 되돌리기용 그림은 해제된 실제 주소 대신 안정적인 노드 ID를 쓴다.
 * English: Replay diagrams use stable node IDs, never freed memory addresses. */
typedef enum { LIST_READY, NODE_CREATE, NODE_VISIT, NODE_LINK, NODE_UNLINK,
               NODE_DELETE, NODE_SWAP, LIST_POINTER, LIST_END } ListEvent;
typedef struct { int value,next,prev; bool alive; } ListNodeView;
typedef struct {
    ListNodeView nodes[LIST_NODES];
    int head,current,previous,saved,partner,visited,found,changed;
    ListEvent event;
    bool done,passed;
    const char *line,*note;
} ListFrame;
typedef struct {
    int level,step,n,answer;
    bool playing; float timer;
    ListFrame frames[LIST_FRAMES];
} ListLesson;
typedef struct { const char *title,*goal,*code,*question,*choices[3],*reason; int answer; } ListTask;
extern const ListTask LIST_TASKS[6];
bool ListLoad(ListLesson *lesson,int level);
bool ListChoose(ListLesson *lesson,int choice);
void ListAdvance(ListLesson *lesson);
void ListBack(ListLesson *lesson);
/* 한글: 최종 상태만 검사한다. 포인터를 하나씩 바꾸는 중간 상태는 분리될 수 있다.
 * English: Validate final states; intermediate rewiring can temporarily split a list. */
bool ListFrameValid(const ListFrame *frame,bool doubly);
const char *ListEventName(ListEvent event);
#endif
