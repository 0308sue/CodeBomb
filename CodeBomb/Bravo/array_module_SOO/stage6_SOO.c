#include "stage6_SOO.h"
#include <stdlib.h>
#include <string.h>

/* 한글: 할당 실패를 호출자에게 알려 노드를 잃지 않게 한다.
 * English: Report allocation failure without changing existing links. */
SooListNode *SooListCreate(int value)
{
    SooListNode *node=malloc(sizeof *node);
    if(node) *node=(SooListNode){.data=value};
    return node;
}
bool SooListInsertAfter(SooListNode *current,int value)
{
    if(!current) return false;
    SooListNode *node=SooListCreate(value);
    if(!node) return false;
    node->next=current->next; node->prev=current;
    if(node->next) node->next->prev=node;
    current->next=node;
    return true;
}
/* 한글: 우회 연결부터 만들고 대상 메모리를 해제한다.
 * English: Reconnect around the target before freeing its memory. */
bool SooListRemoveAfter(SooListNode *current)
{
    if(!current || !current->next) return false;
    SooListNode *target=current->next;
    current->next=target->next;
    if(current->next) current->next->prev=current;
    free(target);
    return true;
}
SooListNode *SooListFind(SooListNode *head,int value)
{
    while(head && head->data!=value) head=head->next;
    return head;
}
/* 한글: next를 저장한 후 링크를 뒤집고 마지막에 head를 바꾼다.
 * English: Save next before reversing the link; update head at the end. */
void SooListReverse(SooListNode **head)
{
    if(!head) return;
    SooListNode *previous=NULL,*current=*head;
    while(current) {
        SooListNode *next=current->next;
        current->next=previous; current->prev=next;
        previous=current; current=next;
    }
    *head=previous;
}
/* 한글: data는 건드리지 않는다. 앞 쌍의 끝, 두 노드, 다음 쌍을 연결한다.
 * English: Preserve data; reconnect the previous pair, both nodes and the suffix. */
void SooListSwapPairs(SooListNode **head)
{
    if(!head) return;
    SooListNode *before=NULL,*first=*head;
    while(first && first->next) {
        SooListNode *second=first->next,*after=second->next;
        if(before) before->next=second; else *head=second;
        second->prev=before;
        second->next=first; first->prev=second;
        first->next=after;
        if(after) after->prev=first;
        before=first; first=after;
    }
}
void SooListFree(SooListNode **head)
{
    if(!head) return;
    while(*head) { SooListNode *next=(*head)->next; free(*head); *head=next; }
}

const ListTask LIST_TASKS[6]={
 {"Follow the Signal","Visit all four nodes by following next.",
  "Node *current = head;\nwhile (current != NULL) {\n    visit(current);\n    current = current->next;\n}",
  "What tells traversal to stop?",{"NULL","data is 0","index 4"},"The final next pointer is NULL, not a special data value.",0},
 {"Insert the Node","Insert node 20 between nodes 10 and 30.",
  "Node *newNode = malloc(sizeof *newNode);\nif (newNode == NULL) return false;\nnewNode->data = 20;\nnewNode->next = current->next;\ncurrent->next = newNode;",
  "Which link must be set first?",{"current->next","newNode->next","head = NULL"},"Save the old successor in newNode->next before replacing current->next.",1},
 {"Remove the Virus","Bypass node 99, then release its memory.",
  "Node *target = current->next;\ncurrent->next = target->next;\nfree(target);\n// Do not access target after free.\n// current and its next must exist.",
  "What must happen before free(target)?",{"Read freed data","Set head NULL","Bypass target"},"Reconnect first. The old target address must never be used after free.",2},
 {"Find the Broken Node","Follow next until the value 99 is found.",
  "Node *findNode(Node *head, int target) {\n    Node *current = head;\n    while (current != NULL) {\n        if (current->data == target)\n            return current;\n        current = current->next;\n    }\n    return NULL;\n}",
  "What does findNode return on success?",{"Node address","Array index","Value 99"},"It returns a pointer to the node. Its data is 99.",0},
 {"Reverse the Circuit","Reverse links without moving nodes or changing data.",
  "Node *prev = NULL, *current = head;\nwhile (current != NULL) {\n    Node *next = current->next;\n    current->next = prev;\n    prev = current;\n    current = next;\n}\nhead = prev;",
  "Why save next before changing a link?",{"To swap data","To keep the rest","To allocate"},"The saved pointer keeps access to the remaining nodes.",1},
 {"Swap the Connections","Swap adjacent nodes in a doubly linked list.",
  "Node *first = *head;\nwhile (first && first->next) {\n    Node *second = first->next;\n    Node *after = second->next;\n    // Reconnect before, second, first, after.\n    // Update next AND prev (see each step).\n    // The first pair changes *head.\n    first = after;\n}",
  "Why pass Node **head to the function?",{"To swap data","To copy nodes","To update head"},"A pointer to head lets the function change the caller's starting node.",2}
};

