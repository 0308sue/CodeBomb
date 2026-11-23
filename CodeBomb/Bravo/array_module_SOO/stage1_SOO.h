#ifndef STAGE1_SOO_H
#define STAGE1_SOO_H

#include <stdbool.h>
#include <string.h>

/* 한글: 각 미션의 문제와 예제 코드를 한곳에 모은다.
 * English: Keep each mission's question and sample code together. */
typedef struct {
    const char *title,*goal,*code,*question,*choices[3],*reason;
    int answer;
} SooIntro;
static const SooIntro SOO_INTRO[6]={
    {"Power Up","Turn on all five circuits.","for (int i = 0; i < 5; i++) {\n    cells[i] = 1;\n}","What is the last index written?",{"3","4","5"},"The loop stops when i reaches 5.",1},
    {"Find the Signal","Find the index of signal 7.","int find(int arr[], int size, int target) {\n    for (int i = 0; i < size; i++) {\n        if (arr[i] == target) return i;\n    }\n    return -1;\n}","What does find return?",{"7","3","2"},"7 is the value. Its zero-based index is 2.",2},
    {"Repair the Circuit","Change every 0 to 1.","for (int i = 0; i < 4; i++) {\n    if (cells[i] == 0) {\n        cells[i] = 1;\n    }\n}","How many writes does the loop make?",{"2","4","0"},"Only indices 1 and 3 start at zero.",0},
    {"Copy the Data","Copy source into target in the same order.","for (int i = 0; i < 4; i++) {\n    target[i] = source[i];\n}","What is target[2] after copying?",{"2","6","8"},"The third source value is 8.",2},
    {"Reverse the Signal","Reverse the array by swapping pairs.","for (int i = 0; i < size / 2; i++) {\n    int other = size - 1 - i;\n    int temp = arr[i];\n    arr[i] = arr[other];\n    arr[other] = temp;\n}","Which value stays in the middle?",{"3","2","4"},"An odd-length array keeps its middle value.",0},
    {"Grid Activation","Turn on all nine circuits in row order.","for (int row = 0; row < 3; row++) {\n    for (int col = 0; col < 3; col++) {\n        grid[row][col] = 1;\n    }\n}","Which cell follows [0][2]?",{"[0][3]","[1][0]","[1][2]"},"The next row starts with column 0.",1}
};

/* 한글: 단계별 기록에는 실제 배열 주소 대신 칸 번호와 값을 저장한다.
 * English: Record slot indices and values instead of memory addresses. */
typedef enum { INTRO_LOOP,INTRO_READ,INTRO_WRITE,INTRO_RETURN,INTRO_END } IntroKind;
typedef struct { IntroKind kind; int slot,value,i,j; } IntroEvent;
typedef struct {
    int cells[9], active,i,j,result,writes;
    bool returned,done,passed;
} IntroState;
typedef struct { int level,step,answer,count; bool playing; float timer; IntroEvent trace[24]; } IntroView;

static const int INTRO_INITIAL[6][9]={{0},{3,5,7,2,9},{1,0,1,0},{4,2,8,6},{1,2,3,4,5},{0}};
static const int INTRO_GOAL[6][9]={{1,1,1,1,1},{3,5,7,2,9},{1,1,1,1},{4,2,8,6,4,2,8,6},{5,4,3,2,1},{1,1,1,1,1,1,1,1,1}};

/* 한글: 예제의 읽기와 쓰기를 기록한다. 뒤집기에서는 덮기 전에 양쪽 값을 읽는다.
 * English: Record reads and writes. Reversal reads both values before overwriting either. */
static void IntroLoad(IntroView *v,int level)
{
    *v=(IntroView){.level=level,.answer=-1};
    const int *initial=INTRO_INITIAL[level];
    #define ADD(K,S,V,I,J) v->trace[v->count++]=(IntroEvent){K,S,V,I,J}
    int iterations=(int[]){5,5,4,4,2,9}[level];
    for(int k=0;k<iterations;k++) {
        int i=level==5?k/3:k, j=level==5?k%3:-1;
        if(level!=5) { ADD(INTRO_LOOP,-1,0,i,j); }
        if(level==0 || level==5) { ADD(INTRO_WRITE,k,1,i,j); }
        if(level==1 || level==2 || level==3) {
            ADD(INTRO_READ,k,0,i,j);
            if(level==1 && initial[k]==7) { ADD(INTRO_RETURN,k,k,i,j); break; }
            if(level==2 && initial[k]==0) { ADD(INTRO_WRITE,k,1,i,j); }
            if(level==3) { ADD(INTRO_WRITE,k+4,initial[k],i,j); }
        }
        if(level==4) {
            int other=4-k;
            ADD(INTRO_READ,k,0,i,other);
            ADD(INTRO_READ,other,0,i,other);
            ADD(INTRO_WRITE,k,initial[other],i,other);
            ADD(INTRO_WRITE,other,initial[k],i,other);
        }
    }
    ADD(INTRO_END,-1,0,-1,-1);
    #undef ADD
}

/* 한글: 처음부터 다시 재생하면 Back도 동일한 상태를 복원한다.
 * English: Replaying from the start makes Back restore the same state. */
