#include "stage6_SOO.h"
#include <assert.h>
#include <stdio.h>

/* 한글: 실제 힙 노드를 만들어 포인터 방향과 노드 정체성을 확인한다.
 * English: Use real heap nodes to verify links and identity, not just data order. */
static void CheckReal(SooListNode *head,SooListNode **expected,int count)
{
    SooListNode *before=NULL;
    for(int k=0;k<count;k++) {
        assert(head==expected[k]);
        assert(head->prev==before);
        before=head;head=head->next;
    }
    assert(head==NULL);
}
int main(void)
{
    const int paths[6][4]={{0,1,2,3},{0,2,1,-1},{0,2,-1,-1},{0,1,2,3},{2,1,0,-1},{1,0,3,2}};
    const int counts[]={4,3,2,4,3,4};
    for(int level=0;level<6;level++) {
        ListLesson v;
        assert(ListLoad(&v,level));
        assert(v.n>1 && v.n<LIST_FRAMES);
        assert(!ListChoose(&v,-1));ListAdvance(&v);assert(v.step==0);
        assert(ListChoose(&v,LIST_TASKS[level].answer));
        assert(!ListChoose(&v,0));
        while(v.step+1<v.n) ListAdvance(&v);
        const ListFrame *f=&v.frames[v.step];
        assert(f->done && f->passed && ListFrameValid(f,level==5));
        int id=f->head;
        for(int k=0;k<counts[level];k++) {assert(id==paths[level][k]);id=f->nodes[id].next;}
        assert(id==-1);
        if(level==0) assert(f->current==-1 && f->visited==4);
        if(level==2) assert(!f->nodes[1].alive);
        if(level==3) assert(f->found==2 && f->visited==3);
        if(level==4 || level==5) for(int k=0;k<counts[level];k++) assert(f->nodes[k].value==(k+1)*10);
        int end=v.step;ListBack(&v);assert(v.step==end-1 && !v.playing);
        ListAdvance(&v);assert(v.step==end);
    }
    ListLesson v;assert(!ListLoad(&v,6));
    assert(ListLoad(&v,0));ListFrame invalid=v.frames[v.n-1];
    invalid.nodes[3].next=0;assert(!ListFrameValid(&invalid,false));
    invalid.nodes[3].next=LIST_NODES;assert(!ListFrameValid(&invalid,false));
    invalid.nodes[3].next=-1;invalid.nodes[1].alive=false;assert(!ListFrameValid(&invalid,false));

    /* 한글: 빈 리스트, 홀수/짝수 길이에서 링크 교환을 두 번 하면 원래 노드 순서가 된다.
     * English: Two pair swaps restore identity for empty, odd and even lists. */
    for(int count=0;count<=5;count++) {
        SooListNode *original[5]={0},*swapped[5]={0},*reversed[5]={0};
        for(int k=0;k<count;k++) {
            original[k]=SooListCreate((k+1)*10);assert(original[k]);
            if(k) {original[k-1]->next=original[k];original[k]->prev=original[k-1];}
        }
        SooListNode *head=count?original[0]:NULL;
        for(int k=0;k<count;k++) swapped[k]=original[(k%2==0 && k+1<count)?k+1:(k%2==1)?k-1:k];
        SooListSwapPairs(&head);CheckReal(head,swapped,count);
        for(int k=0;k<count;k++) assert(original[k]->data==(k+1)*10);
        SooListSwapPairs(&head);CheckReal(head,original,count);
        for(int k=0;k<count;k++) reversed[k]=original[count-1-k];
        SooListReverse(&head);CheckReal(head,reversed,count);
        SooListReverse(&head);CheckReal(head,original,count);
        SooListFree(&head);assert(!head);
    }
    SooListNode *head=SooListCreate(10);assert(head);
    assert(SooListInsertAfter(head,30));SooListNode *last=head->next;
    assert(SooListInsertAfter(head,20));
    assert(head->next->data==20 && head->next->next==last && last->prev==head->next);
    assert(SooListFind(head,20)==head->next && SooListFind(head,99)==NULL);
    assert(SooListRemoveAfter(head));assert(head->next==last && last->prev==head);
    assert(SooListRemoveAfter(head));assert(head->next==NULL);
    assert(!SooListRemoveAfter(head) && !SooListInsertAfter(NULL,20));
    SooListFree(&head);SooListSwapPairs(NULL);SooListReverse(NULL);SooListFree(NULL);
    assert(SooListFind(NULL,99)==NULL && !SooListRemoveAfter(NULL));
    puts("PASS: Stage 6 snapshots, actual C allocation/free, identity, next/prev, odd/even/empty lists.");
    return 0;
}
