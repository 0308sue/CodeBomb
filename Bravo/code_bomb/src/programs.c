/*
 * programs.c - level 1 examples with reference bytecode
 * programs.c - 기준 바이트코드를 포함한 레벨 1 예제
 *
 * Line numbers passed to prog_emit() match the source text.
 * prog_emit()에 넘기는 줄 번호는 소스 텍스트의 줄과 일치한다.
 */
#include "programs.h"

/* ---- Solution: three calls / 정답: 호출 세 번 ---------------------------- */
static const char SRC_SOLUTION[] =
    "void run(void) {\n"
    "    put(take());\n"
    "    put(take());\n"
    "    put(take());\n"
    "}\n";

static void build_solution(Program *p)
{
    int take = prog_find_native(p, "take");
    int put = prog_find_native(p, "put");
    int f = prog_begin_function(p, "run", false);
    for (int line = 2; line <= 4; ++line) {
        prog_emit(p, OP_CALL_NATIVE, take, 0, line);
        prog_emit(p, OP_CALL_NATIVE, put, 1, line);
    }
    prog_emit(p, OP_RET, 0, 0, 5);
    p->entry_func = f;
}

/* ---- Loop: preview of variables and while ------------------------------
 * ---- 반복: 변수와 while 미리 보기 */
static const char SRC_LOOP[] =
    "void run(void) {\n"
    "    while (has_input()) {\n"
    "        int parcel = take();\n"
    "        put(parcel);\n"
    "    }\n"
    "}\n";

static void build_loop(Program *p)
{
    int take = prog_find_native(p, "take");
    int put = prog_find_native(p, "put");
    int has_input = prog_find_native(p, "has_input");
    int f = prog_begin_function(p, "run", false);
    int parcel = prog_add_local(p, f, "parcel");

    int top = prog_here(p);
    prog_emit(p, OP_CALL_NATIVE, has_input, 0, 2);
    int exit_jump = prog_emit(p, OP_JMP_FALSE, 0, 0, 2);
    prog_emit(p, OP_CALL_NATIVE, take, 0, 3);
    prog_emit(p, OP_STORE_LOCAL, parcel, 0, 3);
    prog_emit(p, OP_LOAD_LOCAL, parcel, 0, 4);
    prog_emit(p, OP_CALL_NATIVE, put, 1, 4);
    prog_emit(p, OP_JMP, top, 0, 5);
    prog_patch(p, exit_jump, prog_here(p));
    prog_emit(p, OP_RET, 0, 0, 6);
    p->entry_func = f;
}

/* ---- Bug: one take() too many / 버그: take()를 한 번 더 호출 ------------- */
static const char SRC_TOO_MANY[] =
    "void run(void) {\n"
    "    put(take());\n"
    "    put(take());\n"
    "    put(take());\n"
    "    put(take());   // one too many?\n"
    "}\n";

static void build_too_many(Program *p)
{
    int take = prog_find_native(p, "take");
    int put = prog_find_native(p, "put");
    int f = prog_begin_function(p, "run", false);
    for (int line = 2; line <= 5; ++line) {
        prog_emit(p, OP_CALL_NATIVE, take, 0, line);
        prog_emit(p, OP_CALL_NATIVE, put, 1, line);
    }
    prog_emit(p, OP_RET, 0, 0, 6);
    p->entry_func = f;
}

/* ---- Bug: reading an unset variable / 버그: 값이 없는 변수 읽기 ---------- */
static const char SRC_UNINIT[] =
    "void run(void) {\n"
    "    int parcel;\n"
    "    put(parcel);   // what is in parcel?\n"
    "}\n";

static void build_uninit(Program *p)
{
    int put = prog_find_native(p, "put");
    int f = prog_begin_function(p, "run", false);
    int parcel = prog_add_local(p, f, "parcel");
    prog_emit(p, OP_LOAD_LOCAL, parcel, 0, 3);
    prog_emit(p, OP_CALL_NATIVE, put, 1, 3);
    prog_emit(p, OP_RET, 0, 0, 4);
    p->entry_func = f;
}

static const DemoProgram DEMOS[] = {
    { "Solution", SRC_SOLUTION, 3, build_solution,
      "A working answer for three parcels" },
    { "Loop",     SRC_LOOP,     3, build_loop,
      "while (has_input()): any number of parcels" },
    { "Too many", SRC_TOO_MANY, 4, build_too_many,
      "Bug: take() from an empty inbox" },
    { "Uninit",   SRC_UNINIT,   2, build_uninit,
      "Bug: reads a variable that has no value" },
};

int demo_count(void) { return (int)(sizeof DEMOS / sizeof DEMOS[0]); }

const DemoProgram *demo_get(int index)
{
    if (index < 0 || index >= demo_count()) return NULL;
    return &DEMOS[index];
}

bool demo_build(const DemoProgram *demo, const LevelDef *level, Program *out)
{
    prog_init(out);
    level->register_natives(out);
    demo->build(out);
    return !out->broken && out->entry_func >= 0;
}
