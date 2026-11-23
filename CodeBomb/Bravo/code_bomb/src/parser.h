/*
 * parser.h - builds an abstract syntax tree (AST) from tokens
 * parser.h - 토큰으로부터 추상 구문 트리(AST)를 만든다
 *
 * Supported subset (levels 1-8) / 지원하는 부분 집합 (레벨 1-8):
 *   functions:   int|void name(void | int a, int b, ...) { ... }
 *   statements:  { }  if/else  while  for  return  break  continue
 *                int declarations   expression;   ;
 *   expressions: = += -= *= /= %=  ||  &&  == !=  < <= > >=
 *                + -  * / %  unary - ! +  ++ -- (prefix/postfix)
 *                calls  numbers  'c'  names  ( )  a[i]  a[i][j]
 *   Arrays are provided by the level (cards[], grid[][]); declaring your
 *   own arrays is not supported yet.
 *   배열은 레벨이 제공한다 (cards[], grid[][]). 직접 배열을 선언하는 기능은
 *   아직 지원하지 않는다.
 *
 * The parser stops at the first error (beginners fix one thing at a time).
 * 파서는 첫 번째 오류에서 멈춘다 (초보자는 한 번에 하나씩 고치는 것이 좋다).
 */
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "vm.h"

typedef enum {
    /* expressions / 식 */
    N_NUMBER,      /* value                                  / 값          */
    N_IDENT,       /* name                                   / 이름        */
    N_UNARY,       /* op a                                   / 단항        */
    N_BINARY,      /* a op b                                 / 이항        */
    N_INDEX,       /* a[b]  (a is N_IDENT or N_INDEX for 2D) / 배열 접근   */
    N_ASSIGN,      /* a op= b   (a is N_IDENT or N_INDEX)    / 대입        */
    N_INCDEC,      /* ++a, a++  (a is N_IDENT or N_INDEX)    / 증감        */
    N_CALL,        /* name(args...)  args in a, linked by next / 함수 호출 */

    /* statements / 문장 */
    N_BLOCK,       /* { a... }  end_line = line of '}'                     */
    N_EXPR_STMT,   /* a;                                                   */
    N_DECL,        /* int name [= a];                                      */
    N_IF,          /* if (a) b [else c]                                    */
    N_WHILE,       /* while (a) b                                          */
    N_FOR,         /* for (a; b; c) d   a may be a list of N_DECL          */
    N_RETURN,      /* return [a];                                          */
    N_BREAK,
    N_CONTINUE,
    N_EMPTY,       /* ;                                                    */

    /* top level / 최상위 */
    N_FUNC,        /* params in a (N_PARAM list), body in b                */
    N_PARAM
} NodeKind;

typedef struct Node {
    NodeKind  kind;
    int       line, col, len;   /* source position for errors
                                   오류 표시에 쓰는 소스 위치 */
    int       end_line;         /* last line of a statement (blocks: '}')
                                   문장의 마지막 줄 (블록은 '}' 줄) */
    TokenKind op;               /* operator kind / 연산자 종류 */
    bool      prefix;           /* ++x (true) or x++ (false) */
    bool      returns_value;    /* N_FUNC: int (true) or void (false) */
    int32_t   value;            /* N_NUMBER */
    char      name[VM_NAME_MAX];
    struct Node *a, *b, *c, *d; /* children / 자식 노드 */
    struct Node *next;          /* next item in a list / 목록의 다음 항목 */
} Node;

/* Result shared by the parser and compiler.
 * 파서와 컴파일러가 함께 쓰는 결과 구조체. */
typedef struct {
    bool ok;
    int  line, col, len;        /* error position / 오류 위치 */
    char message[256];
    int  statements;            /* size score / 크기 점수 */
} CompileResult;

/* All nodes live in one array so freeing is a single call.
 * 모든 노드를 배열 하나에 두어 해제를 한 번에 할 수 있다. */
typedef struct {
    Node *nodes;
    int   count;
    int   cap;
} NodePool;

bool parse_program(const char *src, int len, NodePool *pool, Node **out,
                   CompileResult *res);
void node_pool_free(NodePool *pool);

#endif