/* 한글: 복사본은 ID를 사용하므로 실제 free 이후에도 안전하게 Back할 수 있다.
 * English: Snapshot IDs let Back work safely after the real deletion being illustrated. */
static bool Save(ListLesson *v,ListFrame *f,ListEvent event,int changed,const char *line,const char *note)
{
    if(v->n>=LIST_FRAMES) return false;
    f->event=event; f->changed=changed; f->line=line; f->note=note;
    v->frames[v->n++]=*f;
    return true;
}
bool ListFrameValid(const ListFrame *f,bool doubly)
{
    bool seen[LIST_NODES]={0};
    int node=f->head,previous=-1;
    while(node!=-1) {
        if(node<0 || node>=LIST_NODES || seen[node] || !f->nodes[node].alive) return false;
        if(doubly && f->nodes[node].prev!=previous) return false;
        seen[node]=true; previous=node; node=f->nodes[node].next;
    }
    for(int k=0;k<LIST_NODES;k++) if(f->nodes[k].alive!=seen[k]) return false;
    return true;
}

/* 한글: 그래프가 연결되어 있기만 해서는 부족하다. 미션의 정확한 노드 순서도 검사한다.
 * English: A consistent graph is not enough; require the mission's exact identity order. */
static bool MissionGoal(const ListFrame *f,int level)
{
    const int paths[6][4]={{0,1,2,3},{0,2,1,-1},{0,2,-1,-1},{0,1,2,3},{2,1,0,-1},{1,0,3,2}};
    const int counts[]={4,3,2,4,3,4};
    const int values[6][4]={{10,20,30,40},{10,30,20,0},{10,99,30,0},{5,12,99,8},{10,20,30,0},{10,20,30,40}};
    int id=f->head;
    for(int k=0;k<counts[level];k++) {
        if(id!=paths[level][k] || f->nodes[id].value!=values[level][id]) return false;
        id=f->nodes[id].next;
    }
    return id==-1;
}

/* 한글: 예제를 그래프 상태로 기록한다. -1은 NULL, 숫자 ID는 화면용 식별자다.
 * English: Record example graph states. -1 means NULL; IDs are display identities. */
