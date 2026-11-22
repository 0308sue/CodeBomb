/* 한글: 원본/수정/추가 실험은 같은 초기 모델에서 시작한다.
 * English: Start each original/fix/experiment from the same initial model. */
static void MemoryReplay(MemoryLesson *v,int variant)
{
    MemoryBuild(v,variant);v->playing=true;
}
static void DrawStage7(MemoryLesson *v,Vector2 mouse)
{
    const MemoryTask *task=&MEMORY_TASKS[v->level];
    const MemoryFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+34,task->title),40,45,27,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText("HEAP MONITOR / SAFE SIMULATION",48,158,18,ACCENT);
    Color warning={255,165,188,255};
    DrawText(TextFormat("%s: %s",v->level==1?"arr":v->level==2?"data":"ptr",f->done&&f->live?"out of scope / allocation leaked":!f->pointer?"NULL":f->live?"points to live allocation":"DANGLING / released allocation"),48,194,18,f->pointer&&!f->live?warning:INK);
    if(v->variant==2 && v->level==3) DrawText(TextFormat("alias: %s",!f->alias?"NULL":f->live?"same live allocation":"DANGLING / independent pointer"),48,220,17,f->alias&&!f->live?warning:MUTED);
    DrawText(TextFormat("ALLOCATED  %d / 16 int slots",f->live?f->size:0),48,246,17,ACCENT);
    for(int k=0;k<MEMORY_BLOCKS;k++) {
        int x=90+(k%4)*151,y=275+(k/4)*64;
        bool occupied=f->live && k<f->size;
        bool leaked=occupied && f->done && f->failed;
        Color fill=leaked?FAULT:occupied?SAFE:BG;
        DrawRectangleRounded((Rectangle){x,y,128,53},.25f,8,fill);
        if(k==f->active) DrawRectangleRoundedLinesEx((Rectangle){x,y,128,53},.25f,8,3,ACCENT);
        DrawText(TextFormat("[%d]",k),x+8,y+7,13,MUTED);
        DrawText(!occupied?".":(f->initialized&(1u<<k))?TextFormat("%d",f->values[k]):"?",x+56,y+18,22,INK);
    }
    DrawText(TextFormat("STEP %d/%d   %s",v->step,v->n-1,MemoryEventName(f->event)),48,548,17,ACCENT);
    DrawText(f->line,48,578,17,INK);
    Wrapped(f->note,48,610,675,17,f->failed?warning:INK);
    DrawText(". = free slot    ? = allocated, not initialized    1 slot = sizeof(int)",48,669,14,MUTED);
    DrawText("MISSION",797,157,19,ACCENT);Wrapped(task->goal,797,192,430,18,INK);
    DrawText(MemoryCode(v->level,v->variant),797,246,15,INK);
    Wrapped(task->question,797,439,430,17,INK);
    for(int k=0;k<3;k++) if(Button((Rectangle){797+146*k,482,135,42},TextFormat("%c  %s",'A'+k,task->choices[k]),mouse,v->answer<0,v->answer==k)) MemoryChoose(v,k);
    if(v->answer>=0) {
        DrawText(v->answer==task->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+task->answer),797,545,20,ACCENT);
        Wrapped(task->reason,797,581,425,17,INK);
        if(v->level>=2) {
            if(Button((Rectangle){797,639,135,36},"Original",mouse,true,v->variant==1)) MemoryReplay(v,1);
            if(Button((Rectangle){943,639,135,36},"Run fix",mouse,true,v->variant==0)) MemoryReplay(v,0);
            if(v->level==3 && Button((Rectangle){1089,639,140,36},"Alias case",mouse,true,v->variant==2)) MemoryReplay(v,2);
        } else if(v->level==1 && Button((Rectangle){797,639,250,36},"Try delete (wrong)",mouse,true,v->variant==2)) MemoryReplay(v,2);
        else if(v->level==0 && Button((Rectangle){797,639,250,36},"Try malloc failure",mouse,true,v->variant==2)) MemoryReplay(v,2);
    }
    bool more=v->answer>=0 && v->step+1<v->n;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;MemoryNext(v);}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) MemoryBack(v);
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) MemoryLoad(v,v->level);
    DrawText("A/B/C: answer    Space: play/pause    Arrows: step/back    R: reset",40,775,15,MUTED);
}
