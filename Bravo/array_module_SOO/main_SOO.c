#include "raylib.h"
#include "array_core_SOO.h"
#include "quiz_SOO.h"
#include "pointer_SOO.h"
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
    DrawCircle(466,52,17,(Color){249,184,221,255});
    DrawLine(466,34,471,27,ACCENT);
    DrawCircle(472,26,3,ACCENT);
    DrawCircle(460,51,2,BG); DrawCircle(473,51,2,BG);
    DrawLine(463,58,466,60,BG); DrawLine(466,60,470,57,BG);
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
    bool pointers=pointerSmoke;
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
        if(pointers) {
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
        if(pointers) DrawPointer(&pointer,mouse); else DrawApp(&a,mouse);
        if(Button((Rectangle){1060,pointers?65:16,185,42},pointers?"Array missions":"Pointers",mouse,true,false)) {
            a.playing=false; pointer.playing=false; pointers=!pointers;
        }
        EndTextureMode();
        BeginDrawing(); ClearBackground((Color){19,15,34,255});
        DrawTexturePro(canvas.texture,(Rectangle){0,0,W,-H},(Rectangle){ox,oy,W*scale,H*scale},(Vector2){0,0},0,WHITE);
        EndDrawing();
        if(pointerSmoke && ++frames==3) { TakeScreenshot("pointer_preview_SOO.png"); break; }
        if(smoke && ++frames==3) { TakeScreenshot("preview_SOO.png"); break; }
    }
    UnloadRenderTexture(canvas); CloseWindow(); return 0;
}
