#include "raylib.h"
#include "array_core_SOO.h"
#include "quiz_SOO.h"
#include "pointer_SOO.h"
#include "roadmap_SOO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 한글: 1280x800 논리 화면을 비율 유지하여 확대한다.
 * English: Scale a 1280x800 canvas with letterboxing when resized. */
#define W 1280
#define H 800
static const Color BG={29,23,51,255}, PANEL={48,39,75,255};
static const Color INK={255,246,251,255}, MUTED={199,188,225,255};
static const Color SAFE={39,116,96,255}, FAULT={152,62,97,255}, ACCENT={177,228,255,255};
/* 한글: 테마는 기본 도형으로 그려 별도 이미지 없이 실행된다.
 * English: Draw the theme with primitives so no image assets are required. */
static void Workshop(void)
{
    ClearBackground(BG);
    for(int x=16;x<W;x+=24) for(int y=16;y<H;y+=24)
        DrawCircle(x,y,1,(Color){65,52,92,255});
    DrawRectangleRounded((Rectangle){20,12,1240,96},.24f,10,(Color){38,30,65,255});
    DrawCircle(486,52,17,(Color){249,184,221,255});
    DrawLine(486,34,491,27,ACCENT);
    DrawCircle(492,26,3,ACCENT);
    DrawCircle(480,51,2,BG); DrawCircle(493,51,2,BG);
    DrawLine(483,58,486,60,BG); DrawLine(486,60,490,57,BG);
}

static void CardFace(int x,int y,bool fault)
{
    Color tint=fault?(Color){255,180,203,255}:(Color){165,244,207,255};
    DrawCircle(x-5,y,2,tint); DrawCircle(x+5,y,2,tint);
    DrawLine(x-3,y+5,x,y+(fault?3:7),tint);
    DrawLine(x,y+(fault?3:7),x+3,y+5,tint);
}

typedef struct {
    SooState state;
    SooEvent trace[SOO_MAX_EVENTS];
    int level, count, cursor;
    SooScenario scenario;
    bool playing, completed[SOO_LEVEL_COUNT];
    bool quiz, debug;
    int answer;
    float timer, interval;
} App;

/* 한글: 마우스 좌표도 논리 화면에 맞춰 전달한다.
 * English: Mouse positions arrive in the same logical coordinate system. */
