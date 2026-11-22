#ifndef STAGE2_SOO_H
#define STAGE2_SOO_H
#include <stdbool.h>

/* 한글: 화면에는 메모리 주소 대신 가리키는 칸 번호를 저장한다.
 * English: Store target slots in the view, not live memory addresses. */
typedef struct { int cells[5],left,right,total,temp; const char *line; } PointerFrame;
typedef struct { int mission,step,answer,count; float timer; bool playing; PointerFrame frames[20]; } PointerLesson;
typedef struct { const char *title,*goal,*code,*choices[3],*reason; int answer; } PointerTask;
static const PointerTask POINTER_TASKS[]={
 {"Swap the Wires","What values remain in A and B?","void swap_ints(int *a, int *b) {\n    int temp = *a;\n    *a = *b;\n    *b = temp;\n}",{"10, 20","20, 10","20, 20"},"temp keeps 10 before the first write.",1},
 {"Character Switch","What letters remain in A and B?","void swap_chars(char *a, char *b) {\n    char temp = *a;\n    *a = *b;\n    *b = temp;\n}",{"A B","B A","A A"},"A char pointer changes the character it points to.",0},
 {"Find the Power","Which value does the function return?","int find_max(int *arr, int size) {\n    if (size <= 0) return 0;\n    int max = *arr;\n    for (int i = 1; i < size; i++)\n        if (*(arr + i) > max)\n            max = *(arr + i);\n    return max;\n}",{"8","1","9"},"The highest value is 9, at offset 1.",2},
 {"Reverse the Circuit","What is the final order?","void reverse_array(int *arr, int size) {\n    if (size < 2) return;\n    int *left = arr;\n    int *right = arr + size - 1;\n    while (left < right) {\n        int temp = *left;\n        *left = *right;\n        *right = temp;\n        left++; right--;\n    }\n}",{"54321","52341","12345"},"Swap both outside pairs. The middle stays 3.",0},
 {"Power Calculator","What total power does the function return?","int sum_array(int *arr, int size) {\n    int sum = 0;\n    for (int i = 0; i < size; i++)\n        sum += *(arr + i);\n    return sum;\n}",{"18","20","24"},"Each dereferenced value is added once: 2 + 4 + 6 + 8.",1}
};
static const int POINTER_SLOTS[]={2,2,5,5,4};

/* 한글: 정해진 예제를 실행하면서 각 줄의 결과를 값으로 복사한다.
 * English: Run the built-in example and save a copy after each operation. */
static void PointerLessonLoad(PointerLesson *v,int mission)
{
    *v=(PointerLesson){.mission=mission,.answer=-1};
    const int initial[][5]={{10,20},{'B','A'},{3,9,2,8,5},{1,2,3,4,5},{2,4,6,8}};
    PointerFrame f={.left=-1,.right=-1};
    for(int k=0;k<5;k++) f.cells[k]=initial[mission][k];
    #define SAVE(msg) do { f.line=(msg); v->frames[v->count++]=f; } while(0)
    SAVE("Choose an answer to start the sample.");
    if(mission==0 || mission==1 || mission==3) {
        int *left=f.cells,*right=f.cells+POINTER_SLOTS[mission]-1;
        char letters[]={'B','A'};
        char *a=letters,*b=letters+1;
        while(left<right) {
            f.left=(int)(left-f.cells); f.right=(int)(right-f.cells);
            SAVE("The arrows show which slots the pointers address.");
            f.temp=mission==1?*a:*left;
            SAVE("Read the left value and save it in temp.");
            if(mission==1) { *a=*b; *left=*a; } else *left=*right;
            SAVE("Read the right value and write it through the left pointer.");
            if(mission==1) { *b=(char)f.temp; *right=*b; } else *right=f.temp;
            SAVE("Write the saved value through the right pointer.");
            left++; right--;
        }
        if(mission==3) {
            f.left=(int)(left-f.cells); f.right=(int)(right-f.cells);
            SAVE("The pointers meet. The middle value stays the same.");
        }
    } else {
        int *arr=f.cells;
        for(int i=0;i<POINTER_SLOTS[mission];i++) {
            f.left=i;
            int value=*(arr+i);
            if(mission==4) f.total+=value;
            else if(i==0 || value>f.total) f.total=value;
            SAVE(mission==4?"Read the battery and add its value.":"Read the slot and keep the largest value so far.");
        }
        f.left=-1;
        SAVE(mission==4?"Return the total power.":"Return the maximum value.");
    }
    #undef SAVE
}
#endif
