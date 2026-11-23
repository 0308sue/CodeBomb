#ifndef STAGE3_SOO_H
#define STAGE3_SOO_H
#include <stdbool.h>

/* 한글: 카드 값과 별도로 빈 칸을 기록한다. 값 0도 유효한 데이터다.
 * English: Track empty slots separately; zero can be valid data. */
typedef struct { int a[6],used,from,to,maxAt,secondAt,count,saved; const char *note; } ArrayFrame;
typedef struct { int level,step,n,answer,pick[2]; float timer; bool playing; ArrayFrame frames[16]; } ArrayLesson;
typedef struct { const char *title,*goal,*code,*question,*choices[3],*reason; int answer; } ArrayTask;
static const ArrayTask ARRAY_TASKS[]={
 {"Second Power","Find the second largest distinct value.",
  "for (int i = 0; i < size; ++i) {\n    int x = arr[i];\n    if (!hasMax || x > max) {\n        second = max; hasSecond = hasMax;\n        max = x; hasMax = true;\n    } else if (x < max &&\n               (!hasSecond || x > second)) {\n        second = x; hasSecond = true;\n    }\n}",
  "What is the second largest distinct value?",{"9","8","5"},"9 is the maximum. 8 is the next distinct value.",1},
 {"Insert the Chip","Insert 99 at index 2. Capacity is five.",
  "for (int i = count; i > index; --i) {\n    arr[i] = arr[i - 1];\n}\narr[index] = 99;\n++count;",
  "Which value shifts first?",{"30","20","40"},"Shift 40 first, then 30. Moving backwards saves both values.",2},
 {"Remove the Virus","Delete index 2 and keep the other values in order.",
  "for (int i = index; i < count - 1; ++i) {\n    arr[i] = arr[i + 1];\n}\n--count;",
  "Which value ends up at index 2?",{"30","99","40"},"30 moves left into the deleted slot.",0},
 {"Frequency Scanner","Count every occurrence of signal 3.",
  "int matches = 0;\nfor (int i = 0; i < size; ++i) {\n    if (arr[i] == 3) ++matches;\n}\nreturn matches;",
  "How many times does 3 appear?",{"2","3","4"},"The matches are at indices 0, 2 and 4.",1},
 {"Rotate the Circuit","Rotate the array left by one position.",
  "int first = arr[0];\nfor (int i = 0; i < size - 1; ++i) {\n    arr[i] = arr[i + 1];\n}\narr[size - 1] = first;",
  "Which value goes into the last slot?",{"5","2","1"},"Save the first value before shifting. Put it at the end.",2},
 {"Pair the Batteries","Choose two distinct batteries. The target is 120.",
  "for (int i = 0; i < size; ++i)\n    for (int j = i + 1; j < size; ++j)\n        if (power[i] + power[j] == 120)\n            return true;\nreturn false;",
  "",{"","",""},"",0}
};

/* 한글: 같은 최대값은 후보에서 제외한다. 두 번째 값이 없으면 -1을 유지한다.
 * English: Ignore duplicate maxima. Leave secondAt at -1 if no runner-up exists. */
static void ArrayRank(ArrayFrame *f,int i)
{
    if(f->maxAt<0 || f->a[i]>f->a[f->maxAt]) {
        f->secondAt=f->maxAt; f->maxAt=i;
    } else if(f->a[i]<f->a[f->maxAt] &&
              (f->secondAt<0 || f->a[i]>f->a[f->secondAt])) f->secondAt=i;
}

/* 한글: 미리 준비한 예제를 실행하고 각 조작 이후의 배열을 복사한다.
 * English: Run each prepared example and save the board after every operation. */
static void ArrayLessonLoad(ArrayLesson *v,int level)
{
    *v=(ArrayLesson){.level=level,.answer=-1,.pick={-1,-1}};
    const int start[][6]={{3,9,2,8,5},{10,20,30,40},{10,20,99,30,40},{3,1,3,2,3,4},{1,2,3,4,5},{30,45,60,75,90}};
    ArrayFrame f={.used=(int[]){5,4,5,6,5,5}[level],.from=-1,.to=-1,.maxAt=-1,.secondAt=-1};
    for(int i=0;i<6;i++) f.a[i]=start[level][i];
    #define SAVE(msg) do { f.note=(msg); v->frames[v->n++]=f; } while(0)
    SAVE(level==5?"Select two different cards. Select a card again to remove it.":"Choose an answer to see the sample.");
    if(level==0 || level==3) {
        for(int i=0;i<f.used;i++) {
            f.from=i;
            if(level==0) ArrayRank(&f,i);
            else if(f.a[i]==3) f.count++;
            SAVE(level==0?"Read the chip and update the two best values.":f.a[i]==3?"Match found. Add one.":"No match. Keep the count.");
        }
        f.from=-1;
        SAVE(level==0?"Return 8, the second largest distinct value.":"Return 3, the number of matches.");
    } else if(level==1) {
        f.used++;
        for(int i=f.used-1;i>2;i--) {
            f.a[i]=f.a[i-1]; f.from=i-1; f.to=i;
            SAVE("Copy the value one slot to the right.");
        }
        f.a[2]=99; f.from=-1; f.to=2;
        SAVE("Insert 99 in the space at index 2.");
    } else if(level==2 || level==4) {
        int begin=level==2?2:0;
        f.from=begin;
        if(level==4) f.saved=f.a[0];
        SAVE(level==2?"Mark the chip at index 2 for removal.":"Save the first value before shifting.");
        for(int i=begin;i<f.used-1;i++) {
            f.a[i]=f.a[i+1]; f.from=i+1; f.to=i;
            SAVE("Copy the next value one slot to the left.");
        }
        f.from=-1;
        if(level==2) { f.used--; f.to=-1; SAVE("Reduce the count. The last slot is no longer used."); }
        else { f.to=f.used-1; f.a[f.to]=f.saved; SAVE("Put the saved value in the last slot."); }
    }
    #undef SAVE
}

/* 한글: 같은 카드를 다시 누르면 해제한다. 선택은 최대 두 칸이다.
 * English: Clicking a selected slot removes it. At most two slots are selected. */
static void ArrayPick(ArrayLesson *v,int slot)
{
    if(slot<0 || slot>=v->frames[0].used) return;
    if(v->pick[0]==slot) { v->pick[0]=v->pick[1]; v->pick[1]=-1; }
    else if(v->pick[1]==slot) v->pick[1]=-1;
    else if(v->pick[0]<0) v->pick[0]=slot;
    else if(v->pick[1]<0) v->pick[1]=slot;
}
static int ArrayPairSum(const ArrayLesson *v)
{
    int sum=0;
    for(int k=0;k<2;k++) if(v->pick[k]>=0) sum+=v->frames[0].a[v->pick[k]];
    return sum;
}
#endif
