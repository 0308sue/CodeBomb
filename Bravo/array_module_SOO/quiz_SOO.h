#ifndef QUIZ_SOO_H
#define QUIZ_SOO_H

/* 한글: 레벨마다 정답 샘플을 읽고 풀 수 있는 문제 하나를 제공한다.
 * English: Each question refers to the level's correct sample trace. */
typedef struct {
    const char *question;
    const char *choices[3];
    int answer; /* 한글: 0=A, 1=B, 2=C. English: Zero-based choice index. */
    const char *reason;
} SooQuiz;

static const SooQuiz SOO_QUIZZES[SOO_LEVEL_COUNT] = {
    {"Which card does the code write to?", {"Card 0","Card 2","Card 3"}, 1,
     "cards[2] refers to the third card because indexing starts at zero."},
    {"What happens when the loop visits a card that already holds 0?", {"Writes 1","Writes 0","Reads only"}, 2,
     "The condition is false for 0, so the loop reads the card and skips the write."},
    {"Which index does the reverse loop visit first?", {"7","0","8"}, 0,
     "There are eight cards. N - 1 is 7, the last valid index."},
    {"After [0][3], which card do the loops visit next?", {"[0][4]","[1][0]","[1][3]"}, 1,
     "The inner loop ends at column 3. The outer loop advances and j starts again at 0."},
    {"Which faulty card does grid[i][i] repair?", {"[0][1]","[1][2]","[2][2]"}, 2,
     "Both indices must be equal. Only [2][2] is on the diagonal."},
    {"Ten reads and six writes: how many memory operations?", {"16","10","20"}, 0,
     "Each read and write costs one. 10 + 6 = 16, under the limit of 20."}
};
#endif
