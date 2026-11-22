/* 한글: -1(빈칸)과 -2(새 칸)를 별도 표기로 구분한다.
 * English: Distinguish an empty cell (-1) from a new cell (-2). */
static void DrawStage4(GridLesson *v,Vector2 mouse)
{
    const GridTask *t=&GRID_TASKS[v->level];
    const GridFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+18,t->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText("4 x 6 / MEMORY MATRIX",48,158,19,ACCENT);
    DrawText("MISSION",797,157,19,ACCENT);
    Wrapped(t->goal,797,192,430,19,INK);
    DrawText(t->code,797,245,15,INK);
    Wrapped(t->question,797,439,430,17,INK);
    for(int k=0;k<3;k++)
        if(Button((Rectangle){797+146*k,482,135,42},TextFormat("%c  %s",'A'+k,t->choices[k]),mouse,v->answer<0,v->answer==k)) {
            v->answer=k;v->playing=true;
        }
    if(v->answer>=0) {
        DrawText(v->answer==t->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+t->answer),797,545,20,ACCENT);
        Wrapped(t->reason,797,583,425,17,INK);
    } else DrawText("Choose A, B or C to start.",797,545,17,MUTED);
    for(int k=0;k<24;k++) {
        int x=91+k%6*94,y=211+k/6*76;
        Color color=f->cells[k]==-1?BG:f->cells[k]==-2?(Color){91,72,127,255}:PANEL;
        if(f->match&(1u<<k)) color=SAFE;
        DrawRectangleRounded((Rectangle){x,y,82,64},.25f,8,color);
        DrawRectangleRoundedLinesEx((Rectangle){x,y,82,64},.25f,8,k==f->active?3:1,k==f->active?ACCENT:MUTED);
        DrawText(TextFormat("[%d][%d]",k/6,k%6),x+7,y+5,12,MUTED);
        DrawText(TextFormat("%d",f->cells[k]),x+29,y+27,24,INK);
    }
    if(f->from>=0 && f->active>=0) {
        int x=91+f->from%6*94+75,y1=211+f->from/6*76+32,y2=211+f->active/6*76+32;
        DrawLineEx((Vector2){x,y1},(Vector2){x,y2},3,ACCENT);
        DrawTriangle((Vector2){x-5,y2-8},(Vector2){x,y2},(Vector2){x+5,y2-8},ACCENT);
    }
    DrawText(TextFormat("STEP %d/%d   SCANNED %d/24   SCORE %d",v->step,v->n-1,f->scanned,f->score),48,540,17,ACCENT);
    Wrapped(f->note,48,573,675,18,INK);
    DrawText("-1 = empty    -2 = new    green = matching cluster",48,650,16,MUTED);
    bool more=v->answer>=0 && v->step+1<v->n;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;v->step++;}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) {v->playing=false;v->step--;}
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) GridLessonLoad(v,v->level);
}
