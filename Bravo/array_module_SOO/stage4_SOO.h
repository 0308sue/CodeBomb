#ifndef STAGE4_SOO_H
#define STAGE4_SOO_H
#include <string.h>
#include <stdbool.h>

/* 한글: 각 단계는 배열과 강조 정보를 복사한다. Back은 이전 복사본을 표시한다.
 * English: Each frame copies the grid and highlights; Back selects an older frame. */
typedef struct { int cells[24],active,from,score,scanned; unsigned match; const char *note; } GridFrame;
typedef struct { int level,step,n,answer; bool playing; float timer; GridFrame frames[100]; } GridLesson;
typedef struct { int start,delta,length; } GridMatch;
typedef struct { const char *title,*goal,*code,*question,*choices[3],*reason; int answer; } GridTask;
static const GridTask GRID_TASKS[]={
 {"Grid Scanner","Visit all 24 cells from left to right, row by row.",
 "for (int row = 0; row < 4; row++) {\n    for (int col = 0; col < 6; col++) {\n        // Read array[row][col]\n    }\n}",
 "After [0][5], which cell is next?",{"[0][6]","[1][0]","[1][5]"},"Column 5 is the end of the first row.",1},
 {"Pattern Detector","Find the first line of 3 or more equal values.",
 "// For each position in row order:\n// Check horizontal first.\n// If no match, check vertical.\n// Match only values 1 through 4.",
 "Which line is the first match?",{"2 2 2","1 2 3","3 1 4"},"The first match starts at [0][1] and runs horizontally.",0},
 {"Cluster Destroyer","Replace the first cluster with -1. Return its sum.",
 "GridMatch match = firstMatch(array);\nint score = 0;\nfor (int k = 0; k < match.length; k++) {\n    int pos = match.start + k * match.delta;\n    score += array[pos / 6][pos % 6];\n    array[pos / 6][pos % 6] = -1;\n}\nreturn score;",
 "What score does removing 2 + 2 + 2 return?",{"3","2","6"},"Add the removed values: 2 + 2 + 2 = 6.",2},
 {"Gravity System","Move values down in each column. Fill the top with -2.",
 "for (int col = 0; col < 6; col++) {\n    int dest = 3;\n    for (int row = 3; row >= 0; row--) {\n        if (array[row][col] != -1) {\n            // Move value into dest.\n            --dest;\n        }\n    }\n    // Fill rows 0 through dest with -2.\n}",
 "What fills the new space at the top?",{"-1","-2","0"},"-1 means empty. New cells are filled with -2.",1},
 {"Chain Reaction","Find, score, remove and apply gravity once.",
 "int score = ReplaceWithMinusOne(array);\nFallDownAndReplace(array);\n\n// Scan positions in row order.\n// Horizontal wins at the same position.\n// Newly added -2 values cannot match.",
 "Which order is correct?",{"Fall / scan","Scan / remove / fall","Remove / scan"},"Find the first cluster, remove it, then apply gravity.",1}
};
static const int GRID_START[24]={1,2,2,2,3,4,2,3,1,4,2,3,3,1,4,2,3,1,4,2,1,3,4,2};

/* 한글: 전체 가로를 먼저 찾는 것이 아니다. 한 위치에서 가로, 세로 순으로 검사한다.
 * English: At each row-major position check horizontal, then vertical. */
static GridMatch GridFind(const int *a)
{
    for(int pos=0;pos<24;pos++) {
        if(a[pos]<1 || a[pos]>4) continue;
        int row=pos/6,col=pos%6,len=1;
        while(col+len<6 && a[pos+len]==a[pos]) len++;
        if(len>=3) return (GridMatch){pos,1,len};
        len=1;
        while(row+len<4 && a[pos+len*6]==a[pos]) len++;
        if(len>=3) return (GridMatch){pos,6,len};
    }
    return (GridMatch){-1,0,0};
}

static void GridLessonLoad(GridLesson *v,int level)
{
    *v=(GridLesson){.level=level,.answer=-1};
    GridFrame f={.active=-1,.from=-1};
    memcpy(f.cells,GRID_START,sizeof f.cells);
    if(level==3) {
        f.cells[6]=-1; f.cells[12]=-1; f.cells[18]=-1;
        f.cells[13]=-1; f.cells[19]=-1; f.cells[9]=-1;
    }
    #define SAVE(msg) do { f.note=(msg); v->frames[v->n++]=f; } while(0)
    SAVE("Choose an answer to start the sample.");
    if(level==0) {
        for(int pos=0;pos<24;pos++) {
            f.active=pos; f.scanned=pos+1;
            SAVE("Read the highlighted cell. Column advances before row.");
        }
    } else if(level!=3) {
        GridMatch m=GridFind(f.cells);
        int last=m.start>=0?m.start:23;
        for(int pos=0;pos<=last;pos++) {
            f.active=pos; f.scanned=pos+1;
            SAVE("Check horizontal, then vertical, at this position.");
        }
        if(m.length) {
            for(int k=0;k<m.length;k++) f.match|=1u<<(m.start+k*m.delta);
            SAVE("First matching cluster found.");
            if(level==2 || level==4) {
                for(int k=0;k<m.length;k++) {
                    int pos=m.start+k*m.delta;
                    f.active=pos; f.score+=f.cells[pos]; f.cells[pos]=-1;
                    SAVE("Add this value to the score and replace it with -1.");
                }
            }
        }
    }
    if(level==3 || level==4) {
        f.match=0;
        /* 한글: 아래쪽부터 채워 열 안의 상대 순서를 보존한다.
         * English: Pack from the bottom to preserve each column's order. */
        for(int col=0;col<6;col++) {
            int dest=3;
            for(int row=3;row>=0;row--) {
                int from=row*6+col,to=dest*6+col;
                if(f.cells[from]==-1) continue;
                if(row!=dest) {
                    f.cells[to]=f.cells[from]; f.cells[from]=-1;
                    f.from=from; f.active=to;
                    SAVE("Move this value down in the same column.");
                }
                dest--;
            }
            for(int row=dest;row>=0;row--) {
                f.active=row*6+col; f.from=-1; f.cells[f.active]=-2;
                SAVE("Fill the new top cell with -2.");
            }
        }
    }
    f.active=-1; f.from=-1;
    SAVE("Sample complete. Review any step with Back.");
    #undef SAVE
}
#endif
