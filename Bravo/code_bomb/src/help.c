/*
 * help.c - field manual pages, diagrams and the pop-up
 * help.c - 현장 매뉴얼 페이지, 그림, 팝업
 *
 * Each topic has 2-3 pages. A page is: a heading, a short explanation, a
 * diagram (drawn here with rectangles and arrows), a code example and a tip.
 * Text is kept short on purpose: it should be read in under a minute and
 * give exactly what the next missions need.
 * 주제마다 2-3쪽이 있다. 한 쪽은 제목, 짧은 설명, 그림(여기서 사각형과
 * 화살표로 그림), 코드 예제, 팁으로 이루어진다. 글은 일부러 짧게 썼다:
 * 1분 안에 읽고, 다음 미션에 필요한 것만 얻을 수 있어야 한다.
 *
 * To add a page, add a HelpPage to the topic in DOCS. A new diagram is a new
 * Diagram value plus a case in draw_diagram().
 * 페이지를 추가하려면 DOCS의 주제에 HelpPage를 넣는다. 새 그림은 Diagram
 * 값을 하나 추가하고 draw_diagram()에 case를 넣는다.
 */
#include "help.h"

#include "roadmap_SOO.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * Content / 내용
 * ====================================================================== */

typedef enum {
    DIA_NONE, DIA_ARRAY, DIA_LOOP, DIA_SWAP, DIA_POINTER, DIA_PTR_ARRAY,
    DIA_MAX, DIA_SHIFT, DIA_GRID, DIA_GRID_MATCH, DIA_STRING, DIA_STRING_WALK,
    DIA_LIST, DIA_LIST_INSERT, DIA_HEAP, DIA_DANGLING, DIA_VECTOR,
    DIA_ITERATOR, DIA_CARDS_COST, DIA_STRIKE, DIA_CUBE_TIMER, DIA_MAILROOM,
    DIA_CALL, DIA_BELT_NIGHT, DIA_BELT_WEIGH, DIA_BELT_COUNT, DIA_BELT_MAX,
    DIA_BELT_COUNTDOWN
} Diagram;

typedef struct {
    const char *heading;
    const char *text;        /* paragraphs separated by '\n' / 문단은 '\n' */
    const char *code;        /* example; "//" starts a comment / 예제 */
    const char *code_label;  /* NULL = "EXAMPLE" */
    Diagram     diagram;
    const char *tip;
} HelpPage;

#define MAX_PAGES 3

typedef struct {
    const char *title;       /* NULL = chapter title from roadmap_SOO.h */
    HelpPage    pages[MAX_PAGES];
    int         count;
    const char *badge;       /* NULL = chapter number or "?" */
} HelpDoc;