static bool Button(Rectangle r,const char *label,Vector2 mouse,bool enabled,bool selected)
{
    bool hover=enabled && CheckCollisionPointRec(mouse,r);
    Color fill=selected ? (Color){85,65,127,255} : PANEL;
    if(hover) fill=(Color){103,77,143,255};
    DrawRectangleRounded(r,.13f,6,fill);
    DrawRectangleRoundedLinesEx(r,.13f,6,1,selected?ACCENT:(Color){91,74,123,255});
    int size=17;
    while(size>12 && MeasureText(label,size)>r.width-18) size--;
    DrawText(label,(int)(r.x+(r.width-MeasureText(label,size))/2),(int)(r.y+(r.height-size)/2),size,enabled?INK:MUTED);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* 한글: 패널 폭에 맞춰 긴 설명을 단어 단위로 줄바꿈한다.
 * English: Wrap long explanations by word to keep text inside each panel. */
static void Wrapped(const char *text,int x,int y,int width,int size,Color color)
{
    char word[256],line[512]="";
    const char *p=text;
    while(*p) {
        int len=0;
        while(*p==' ') p++;
        while(*p && *p!=' ' && len<254) word[len++]=*p++;
        word[len]=0;
        if(!len) break;
        char candidate[768];
        snprintf(candidate,sizeof(candidate),"%s%s%s",line,line[0]?" ":"",word);
        if(MeasureText(candidate,size)>width && line[0]) {
            DrawText(line,x,y,size,color); y+=size+7; line[0]=0;
        }
        size_t used=strlen(line);
        snprintf(line+used,sizeof(line)-used,"%s%s",used?" ":"",word);
    }
    if(line[0]) DrawText(line,x,y,size,color);
}

/* 한글: 레벨/샘플 변경 시 데이터, 커서와 타이머를 함께 초기화한다.
 * English: Reset data, cursor and timer together when a level/trace changes. */
static void Load(App *a)
{
    SooReset(&a->state,&SOO_LEVELS[a->level]);
    a->count=SooBuildSample(a->state.level,a->scenario,a->trace,SOO_MAX_EVENTS);
    a->cursor=0; a->timer=0; a->playing=false; a->answer=-1;
    if(a->count<0) { a->count=0; a->state.status=SOO_FAILURE; }
}

/* 한글: 한 번에 한 이벤트만 적용하여 읽기/쓰기 차이를 보여준다.
 * English: Apply one event at a time to distinguish reads from writes. */
static void Step(App *a)
{
    if(a->cursor>=a->count || a->state.status>=SOO_SUCCESS) { a->playing=false; return; }
    SooApplyEvent(&a->state,a->trace[a->cursor++]);
    if(a->state.status==SOO_SUCCESS && a->scenario==SOO_CORRECT &&
       (!a->quiz || (a->debug ? a->answer==1 : a->answer==SOO_QUIZZES[a->level].answer))) a->completed[a->level]=true;
    if(a->state.status>=SOO_SUCCESS || a->cursor>=a->count) a->playing=false;
}

/* 한글: 이전 단계는 초기 상태부터 재생하여 정확하게 복원한다.
 * English: Replay from the initial state for deterministic backward stepping. */
static void Back(App *a)
{
    int target=a->cursor>0 ? a->cursor-1 : 0;
    SooReset(&a->state,&SOO_LEVELS[a->level]);
    a->cursor=0; a->playing=false; a->timer=0;
    while(a->cursor<target) Step(a);
}

/* 한글: 색상 외에도 값과 SAFE/FAULT, 목표를 표시하여 정보를 중복 제공한다.
 * English: Values, labels and targets convey meaning without relying only on colour. */
static void Cards(const App *a)
{
    const SooLevel *l=a->state.level;
    int cell=l->cols>8?64:l->rows>1?83:80, gap=9;
    int left=34+(742-l->cols*(cell+gap)+gap)/2;
    int top=l->rows>1?270:360;
    for(int r=0;r<l->rows;r++) for(int c=0;c<l->cols;c++) {
        int k=r*l->cols+c,x=left+c*(cell+gap),y=top+r*(cell+gap);
        bool active=a->state.active_row==r && a->state.active_col==c;
        DrawRectangleRounded((Rectangle){x,y,cell,cell},.12f,6,a->state.cells[k]?FAULT:SAFE);
        if(active) DrawRectangleRoundedLinesEx((Rectangle){x-3,y-3,cell+6,cell+6},.12f,6,3,ACCENT);
        DrawText(l->rows==1?TextFormat("[%d]",c):TextFormat("[%d][%d]",r,c),x+6,y+6,15,WHITE);
        DrawText(TextFormat("%d",a->state.cells[k]),x+cell/2-7,y+cell/2-9,25,WHITE);
        DrawText(a->state.cells[k]?"FAULT":"SAFE",x+6,y+cell-18,12,WHITE);
        CardFace(x+cell-13,y+cell-14,a->state.cells[k]!=0);
        if(l->rows==1) DrawText(TextFormat("to %d",l->goal[k]),x+6,y+cell+10,15,MUTED);
        else DrawText(TextFormat("to %d",l->goal[k]),x+cell-32,y+24,12,WHITE);
    }
}

/* 한글: 답을 고르면 선택을 기록하고 기존 정답 기록을 재생한다.
 * English: Record the choice, then reveal the existing correct trace. */
static void Answer(App *a,int choice)
{
    if(!a->quiz || a->answer>=0 || choice<0 || choice>2) return;
    a->answer=choice;
    a->playing=true;
    a->timer=0;
}

static void DrawQuiz(App *a,Vector2 mouse)
{
    const SooQuiz *q=&SOO_QUIZZES[a->level];
    DrawText("READ THE CODE",824,295,16,ACCENT);
    DrawText(SooSampleCode(a->state.level,SOO_CORRECT),824,320,15,INK);
    Wrapped(q->question,824,480,414,18,INK);
    for(int n=0;n<3;n++) {
        char label[50];
        snprintf(label,sizeof(label),"%c  %s",'A'+n,q->choices[n]);
        if(Button((Rectangle){824+n*140,535,133,42},label,mouse,a->answer<0,a->answer==n))
            Answer(a,n);
    }
    if(a->answer<0) DrawText("Choose A, B or C to reveal the trace.",824,595,16,MUTED);
    else {
        bool correct=a->answer==q->answer;
        DrawText(correct?"Correct!":TextFormat("Not quite. Answer: %c",'A'+q->answer),824,595,18,correct?ACCENT:(Color){255,147,157,255});
        Wrapped(q->reason,824,622,416,16,INK);
    }
}

/* 한글: 같은 초기 배열에서 원본/수정 예제를 재생한다. 답은 유지한다.
 * English: Replay either example from the same initial array; keep the answer. */
static void CompareBounds(App *a,SooScenario scenario)
{
    int answer=a->answer;
    a->scenario=scenario;
    Load(a);
    a->answer=answer;
    a->playing=true;
}

static void DrawBounds(App *a,Vector2 mouse)
{
    bool fixed=a->scenario==SOO_CORRECT;
    DrawText(fixed?"FIXED SAMPLE":"ORIGINAL SAMPLE",824,295,16,ACCENT);
    /* 한글: C 코드는 학습용으로 표시한다. 범위 검사는 재생 엔진이 한다.
     * English: Show the C example; the trace engine checks bounds before access. */
    DrawText(TextFormat("for (int i = 0; i %s 8; ++i) {\n"
                        "    if (cards[i] == 1)\n"
                        "        cards[i] = 0;\n}",fixed?"<":"<="),
             824,320,19,INK);
    Wrapped("Which loop condition visits all 8 cards safely?",824,480,414,18,INK);
    const char *choices[]={"A  i <= 8","B  i < 8","C  i < 7"};
    for(int n=0;n<3;n++)
        if(Button((Rectangle){824+n*140,535,133,42},choices[n],mouse,a->answer<0,a->answer==n)) Answer(a,n);
    if(a->answer<0) DrawText("Choose A, B or C.",824,595,16,MUTED);
    else {
        DrawText(a->answer==1?"Correct: i < 8":"Not quite. Use i < 8.",824,595,18,a->answer==1?ACCENT:(Color){255,147,157,255});
        Wrapped(a->answer==2?"i < 7 skips card 7. i < 8 visits indices 0 to 7.":
                "Eight cards use indices 0 to 7. i <= 8 tries to read card 8.",824,622,416,16,INK);
        DrawText(TextFormat("Step %d / %d   |   i = %s",a->cursor,a->count,
                 a->state.i<0?"--":TextFormat("%d",a->state.i)),824,680,15,MUTED);
        if(Button((Rectangle){35,720,220,43},"Run original",mouse,true,!fixed)) CompareBounds(a,SOO_BOUNDS);
        if(Button((Rectangle){267,720,220,43},"Run fix",mouse,true,fixed)) CompareBounds(a,SOO_CORRECT);
    }
}

/* 한글: 편집기는 팀원 담당이므로 샘플 재생 화면임을 명시한다.
 * English: Clearly label sample playback; the teammate's editor is not included. */
static void DrawApp(App *a,Vector2 mouse)
{
    Workshop();
    DrawText("SEOUL  <---->  DUBLIN",30,20,18,ACCENT);
    DrawText("Circuit Pals / Arrays",30,47,30,INK);
    DrawText(a->debug?"FIND THE BOUNDS BUG":a->quiz?"PREDICT THE RESULT":"EXPLORE SAMPLE TRACES",30,85,17,MUTED);
    if(Button((Rectangle){541,60,155,42},"Predict",mouse,true,a->quiz&&!a->debug) && (!a->quiz||a->debug)) {
        a->quiz=true; a->debug=false; a->scenario=SOO_CORRECT; Load(a);
    }
    if(Button((Rectangle){707,60,155,42},"Explore",mouse,true,!a->quiz) && a->quiz) {
        a->quiz=false; a->debug=false; a->scenario=SOO_CORRECT; Load(a);
    }
    if(Button((Rectangle){873,60,166,42},"Find the bug",mouse,true,a->debug) && !a->debug) {
        a->quiz=true; a->debug=true; a->level=1; a->scenario=SOO_BOUNDS; Load(a);
    }
    DrawText(TextFormat("Speed %.2fs / step",a->interval),1050,85,17,MUTED);
    for(int n=0;n<SOO_LEVEL_COUNT;n++) {
        char label[32]; snprintf(label,sizeof(label),"%d  %s",n+1,a->completed[n]?"DEFUSED":"LEVEL");
        if(Button((Rectangle){30+n*205,119,193,44},label,mouse,!a->debug,a->level==n)) { a->level=n; Load(a); }
    }
    const SooLevel *l=a->state.level;
    DrawRectangleRounded((Rectangle){20,183,770,526},.025f,6,PANEL);
    DrawText(l->title,38,200,25,INK); DrawText(l->concept,38,233,18,ACCENT);
    Cards(a);
    if(l->rows != 4) DrawText("0 = SAFE   1 = FAULT   to = TARGET   blue outline = accessed card",38,683,16,MUTED);
    DrawRectangleRounded((Rectangle){806,183,454,526},.025f,6,PANEL);
    DrawText("MISSION",824,200,19,ACCENT);
    Wrapped(a->debug?"Fix the loop so it checks all 8 cards without going out of bounds.":l->objective,824,231,415,19,INK);
    if(a->debug) DrawBounds(a,mouse);
    else if(a->quiz) {
        DrawQuiz(a,mouse);
        if(a->answer>=0) DrawText(TextFormat("i = %d   j = %d   |   Memory ops: %d / %d",
            a->state.i,a->state.j,a->state.operations,l->budget),824,680,15,MUTED);
    } else {
        char iv[20],jv[20];
        if(a->state.i<0) snprintf(iv,sizeof(iv),"--"); else snprintf(iv,sizeof(iv),"%d",a->state.i);
        if(a->state.j<0) snprintf(jv,sizeof(jv),"--"); else snprintf(jv,sizeof(jv),"%d",a->state.j);
        DrawText(TextFormat("i = %s   j = %s",iv,jv),824,315,22,INK);
        DrawText(TextFormat("MEMORY OPS  %d / %d",a->state.operations,l->budget),824,350,20,INK);
        DrawRectangle(824,381,410,9,(Color){45,58,79,255});
        DrawRectangle(824,381,(int)(410.0f*a->state.operations/l->budget),9,a->state.operations>=l->budget?FAULT:ACCENT);
        DrawText("REFERENCE TRACE / NOT EDITABLE",824,410,16,MUTED);
        DrawText(SooSampleCode(l,a->scenario),824,441,15,INK);
        DrawText(TextFormat("Trace step %d / %d",a->cursor,a->count),824,627,18,MUTED);
        DrawText("Read/write = 1 op. Loop update = 0.",824,657,16,MUTED);
        const char *scenarios[]={"Correct","Missing step","Out of bounds","Repeated scans"};
        DrawText("COMPARE",35,733,16,MUTED);
        for(int n=0;n<4;n++) if(Button((Rectangle){127+n*155,720,147,43},scenarios[n],mouse,true,a->scenario==(SooScenario)n)) {
            a->scenario=(SooScenario)n; Load(a);
        }
    }
    bool revealed=!a->quiz || a->answer>=0;
    bool live=revealed && a->state.status<SOO_SUCCESS;
    if(a->quiz && !a->debug) DrawText(revealed?"Review the trace or reset to try again.":"Choose an answer before playing the trace.",35,733,17,MUTED);
    if(Button((Rectangle){810,720,101,43},a->playing?"Pause":"Play",mouse,live,false)) a->playing=!a->playing;
    if(Button((Rectangle){920,720,101,43},"Step",mouse,live,false)) { a->playing=false; Step(a); }
    if(Button((Rectangle){1030,720,101,43},"Back",mouse,a->cursor>0,false)) Back(a);
    if(Button((Rectangle){1140,720,110,43},"Reset",mouse,true,false)) { if(a->debug) a->scenario=SOO_BOUNDS; Load(a); }
    DrawText("Space: play/pause    Right: step    Left: back    R: reset    1-6: level    S: speed    F11: fullscreen",30,774,15,MUTED);
    /* 한글: 4행 격자 아래에 로그를 두어 카드를 가리지 않는다.
     * English: Put logs below the four-row grid so cards stay visible. */
    Color status=a->state.status==SOO_FAILURE?(Color){255,147,157,255}:a->state.status==SOO_SUCCESS?(Color){93,227,184,255}:INK;
    int log_y=l->rows==4?641:573;
    /* 한글: 퀴즈에서는 선택의 정오와 샘플 자체의 성공을 구분한다.
     * English: Keep quiz correctness separate from the sample's outcome. */
    bool sample_done=a->quiz && a->state.status==SOO_SUCCESS;
    const char *heading=sample_done?"SAMPLE COMPLETE":a->state.status==SOO_SUCCESS?"DEFUSED":
        a->state.status==SOO_FAILURE?"SIGNAL FAILED":"EXECUTION LOG";
    const char *message=a->quiz && a->answer<0?(a->debug?"Find the condition that keeps i inside the array.":"Read the code and predict what happens."):
        sample_done?TextFormat("The sample reached the target in %d operations.",a->state.operations):
        a->state.message;
    DrawText(heading,40,log_y,17,status);
    Wrapped(message,40,log_y+22,727,l->rows==4?13:17,status);
}

/* 한글: 미리 정해진 예제만 재생한다. 팀원 에디터 없이도 답을 고르고 확인할 수 있다.
 * English: Play a fixed lesson so players can answer and inspect it without the editor. */
typedef struct { int choice, step; bool playing; float elapsed; } PointerView;

static void PointerNext(PointerView *v)
{
    if(v->choice>=0 && v->step<SOO_SWAP_LAST) v->step++;
    if(v->step==SOO_SWAP_LAST) v->playing=false;
}

static void PointerAnswer(PointerView *v,int choice)
{
    if(v->choice<0) { v->choice=choice; v->playing=true; v->elapsed=0; }
}

/* 한글: 포인터의 대상과 현재 값을 따로 그려 주소와 값의 차이를 보여준다.
 * English: Draw pointer targets separately from values to distinguish the two. */
static void DrawPointer(PointerView *v,Vector2 mouse)
{
    const char *notes[]={
        "Predict the result before running the sample.",
        "&a and &b give the addresses. p points to a, and q points to b.",
        "temp = *p saves the original value of a (3).",
        "*p = *q copies 7 into a. Both a and b now hold 7.",
        "*q = temp copies the saved 3 into b. The values are swapped."
    };
    Workshop();
    DrawText("SEOUL  <---->  DUBLIN",30,20,18,ACCENT);
    DrawText("Circuit Pals / Pointers",30,47,30,INK);
    DrawRectangleRounded((Rectangle){20,143,750,555},.025f,6,PANEL);
    DrawRectangleRounded((Rectangle){790,143,470,555},.025f,6,PANEL);
    DrawText("Swap two values",42,165,24,INK);
    DrawText("Pointers hold addresses",42,200,18,ACCENT);
    SooSwapFrame f=SooSwapAt(v->step);
    if(v->step>=1) {
        DrawText("p holds &a",136,280,20,ACCENT);
        DrawText("q holds &b",363,280,20,ACCENT);
        for(int x=190;x<=417;x+=227) {
            DrawLine(x,310,x,353,ACCENT);
            DrawTriangle((Vector2){x-7,344},(Vector2){x,355},(Vector2){x+7,344},ACCENT);
        }
    }
    const char *names[]={"a","b","temp"};
    const char *values[]={TextFormat("%d",f.a),TextFormat("%d",f.b),v->step>=2?TextFormat("%d",f.temp):"--"};
    for(int k=0;k<3;k++) {
        int x=105+k*227;
        bool changed=(v->step==2&&k==2)||(v->step==3&&k==0)||(v->step==4&&k==1);
        DrawRectangleRounded((Rectangle){x,355,170,105},.1f,6,changed?SAFE:BG);
        DrawText(names[k],x+15,369,20,INK);
        DrawText(values[k],x+64,403,28,INK);
    }
    DrawText(TextFormat("STEP %d / %d",v->step,SOO_SWAP_LAST),42,530,19,ACCENT);
    Wrapped(notes[v->step],42,568,700,20,INK);
    DrawText("MISSION",812,165,18,ACCENT);
    Wrapped("After swap_ints(&a, &b), what values do a and b hold?",812,195,415,18,INK);
    DrawText("void swap_ints(int *p, int *q) {\n"
             "    int temp = *p;\n"
             "    *p = *q;\n"
             "    *q = temp;\n"
             "}\n\n"
             "int a = 3, b = 7;\n"
             "swap_ints(&a, &b);",812,258,17,INK);
    const char *choices[]={"A  3, 7","B  7, 3","C  7, 7"};
    for(int k=0;k<3;k++)
        if(Button((Rectangle){812+k*143,478,133,42},choices[k],mouse,v->choice<0,v->choice==k))
            PointerAnswer(v,k);
    if(v->choice>=0) {
        DrawText(SooSwapCorrect(v->choice)?"Correct!":"Not quite. Answer: B",812,545,20,ACCENT);
        Wrapped("temp keeps the old value of a before a is overwritten.",812,583,415,18,INK);
    } else DrawText("Choose A, B or C.",812,545,18,MUTED);
    bool more=v->choice>=0 && v->step<SOO_SWAP_LAST;
    if(Button((Rectangle){35,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){205,720,155,43},"Step",mouse,more,false)) { v->playing=false; PointerNext(v); }
    if(Button((Rectangle){375,720,155,43},"Back",mouse,v->step>0,false)) { v->playing=false; v->step--; }
    if(Button((Rectangle){545,720,155,43},"Reset",mouse,true,false)) *v=(PointerView){.choice=-1};
    DrawText("A/B/C: answer   Space: play/pause   Arrows: step/back   R: reset   F11: fullscreen",35,774,15,MUTED);
}

/* 한글: 챕터별 카드에 레벨을 배치하고 구현된 레벨만 열 수 있게 한다.
 * English: Lay out numbered missions by chapter and open only playable ones. */
static int DrawRoadmap(Vector2 mouse)
{
    static const Color chapterColors[] = {
        {255,187,220,255},{173,231,211,255},{255,209,164,255},{187,196,255,255},
        {167,223,250,255},{231,193,255,255},{254,222,151,255},{255,175,182,255}
    };
    Workshop();
    DrawText("SEOUL  <-->  DUBLIN",42,26,15,ACCENT);
    DrawText("Circuit Pals",42,48,34,INK);
    DrawText("MISSION MAP",550,36,22,INK);
    DrawText("40 planned levels  /  40 ready to play",550,65,17,MUTED);
    DrawText("Choose a mission",40,128,23,INK);
    DrawText("Bright = playable    Dim = planned",885,135,16,MUTED);
    int chosen=0;
    for(int n=0;n<8;n++) {
        const SooChapter *ch=&SOO_CHAPTERS[n];
        int x=36+(n%2)*613, y=170+(n/2)*145;
        Color tint=chapterColors[n];
        DrawRectangleRounded((Rectangle){x,y+4,595,132},.18f,10,(Color){22,17,39,255});
        DrawRectangleRounded((Rectangle){x,y,595,132},.18f,10,PANEL);
        DrawRectangleRoundedLinesEx((Rectangle){x,y,595,132},.18f,10,1,Fade(tint,.42f));
        DrawCircle(x+34,y+34,17,tint);
        DrawText(TextFormat("%d",n+1),x+29,y+26,16,BG);
        DrawText(ch->title,x+61,y+20,22,INK);
        DrawText(ch->difficulty,x+62,y+49,15,tint);
        for(int level=ch->first;level<=ch->last;level++) {
            bool ready=SOO_ROUTES[level]!=0;
            Rectangle slot={x+61+(level-ch->first)*65,y+78,55,36};
            bool hover=ready&&CheckCollisionPointRec(mouse,slot);
            DrawRectangleRounded(slot,.36f,8,ready?(hover?WHITE:tint):BG);
            DrawText(TextFormat("%02d",level),(int)slot.x+16,(int)slot.y+9,18,ready?BG:MUTED);
            if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) chosen=SOO_ROUTES[level];
        }
    }
    DrawText("C and C++ learning game    /    F11: full screen",40,770,16,MUTED);
    return chosen;
}

