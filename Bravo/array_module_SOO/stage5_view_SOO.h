/* 한글: 문자열 끝의 널 문자와 아직 쓰지 않은 칸을 따로 보여준다.
 * English: Display null terminators separately from unwritten memory slots. */
static void StringRow(const char *text,unsigned mask,int slots,int active,int y,unsigned matched)
{
    for(int k=0;k<slots;k++) {
        int x=66+k*69;
        bool written=(mask&(1u<<k))!=0;
        DrawRectangleRounded((Rectangle){x,y,61,67},.22f,8,(matched&(1u<<k))?SAFE:written?BG:PANEL);
        DrawRectangleRoundedLinesEx((Rectangle){x,y,61,67},.22f,8,k==active?3:1,k==active?ACCENT:MUTED);
        DrawText(TextFormat("[%d]",k),x+7,y+6,13,MUTED);
        DrawText(!written?"--":text[k]?TextFormat("%c",text[k]):"\\0",x+17,y+32,22,INK);
        if(k==active) {
            DrawLine(x+30,y-19,x+30,y-3,ACCENT);
            DrawTriangle((Vector2){x+25,y-8},(Vector2){x+30,y},(Vector2){x+35,y-8},ACCENT);
        }
    }
}
static void DrawStage5(StringLesson *v,Vector2 mouse)
{
    const StringTask *t=&STRING_TASKS[v->level];
    const StringFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+23,t->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText("PASSWORD MEMORY / CHAR POINTERS",48,158,19,ACCENT);
    DrawText("MISSION",797,157,19,ACCENT);Wrapped(t->goal,797,192,430,19,INK);
    DrawText(t->code,797,245,15,INK);
    DrawText(v->level==4?"STORED PASSWORD":v->level>=2?"SOURCE":"STRING",48,221,17,MUTED);
    unsigned mask=(1u<<(strlen(f->first)+1))-1;
    unsigned vowels=0;
    if(v->level==1) for(int k=0;k<=f->firstAt;k++)
        if(StringVowel(f->first[k])) vowels|=1u<<k;
    StringRow(f->first,mask,(int)strlen(f->first)+1,f->firstAt,258,vowels);
    if(v->level>=2) {
        DrawText(v->level==4?"INPUT PASSWORD":"DESTINATION",48,367,17,MUTED);
        int slots=v->level==3?9:v->level==4?(int)strlen(v->input)+1:5;
        StringRow(f->second,f->initialized,slots,f->secondAt,401,0);
    } else DrawText(TextFormat("%s: %d",v->level==0?"LENGTH":"VOWELS",f->count),48,402,28,ACCENT);
    if(v->level!=4) {
        Wrapped(t->question,797,440,430,17,INK);
        for(int k=0;k<3;k++) if(Button((Rectangle){797+146*k,482,135,42},TextFormat("%c  %s",'A'+k,t->choices[k]),mouse,v->answer<0,v->answer==k)) {
            v->answer=k;v->playing=true;
        }
        if(v->answer>=0) {
            DrawText(v->answer==t->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+t->answer),797,545,20,ACCENT);
            Wrapped(t->reason,797,583,425,17,INK);
        }
        if(v->level==3) {
            if(Button((Rectangle){797,645,200,32},"Try 8-byte buffer",mouse,v->step+1==v->n && v->capacity==STRING_CAP,false)) {
                v->capacity=8;StringBuild(v);v->answer=t->answer;v->step=v->n-1;
            }
        }
    } else {
        DrawText("TYPE PASSWORD (up to 8 letters)",797,440,16,MUTED);
        DrawRectangleRounded((Rectangle){797,478,430,43},.1f,8,BG);
        DrawText(TextFormat("%s%s",v->input,v->answer<0?"_":""),811,490,20,INK);
        if(Button((Rectangle){797,544,200,42},"Verify / Enter",mouse,v->answer<0,false)) {StringBuild(v);v->answer=0;v->playing=true;}
        if(v->step>0) DrawText(TextFormat("%s %d (%s)",f->done?"RETURN":"BYTE DIFF",f->result,f->result==0?"equal":f->result<0?"stored < input":"stored > input"),797,605,19,ACCENT);
    }
    DrawText(TextFormat("STEP %d / %d",v->step,v->n-1),48,525,17,ACCENT);
    Wrapped(f->note,48,564,670,19,f->failed?(Color){255,160,189,255}:INK);
    DrawText("\\0 = null terminator     -- = not written",48,650,17,MUTED);
    bool more=v->answer>=0 && v->step+1<v->n;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;v->step++;}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) {v->playing=false;v->step--;}
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) StringLoad(v,v->level);
}
