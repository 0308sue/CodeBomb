/* 한글: 선의 화살촉은 대상 노드에 붙인다. 파랑 next, 분홍 prev다.
 * English: Arrowheads end at their target node. Blue is next; pink is prev. */
static int ListNodeX(int level,int id)
{
    if(level==1) return (int[]){90,530,310,0,0,0}[id];
    return 72+id*165;
}
static void ListWire(int x1,int x2,int lane,bool backwards,Color color)
{
    int y=backwards?443:350;
    DrawLineEx((Vector2){x1,y},(Vector2){x1,lane},2,color);
    DrawLineEx((Vector2){x1,lane},(Vector2){x2,lane},2,color);
    DrawLineEx((Vector2){x2,lane},(Vector2){x2,y},2,color);
    if(backwards) DrawTriangle((Vector2){x2-5,y+8},(Vector2){x2,y},(Vector2){x2+5,y+8},color);
    else DrawTriangle((Vector2){x2-5,y-8},(Vector2){x2,y},(Vector2){x2+5,y-8},color);
}
static const char *ListID(int id)
{
    return id<0?"NULL":TextFormat("N%d",id);
}
static void DrawStage6(ListLesson *v,Vector2 mouse)
{
    const ListTask *task=&LIST_TASKS[v->level];
    const ListFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+28,task->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText(v->level==5?"DOUBLY LINKED CIRCUIT":"LINKED LIST CIRCUIT",48,158,19,ACCENT);
    DrawText(TextFormat("head: %s   current: %s   visited: %d",ListID(f->head),ListID(f->current),f->visited),48,194,18,INK);
    if(v->level>=4) DrawText(TextFormat("prev: %s   saved next: %s",ListID(f->previous),ListID(f->saved)),48,222,17,MUTED);
    else if(v->level==2 && !f->nodes[1].alive) DrawText("target: released / do not use",48,222,17,MUTED);
    else if(v->level==1 || v->level==2) DrawText(TextFormat("%s: %s",v->level==1?"newNode":"target",ListID(f->partner)),48,222,17,MUTED);
    DrawText("MISSION",797,157,19,ACCENT);Wrapped(task->goal,797,192,430,18,INK);
    DrawText(task->code,797,245,15,INK);
    Wrapped(task->question,797,441,430,17,INK);
    for(int k=0;k<3;k++) if(Button((Rectangle){797+146*k,482,135,42},TextFormat("%c  %s",'A'+k,task->choices[k]),mouse,v->answer<0,v->answer==k)) ListChoose(v,k);
    if(v->answer>=0) {
        DrawText(v->answer==task->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+task->answer),797,545,20,ACCENT);
        Wrapped(task->reason,797,583,425,17,INK);
    } else DrawText("Choose A, B or C to begin.",797,545,17,MUTED);
    int count=(int[]){4,3,3,4,3,4}[v->level];
    Color prevColor={255,186,220,255};
    /* 한글: ID는 같은 노드를 뜻하며 화면 순서는 리스트 순서가 아니다.
     * English: IDs preserve identity. Drawing order is not traversal order. */
    for(int k=0;k<count;k++) if(f->nodes[k].alive) {
        int next=f->nodes[k].next,prev=f->nodes[k].prev;
        if(next>=0) ListWire(ListNodeX(v->level,k)+56,ListNodeX(v->level,next)+56,304-k*15,false,ACCENT);
        if(v->level==5 && prev>=0) ListWire(ListNodeX(v->level,k)+56,ListNodeX(v->level,prev)+56,463+k*13,true,prevColor);
    }
    for(int k=0;k<count;k++) {
        int x=ListNodeX(v->level,k);
        const ListNodeView *node=&f->nodes[k];
        bool unseen=v->level==1 && k==2 && !node->alive;
        Color color=!node->alive?BG:f->found==k?SAFE:v->level==2&&k==1?FAULT:PANEL;
        DrawRectangleRounded((Rectangle){x,350,112,93},.2f,10,color);
        DrawRectangleRoundedLinesEx((Rectangle){x,350,112,93},.2f,10,k==f->changed||k==f->current?3:1,k==f->current?ACCENT:k==f->changed?prevColor:MUTED);
        DrawText(TextFormat("N%d",k),x+9,357,15,MUTED);
        DrawText(!node->alive?(unseen?"new?":"freed"):TextFormat("%d",node->value),x+25,379,25,INK);
        DrawText(node->alive?TextFormat("next %s",ListID(node->next)):"--",x+8,420,13,MUTED);
        if(f->head==k) DrawText("HEAD",x+28,326,16,prevColor);
    }
    DrawText(TextFormat("STEP %d / %d   %s",v->step,v->n-1,ListEventName(f->event)),48,530,16,ACCENT);
    DrawText(f->line,48,560,18,INK);
    Wrapped(f->note,48,593,674,17,INK);
    DrawText(v->level==5?"Blue: next   Pink: prev   N = node identity, not an array index":"N = node identity, not an array index. Follow arrows from HEAD.",48,663,15,MUTED);
    bool more=v->answer>=0 && v->step+1<v->n;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;ListAdvance(v);}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) ListBack(v);
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) ListLoad(v,v->level);
    DrawText("A/B/C: answer    Space: play/pause    Arrows: step/back    R: reset",40,775,15,MUTED);
}
