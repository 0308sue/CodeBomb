#ifndef ROADMAP_SOO_H
#define ROADMAP_SOO_H

/* 한글: 범위는 학습 계획이며, route가 0인 레벨은 아직 준비 중이다.
 * English: Ranges describe the curriculum. A route of zero means planned. */
typedef struct { int first, last; const char *title, *difficulty; } SooChapter;
static const SooChapter SOO_CHAPTERS[] = {
    {1,6,"Loops and array basics","Easy"},
    {7,11,"Pointers and swapping","Easy / Medium"},
    {12,17,"Array operations","Medium"},
    {18,22,"2D arrays and patterns","Medium"},
    {23,27,"Strings and char pointers","Medium"},
    {28,33,"Linked lists","Hard"},
    {34,37,"Memory allocation","Hard"},
    {38,40,"Vector and iterator / C++","Hard"}
};
/* 한글: 11~16은 Stage 1, 21~25는 Stage 2, 4~5는 기존 2D 미션이다.
 * English: Routes 11–16 open Stage 1; 21–25 open Stage 2; 31–36 open Stage 3; 4–5 open existing 2D lessons. */
static const int SOO_ROUTES[41] = {
    [1]=11,[2]=12,[3]=13,[4]=14,[5]=15,[6]=16,[7]=21,[8]=22,[9]=23,[10]=24,[11]=25,[12]=31,[13]=32,[14]=33,[15]=34,[16]=35,[17]=36,[18]=41,[19]=42,[20]=43,[21]=44,[22]=45,[23]=51,[24]=52,[25]=53,[26]=54,[27]=55,[28]=61,[29]=62,[30]=63,[31]=64,[32]=65,[33]=66,[34]=71,[35]=72,[36]=73,[37]=74,[38]=81,[39]=82,[40]=83
};
#endif
