/* 한글: 최종 미션도 조작마다 상태를 남겨 Back과 재시도가 가능하다.
 * English: Keep a history of player actions for Back and retries in the final mission. */
typedef struct { int n,step;VectorFrame frames[32]; } VectorBossView;
static void BossReset(VectorBossView *v)
{
    *v=(VectorBossView){.n=1};
    VectorReset(&v->frames[0],(int[]){10,99,20,30},4,4);
    VectorIterator(&v->frames[0],0);
    v->frames[0].note="Scan modules until 99 is found. Follow the iterator.";
}
static void BossAct(VectorBossView *v,BossAction action)
{
    if(v->step+1>=32 || v->frames[v->step].done || v->frames[v->step].failed) return;
    VectorFrame next=v->frames[v->step];
    VectorBoss(&next,action);
    v->frames[++v->step]=next;v->n=v->step+1;
}
static void VectorReplay(VectorLesson *v,int variant)
{
    int answer=v->answer,level=v->level;
    VectorLoad(v,level,variant);v->answer=answer;v->playing=true;
}
static void DrawStage8(VectorLesson *v,VectorBossView *boss,Vector2 mouse)
{
    const char *titles[]={"Expand the Circuit","Iterator Rescue","Final Defusal"};
    const char *goals[]={"Append 40, 50 and 60. Observe size and capacity.","Remove 99 and safely use the iterator returned by erase.","Find 99, erase it, finish the scan, add 40, and verify."};
    const VectorFrame *f=v->level==2?&boss->frames[boss->step]:&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+38,titles[v->level]),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText(v->level==2?(f->passed?"BOMB DEFUSED / SYSTEM STABLE":"FINAL BOSS / REPAIR TERMINAL"):"VECTOR STORAGE / C++ MODEL",48,158,19,ACCENT);
    DrawText(TextFormat("SIZE  %d    CAPACITY  %d (demo)",f->size,f->capacity),48,204,22,INK);
    DrawText(v->level==2 && f->pushed?"it: scan finished / no longer used":!f->valid?"it: not acquired / invalid":f->it==f->size?"it: end() / cannot read":TextFormat("it: offset %d / valid",f->it),48,246,18,f->failed?(Color){255,165,188,255}:ACCENT);
    for(int k=0;k<VECTOR_LIMIT;k++) {
        int x=48+k*84;
        Color fill=k>=f->capacity?PANEL:k>=f->size?BG:f->data[k]==99?FAULT:SAFE;
        DrawRectangleRounded((Rectangle){x,323,76,86},.22f,8,fill);
        DrawRectangleRoundedLinesEx((Rectangle){x,323,76,86},.22f,8,k==f->active?3:1,k==f->active?ACCENT:MUTED);
        DrawText(TextFormat("[%d]",k),x+6,331,13,MUTED);
        DrawText(k<f->size?TextFormat("%d",f->data[k]):"--",x+20,360,25,INK);
        if(f->valid && k==f->it) {
            DrawText(f->it==f->size?"end":"it",x+20,281,17,ACCENT);
            DrawLine(x+38,304,x+38,317,ACCENT);
            DrawTriangle((Vector2){x+33,313},(Vector2){x+38,322},(Vector2){x+43,313},ACCENT);
        }
    }
    int progress=v->level==2?(f->passed?100:f->pushed?90:f->removed?(f->valid&&f->it==f->size?75:50):f->found?25:0):v->level==0?f->size*100/6:0;
    if(v->level!=1) {
        DrawText(TextFormat("SYSTEM PROGRESS  %d%%",progress),48,438,18,ACCENT);
        DrawRectangleRounded((Rectangle){48,469,660,10},.7f,8,BG);
        if(progress) DrawRectangleRounded((Rectangle){48,469,660*progress/100.0f,10},.7f,8,SAFE);
    }
    DrawText(VectorEventName(f->event),48,507,17,ACCENT);
    DrawText(f->line,48,537,17,INK);
    Wrapped(f->note,48,574,674,18,f->failed?(Color){255,165,188,255}:INK);
    DrawText("Bright: elements   Dark: spare storage   Faded: outside capacity",48,667,15,MUTED);
    DrawText("MISSION",797,157,19,ACCENT);Wrapped(goals[v->level],797,192,430,18,INK);
    DrawText(VectorCode(v->level,v->variant),797,246,15,INK);
    if(v->level==2) {
        bool live=!f->done && !f->failed;
        if(Button((Rectangle){797,462,202,41},"1  Scan next",mouse,live&&f->valid&&f->it<f->size&&!f->found,false)) BossAct(boss,BOSS_SCAN);
        if(Button((Rectangle){1011,462,218,41},"2  it = erase(it)",mouse,live&&f->found,false)) BossAct(boss,BOSS_ERASE);
        if(Button((Rectangle){797,515,202,41},"3  push_back(40)",mouse,live&&f->removed&&!f->pushed&&f->valid&&f->it==f->size,false)) BossAct(boss,BOSS_PUSH);
        if(Button((Rectangle){1011,515,218,41},"4  Verify circuit",mouse,live,false)) BossAct(boss,BOSS_CHECK);
        Wrapped("Use Scan again after erasing. Reach end() before adding 40. Never reuse the old iterator after push_back.",797,585,425,17,INK);
        if(Button((Rectangle){40,720,155,43},"Back",mouse,boss->step>0,false)) boss->step--;
        if(Button((Rectangle){211,720,155,43},"Reset",mouse,true,false)) BossReset(boss);
    } else {
        const char *q=v->level==0?"What does size() return after three pushes?":"Which operation rescues it after erasing?";
        const char *choices0[]={"3","6","capacity"};
        const char *choices1[]={"Read old it","++old it","it = erase(it)"};
        Wrapped(q,797,439,430,17,INK);
        for(int k=0;k<3;k++) if(Button((Rectangle){797+146*k,482,135,42},TextFormat("%c  %s",'A'+k,v->level==0?choices0[k]:choices1[k]),mouse,v->answer<0,v->answer==k)) VectorChoose(v,k);
        if(v->answer>=0) {
            int answer=v->level==0?1:2;
            DrawText(v->answer==answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+answer),797,545,20,ACCENT);
            Wrapped(v->level==0?"Size counts elements. Capacity counts available storage and depends on the implementation.":"The old iterator is invalid. Use erase's return, and check it is not end() before reading.",797,581,425,17,INK);
            if(v->level==1) {
                if(Button((Rectangle){797,641,135,35},"Original",mouse,true,v->variant==1)) VectorReplay(v,1);
                if(Button((Rectangle){943,641,135,35},"Run fix",mouse,true,v->variant==0)) VectorReplay(v,0);
                if(Button((Rectangle){1089,641,140,35},"Last element",mouse,true,v->variant==2)) VectorReplay(v,2);
            }
        }
        bool more=v->answer>=0 && v->step+1<v->n;
        if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
        if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;VectorNext(v);}
        if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) VectorBack(v);
        if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) VectorLoad(v,v->level,v->level==1?1:0);
    }
}