static const HelpDoc DOCS[HELP_TOPIC_COUNT] = {
    /* ---- 1 Loops and array basics ---- */
    { NULL, {
        { "Arrays",
          "An array stores several values of the same type side by side in "
          "memory. Each slot has a number called its index, and the first "
          "index is 0.\n"
          "So an array of 5 cards has indices 0, 1, 2, 3 and 4. Read a slot "
          "with cards[i]; change it with cards[i] = value.",
          "int cards[5] = {3, 5, 7, 0, 1};\n"
          "\n"
          "int x = cards[2];   // x is 7\n"
          "cards[3] = 1;       // slot 3 becomes 1\n"
          "\n"
          "// cards[5] does not exist!",
          NULL, DIA_ARRAY,
          "The last index is always size - 1." },
        { "for loops",
          "A for loop repeats a block of code. It has three parts: a start "
          "(int i = 0), a condition checked before every pass (i < 5) and an "
          "update after every pass (i++).\n"
          "Use the loop variable as the index and the loop visits every slot "
          "in order.",
          "for (int i = 0; i < 5; i++) {\n"
          "    if (cards[i] == 0)\n"
          "        cards[i] = 1;   // turn every 0 into 1\n"
          "}",
          NULL, DIA_LOOP,
          "Write i < size, not i <= size: the second one reads one slot past "
          "the end." },
        { "How missions work",
          "Each mission shows a short piece of C and asks one question about "
          "it. Choose A, B or C first, then watch the sample run.\n"
          "Every READ and WRITE lights up a card, so you can see exactly what "
          "the code does. Step forward and back as often as you like: "
          "missions are untimed practice.",
          "A / B / C     answer\n"
          "Space         play or pause\n"
          "Left, Right   step back / forward\n"
          "R             start again",
          "CONTROLS", DIA_NONE,
          "Predict before you watch. Being wrong and seeing why is how it "
          "sticks." } }, 3, NULL },

    /* ---- 2 Pointers and swapping ---- */
    { NULL, {
        { "What is a pointer?",
          "Every variable lives at an address in memory. A pointer is a "
          "variable that stores an address.\n"
          "&x gives the address of x. *p follows the pointer and gives the "
          "value it points at (this is called dereferencing). Writing to *p "
          "changes x itself.",
          "int x = 7;\n"
          "int *p = &x;       // p holds the address of x\n"
          "\n"
          "printf(\"%d\", *p);  // prints 7\n"
          "*p = 9;            // x is now 9",
          NULL, DIA_POINTER,
          "Read int *p as \"p is a pointer to an int\"." },
        { "Swapping with temp",
          "To swap two values you need a third place to keep one of them, "
          "otherwise the first write destroys it.\n"
          "A swap function must take pointers: C passes copies of its "
          "arguments, so swapping plain ints would only swap the copies.",
          "void swap(int *a, int *b) {\n"
          "    int temp = *a;   // 1. save a\n"
          "    *a = *b;         // 2. copy b into a\n"
          "    *b = temp;       // 3. old a into b\n"
          "}\n"
          "\n"
          "swap(&x, &y);",
          NULL, DIA_SWAP,
          "The order of the three lines matters. Try them on paper." },
        { "Pointer arithmetic",
          "An array name works like a pointer to its first slot. p + 1 points "
          "at the next element: C moves by the size of one element for you.\n"
          "*(p + i) is the same as p[i] and arr[i].",
          "int arr[5] = {4, 8, 15, 16, 23};\n"
          "int *p = arr;           // same as &arr[0]\n"
          "\n"
          "int third = *(p + 2);   // 15, same as arr[2]\n"
          "p++;                    // now points at arr[1]",
          NULL, DIA_PTR_ARRAY,
          "Two pointers starting at both ends and moving inward can reverse "
          "an array." } }, 3, NULL },

    /* ---- 3 Array operations ---- */
    { NULL, {
        { "Search, count, maximum",
          "Most array jobs are one loop with a variable that remembers "
          "something: the best value so far, a count, or where a value was "
          "found.\n"
          "Start a maximum at the first element, not at 0, so it still works "
          "when every value is negative.",
          "int max = arr[0];\n"
          "for (int i = 1; i < n; i++) {\n"
          "    if (arr[i] > max)\n"
          "        max = arr[i];\n"
          "}\n"
          "\n"
          "int count = 0;   // how many 3s?\n"
          "for (int i = 0; i < n; i++)\n"
          "    if (arr[i] == 3) count++;",
          NULL, DIA_MAX,
          "If a second value equal to the maximum shouldn't count, compare "
          "with > rather than >=." },
        { "Insert and delete",
          "An array can't open a gap by itself. To insert, shift elements one "
          "place right to make room, starting from the end so nothing is "
          "overwritten.\n"
          "To delete, shift left over the gap and make the used length "
          "smaller. Keep a separate count of used slots: a 0 in a slot can be "
          "real data.",
          "// insert value at pos (used < capacity)\n"
          "for (int i = used; i > pos; i--)\n"
          "    arr[i] = arr[i - 1];   // from the back\n"
          "arr[pos] = value;\n"
          "used++;\n"
          "\n"
          "// delete at pos\n"
          "for (int i = pos; i < used - 1; i++)\n"
          "    arr[i] = arr[i + 1];\n"
          "used--;",
          NULL, DIA_SHIFT,
          "Rotating left is a delete at 0 followed by an insert at the end: "
          "save arr[0] first." } }, 2, NULL },

    /* ---- 4 2D arrays and patterns ---- */
    { NULL, {
        { "Rows and columns",
          "A 2D array is an array of rows. In grid[r][c] the first index is "
          "the row (down) and the second is the column (across).\n"
          "Nested loops visit it row by row: the outer loop picks the row, "
          "the inner loop walks along it. This is also the order C stores it "
          "in memory.",
          "int grid[3][4];\n"
          "\n"
          "for (int r = 0; r < 3; r++)\n"
          "    for (int c = 0; c < 4; c++)\n"
          "        grid[r][c] = 0;\n"
          "\n"
          "grid[1][2] = 5;   // row 1, column 2",
          NULL, DIA_GRID,
          "Neighbours: left [r][c-1], right [r][c+1], up [r-1][c], down "
          "[r+1][c]. Check the edges before you look." },
        { "Patterns and gravity",
          "To find three equal values in a row, compare a cell with the two "
          "to its right. Stop the loop two columns early so you never read "
          "past the edge.\n"
          "Gravity works one column at a time: read from the bottom up and "
          "move each value down to the lowest free slot, keeping their "
          "order.",
          "for (int r = 0; r < ROWS; r++)\n"
          "    for (int c = 0; c < COLS - 2; c++)\n"
          "        if (grid[r][c] == grid[r][c+1] &&\n"
          "            grid[r][c] == grid[r][c+2])\n"
          "            mark(r, c);   // three in a row",
          NULL, DIA_GRID_MATCH,
          "For vertical matches swap the roles: stop at r < ROWS - 2." } },
      2, NULL },

    /* ---- 5 Strings and char pointers ---- */
    { NULL, {
        { "Strings are char arrays",
          "A C string is an array of char that ends with the null character "
          "'\\0'. \"BOMB\" needs 5 bytes: four letters plus '\\0'.\n"
          "Functions find the end of a string by looking for '\\0'. Without "
          "it they keep reading past the array.",
          "char word[5] = \"BOMB\";   // B O M B \\0\n"
          "\n"
          "int len = 0;\n"
          "while (word[len] != '\\0')\n"
          "    len++;                // len is 4",
          NULL, DIA_STRING,
          "strlen() doesn't count the '\\0', but the array needs room for "
          "it." },
        { "Walking with a char pointer",
          "A char * can step through a string: *s is the current character "
          "and s++ moves to the next one.\n"
          "Copy, join (concatenate) and compare are all this same loop. "
          "Before writing, make sure the destination array is big enough for "
          "every character and the '\\0'.",
          "void copy(char *dst, const char *src) {\n"
          "    while (*src != '\\0') {\n"
          "        *dst = *src;\n"
          "        dst++;\n"
          "        src++;\n"
          "    }\n"
          "    *dst = '\\0';   // don't forget the end\n"
          "}",
          NULL, DIA_STRING_WALK,
          "strcmp() returns 0 when two strings match, and a negative number "
          "when the first one comes earlier." } }, 2, NULL },

    /* ---- 6 Linked lists ---- */
    { NULL, {
        { "Nodes and next",
          "A linked list is a chain of nodes. Each node holds some data and a "
          "pointer to the next node. head points at the first node, and the "
          "last node's next is NULL.\n"
          "Nodes can be anywhere in memory, so you can't jump straight to the "
          "third one: you follow next, one node at a time.",
          "typedef struct Node {\n"
          "    int data;\n"
          "    struct Node *next;\n"
          "} Node;\n"
          "\n"
          "for (Node *cur = head; cur != NULL;\n"
          "     cur = cur->next)\n"
          "    printf(\"%d \", cur->data);",
          NULL, DIA_LIST,
          "cur->data is short for (*cur).data." },
        { "Insert and delete",
          "Change the links, not the data. To insert B after A, first point B "
          "at A's old next, then point A at B. The other order loses the rest "
          "of the list.\n"
          "To delete the node after A, link A past it, then free it.",
          "// insert b after a\n"
          "b->next = a->next;   // 1. b points at c\n"
          "a->next = b;         // 2. a points at b\n"
          "\n"
          "// delete the node after a\n"
          "Node *gone = a->next;\n"
          "a->next = gone->next;\n"
          "free(gone);",
          NULL, DIA_LIST_INSERT,
          "Draw boxes and arrows on paper before you change two links." } },
      2, NULL },

    /* ---- 7 Memory allocation ---- */
    { NULL, {
        { "malloc and free",
          "Local variables live on the stack and disappear when their "
          "function returns. malloc() reserves memory on the heap that stays "
          "until you free() it.\n"
          "malloc returns a pointer to the new block, or NULL if there is no "
          "memory left. Use sizeof so the size fits the type.",
          "int n = 4;\n"
          "int *nums = malloc(n * sizeof(int));\n"
          "if (nums == NULL)\n"
          "    return;          // out of memory\n"
          "\n"
          "nums[0] = 42;        // use it like an array\n"
          "free(nums);          // give it back\n"
          "nums = NULL;",
          NULL, DIA_HEAP,
          "C++ uses new / delete, and new[] / delete[] for arrays. Never mix "
          "the pairs." },
        { "Leaks and dangling pointers",
          "A leak is heap memory nobody points at any more: it can never be "
          "freed. A dangling pointer still points at memory that was freed, "
          "which may now belong to something else.\n"
          "Rules: every malloc gets exactly one free, set the pointer to NULL "
          "after free, and never free twice.",
          "int *a = malloc(sizeof(int));\n"
          "a = malloc(sizeof(int));   // LEAK: first block lost\n"
          "\n"
          "free(a);\n"
          "*a = 5;                    // DANGLING: a was freed\n"
          "a = NULL;                  // safe: clearly empty",
          NULL, DIA_DANGLING,
          "If two pointers share one block, freeing through one leaves the "
          "other dangling." } }, 2, NULL },

    /* ---- 8 Vector and iterator (C++) ---- */
    { NULL, {
        { "std::vector",
          "A vector (C++) is an array that manages its own heap memory. "
          "size() is how many elements are in use; capacity() is how many fit "
          "before it has to grow.\n"
          "push_back() adds at the end. When size reaches capacity the vector "
          "allocates a bigger block and copies every element across.",
          "std::vector<int> v;\n"
          "v.push_back(10);\n"
          "v.push_back(20);\n"
          "v.push_back(30);\n"
          "\n"
          "// v.size() == 3, v[1] == 20",
          NULL, DIA_VECTOR,
          "Growing moves the elements, so old pointers into the vector "
          "become invalid." },
        { "Iterators and erase",
          "An iterator is like a pointer to an element: *it reads it, ++it "
          "moves on. begin() is the first element and end() is one past the "
          "last, so never read *end().\n"
          "erase(it) removes an element and shifts the rest left, which makes "
          "it invalid. Keep the iterator that erase returns instead.",
          "for (auto it = v.begin(); it != v.end(); ) {\n"
          "    if (*it == 99)\n"
          "        it = v.erase(it);   // next element\n"
          "    else\n"
          "        ++it;\n"
          "}",
          NULL, DIA_ITERATOR,
          "This is chapter 7's dangling pointer again, wearing C++ clothes." } },
      2, NULL },

    /* ---- Bomb defusal ---- */
    { "Bomb defusal", {
        { "Write the code",
          "Each bomb runs your run() function. A 1D bomb gives you cards[i] "
          "and N; a 2D bomb gives you grid[i][j], ROWS and COLS. A card is 0 "
          "(SAFE) or 1 (FAULT).\n"
          "Make every card match the target written under it. Then press Play "
          "randomised: a new bomb with the same rule but different cards and a "
          "different N appears, and your code runs on it. Code that only fixes "
          "the cards you can see fails there; use N, ROWS and COLS.",
          "void run(void) {\n"
          "    for (int i = 0; i < N; i++) {\n"
          "        if (cards[i] == 1)   // 1 read\n"
          "            cards[i] = 0;    // 1 write\n"
          "    }\n"
          "}",
          NULL, DIA_CARDS_COST,
          "Each read and write costs 1 operation. A level is complete when the "
          "bomb on screen AND a random bomb are defused." },
        { "Strikes and controls",
          "A strike happens when the code reads or writes outside the array "
          "(cards[N] or cards[-1]) or uses more operations than the budget. "
          "Compile errors are underlined in red before anything runs.\n"
          "Step and Back move one operation at a time, so you can find the "
          "exact line that went wrong.",
          "F5 or Ctrl+Enter   run / pause\n"
          "F6                 step\n"
          "F7                 back\n"
          "F8                 reset\n"
          "Esc                leave the editor\n"
          "\n"
          "macOS: Cmd+Enter, fn+F6, fn+F7, fn+F8",
          "CONTROLS", DIA_STRIKE,
          "Stuck? The Hint button next to MISSION gives three hints, one at a "
          "time." },
        { "The Cube bomb challenge",
          "The Cube bomb is the timed challenge: five modules, one on each "
          "side, and a countdown on the top display. Press START to begin.\n"
          "Drag the cube to turn it and click a face to pick its module. Any "
          "run that ends in a strike, or ends with its module unsolved, sets "
          "the bomb off. So does running out of time. A module only counts as "
          "defused if your code also defuses hidden random bombs, so code that "
          "fixes single cards explodes. Compile errors and Play randomised are "
          "safe.",
          "Practice bombs 1-6   untimed\n"
          "Cube bomb            10:00, five modules\n"
          "\n"
          "Strike or wrong result   BOOM\n"
          "Fails a hidden random    BOOM\n"
          "Time runs out            BOOM\n"
          "Compile error            safe\n"
          "Play randomised          safe practice",
          "RULES", DIA_CUBE_TIMER,
          "Read all five missions first: the easy modules buy you time for "
          "the hard ones." } }, 3, NULL },

    /* ---- Mailroom ---- */
    { "Mailroom", {
        { "Your job",
          "Parcels arrive on the INBOX belt. Deliver every parcel to the "
          "OUTBOX belt, in the same order.\n"
          "take() picks up the next parcel and gives back its number. put(n) "
          "places a parcel stamped n on the outbox.",
          "void run(void) {\n"
          "    put(take());   // move one parcel\n"
          "    put(take());\n"
          "    put(take());\n"
          "}",
          NULL, DIA_MAILROOM,
          "Every statement ends with a semicolon ;" },
        { "Statements and calls",
          "A statement is one instruction, and the program runs them from top "
          "to bottom. A function call is a name followed by brackets holding "
          "its arguments.\n"
          "int take(void) gives back a value; void put(int n) needs one. In "
          "put(take()) the result of take() goes straight into put(). You can "
          "also keep it in a variable first.",
          "int parcel = take();   // keep the number\n"
          "put(parcel);           // deliver it\n"
          "\n"
          "put(take());           // the same, in one line",
          NULL, DIA_CALL,
          "A variable must get a value before it is read, or the machine "
          "stops." },
        { "Run, step and submit",
          "Run plays your code with the robot. Step runs one line; Step op "
          "runs one machine instruction, so you can watch the bytecode and "
          "the operand stack in the Machine view.\n"
          "Submit tests your code on 5 random inboxes. It has to work for "
          "all of them, not just the one on screen.",
          "F5 or Ctrl+Enter   run / pause\n"
          "F6                 step one line\n"
          "F7                 step one instruction\n"
          "F8                 reset\n"
          "F9                 submit\n"
          "Esc                leave the editor\n"
          "\n"
          "macOS: Cmd+Enter, fn+F6 ... fn+F9",
          "CONTROLS", DIA_NONE,
          "Examples shows programs that fail on purpose. Run them to see how "
          "the machine reports each mistake." } }, 3, "1" },

    /* ---- Mailroom 2: Night Shift ---- */
    { "Night Shift", {
        { "while loops",
          "A while loop repeats its block for as long as its condition is "
          "true. The condition is checked before every pass, so if it is "
          "false at the start, the block never runs.\n"
          "has_input() answers 1 (true) while parcels are waiting and 0 "
          "(false) when the inbox is empty.",
          "int n = 3;\n"
          "while (n > 0) {   // checked before each pass\n"
          "    n--;          // runs 3 times\n"
          "}\n"
          "// here n is 0",
          NULL, DIA_BELT_NIGHT,
          "In C, 0 means false and any other number means true." },
        { "Any number of parcels",
          "Level 1's three put(take()) lines only work when exactly three "
          "parcels arrive. Tonight it could be two or seven, and Submit tries "
          "5 random inboxes, so the code has to ask instead of guessing.\n"
          "Put the work for one parcel inside the loop and let the condition "
          "decide when to stop.",
          "while ( /* parcels left? */ ) {\n"
          "    // move one parcel\n"
          "}",
          "SHAPE", DIA_NONE,
          "A loop that never becomes false calls take() on an empty inbox, "
          "and the machine stops with an error." } }, 2, "2" },

    /* ---- Mailroom 4: Weigh Station ---- */
    { "Weigh Station", {
        { "Variables",
          "A variable is a named box that keeps a number. int a = take(); "
          "stores the parcel's number in a, and it stays there until you "
          "change it.\n"
          "The robot can hold only one parcel, but your program can remember "
          "as many numbers as it has variables. Watch them in the inspector.",
          "int a = 4;\n"
          "int b = 9;\n"
          "int sum = a + b;   // 13\n"
          "a = a + 1;         // a is now 5",
          NULL, DIA_BELT_WEIGH,
          "A variable lives inside the { } block where it is declared." },
        { "Arithmetic",
          "+ - * / and % all work on int values. put() doesn't need a parcel "
          "in the robot's hands: put(a + b) stamps a brand new parcel with the "
          "result.\n"
          "Each pass of the loop should take exactly one pair.",
          "put(7 + 8);   // a new parcel stamped 15\n"
          "put(a - b);   // can be negative\n"
          "put(a * 2);",
          NULL, DIA_NONE,
          "Integer division drops the remainder: 7 / 2 is 3, and 7 % 2 is 1." } },
      2, "4" },

    /* ---- Mailroom 6: Head Count ---- */
    { "Head Count", {
        { "Counting",
          "A counter is a variable that starts at 0 and goes up by one each "
          "time something happens. count++ is short for count = count + 1.\n"
          "Where you set it back to 0 matters: once per group, not once per "
          "program.",
          "int count = 0;\n"
          "count++;      // 1\n"
          "count++;      // 2\n"
          "count += 3;   // 5",
          NULL, DIA_BELT_COUNT,
          "count-- goes down by one." },
        { "Sentinel loops",
          "A parcel stamped 0 is a sentinel: a special value that marks the "
          "end of a group. It is not a real parcel, so don't count it.\n"
          "A condition can read and test in one go: while (take() != 0) takes "
          "the next parcel every time the condition is checked.",
          "int box = take();\n"
          "while (box != 0) {\n"
          "    // ... use box ...\n"
          "    box = take();   // read the next one\n"
          "}",
          NULL, DIA_NONE,
          "An empty group is a 0 on its own. Its count is 0." } }, 2, "6" },

    /* ---- Mailroom 7: Biggest Box ---- */
    { "Biggest Box", {
        { "if and else",
          "if runs a block only when its condition is true; else runs another "
          "block when it is false.\n"
          "Comparisons give 1 or 0: a > b, a < b, a >= b, a <= b, a == b "
          "(equal: two = signs) and a != b (not equal).",
          "if (box > best) {\n"
          "    best = box;   // only when bigger\n"
          "} else {\n"
          "    // otherwise nothing changes\n"
          "}",
          NULL, DIA_BELT_MAX,
          "= stores a value and == compares two. if (a = b) is a classic bug." },
        { "Best so far",
          "To find the biggest of a group, keep the best value seen so far. "
          "Start it at the group's first parcel, then compare each of the "
          "others with it.\n"
          "Start again for every group, otherwise a big parcel from an earlier "
          "group wins forever.",
          "int best = take();   // first of the group\n"
          "// for each of the other two parcels:\n"
          "//     if it is bigger, it becomes best\n"
          "put(best);",
          "SHAPE", DIA_NONE,
          "Two parcels can be equal. Either one is the biggest." } }, 2, "7" },

    /* ---- Mailroom 9: Countdown ---- */
    { "Countdown", {
        { "for loops",
          "A for loop keeps the start, the condition and the update on one "
          "line: for (start; condition; update). It is a while loop with the "
          "counting built in.\n"
          "The loop variable can count down as well as up.",
          "for (int i = 1; i <= 3; i++)\n"
          "    put(i);   // 1, 2, 3\n"
          "\n"
          "for (int i = 3; i > 0; i--)\n"
          "    put(i);   // 3, 2, 1",
          NULL, DIA_BELT_COUNTDOWN,
          "The update runs after each pass, then the condition is checked "
          "again." },
        { "Loops inside loops",
          "One parcel can make many outputs. The outer while loop takes one "
          "parcel per pass, and an inner for loop delivers its countdown.\n"
          "Check the edges: the countdown ends at 1, so a 0 must never reach "
          "the outbox.",
          "i = 3   put(3)\n"
          "i = 2   put(2)\n"
          "i = 1   put(1)\n"
          "i = 0   i > 0 is false: stop",
          "TRACE FOR n = 3", DIA_NONE,
          "Trace the first and the last pass on paper before you run." } },
      2, "9" },
};

