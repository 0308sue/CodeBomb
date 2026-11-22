/* 한글: 배열 이동의 출발 칸과 도착 칸을 화살표로 표시한다.
 * English: An arrow identifies the source and destination of each shift. */
static void DrawStage3(ArrayLesson *v,Vector2 mouse)
{
    const ArrayTask *t=&ARRAY_TASKS[v->level];
    const ArrayFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+12,t->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText(v->level==5?"MINI BOSS / BATTERY BOARD":"ARRAY WORKBENCH",48,158,19,ACCENT);
    DrawText("MISSION",797,157,19,ACCENT);
    Wrapped(t->goal,797,192,430,19,INK);
    DrawText(t->code,797,250,15,INK);
    int slots=v->level==3?6:5, left=385-slots*103/2;
    for(int k=0;k<slots;k++) {
        int x=left+k*103;
        Rectangle tile={x,325,91,91};
        bool picked=v->pick[0]==k || v->pick[1]==k;
        bool deletion=v->level==2 && v->step==1 && k==f->from;
        Color color=deletion?FAULT:picked||k==f->to||k==f->maxAt?SAFE:
            k==f->secondAt?(Color){105,76,147,255}:BG;
        DrawRectangleRounded(tile,.22f,10,color);
        if(k==f->from || picked) DrawRectangleRoundedLinesEx(tile,.22f,10,2,ACCENT);
        DrawText(TextFormat("[%d]",k),x+8,335,15,MUTED);
        DrawText(k<f->used?TextFormat("%d",f->a[k]):"--",x+22,366,27,INK);
        if(v->level==5 && CheckCollisionPointRec(mouse,tile) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ArrayPick(v,k);
    }
    if(f->from>=0 && f->to>=0) {
        int start=left+f->from*103+45,end=left+f->to*103+45;
        int dir=end>start?1:-1;
        DrawLineEx((Vector2){start,305},(Vector2){end,305},3,ACCENT);
        DrawTriangle((Vector2){end-dir*9,299},(Vector2){end,305},(Vector2){end-dir*9,311},ACCENT);
    }
    if(v->level==0) {
        DrawText(f->maxAt<0?"Max: --":TextFormat("Max: %d",f->a[f->maxAt]),48,450,20,(Color){165,244,207,255});
        DrawText(f->secondAt<0?"Second: --":TextFormat("Second: %d",f->a[f->secondAt]),270,450,20,(Color){220,187,255,255});
    }
    if(v->level==3) DrawText(TextFormat("Matches: %d",f->count),48,450,22,ACCENT);
    if(v->level==4 && v->step) DrawText(TextFormat("Saved first value: %d",f->saved),48,450,20,ACCENT);
    if(v->level==5) {
        int sum=ArrayPairSum(v);
        bool two=v->pick[1]>=0, win=two && sum==120;
        DrawText(TextFormat("POWER  %d / 120",sum),48,454,24,ACCENT);
        DrawRectangleRounded((Rectangle){48,494,430,12},.75f,8,BG);
        if(sum) DrawRectangleRounded((Rectangle){48,494,430*(sum>120?1.0f:sum/120.0f),12},.75f,8,two&&!win?FAULT:SAFE);
        DrawText(win?"Power matched! Mission complete.":two?"Power does not match. Change a battery.":"Choose two different battery cards.",48,552,20,win?ACCENT:INK);
        if(Button((Rectangle){40,720,155,43},"Reset",mouse,true,false)) ArrayLessonLoad(v,v->level);
        return;
    }
    Wrapped(t->question,797,440,430,18,INK);
    for(int k=0;k<3;k++) if(Button((Rectangle){797+146*k,478,135,42},TextFormat("%c  %s",'A'+k,t->choices[k]),mouse,v->answer<0,v->answer==k)) {
        v->answer=k; v->playing=true;
    }
    if(v->answer>=0) {
        DrawText(v->answer==t->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+t->answer),797,543,20,ACCENT);
        Wrapped(t->reason,797,581,425,17,INK);
    }
    DrawText(TextFormat("STEP %d / %d",v->step,v->n-1),48,524,17,ACCENT);
    Wrapped(f->note,48,555,670,19,INK);
    bool more=v->answer>=0 && v->step+1<v->n;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;v->step++;}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) {v->playing=false;v->step--;}
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) ArrayLessonLoad(v,v->level);
}