bool ListLoad(ListLesson *v,int level)
{
    if(!v || level<0 || level>5) return false;
    *v=(ListLesson){.level=level,.answer=-1};
    ListFrame f={.head=0,.current=-1,.previous=-1,.saved=-1,.partner=-1,.found=-1,.changed=-1};
    for(int k=0;k<LIST_NODES;k++) f.nodes[k]=(ListNodeView){.next=-1,.prev=-1};
    int count=(int[]){4,2,3,4,3,4}[level];
    for(int k=0;k<count;k++) {
        f.nodes[k]=(ListNodeView){.value=(k+1)*10,.next=k+1<count?k+1:-1,.prev=level==5?k-1:-1,.alive=true};
    }
    if(level==1) f.nodes[1].value=30;
    if(level==2) f.nodes[1].value=99;
    if(level==3) {int values[]={5,12,99,8};for(int k=0;k<4;k++) f.nodes[k].value=values[k];}
    #define SNAP(E,C,L,N) do {if(!Save(v,&f,E,C,L,N)) return false;} while(0)
    SNAP(LIST_READY,-1,"Ready","Choose an answer. Follow the links, not the drawing order.");
    if(level==0 || level==3) {
        f.current=f.head;
        SNAP(LIST_POINTER,-1,"current = head","Start at the node addressed by head.");
        while(f.current!=-1) {
            f.visited++;
            SNAP(NODE_VISIT,f.current,"visit(current)","Read the current node's data.");
            if(level==3 && f.nodes[f.current].value==99) {
                f.found=f.current;
                SNAP(LIST_POINTER,f.current,"return current","Return this node pointer, not its value or an array index.");break;
            }
            f.current=f.nodes[f.current].next;
            SNAP(LIST_POINTER,f.current,"current = current->next","Follow next. NULL means there is no next node.");
        }
    } else if(level==1) {
        f.current=f.head; f.partner=2;
        f.nodes[2]=(ListNodeView){20,-1,-1,true};
        SNAP(NODE_CREATE,2,"newNode = malloc(...)","New node N2 holds 20. It is not reachable from head yet.");
        f.nodes[2].next=f.nodes[f.current].next;
        SNAP(NODE_LINK,2,"newNode->next = current->next","The new node now points to the old successor, node 30.");
        f.nodes[f.current].next=2;
        SNAP(NODE_LINK,f.current,"current->next = newNode","The chain from head is now 10 -> 20 -> 30.");
    } else if(level==2) {
        f.current=f.head; f.partner=1;
        SNAP(LIST_POINTER,1,"target = current->next","Target points to node 99. Keep it until after reconnecting.");
        f.nodes[0].next=f.nodes[1].next;
        SNAP(NODE_UNLINK,0,"current->next = target->next","Head can now reach 30 directly. Node 99 is detached.");
        f.nodes[1].alive=false; f.nodes[1].next=-1; f.partner=-1;
        SNAP(NODE_DELETE,1,"free(target)","Node 99 is released. Never dereference the freed target.");
    } else if(level==4) {
        f.current=f.head;
        SNAP(LIST_POINTER,-1,"prev = NULL; current = head","All nodes stay in place. Only links will change.");
        while(f.current!=-1) {
            int c=f.current;
            f.saved=f.nodes[c].next;
            SNAP(LIST_POINTER,c,"next = current->next","Save the original successor before breaking the link.");
            f.nodes[c].next=f.previous;
            SNAP(NODE_LINK,c,"current->next = prev","Reverse one arrow. Saved next still reaches the unprocessed nodes.");
            f.previous=c; f.current=f.saved;
            SNAP(LIST_POINTER,c,"prev = current; current = next","Advance both working pointers.");
        }
        f.head=f.previous;
        SNAP(NODE_LINK,f.head,"head = prev","Head now addresses node 30. The chain is 30 -> 20 -> 10.");
    } else {
        int before=-1,first=f.head;
        while(first!=-1 && f.nodes[first].next!=-1) {
            int second=f.nodes[first].next,after=f.nodes[second].next;
            f.current=first;f.partner=second;f.previous=before;f.saved=after;
            SNAP(LIST_POINTER,first,"first, second, after saved","Keep the next pair reachable before rewiring this pair.");
            if(before==-1) f.head=second; else f.nodes[before].next=second;
            SNAP(NODE_LINK,second,before==-1?"*head = second":"before->next = second","Connect the prefix to the second node. No data values change.");
            f.nodes[second].prev=before;
            SNAP(NODE_LINK,second,"second->prev = before","Repair the backward connection to the prefix.");
            f.nodes[second].next=first;
            SNAP(NODE_LINK,second,"second->next = first","Second now links forward to first.");
            f.nodes[first].prev=second;
            SNAP(NODE_LINK,first,"first->prev = second","First links backward to second.");
            f.nodes[first].next=after;
            SNAP(NODE_LINK,first,"first->next = after","Reconnect the remaining suffix.");
            if(after!=-1) {
                f.nodes[after].prev=first;
                SNAP(NODE_LINK,after,"after->prev = first","Repair the suffix's backward link.");
            }
            SNAP(NODE_SWAP,first,"Pair reconnected","The pair is swapped by identity, not by changing data.");
            before=first;first=after;
        }
        f.current=-1;f.partner=-1;f.saved=-1;f.previous=-1;
    }
    f.done=true;
    f.passed=ListFrameValid(&f,level==5) && MissionGoal(&f,level);
    if(level==0) f.passed=f.passed && f.visited==4 && f.current==-1;
    if(level==3) f.passed=f.passed && f.found==2;
    SNAP(LIST_END,-1,"Sample finished",f.passed?"Sample passed. Review each link with Back.":"The sample graph failed its final consistency check.");
    #undef SNAP
    return true;
}
bool ListChoose(ListLesson *v,int choice)
{
    if(!v || v->answer>=0 || choice<0 || choice>2) return false;
    v->answer=choice;v->playing=true;v->timer=0;return true;
}
void ListAdvance(ListLesson *v)
{
    if(v->answer>=0 && v->step+1<v->n) v->step++;
    if(v->step+1>=v->n) v->playing=false;
}
void ListBack(ListLesson *v)
{
    v->playing=false;v->timer=0;
    if(v->step>0) v->step--;
}
const char *ListEventName(ListEvent event)
{
    switch(event) {
        case NODE_CREATE:return "NODE_CREATE";case NODE_VISIT:return "NODE_VISIT";
        case NODE_LINK:return "NODE_LINK";case NODE_UNLINK:return "NODE_UNLINK";
        case NODE_DELETE:return "NODE_DELETE";case NODE_SWAP:return "NODE_SWAP";
        case LIST_POINTER:return "POINTER UPDATE";case LIST_END:return "END";
        default:return "READY";
    }
}