/* Chapter colours (also used by the mission map). / 챕터 색 (미션 지도도 사용) */
static const Color TINTS[8] = {
    { 255, 187, 220, 255 }, { 173, 231, 211, 255 }, { 255, 209, 164, 255 },
    { 187, 196, 255, 255 }, { 167, 223, 250, 255 }, { 231, 193, 255, 255 },
    { 254, 222, 151, 255 }, { 255, 175, 182, 255 }
};

Color help_chapter_color(int chapter) { return TINTS[((chapter % 8) + 8) % 8]; }

static Color topic_tint(HelpTopic t)
{
    if (t <= HELP_CHAPTER_8) return TINTS[t];
    return t == HELP_BOMB ? (Color){ 255, 120, 100, 255 } : UI_ACCENT;
}

static const char *topic_title(HelpTopic t)
{
    if (DOCS[t].title) return DOCS[t].title;
    return SOO_CHAPTERS[t].title;
}

HelpTopic help_topic_for_level(int level)
{
    int n = (int)(sizeof SOO_CHAPTERS / sizeof SOO_CHAPTERS[0]);
    for (int k = 0; k < n && k <= HELP_CHAPTER_8; k++)
        if (level >= SOO_CHAPTERS[k].first && level <= SOO_CHAPTERS[k].last)
            return (HelpTopic)k;
    return HELP_CHAPTER_1;
}