#include "stage1_SOO.h"
#include "stage2_SOO.h"
#include "stage3_SOO.h"
#include "stage3_view_SOO.h"
#include "stage4_SOO.h"
#include "stage4_view_SOO.h"
#include "stage5_SOO.h"
#include "stage5_view_SOO.h"
#include "stage6_SOO.h"
#include "stage6_view_SOO.h"
#include "stage7_SOO.h"
#include "stage7_view_SOO.h"
#include "stage8_SOO.h"
#include "stage8_view_SOO.h"

/* 한글: 숫자 카드 아래의 인덱스와 위쪽 포인터를 따로 표시한다.
 * English: Show pointers above the value cards and indices inside them. */
static void DrawStage2(PointerLesson *v,Vector2 mouse)
{
    const PointerTask *task=&POINTER_TASKS[v->mission];
    const PointerFrame *f=&v->frames[v->step];
    Workshop();
    DrawText(TextFormat("%02d / %s",v->mission+7,task->title),40,45,28,INK);
    DrawRectangleRounded((Rectangle){25,133,729,560},.07f,10,PANEL);
    DrawRectangleRounded((Rectangle){775,133,480,560},.08f,10,PANEL);
    DrawText("POINTER BOARD",48,158,19,ACCENT);
    DrawText("MISSION",797,157,19,ACCENT);
    Wrapped(task->goal,797,192,430,19,INK);
    DrawText(task->code,797,239,16,INK);
    for(int k=0;k<3;k++) {
        if(Button((Rectangle){797+146*k,478,135,42},TextFormat("%c  %s",'A'+k,task->choices[k]),mouse,v->answer<0,v->answer==k)) {
            v->answer=k; v->playing=true;
        }
    }
    if(v->answer>=0) {
        DrawText(v->answer==task->answer?"Correct!":TextFormat("Not quite. Answer: %c",'A'+task->answer),797,543,20,ACCENT);
        Wrapped(task->reason,797,581,425,17,INK);
    } else DrawText("Choose A, B or C to play the example.",797,543,16,MUTED);
    int count=POINTER_SLOTS[v->mission], start=385-count*112/2;
    for(int k=0;k<count;k++) {
        int x=start+k*112;
        bool first=k==f->left, second=k==f->right;
        DrawRectangleRounded((Rectangle){x,315,96,92},.23f,10,first||second?SAFE:BG);
        DrawText(count==2?(k?"WIRE B":"WIRE A"):TextFormat("[%d]",k),x+10,328,15,MUTED);
        DrawText(v->mission==1?TextFormat("%c",f->cells[k]):TextFormat("%d",f->cells[k]),x+30,361,29,INK);
        if(first || second) {
            const char *label=count==2?(first?"a":"b"):v->mission==3?(first&&second?"left + right":first?"left":"right"):TextFormat("arr + %d",k);
            DrawText(label,x+3,254,17,ACCENT);
            DrawLineEx((Vector2){x+48,280},(Vector2){x+48,310},2,ACCENT);
            DrawTriangle((Vector2){x+43,303},(Vector2){x+48,312},(Vector2){x+53,303},ACCENT);
        }
    }
    if(v->mission==2) DrawText(v->step?TextFormat("Max so far: %d",f->total):"Max so far: --",48,451,22,ACCENT);
    else if(v->mission==4) {
        DrawText(TextFormat("POWER  %d / 20",f->total),48,445,20,ACCENT);
        DrawRectangleRounded((Rectangle){48,480,420,12},.6f,8,BG);
        if(f->total) DrawRectangleRounded((Rectangle){48,480,420*f->total/20.0f,12},.6f,8,SAFE);
    } else if(f->left>=0) DrawText(v->mission==1?
        (f->temp?TextFormat("temp: %c",f->temp):"temp: --"):
        (f->temp?TextFormat("temp: %d",f->temp):"temp: --"),48,451,22,ACCENT);
    DrawText(TextFormat("STEP %d / %d",v->step,v->count-1),48,524,17,ACCENT);
    Wrapped(f->line,48,555,670,19,INK);
    bool more=v->answer>=0 && v->step+1<v->count;
    if(Button((Rectangle){40,720,155,43},v->playing?"Pause":"Play",mouse,more,false)) v->playing=!v->playing;
    if(Button((Rectangle){211,720,155,43},"Step",mouse,more,false)) {v->playing=false;v->step++;}
    if(Button((Rectangle){382,720,155,43},"Back",mouse,v->step>0,false)) {v->playing=false;v->step--;}
    if(Button((Rectangle){553,720,155,43},"Reset",mouse,true,false)) PointerLessonLoad(v,v->mission);
    DrawText("A/B/C: answer    Space: play/pause    Arrows: step/back    R: reset",40,775,15,MUTED);
}