static IntroState IntroAt(const IntroView *v)
{
    IntroState s={.active=-1,.i=-1,.j=-1};
    memcpy(s.cells,INTRO_INITIAL[v->level],sizeof s.cells);
    for(int k=0;k<v->step;k++) {
        IntroEvent e=v->trace[k];
        s.active=e.slot; s.i=e.i; s.j=e.j;
        if(e.kind==INTRO_WRITE) { s.cells[e.slot]=e.value; s.writes++; }
        if(e.kind==INTRO_RETURN) { s.result=e.value; s.returned=true; }
        if(e.kind==INTRO_END) {
            s.done=true;
            s.passed=v->level==1?(s.returned && s.result==2):
                memcmp(s.cells,INTRO_GOAL[v->level],sizeof s.cells)==0;
        }
    }
    return s;
}

static void IntroChoose(IntroView *v,int choice)
{
    if(v->answer<0) { v->answer=choice; v->playing=true; v->timer=0; }
}
static void IntroNext(IntroView *v)
{
    if(v->answer>=0 && v->step<v->count) v->step++;
    if(v->step==v->count) v->playing=false;
}

#ifndef SOO_STAGE1_HEADLESS
static void DrawIntro(IntroView *v,Vector2 mouse)
{
    const SooIntro *m=&SOO_INTRO[v->level];
    IntroState s=IntroAt(v);
    Workshop();
    DrawText(TextFormat("%02d / %s",v->level+1,m->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText("CIRCUIT BOARD",48,157,19,ACCENT);
    DrawText("MISSION",797,157,19,ACCENT);
    Wrapped(m->goal,797,190,430,19,INK);
    DrawText(m->code,797,250,16,INK);
    Wrapped(m->question,797,430,430,18,INK);
    for(int k=0;k<3;k++)
        if(Button((Rectangle){797+146*k,478,135,42},TextFormat("%c  %s",'A'+k,m->choices[k]),mouse,v->answer<0,v->answer==k)) IntroChoose(v,k);
    if(v->answer>=0) {
        DrawText(v->answer==m->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+m->answer),797,543,20,ACCENT);
        Wrapped(m->reason,797,581,425,17,INK);
    } else DrawText("Choose an answer to play the example.",797,543,16,MUTED);
    int count=(int[]){5,5,4,8,5,9}[v->level];
    int cols=v->level==3?4:v->level==5?3:count;
    int rows=count/cols;
    int top=rows==3?210:rows==2?245:300;
    if(v->level==3) {
        DrawText("SOURCE",60,top+25,15,MUTED);
        DrawText("TARGET",60,top+122,15,MUTED);
        if(s.active>=4) {
            int x=385-cols*100/2+(s.active%4)*100+44;
            DrawLineEx((Vector2){x,top+78},(Vector2){x,top+99},3,ACCENT);
            DrawTriangle((Vector2){x-5,top+93},(Vector2){x,top+100},(Vector2){x+5,top+93},ACCENT);
        }
    }
    for(int k=0;k<count;k++) {
        int x=385-cols*100/2+(k%cols)*100,y=top+(k/cols)*97;
        bool binary=v->level==0 || v->level==2 || v->level==5;
        Color fill=binary?(s.cells[k]?SAFE:FAULT):BG;
        if(k==s.active || (v->level==1&&s.returned&&k==s.result)) fill=SAFE;
        DrawRectangleRounded((Rectangle){x,y,88,78},.22f,10,fill);
        if(k==s.active) DrawRectangleRoundedLinesEx((Rectangle){x,y,88,78},.22f,10,3,ACCENT);
        DrawText(v->level==5?TextFormat("[%d][%d]",k/3,k%3):TextFormat("[%d]",k%cols),x+9,y+8,15,INK);
        DrawText(TextFormat("%d",s.cells[k]),x+32,y+34,27,INK);
        if(binary) CardFace(x+73,y+58,!s.cells[k]);
    }
    const char *log="Pick an answer to begin.";
    if(v->step) {
        IntroEvent e=v->trace[v->step-1];
        switch(e.kind) {
            case INTRO_LOOP: log=TextFormat("Loop i = %d",e.i); break;
            case INTRO_READ: log=TextFormat("Read index %d: %d",e.slot,s.cells[e.slot]); break;
            case INTRO_WRITE: log=v->level==5?TextFormat("Write grid[%d][%d] = %d",e.i,e.j,e.value):
                TextFormat("Write %s[%d] = %d",v->level==3?"target":"array",e.slot%cols,e.value); break;
            case INTRO_RETURN: log=TextFormat("Found 7 at index %d. Return %d.",s.result,s.result); break;
            case INTRO_END: log=s.passed?(s.returned?"Sample passed. Returned index 2.":"Sample passed. The values match the goal."):
                (v->level==1?"Failed: the returned index does not match.":"Failed: the final values do not match the goal."); break;
        }
    }
    DrawText(TextFormat("STEP %d / %d   |   WRITES %d",v->step,v->count,s.writes),48,522,17,ACCENT);
    Wrapped(log,48,556,677,19,INK);
    if(v->level==4 && s.i>=0) {
        DrawText(TextFormat("Pair: [%d] <--> [%d]   |   temp = %d",s.i,s.j,INTRO_INITIAL[4][s.i]),48,606,17,MUTED);
        DrawLine(385-cols*100/2+s.i*100+44,392,385-cols*100/2+s.j*100+44,392,ACCENT);
    }
    if(v->level==5 && s.i>=0) DrawText(TextFormat("row = %d    col = %d",s.i,s.j),48,606,17,MUTED);
    bool more=v->answer>=0 && v->step<v->count;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;IntroNext(v);}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) {v->playing=false;v->step--;}
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) IntroLoad(v,v->level);
    DrawText("A/B/C: answer    Space: play/pause    Arrows: step/back    R: reset",40,775,15,MUTED);
}
#endif
#endif