/* ========================================================================
 * State / 상태
 * ====================================================================== */

static bool      g_open;
static bool      g_fresh;     /* opened this frame: ignore the same click
                                 이번 프레임에 열림: 같은 클릭은 무시 */
static HelpTopic g_topic;
static int       g_page;
static bool      g_seen[HELP_TOPIC_COUNT];

int help_page_count(HelpTopic topic)
{
    return topic >= 0 && topic < HELP_TOPIC_COUNT ? DOCS[topic].count : 0;
}

void help_set_page(int page)
{
    if (page >= 0 && page < DOCS[g_topic].count) g_page = page;
}

void help_open(HelpTopic topic)
{
    if (topic < 0 || topic >= HELP_TOPIC_COUNT) return;
    g_open = true;
    g_fresh = true;
    g_topic = topic;
    g_page = 0;
    g_seen[topic] = true;
}

void help_open_once(HelpTopic topic)
{
    if (topic >= 0 && topic < HELP_TOPIC_COUNT && !g_seen[topic])
        help_open(topic);
}

bool help_active(void) { return g_open; }

void help_close(void) { g_open = false; }

static void next_page(void)
{
    if (g_page + 1 < DOCS[g_topic].count) g_page++;
    else g_open = false;
}

void help_update(void)
{
    if (!g_open) return;
    if (IsKeyPressed(KEY_ESCAPE)) g_open = false;
    else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) next_page();
    else if (IsKeyPressed(KEY_LEFT) && g_page > 0) g_page--;
    /* don't let typed letters reach the editor after closing
       닫은 뒤 입력한 글자가 에디터로 가지 않게 한다 */
    while (GetCharPressed() > 0) {
    }
}

/* ========================================================================
 * Drawing helpers / 그리기 도우미
 * ====================================================================== */

static const Color D_SAFE   = { 52, 142, 112, 255 };
static const Color D_CELL   = { 58, 47, 92, 255 };
static const Color D_EMPTY  = { 34, 27, 56, 255 };
static const Color D_LINE   = { 140, 210, 255, 255 };

static void ctext(const char *t, float cx, float y, int size, Color c)
{
    DrawText(t, (int)(cx - MeasureText(t, size) / 2.0f), (int)y, size, c);
}

static void cell(float x, float y, float w, float h, const char *value,
                 Color fill, bool highlight)
{
    Rectangle r = { x, y, w, h };
    DrawRectangleRounded(r, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.18f, 6, highlight ? 3.0f : 1.5f,
                                highlight ? UI_ACCENT : UI_BORDER);
    if (value) ctext(value, x + w / 2, y + (h - FS_BODY) / 2, FS_BODY, UI_INK);
}

static void arrow(Vector2 a, Vector2 b, Color c)
{
    Vector2 d = { b.x - a.x, b.y - a.y };
    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1.0f) return;
    d.x /= len;
    d.y /= len;
    Vector2 n = { -d.y, d.x };
    Vector2 base = { b.x - d.x * 16, b.y - d.y * 16 };
    DrawLineEx(a, base, 3.0f, c);
    Vector2 l = { base.x + n.x * 9, base.y + n.y * 9 };
    Vector2 r = { base.x - n.x * 9, base.y - n.y * 9 };
    DrawTriangle(b, l, r, c);        /* one of the two windings is drawn
                                        두 방향 중 하나가 그려진다 */
    DrawTriangle(b, r, l, c);
}

/* A row of n cells with index labels above; returns cell width.
 * 위에 인덱스가 붙은 칸 n개를 그린다. 칸 너비를 돌려준다. */
static float cell_row(float x, float y, int n, const char *const *values,
                      const Color *fills, int highlight, bool indices,
                      float w)
{
    for (int k = 0; k < n; k++) {
        float cx = x + k * (w + 10);
        if (indices)
            ctext(TextFormat("[%d]", k), cx + w / 2, y - 28, FS_SMALL,
                  UI_MUTED);
        cell(cx, y, w, 70, values[k], fills ? fills[k] : D_CELL,
             k == highlight);
    }
    return w;
}

static float cx_of(float x, int k, float w) { return x + k * (w + 10) + w / 2; }

/* ========================================================================
 * Diagrams / 그림
 * ====================================================================== */

#define GAP -9999      /* space between groups in a belt diagram
                          벨트 그림에서 묶음 사이의 빈칸 */