int main(int argc,char **argv)
{
    /* 한글: --smoke는 테스트용 캡처를 저장하고 종료한다.
     * English: --smoke saves a deterministic QA screenshot and exits. */
    bool smoke=argc>1 && strcmp(argv[1],"--smoke")==0;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(W,H,"Array Circuit SOO | C and Raylib");
    /* 한글: 창 초기화가 실패하면 그래픽 자원을 만들지 않는다.
     * English: Do not allocate graphics resources if window setup failed. */
    if(!IsWindowReady()) {
        fputs("Could not open the Raylib window. Run from a desktop session.\n",stderr);
        return EXIT_FAILURE;
    }
    SetWindowMinSize(768,480);
    if(!smoke && !(argc>1 && strcmp(argv[1],"--pointer-smoke")==0)) {
        int monitor=GetCurrentMonitor();
        int width=GetMonitorWidth(monitor)*85/100;
        int height=GetMonitorHeight(monitor)*80/100;
        if(width>0 && height>0) {
            if(width>W) width=W;
            if(width>height*W/H) width=height*W/H;
            if(width<768) width=768;
            SetWindowSize(width,width*H/W);
        }
    }
    SetTargetFPS(60);
    RenderTexture2D canvas=LoadRenderTexture(W,H);
    SetTextureFilter(canvas.texture,TEXTURE_FILTER_BILINEAR);
    App a={0}; a.quiz=true; a.interval=.45f; Load(&a);
    bool pointerSmoke=argc>1 && strcmp(argv[1],"--pointer-smoke")==0;
    bool mapSmoke=argc>1 && strcmp(argv[1],"--map-smoke")==0;
    bool map=mapSmoke || (!smoke && !pointerSmoke);
    bool pointers=pointerSmoke;
    bool introSmoke=argc>1 && strcmp(argv[1],"--intro-smoke")==0;
    bool intro=introSmoke;
    IntroView lesson={0};
    bool stage2Smoke=argc>1 && strcmp(argv[1],"--stage2-smoke")==0;
    bool stage2=stage2Smoke;
    PointerLesson pointerLesson={0};
    bool stage3Smoke=argc>1 && strcmp(argv[1],"--stage3-smoke")==0;
    bool stage3=stage3Smoke;
    ArrayLesson arrayLesson={0};
    bool stage4Smoke=argc>1 && strcmp(argv[1],"--stage4-smoke")==0;
    bool stage4=stage4Smoke;
    GridLesson gridLesson={0};
    bool stage5Smoke=argc>1 && strcmp(argv[1],"--stage5-smoke")==0;
    bool stage5=stage5Smoke;
    StringLesson stringLesson={0};
    bool stage6Smoke=argc>1 && strcmp(argv[1],"--stage6-smoke")==0;
    bool stage6=stage6Smoke;
    ListLesson listLesson={0};
    bool stage7Smoke=argc>1 && strcmp(argv[1],"--stage7-smoke")==0;
    bool stage7=stage7Smoke;
    MemoryLesson memoryLesson={0};
    bool stage8Smoke=argc>1 && strcmp(argv[1],"--stage8-smoke")==0;
    bool stage8=stage8Smoke;
    VectorLesson vectorLesson={0};
    VectorBossView vectorBoss={0};
    if(stage8Smoke) {
        int level=argc>2?atoi(argv[2])-38:2;
        if(level<0 || level>2) level=2;
        map=false;VectorLoad(&vectorLesson,level,argc>3?atoi(argv[3]):0);BossReset(&vectorBoss);
        if(level==2) {
            BossAct(&vectorBoss,BOSS_SCAN);BossAct(&vectorBoss,BOSS_SCAN);
            BossAct(&vectorBoss,BOSS_ERASE);BossAct(&vectorBoss,BOSS_SCAN);BossAct(&vectorBoss,BOSS_SCAN);
            BossAct(&vectorBoss,BOSS_PUSH);BossAct(&vectorBoss,BOSS_CHECK);
        } else {VectorChoose(&vectorLesson,level==0?1:2);vectorLesson.step=vectorLesson.n-1;vectorLesson.playing=false;}
    }
    if(stage7Smoke) {
        int level=argc>2?atoi(argv[2])-34:3;
        if(level<0 || level>3) level=3;
        map=false;MemoryLoad(&memoryLesson,level);
        if(argc>3) {int variant=atoi(argv[3]);MemoryBuild(&memoryLesson,variant>=0&&variant<=2?variant:0);}
        MemoryChoose(&memoryLesson,MEMORY_TASKS[level].answer);
        memoryLesson.step=memoryLesson.n-1;memoryLesson.playing=false;
    }
    if(stage6Smoke) {
        int level=argc>2?atoi(argv[2])-28:5;
        if(level<0 || level>5) level=5;
        map=false; ListLoad(&listLesson,level);
        ListChoose(&listLesson,LIST_TASKS[level].answer);
        int step=argc>3?atoi(argv[3]):listLesson.n-1;
        listLesson.step=step>=0 && step<listLesson.n?step:listLesson.n-1;
        listLesson.playing=false;
    }
    if(stage5Smoke) {
        int level=argc>2?atoi(argv[2])-23:3;
        if(level<0 || level>4) level=3;
        map=false; StringLoad(&stringLesson,level);
        stringLesson.answer=STRING_TASKS[level].answer;stringLesson.step=stringLesson.n-1;
    }
    if(stage4Smoke) {
        int level=argc>2?atoi(argv[2])-18:4;
        if(level<0 || level>4) level=4;
        map=false; GridLessonLoad(&gridLesson,level);
        gridLesson.answer=GRID_TASKS[level].answer;gridLesson.step=gridLesson.n-1;
    }
    if(stage3Smoke) {
        int level=argc>2?atoi(argv[2])-12:5;
        if(level<0 || level>5) level=5;
        map=false; ArrayLessonLoad(&arrayLesson,level);
        if(level==5) { ArrayPick(&arrayLesson,0); ArrayPick(&arrayLesson,4); }
        else { arrayLesson.answer=ARRAY_TASKS[level].answer; arrayLesson.step=arrayLesson.n-1; }
    }
    if(stage2Smoke) {
        int n=argc>2?atoi(argv[2])-7:4;
        if(n<0 || n>4) n=4;
        map=false; PointerLessonLoad(&pointerLesson,n);
        pointerLesson.answer=POINTER_TASKS[n].answer;
        pointerLesson.step=pointerLesson.count-1;
    }
    if(introSmoke) {
        map=false;
        int level=argc>2?atoi(argv[2]):4;
        IntroLoad(&lesson,level>=1&&level<=6?level-1:3);
        lesson.answer=SOO_INTRO[lesson.level].answer;
        lesson.step=lesson.count;
    }
    PointerView pointer={.choice=-1};
    if(pointerSmoke) { PointerAnswer(&pointer,1); pointer.step=3; pointer.playing=false; }
    int frames=0;
    while(!WindowShouldClose()) {
        /* 한글: 그리기와 클릭 판정에 같은 비율을 사용한다.
         * English: Apply the same scale to rendering and hit testing. */
        if(IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        if(GetScreenWidth()<=0 || GetScreenHeight()<=0) continue;
        float sx=(float)GetScreenWidth()/W,sy=(float)GetScreenHeight()/H,scale=sx<sy?sx:sy;
        float ox=(GetScreenWidth()-W*scale)/2,oy=(GetScreenHeight()-H*scale)/2;
        Vector2 raw=GetMousePosition(),mouse={(raw.x-ox)/scale,(raw.y-oy)/scale};
        if(map) {
            /* 한글: 지도에서는 미션 단축키를 적용하지 않는다.
             * English: Mission shortcuts stay inactive on the map. */
        } else if(stage8) {
            VectorLesson *v=&vectorLesson;
            if(v->level==2) {
                if(IsKeyPressed(KEY_R)) BossReset(&vectorBoss);
                if(IsKeyPressed(KEY_LEFT) && vectorBoss.step>0) vectorBoss.step--;
            } else {
                if(IsKeyPressed(KEY_R)) VectorLoad(v,v->level,v->level==1?1:0);
                if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) VectorChoose(v,k);
                bool more=v->answer>=0 && v->step+1<v->n;
                if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
                if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;VectorNext(v);}
                if(IsKeyPressed(KEY_LEFT)) VectorBack(v);
                if(v->playing && v->step+1<v->n) {
                    v->timer+=GetFrameTime();
                    if(v->timer>=.85f) {v->timer=0;VectorNext(v);}
                }
            }
        } else if(stage7) {
            MemoryLesson *v=&memoryLesson;
            if(IsKeyPressed(KEY_R)) MemoryLoad(v,v->level);
            if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) MemoryChoose(v,k);
            bool more=v->answer>=0 && v->step+1<v->n;
            if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
            if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;MemoryNext(v);}
            if(IsKeyPressed(KEY_LEFT)) MemoryBack(v);
            if(v->playing && v->step+1<v->n) {
                v->timer+=GetFrameTime();
                if(v->timer>=.85f) {v->timer=0;MemoryNext(v);}
            }
        } else if(stage6) {
            ListLesson *v=&listLesson;
            if(IsKeyPressed(KEY_R)) ListLoad(v,v->level);
            if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) ListChoose(v,k);
            bool more=v->answer>=0 && v->step+1<v->n;
            if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
            if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;ListAdvance(v);}
            if(IsKeyPressed(KEY_LEFT)) ListBack(v);
            if(v->playing && v->step+1<v->n) {
                v->timer+=GetFrameTime();
                if(v->timer>=.85f) {v->timer=0;ListAdvance(v);}
            }
        } else if(stage5) {
            StringLesson *v=&stringLesson;
            if(v->level==4 && v->answer<0) {
                char previous[STRING_CAP]; strcpy(previous,v->input);
                int ch;
                while((ch=GetCharPressed())>0) {
                    size_t len=strlen(v->input);
                    if(ch>=32 && ch<=126 && len<8) {v->input[len]=(char)ch;v->input[len+1]=0;}
                }
                size_t len=strlen(v->input);
                if(IsKeyPressed(KEY_BACKSPACE) && len) v->input[len-1]=0;
                if(strcmp(previous,v->input)!=0) StringBuild(v);
                if(IsKeyPressed(KEY_ENTER)) {StringBuild(v);v->answer=0;v->playing=true;}
            } else {
                if(IsKeyPressed(KEY_R)) StringLoad(v,v->level);
                if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) {v->answer=k;v->playing=true;}
            }
            bool more=v->answer>=0 && v->step+1<v->n;
            if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
            if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;v->step++;}
            if(v->step>0 && IsKeyPressed(KEY_LEFT)) {v->playing=false;v->step--;}
            if(v->playing && v->step+1<v->n) {
                v->timer+=GetFrameTime();
                if(v->timer>=.65f) {v->timer=0;v->step++;}
            }
            if(v->step+1==v->n) v->playing=false;
        } else if(stage4) {
            GridLesson *v=&gridLesson;
            if(IsKeyPressed(KEY_R)) GridLessonLoad(v,v->level);
            if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) {v->answer=k;v->playing=true;}
            bool more=v->answer>=0 && v->step+1<v->n;
            if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
            if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;v->step++;}
            if(v->step>0 && IsKeyPressed(KEY_LEFT)) {v->playing=false;v->step--;}
            if(v->playing && v->step+1<v->n) {
                v->timer+=GetFrameTime();
                if(v->timer>=.65f) {v->timer=0;v->step++;}
            }
            if(v->step+1==v->n) v->playing=false;
        } else if(stage3) {
            ArrayLesson *v=&arrayLesson;
            if(IsKeyPressed(KEY_R)) ArrayLessonLoad(v,v->level);
            if(v->level!=5) {
                if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) {v->answer=k;v->playing=true;}
                bool more=v->answer>=0 && v->step+1<v->n;
                if(more && IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
                if(more && IsKeyPressed(KEY_RIGHT)) {v->playing=false;v->step++;}
                if(v->step>0 && IsKeyPressed(KEY_LEFT)) {v->playing=false;v->step--;}
                if(v->playing && v->step+1<v->n) {
                    v->timer+=GetFrameTime();
                    if(v->timer>=.85f) {v->timer=0;v->step++;}
                }
                if(v->step+1==v->n) v->playing=false;
            }
        } else if(stage2) {
            PointerLesson *v=&pointerLesson;
            if(IsKeyPressed(KEY_R)) PointerLessonLoad(v,v->mission);
            if(v->answer<0) for(int k=0;k<3;k++) if(IsKeyPressed(KEY_A+k)) {
                v->answer=k; v->playing=true;
            }
            if(v->answer>=0 && v->step+1<v->count) {
                if(IsKeyPressed(KEY_SPACE)) v->playing=!v->playing;
                if(IsKeyPressed(KEY_RIGHT)) {v->playing=false;v->step++;}
            }
            if(IsKeyPressed(KEY_LEFT) && v->step>0) {v->playing=false;v->step--;}
            if(v->playing && v->step+1<v->count) {
                v->timer+=GetFrameTime();
                if(v->timer>=.85f) {v->timer=0;v->step++;}
            }
            if(v->step+1==v->count) v->playing=false;
        } else if(intro) {
            if(IsKeyPressed(KEY_R)) IntroLoad(&lesson,lesson.level);
            if(IsKeyPressed(KEY_A)) IntroChoose(&lesson,0);
            if(IsKeyPressed(KEY_B)) IntroChoose(&lesson,1);
            if(IsKeyPressed(KEY_C)) IntroChoose(&lesson,2);
            if(IsKeyPressed(KEY_SPACE) && lesson.answer>=0 && lesson.step<lesson.count)
                lesson.playing=!lesson.playing;
            if(IsKeyPressed(KEY_RIGHT)) { lesson.playing=false; IntroNext(&lesson); }
            if(IsKeyPressed(KEY_LEFT) && lesson.step>0) { lesson.playing=false; lesson.step--; }
            if(lesson.playing) {
                lesson.timer+=GetFrameTime();
                if(lesson.timer>=.65f) { lesson.timer=0; IntroNext(&lesson); }
            }
        } else if(pointers) {
            if(IsKeyPressed(KEY_R)) pointer=(PointerView){.choice=-1};
            if(IsKeyPressed(KEY_A)) PointerAnswer(&pointer,0);
            if(IsKeyPressed(KEY_B)) PointerAnswer(&pointer,1);
            if(IsKeyPressed(KEY_C)) PointerAnswer(&pointer,2);
            if(IsKeyPressed(KEY_SPACE) && pointer.choice>=0 && pointer.step<SOO_SWAP_LAST)
                pointer.playing=!pointer.playing;
            if(IsKeyPressed(KEY_RIGHT)) { pointer.playing=false; PointerNext(&pointer); }
            if(IsKeyPressed(KEY_LEFT) && pointer.step>0) { pointer.playing=false; pointer.step--; }
            if(pointer.playing) {
                pointer.elapsed+=GetFrameTime();
                if(pointer.elapsed>=1.35f) { pointer.elapsed=0; PointerNext(&pointer); }
            }
        } else {
            for(int n=0;n<6;n++) if(!a.debug && IsKeyPressed(KEY_ONE+n)) { a.level=n; Load(&a); }
            if(IsKeyPressed(KEY_R)) { if(a.debug) a.scenario=SOO_BOUNDS; Load(&a); }
            if(IsKeyPressed(KEY_S)) a.interval=a.interval>.3f?.15f:a.interval>.1f?.05f:.45f;
            if(IsKeyPressed(KEY_A)) Answer(&a,0);
            if(IsKeyPressed(KEY_B)) Answer(&a,1);
            if(IsKeyPressed(KEY_C)) Answer(&a,2);
            bool revealed=!a.quiz || a.answer>=0;
            if(revealed && IsKeyPressed(KEY_SPACE) && a.state.status<SOO_SUCCESS) a.playing=!a.playing;
            if(revealed && IsKeyPressed(KEY_RIGHT)) { a.playing=false; Step(&a); }
            if(revealed && IsKeyPressed(KEY_LEFT)) Back(&a);
            if(a.playing) { a.timer+=GetFrameTime(); if(a.timer>=a.interval) { a.timer=0; Step(&a); } }
            if(smoke && frames==0) {
                /* 한글: QA 인자는 레벨(1~6), 시나리오(0~3), 단계 수다.
                 * English: QA arguments are level (1-6), scenario (0-3), step count. */
                a.level=argc>2?atoi(argv[2])-1:3;
                if(a.level<0 || a.level>=SOO_LEVEL_COUNT) a.level=3;
                int scenario=argc>3?atoi(argv[3]):0;
                a.scenario=scenario>=0 && scenario<=3?(SooScenario)scenario:SOO_CORRECT;
                a.quiz=argc>5;
                a.debug=argc>6;
                if(a.debug) { a.level=1; a.scenario=argc>7?SOO_CORRECT:SOO_BOUNDS; }
                else if(a.quiz) a.scenario=SOO_CORRECT;
                Load(&a);
                if(a.quiz) Answer(&a,atoi(argv[5]));
                int steps=argc>4?atoi(argv[4]):8;
                if(!a.quiz || a.answer>=0) for(int k=0;k<steps;k++) Step(&a);
            }
        }
        BeginTextureMode(canvas);
        if(map) {
            int route=DrawRoadmap(mouse);
            if(route) {
                map=false; pointers=route==7; intro=route>=11 && route<=16; stage2=route>=21 && route<=25;
                stage3=route>=31 && route<=36;
                stage4=route>=41 && route<=45;
                stage5=route>=51 && route<=55;
                stage6=route>=61 && route<=66;
                stage7=route>=71 && route<=74;
                stage8=route>=81 && route<=83;
                if(stage8) {VectorLoad(&vectorLesson,route-81,route==82?1:0);BossReset(&vectorBoss);}
                else if(stage7) MemoryLoad(&memoryLesson,route-71);
                else if(stage6) ListLoad(&listLesson,route-61);
                else if(stage5) StringLoad(&stringLesson,route-51);
                else if(stage4) GridLessonLoad(&gridLesson,route-41);
                else if(stage3) ArrayLessonLoad(&arrayLesson,route-31);
                else if(stage2) PointerLessonLoad(&pointerLesson,route-21);
                else if(intro) IntroLoad(&lesson,route-11);
                else if(pointers) pointer=(PointerView){.choice=-1};
                else { a.level=route-1; a.quiz=true; a.debug=false; a.scenario=SOO_CORRECT; Load(&a); }
            }
        } else {
        if(stage8) DrawStage8(&vectorLesson,&vectorBoss,mouse);
        else if(stage7) DrawStage7(&memoryLesson,mouse);
        else if(stage6) DrawStage6(&listLesson,mouse);
        else if(stage5) DrawStage5(&stringLesson,mouse);
        else if(stage4) DrawStage4(&gridLesson,mouse);
        else if(stage3) DrawStage3(&arrayLesson,mouse);
        else if(stage2) DrawStage2(&pointerLesson,mouse);
        else if(intro) {
            DrawIntro(&lesson,mouse);
            if(Button((Rectangle){1060,58,185,42},"Find the bug",mouse,true,false)) {
                intro=false; lesson.playing=false;
                a.quiz=true; a.debug=true; a.level=1; a.scenario=SOO_BOUNDS; Load(&a);
            }
        } else {
        if(pointers) DrawPointer(&pointer,mouse); else DrawApp(&a,mouse);
        if(Button((Rectangle){1060,pointers?65:16,185,42},pointers?"Array missions":"Pointers",mouse,true,false)) {
            a.playing=false; pointer.playing=false; pointers=!pointers;
        }
        }
        if(Button((Rectangle){870,16,170,38},"Mission map",mouse,true,false)) {
            a.playing=false; pointer.playing=false; lesson.playing=false; pointerLesson.playing=false; arrayLesson.playing=false; gridLesson.playing=false; stringLesson.playing=false; listLesson.playing=false; memoryLesson.playing=false; vectorLesson.playing=false; map=true;
        }
        }
        EndTextureMode();
        BeginDrawing(); ClearBackground((Color){19,15,34,255});
        DrawTexturePro(canvas.texture,(Rectangle){0,0,W,-H},(Rectangle){ox,oy,W*scale,H*scale},(Vector2){0,0},0,WHITE);
        EndDrawing();
        if((mapSmoke || introSmoke || stage2Smoke || stage3Smoke || stage4Smoke || stage5Smoke || stage6Smoke || stage7Smoke || stage8Smoke) && ++frames==3) {
            Image preview=LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&preview);
            ExportImage(preview,stage8Smoke?"stage8_preview_SOO.png":stage7Smoke?"stage7_preview_SOO.png":stage6Smoke?"stage6_preview_SOO.png":stage5Smoke?"stage5_preview_SOO.png":stage4Smoke?"stage4_preview_SOO.png":stage3Smoke?"stage3_preview_SOO.png":stage2Smoke?"stage2_preview_SOO.png":introSmoke?"stage1_preview_SOO.png":"roadmap_preview_SOO.png");
            UnloadImage(preview);
            break;
        }
        if(pointerSmoke && ++frames==3) { TakeScreenshot("pointer_preview_SOO.png"); break; }
        if(smoke && ++frames==3) { TakeScreenshot("preview_SOO.png"); break; }
    }
    UnloadRenderTexture(canvas); CloseWindow(); return 0;
}