/* INBOX row -> rule -> OUTBOX row, for the Mailroom levels. A 0 is drawn as
 * an end marker when markers is true.
 * 우편실 레벨용: INBOX 줄 -> 규칙 -> OUTBOX 줄. markers가 true면 0은 끝
 * 표시로 그린다. */
static void draw_belt(Rectangle box, const int *in, int nin, const int *out,
                      int nout, const char *rule, bool markers)
{
    const Color parcel = { 150, 104, 60, 255 };
    float x0 = box.x + 170, y1 = box.y + 50, y2 = box.y + 200;
    DrawText("INBOX", (int)box.x + 36, (int)y1 + 18, FS_SMALL, D_LINE);
    DrawText("OUTBOX", (int)box.x + 36, (int)y2 + 18, FS_SMALL, UI_SUCCESS);
    float x = x0;
    for (int k = 0; k < nin; k++) {
        if (in[k] == GAP) {
            x += 22;
            continue;
        }
        bool marker = markers && in[k] == 0;
        cell(x, y1, 56, 56, TextFormat("%d", in[k]), marker ? D_EMPTY : parcel,
             marker);
        x += 64;
    }
    x = x0;
    for (int k = 0; k < nout; k++) {
        cell(x, y2, 56, 56, TextFormat("%d", out[k]), parcel, false);
        x += 64;
    }
    float ax = box.x + 100;
    arrow((Vector2){ ax, y1 + 64 }, (Vector2){ ax, y2 - 8 }, UI_ACCENT);
    DrawText(rule, (int)x0, (int)(y1 + y2) / 2 + 12, FS_SMALL, UI_ACCENT);
}

static void draw_diagram(Diagram d, Rectangle box)
{
    float x0 = box.x + 40, y0 = box.y + 90;
    float mid = box.x + box.width / 2;
    switch (d) {
    case DIA_ARRAY: {
        static const char *const v[5] = { "3", "5", "7", "0", "1" };
        DrawText("cards", (int)x0, (int)y0 + 22, FS_SMALL, UI_MUTED);
        cell_row(x0 + 90, y0, 5, v, NULL, 2, true, 96);
        ctext("cards[2] is 7", cx_of(x0 + 90, 2, 96), y0 + 100, FS_SMALL,
              UI_ACCENT);
        ctext("5 slots: indices 0 to 4", mid, box.y + box.height - 50,
              FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_LOOP: {
        static const char *const v[5] = { "1", "1", "1", "0", "0" };
        static const Color f[5] = { { 52, 142, 112, 255 },
                                    { 52, 142, 112, 255 },
                                    { 52, 142, 112, 255 },
                                    { 58, 47, 92, 255 },
                                    { 58, 47, 92, 255 } };
        cell_row(x0 + 40, y0, 5, v, f, 3, true, 96);
        float ax = cx_of(x0 + 40, 3, 96);
        arrow((Vector2){ ax, y0 + 150 }, (Vector2){ ax, y0 + 78 }, UI_ACCENT);
        ctext("i = 3", ax, y0 + 156, FS_SMALL, UI_ACCENT);
        ctext("i: 0, 1, 2, 3, 4, then i < 5 is false and the loop stops", mid,
              box.y + box.height - 50, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_MAX: {
        static const char *const v[5] = { "4", "9", "2", "9", "6" };
        cell_row(x0 + 40, y0, 5, v, NULL, 1, true, 96);
        DrawText("max", (int)x0 + 40, (int)y0 + 120, FS_BODY, UI_MUTED);
        cell(x0 + 130, y0 + 104, 96, 62, "9", D_SAFE, true);
        arrow((Vector2){ cx_of(x0 + 40, 1, 96), y0 + 74 },
              (Vector2){ x0 + 200, y0 + 102 }, UI_ACCENT);
        ctext("best so far: updated only when a value is bigger", mid,
              box.y + box.height - 50, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_SWAP: {
        float y = y0 + 10;
        DrawText("a", (int)x0 + 40, (int)y - 34, FS_SMALL, UI_MUTED);
        cell(x0 + 40, y, 110, 70, "4", D_CELL, false);
        DrawText("b", (int)box.x + (int)box.width - 190, (int)y - 34, FS_SMALL,
                 UI_MUTED);
        cell(box.x + box.width - 190, y, 110, 70, "9", D_CELL, false);
        DrawText("temp", (int)mid - 55, (int)y + 136, FS_SMALL, UI_MUTED);
        cell(mid - 55, y + 160, 110, 62, "4", D_EMPTY, true);
        arrow((Vector2){ x0 + 120, y + 74 }, (Vector2){ mid - 58, y + 178 },
              UI_ACCENT);
        ctext("1", x0 + 72, y + 124, FS_BODY, UI_ACCENT);
        arrow((Vector2){ box.x + box.width - 194, y + 28 },
              (Vector2){ x0 + 154, y + 28 }, D_LINE);
        ctext("2", mid, y - 8, FS_BODY, D_LINE);
        arrow((Vector2){ mid + 58, y + 190 },
              (Vector2){ box.x + box.width - 140, y + 74 }, UI_PINK);
        ctext("3", box.x + box.width - 100, y + 124, FS_BODY, UI_PINK);
        break;
    }
    case DIA_POINTER: {
        float y = y0 + 10;
        DrawText("int x", (int)x0 + 40, (int)y - 34, FS_SMALL, UI_MUTED);
        cell(x0 + 40, y, 150, 74, "7", D_SAFE, false);
        ctext("address 0x1004", x0 + 115, y + 88, FS_SMALL, UI_MUTED);
        float px = box.x + box.width - 300;
        DrawText("int *p = &x", (int)px, (int)y - 34, FS_SMALL, UI_MUTED);
        cell(px, y, 230, 74, "0x1004", D_CELL, true);
        arrow((Vector2){ px - 6, y + 37 }, (Vector2){ x0 + 196, y + 37 },
              UI_ACCENT);
        ctext("*p follows the arrow: *p is 7", mid, y + 150, FS_SMALL,
              UI_ACCENT);
        ctext("&x is where x lives, not what it holds", mid, y + 186,
              FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_PTR_ARRAY: {
        static const char *const v[5] = { "4", "8", "15", "16", "23" };
        float x = x0 + 40;
        DrawText("arr", (int)x0 - 10, (int)y0 + 22, FS_SMALL, UI_MUTED);
        cell_row(x, y0, 5, v, NULL, 2, true, 96);
        static const char *const labels[3] = { "p", "p + 1", "p + 2" };
        for (int k = 0; k < 3; k++) {
            float ax = cx_of(x, k, 96);
            arrow((Vector2){ ax, y0 + 150 }, (Vector2){ ax, y0 + 78 },
                  k == 2 ? UI_ACCENT : D_LINE);
            ctext(labels[k], ax, y0 + 156, FS_SMALL,
                  k == 2 ? UI_ACCENT : D_LINE);
        }
        ctext("*(p + 2) == arr[2] == 15", mid, box.y + box.height - 46,
              FS_SMALL, UI_ACCENT);
        break;
    }
    case DIA_SHIFT: {
        static const char *const before[6] = { "10", "20", "30", "40", NULL,
                                               NULL };
        static const Color f[6] = { { 58, 47, 92, 255 }, { 58, 47, 92, 255 },
                                    { 58, 47, 92, 255 }, { 58, 47, 92, 255 },
                                    { 34, 27, 56, 255 }, { 34, 27, 56, 255 } };
        float x = x0 + 10, w = 88;
        cell_row(x, y0 + 30, 6, before, f, 1, true, w);
        for (int k = 3; k >= 1; k--) {
            float a = cx_of(x, k, w), b = cx_of(x, k + 1, w);
            arrow((Vector2){ a + 8, y0 + 112 }, (Vector2){ b - 8, y0 + 112 },
                  UI_ACCENT);
            ctext(TextFormat("%d", 4 - k), (a + b) / 2, y0 + 120, FS_SMALL,
                  UI_ACCENT);
        }
        ctext("insert 25 at [1]: move 40, then 30, then 20 one place right",
              mid, box.y + box.height - 60, FS_SMALL, UI_MUTED);
        ctext("used = 4", mid, y0 - 70, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_GRID:
    case DIA_GRID_MATCH: {
        float x = mid - 2 * 82 + 30, y = y0 - 10, w = 72, h = 56;
        for (int c = 0; c < 4; c++)
            ctext(TextFormat("c%d", c), x + c * 82 + w / 2, y - 26, FS_SMALL,
                  UI_MUTED);
        for (int r = 0; r < 3; r++) {
            DrawText(TextFormat("r%d", r), (int)x - 50, (int)(y + r * 66 + 16),
                     FS_SMALL, UI_MUTED);
            for (int c = 0; c < 4; c++) {
                bool hit = d == DIA_GRID ? (r == 1 && c == 2)
                                         : (r == 2 && c >= 1);
                const char *val = d == DIA_GRID ? (hit ? "5" : "0")
                                                : (hit ? "3" : (r + c) % 2 ? "1"
                                                                           : "2");
                cell(x + c * 82, y + r * 66, w, h, val, hit ? D_SAFE : D_CELL,
                     hit);
            }
        }
        ctext(d == DIA_GRID ? "grid[1][2]: row 1, column 2"
                            : "grid[2][1], [2][2], [2][3] match",
              mid, box.y + box.height - 46, FS_SMALL, UI_ACCENT);
        break;
    }
    case DIA_STRING:
    case DIA_STRING_WALK: {
        static const char *const v[6] = { "B", "O", "M", "B", "\\0", "--" };
        static const Color f[6] = { { 58, 47, 92, 255 }, { 58, 47, 92, 255 },
                                    { 58, 47, 92, 255 }, { 58, 47, 92, 255 },
                                    { 52, 142, 112, 255 },
                                    { 34, 27, 56, 255 } };
        float x = x0 + 10, w = 88;
        int at = d == DIA_STRING ? 4 : 2;
        cell_row(x, y0, d == DIA_STRING ? 5 : 6, v, f, at, true, w);
        float ax = cx_of(x, at, w);
        arrow((Vector2){ ax, y0 + 150 }, (Vector2){ ax, y0 + 78 }, UI_ACCENT);
        ctext(d == DIA_STRING ? "the end marker" : "s  (*s is 'M')", ax,
              y0 + 156, FS_SMALL, UI_ACCENT);
        ctext(d == DIA_STRING ? "\"BOMB\": 4 letters, 5 bytes"
                              : "s++ moves to the next character",
              mid, box.y + box.height - 46, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_LIST:
    case DIA_LIST_INSERT: {
        bool ins = d == DIA_LIST_INSERT;
        float y = y0 + (ins ? -10 : 20), w = 150, gap = 70;
        static const char *const names[3] = { "10", "20", "30" };
        static const char *const ins_names[3] = { "A", "C", NULL };
        int n = ins ? 2 : 3;
        float start = ins ? x0 + 60 : x0 + 10;
        float step = ins ? 380 : w + gap;
        DrawText("head", (int)start - 6, (int)y - 34, FS_SMALL, UI_PINK);
        for (int k = 0; k < n; k++) {
            float nx = start + k * step;
            cell(nx, y, w - 50, 66, ins ? ins_names[k] : names[k], D_CELL,
                 false);
            cell(nx + w - 50, y, 50, 66, NULL, D_EMPTY, false);
            DrawCircle((int)(nx + w - 25), (int)(y + 33), 6, D_LINE);
            if (k + 1 < n && !ins)
                arrow((Vector2){ nx + w - 20, y + 33 },
                      (Vector2){ nx + step - 4, y + 33 }, D_LINE);
        }
        if (!ins) {
            ctext("NULL", start + 2 * step + w + 40, y + 20, FS_SMALL,
                  UI_MUTED);
            ctext("the last next is NULL: the list ends here", mid,
                  box.y + box.height - 46, FS_SMALL, UI_MUTED);
        } else {
            float bx = mid - 50, by = y + 140;
            cell(bx - 50, by, 100, 66, "B", D_SAFE, true);
            cell(bx + 50, by, 50, 66, NULL, D_EMPTY, false);
            DrawCircle((int)(bx + 75), (int)(by + 33), 6, UI_ACCENT);
            Vector2 b1 = { bx + 80, by + 30 }, b2 = { start + step - 4, y + 60 };
            arrow(b1, b2, UI_ACCENT);
            ctext("1", (b1.x + b2.x) / 2 + 30, (b1.y + b2.y) / 2 - 10, FS_BODY,
                  UI_ACCENT);
            Vector2 a1 = { start + w - 20, y + 40 }, a2 = { bx - 50, by + 20 };
            arrow(a1, a2, UI_PINK);
            ctext("2", (a1.x + a2.x) / 2 - 30, (a1.y + a2.y) / 2 - 10, FS_BODY,
                  UI_PINK);
            ctext("1: B->next = A->next     2: A->next = B", mid,
                  box.y + box.height - 40, FS_SMALL, UI_MUTED);
        }
        break;
    }
    case DIA_HEAP:
    case DIA_DANGLING: {
        bool dang = d == DIA_DANGLING;
        Rectangle stack = { box.x + 40, y0 - 30, 230, 190 };
        Rectangle heap = { box.x + 340, y0 - 30, box.width - 380, 190 };
        DrawRectangleRoundedLinesEx(stack, 0.08f, 6, 2.0f, UI_BORDER);
        DrawRectangleRoundedLinesEx(heap, 0.08f, 6, 2.0f, UI_BORDER);
        DrawText("STACK", (int)stack.x + 14, (int)stack.y + 12, FS_SMALL,
                 UI_MUTED);
        DrawText("HEAP", (int)heap.x + 14, (int)heap.y + 12, FS_SMALL, UI_MUTED);
        DrawText(dang ? "int *a" : "int *nums", (int)stack.x + 20,
                 (int)stack.y + 66, FS_SMALL, UI_INK);
        cell(stack.x + 40, stack.y + 100, 150, 60, NULL, D_CELL, true);
        DrawCircle((int)(stack.x + 115), (int)(stack.y + 130), 7, UI_ACCENT);
        float hx = heap.x + 40, hy = heap.y + 90;
        for (int k = 0; k < 4; k++)
            cell(hx + k * 80, hy, 70, 60, dang ? NULL : (k ? "?" : "42"),
                 dang ? D_EMPTY : D_CELL, false);
        arrow((Vector2){ stack.x + 122, stack.y + 130 },
              (Vector2){ hx - 4, hy + 30 }, dang ? UI_ERROR : UI_ACCENT);
        if (dang) {
            DrawLineEx((Vector2){ hx, hy }, (Vector2){ hx + 310, hy + 60 },
                       4.0f, UI_ERROR);
            DrawLineEx((Vector2){ hx, hy + 60 }, (Vector2){ hx + 310, hy },
                       4.0f, UI_ERROR);
            ctext("freed", hx + 155, hy - 34, FS_SMALL, UI_ERROR);
        } else {
            ctext("malloc(4 * sizeof(int))", hx + 155, hy - 34, FS_SMALL,
                  UI_ACCENT);
        }
        ctext(dang ? "a still points here, but the block was freed"
                   : "the block stays until free(nums)",
              mid, box.y + box.height - 46, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_VECTOR:
    case DIA_ITERATOR: {
        bool it = d == DIA_ITERATOR;
        static const char *const v[6] = { "10", "20", "30", NULL, NULL, NULL };
        static const char *const vi[6] = { "10", "99", "20", "30", NULL, NULL };
        static const Color f[6] = { { 52, 142, 112, 255 },
                                    { 52, 142, 112, 255 },
                                    { 52, 142, 112, 255 },
                                    { 34, 27, 56, 255 }, { 34, 27, 56, 255 },
                                    { 34, 27, 56, 255 } };
        static const Color fi[6] = { { 52, 142, 112, 255 },
                                     { 170, 62, 98, 255 },
                                     { 52, 142, 112, 255 },
                                     { 52, 142, 112, 255 },
                                     { 34, 27, 56, 255 },
                                     { 34, 27, 56, 255 } };
        float x = x0 + 10, w = 88;
        cell_row(x, y0, 6, it ? vi : v, it ? fi : f, it ? 1 : -1, true, w);
        if (!it) {
            float a = x, b = x + 3 * (w + 10) - 10;
            DrawLineEx((Vector2){ a, y0 + 94 }, (Vector2){ b, y0 + 94 }, 3.0f,
                       UI_ACCENT);
            ctext("size 3", (a + b) / 2, y0 + 104, FS_SMALL, UI_ACCENT);
            float c = x + 6 * (w + 10) - 10;
            DrawLineEx((Vector2){ a, y0 + 140 }, (Vector2){ c, y0 + 140 }, 3.0f,
                       D_LINE);
            ctext("capacity 6", (a + c) / 2, y0 + 150, FS_SMALL, D_LINE);
        } else {
            float ax = cx_of(x, 1, w);
            arrow((Vector2){ ax, y0 + 150 }, (Vector2){ ax, y0 + 78 },
                  UI_ACCENT);
            ctext("it", ax, y0 + 156, FS_SMALL, UI_ACCENT);
            float ex = cx_of(x, 4, w);
            arrow((Vector2){ ex, y0 + 150 }, (Vector2){ ex, y0 + 78 },
                  UI_MUTED);
            ctext("end()", ex, y0 + 156, FS_SMALL, UI_MUTED);
            ctext("erase(it) shifts 20, 30 left and returns the new [1]", mid,
                  box.y + box.height - 40, FS_SMALL, UI_MUTED);
        }
        break;
    }
    case DIA_CARDS_COST:
    case DIA_STRIKE: {
        bool strike = d == DIA_STRIKE;
        static const char *const v[5] = { "0", "1", "0", "1", "0" };
        static const Color f[5] = { { 52, 142, 112, 255 },
                                    { 170, 62, 98, 255 },
                                    { 52, 142, 112, 255 },
                                    { 170, 62, 98, 255 },
                                    { 52, 142, 112, 255 } };
        float x = x0 + 30, w = 86;
        cell_row(x, y0, 5, v, f, strike ? -1 : 1, true, w);
        if (strike) {
            float sx = x + 5 * (w + 10);
            Rectangle out = { sx, y0, w, 70 };
            DrawRectangleRoundedLinesEx(out, 0.18f, 6, 3.0f, UI_ERROR);
            ctext("[5]", sx + w / 2, y0 - 28, FS_SMALL, UI_ERROR);
            ctext("??", sx + w / 2, y0 + 20, FS_BODY, UI_ERROR);
            ctext("cards[N] is one past the end: STRIKE", mid, y0 + 120,
                  FS_SMALL, UI_ERROR);
        } else {
            for (int k = 0; k < 5; k++)
                ctext(TextFormat("to %s", "0"), cx_of(x, k, w), y0 + 80,
                      FS_SMALL, UI_ACCENT);
            ctext("READ cards[1] = 1 op     WRITE cards[1] = 0 = 1 op", mid,
                  y0 + 130, FS_SMALL, UI_INK);
        }
        Rectangle lcd = { mid - 110, box.y + box.height - 70, 220, 50 };
        DrawRectangleRounded(lcd, 0.2f, 6, (Color){ 26, 8, 10, 255 });
        ctext(strike ? "STRIKE" : "OPS 02/12", mid, lcd.y + 10, FS_BODY,
              (Color){ 255, 72, 60, 255 });
        break;
    }
    case DIA_CUBE_TIMER: {
        Rectangle lcd = { mid - 150, y0 - 40, 300, 90 };
        DrawRectangleRounded(lcd, 0.15f, 6, (Color){ 26, 8, 10, 255 });
        DrawRectangleRoundedLinesEx(lcd, 0.15f, 6, 2.0f, UI_BORDER);
        ctext("10:00", mid, lcd.y + 25, FS_TITLE, (Color){ 255, 72, 60, 255 });
        for (int k = 0; k < 5; k++) {
            Vector2 c = { mid - 120 + k * 60.0f, y0 + 90 };
            DrawCircleV(c, 16, k < 2 ? (Color){ 70, 255, 140, 255 }
                                     : (Color){ 60, 46, 40, 255 });
        }
        ctext("modules 2 / 5", mid, y0 + 120, FS_SMALL, UI_MUTED);
        ctext("START -> defuse all five before 00:00", mid,
              box.y + box.height - 46, FS_SMALL, UI_ACCENT);
        break;
    }
    case DIA_MAILROOM:
    case DIA_CALL: {
        bool call = d == DIA_CALL;
        float y = y0 + 10;
        DrawText("INBOX", (int)x0, (int)y - 34, FS_SMALL, D_LINE);
        static const char *const in[3] = { "5", "1", "79" };
        for (int k = 0; k < 3; k++)
            cell(x0 + k * 74, y, 64, 64, in[k], (Color){ 150, 104, 60, 255 },
                 false);
        float rx = mid - 40;
        DrawRectangleRounded((Rectangle){ rx, y - 10, 80, 84 }, 0.2f, 6,
                             (Color){ 240, 180, 40, 255 });
        DrawRectangle((int)rx + 14, (int)y + 6, 52, 22, (Color){ 40, 50, 70, 255 });
        DrawCircle((int)rx + 28, (int)y + 17, 5, D_LINE);
        DrawCircle((int)rx + 52, (int)y + 17, 5, D_LINE);
        float ox = box.x + box.width - 260;
        DrawText("OUTBOX", (int)ox, (int)y - 34, FS_SMALL, UI_SUCCESS);
        cell(ox, y, 64, 64, "5", (Color){ 150, 104, 60, 255 }, true);
        cell(ox + 74, y, 64, 64, NULL, D_EMPTY, false);
        cell(ox + 148, y, 64, 64, NULL, D_EMPTY, false);
        arrow((Vector2){ x0 + 220, y + 32 }, (Vector2){ rx - 6, y + 32 },
              UI_ACCENT);
        ctext(call ? "take() -> 5" : "take()", (x0 + 220 + rx) / 2, y + 74,
              FS_SMALL, UI_ACCENT);
        arrow((Vector2){ rx + 86, y + 32 }, (Vector2){ ox - 6, y + 32 },
              UI_SUCCESS);
        ctext(call ? "put(5)" : "put(n)", (rx + 86 + ox) / 2, y + 74, FS_SMALL,
              UI_SUCCESS);
        ctext(call ? "put(take()): take's answer becomes put's argument"
                   : "same parcels, same order",
              mid, box.y + box.height - 46, FS_SMALL, UI_MUTED);
        break;
    }
    case DIA_BELT_NIGHT: {
        static const int in[6] = { 12, 7, 40, 3, 88, 5 };
        draw_belt(box, in, 6, in, 6, "same parcels, as many as there are",
                  false);
        break;
    }
    case DIA_BELT_WEIGH: {
        static const int in[8] = { 3, 4, GAP, 10, 5, GAP, 21, 9 };
        static const int out[3] = { 7, 15, 30 };
        draw_belt(box, in, 8, out, 3, "each pair becomes its sum", false);
        break;
    }
    case DIA_BELT_COUNT: {
        static const int in[8] = { 4, 7, 0, GAP, 0, GAP, 9, 0 };
        static const int out[3] = { 2, 0, 1 };
        draw_belt(box, in, 8, out, 3, "count before each 0 (the 0 is a marker)",
                  true);
        break;
    }
    case DIA_BELT_MAX: {
        static const int in[7] = { 5, 9, 2, GAP, 7, 7, 1 };
        static const int out[2] = { 9, 7 };
        draw_belt(box, in, 7, out, 2, "biggest of each group of 3", false);
        break;
    }
    case DIA_BELT_COUNTDOWN: {
        static const int in[2] = { 3, 2 };
        static const int out[5] = { 3, 2, 1, 2, 1 };
        draw_belt(box, in, 2, out, 5, "n becomes n, n-1, ... 1", false);
        break;
    }
    case DIA_NONE:
    default:
        break;
    }
}

/* Code on a fixed character grid (like the editor), so columns and
 * comments line up; "//" comments are dimmed.
 * 코드는 에디터처럼 고정 글자 칸에 그려 열과 주석이 맞는다. "//" 주석은
 * 흐리게 그린다. */
#define CODE_ADVANCE 12

static void draw_code(const char *code, int x, int y)
{
    int lh = ui_line_height(FS_SMALL), col = 0;
    bool comment = false;
    for (const char *p = code; *p; p++) {
        if (*p == '\n') {
            y += lh;
            col = 0;
            comment = false;
            continue;
        }
        if (p[0] == '/' && p[1] == '/') comment = true;
        char ch[2] = { *p, '\0' };
        int w = MeasureText(ch, FS_SMALL);
        DrawText(ch, x + col * CODE_ADVANCE + (CODE_ADVANCE - w) / 2, y, FS_SMALL,
                 comment ? UI_MUTED : UI_INK);
        col++;
    }
}

/* Paragraphs separated by '\n', wrapped. / '\n'으로 나뉜 문단을 줄바꿈. */
static int paragraphs(const char *text, int x, int y, int width, int size,
                      Color c)
{
    char para[1024];
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if (n > sizeof para - 1) n = sizeof para - 1;
        memcpy(para, p, n);
        para[n] = '\0';
        y = ui_wrapped(para, x, y, width, size, c) + 14;
        if (!end) break;
        p = end + 1;
    }
    return y;
}

/* ========================================================================
 * The pop-up / 팝업
 * ====================================================================== */

bool help_button(Rectangle r, HelpTopic topic)
{
    bool hover = CheckCollisionPointRec(ui_mouse(), r);
    DrawRectangleRounded(r, 0.3f, 8, hover ? UI_HOVER : UI_PANEL);
    DrawRectangleRoundedLinesEx(r, 0.3f, 8, 2.0f, topic_tint(topic));
    float cy = r.y + r.height / 2;
    DrawCircle((int)(r.x + 26), (int)cy, 13, topic_tint(topic));
    DrawText("?", (int)r.x + 21, (int)cy - 10, FS_SMALL, UI_BG);
    DrawText("Help", (int)r.x + 48, (int)cy - 10, FS_SMALL, UI_INK);
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        help_open(topic);
        return true;
    }
    return false;
}

void help_draw(void)
{
    if (!g_open) return;
    ui_block_input(g_fresh);
    g_fresh = false;
    const HelpDoc *doc = &DOCS[g_topic];
    const HelpPage *pg = &doc->pages[g_page];
    Color tint = topic_tint(g_topic);

    DrawRectangle(0, 0, UI_W, UI_H, (Color){ 8, 5, 18, 200 });
    Rectangle win = { 150, 110, 1620, 980 };
    DrawRectangleRounded((Rectangle){ win.x, win.y + 8, win.width, win.height },
                         0.04f, 10, (Color){ 12, 8, 24, 255 });
    DrawRectangleRounded(win, 0.04f, 10, UI_PANEL);
    DrawRectangleRoundedLinesEx(win, 0.04f, 10, 3.0f, Fade(tint, 0.7f));

    /* title row / 제목 줄 */
    DrawCircle((int)win.x + 70, (int)win.y + 66, 30, tint);
    const char *badge = doc->badge ? doc->badge
                        : g_topic <= HELP_CHAPTER_8 ? TextFormat("%d", g_topic + 1)
                                                    : "?";
    ctext(badge, win.x + 70, win.y + 51, FS_BODY, UI_BG);
    DrawText(topic_title(g_topic), (int)win.x + 124, (int)win.y + 34, FS_TITLE,
             UI_INK);
    DrawText(TextFormat("FIELD MANUAL   /   page %d of %d", g_page + 1,
                        doc->count),
             (int)win.x + 126, (int)win.y + 82, FS_SMALL, tint);
    Rectangle close = { win.x + win.width - 170, win.y + 34, 136, 52 };
    if (ui_button(close, "Close", true, false)) g_open = false;

    /* left: explanation and tip / 왼쪽: 설명과 팁 */
    int lx = (int)win.x + 50, lw = 720;
    int y = (int)win.y + 150;
    DrawText(pg->heading, lx, y, FS_BODY, tint);
    int text_end = paragraphs(pg->text, lx, y + 56, lw, FS_SMALL, UI_INK);
    int tip_h = ui_wrapped_height(pg->tip, lw - 60, FS_SMALL) + 82;
    Rectangle tip = { (float)lx, (float)text_end + 16, (float)lw, (float)tip_h };
    DrawRectangleRounded(tip, 0.1f, 8, UI_BG);
    DrawRectangleRec((Rectangle){ tip.x, tip.y + 16, 6, tip.height - 32 }, tint);
    DrawText("TIP", lx + 30, (int)tip.y + 22, FS_SMALL, tint);
    ui_wrapped(pg->tip, lx + 30, (int)tip.y + 58, lw - 60, FS_SMALL, UI_INK);

    /* right: diagram and code / 오른쪽: 그림과 코드 */
    float rx = win.x + 820, rw = win.width - 870;
    float code_y = win.y + 150;
    if (pg->diagram != DIA_NONE) {
        Rectangle dbox = { rx, win.y + 150, rw, 330 };
        DrawRectangleRounded(dbox, 0.06f, 8, UI_BG);
        draw_diagram(pg->diagram, dbox);
        code_y = dbox.y + dbox.height + 24;
    }
    Rectangle cbox = { rx, code_y, rw, win.y + 850 - code_y };
    DrawRectangleRounded(cbox, 0.05f, 8, (Color){ 22, 17, 40, 255 });
    DrawText(pg->code_label ? pg->code_label : "EXAMPLE", (int)rx + 26,
             (int)cbox.y + 20, FS_SMALL, UI_MUTED);
    draw_code(pg->code, (int)rx + 26, (int)cbox.y + 58);

    /* footer: page tabs and navigation / 아래: 페이지 탭과 이동 */
    float fy = win.y + win.height - 96;
    float tx = win.x + 50;
    for (int k = 0; k < doc->count; k++) {
        float w = fmaxf(220.0f,
                        (float)MeasureText(doc->pages[k].heading, FS_SMALL) + 48);
        if (ui_button((Rectangle){ tx, fy, w, 56 }, doc->pages[k].heading, true,
                      k == g_page))
            g_page = k;
        tx += w + 14;
    }
    bool last = g_page + 1 >= doc->count;
    Rectangle next = { win.x + win.width - 250, fy, 200, 56 };
    if (ui_button(next, last ? "Got it" : "Next", true, last)) next_page();
    if (g_page > 0 &&
        ui_button((Rectangle){ next.x - 214, fy, 200, 56 }, "Back", true, false))
        g_page--;
    ui_text_right("Left / Right turn pages    Esc closes",
                  (int)(win.x + win.width - 50), (int)(win.y + win.height) - 30,
                  FS_SMALL, UI_MUTED);
    ui_block_input(false);
}
