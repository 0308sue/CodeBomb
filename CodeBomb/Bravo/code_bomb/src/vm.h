/*
 * vm.h - Code Foundry virtual machine
 * vm.h - Code Foundry 가상 머신
 *
 * A small stack-based bytecode VM that runs the player's compiled C code.
 * 플레이어가 작성한 C 코드를 컴파일한 바이트코드를 실행하는 작은 스택 기반 VM.
 *
 * Design goals / 설계 목표:
 *   - Resumable: vm_run() executes a budget of instructions and returns, so
 *     the raylib main loop can draw frames between instructions.
 *     재개 가능: vm_run()은 정해진 개수의 명령만 실행하고 반환하므로
 *     raylib 메인 루프가 명령 사이에 화면을 그릴 수 있다.
 *   - Teachable: memory is a simulated byte array, so pointers are real
 *     integer addresses. Bad accesses become friendly errors, not crashes.
 *     교육용: 메모리는 시뮬레이션된 바이트 배열이므로 포인터가 실제 정수 주소다.
 *     잘못된 접근은 크래시가 아니라 친절한 오류 메시지가 된다.
 *   - Host-driven: game actions (take, put, ...) are "natives" that may
 *     YIELD, pausing the VM until the scene has finished animating.
 *     호스트 주도: 게임 동작(take, put 등)은 "네이티브" 함수이며 YIELD 하여
 *     장면 애니메이션이 끝날 때까지 VM을 멈출 수 있다.
 *
 * Memory layout / 메모리 배치:
 *     0     .. 4095    NULL guard page (any access is an error)
 *                      NULL 보호 영역 (접근하면 오류)
 *     4096  .. 8191    globals (zero-initialised, like real C)
 *                      전역 변수 (실제 C처럼 0으로 초기화)
 *     8192  .. 32767   call stack (parameters + locals)
 *                      호출 스택 (매개변수 + 지역 변수)
 *     32768 .. 65535   heap (malloc/free arrive in milestone 3)
 *                      힙 (malloc/free는 마일스톤 3에서 추가)
 */
#ifndef VM_H
#define VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VM_MEM_SIZE         (64 * 1024)
#define VM_NULL_GUARD       4096
#define VM_GLOBALS_BASE     4096
#define VM_STACK_BASE       8192
#define VM_STACK_LIMIT      32768
#define VM_HEAP_BASE        32768

#define VM_OPSTACK_MAX      256
#define VM_FRAMES_MAX       64
#define VM_CODE_MAX         4096
#define VM_FUNCS_MAX        32
#define VM_NATIVES_MAX      16
#define VM_ARRAYS_MAX       4
#define VM_CONSTS_MAX       8
#define VM_NATIVE_ARGS_MAX  8
#define VM_DEBUG_LOCALS_MAX 256
#define VM_NAME_MAX         32
#define VM_ERROR_MAX        256
#define VM_DEFAULT_STEP_LIMIT 100000L

/* Instruction set. "a" and "b" are the instruction's operands.
 * 명령어 집합. "a"와 "b"는 명령의 피연산자다. */
typedef enum {
    OP_NOP,
    OP_PUSH,          /* push a                    / a를 push              */
    OP_POP,           /* discard top               / 맨 위 값을 버림       */
    OP_DUP,           /* duplicate top             / 맨 위 값을 복제       */
    OP_DUP2,          /* duplicate top two (a b -> a b a b), used for
                         grid[r][c] += v / 맨 위 두 값을 복제 (복합 대입용) */
    OP_LOAD_LOCAL,    /* push int at fp + a        / fp+a의 int를 push     */
    OP_STORE_LOCAL,   /* pop, store at fp + a      / pop 후 fp+a에 저장    */
    OP_ADDR_LOCAL,    /* push address fp + a (&x)  / 주소 fp+a를 push      */
    OP_LOAD_GLOBAL,   /* push int at globals + a   / 전역+a의 int를 push   */
    OP_STORE_GLOBAL,  /* pop, store at globals + a / pop 후 전역+a에 저장  */
    OP_LOAD,          /* pop addr, push *addr      / 주소를 pop, *주소 push */
    OP_STORE,         /* pop value, pop addr, *addr = value
                         값과 주소를 pop 하고 *주소 = 값                    */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_NOT,
    OP_JMP,           /* pc = a                                           */
    OP_JMP_FALSE,     /* pop, if zero pc = a       / pop, 0이면 pc = a     */
    OP_JMP_TRUE,      /* pop, if non-zero pc = a   / pop, 0이 아니면 pc = a */
    OP_CALL,          /* call function a, b args   / 함수 a를 인자 b개로 호출 */
    OP_CALL_NATIVE,   /* call native a, b args     / 네이티브 a를 호출      */
    OP_RET,           /* return (void)             / 반환 (void)           */
    OP_RET_VAL,       /* pop value and return it   / 값을 pop 하여 반환     */
    OP_HALT,
    OP_COUNT
} OpCode;

typedef struct {
    uint8_t op;
    int32_t a;
    int32_t b;
    int32_t line;     /* source line for highlighting and errors
                         하이라이트와 오류 표시에 쓰는 소스 줄 번호 */
} Instr;

typedef struct {
    char name[VM_NAME_MAX];
    int  entry;          /* first instruction        / 첫 명령의 위치       */
    int  nparams;
    int  frame_size;     /* bytes for params+locals  / 매개변수+지역 변수 바이트 */
    bool returns_value;
} Function;

struct VM;

typedef enum {
    NATIVE_OK,      /* finished, keep running / 완료, 계속 실행 */
    NATIVE_YIELD,   /* finished, pause until vm_resume()
                       완료, vm_resume() 호출 전까지 일시정지 */
    NATIVE_ERROR    /* failed; the native already called vm_error()
                       실패, 네이티브가 이미 vm_error()를 호출함 */
} NativeResult;

typedef NativeResult (*NativeFn)(struct VM *vm, const int32_t *args,
                                 int32_t *ret, void *user);

typedef struct {
    char     name[VM_NAME_MAX];
    int      argc;
    bool     returns_value;
    NativeFn fn;
} Native;

/* Variable names for the inspector and error messages.
 * 인스펙터와 오류 메시지에 쓰는 변수 이름 정보. */
typedef struct {
    int  func;
    int  offset;
    bool is_param;
    char name[VM_NAME_MAX];
    int  scope_start;    /* first pc where the name is visible
                            이름이 보이기 시작하는 pc */
    int  scope_end;      /* first pc after the name's block ends
                            이름의 블록이 끝난 직후의 pc */
} DebugLocal;

/* Level-provided array such as cards[] or grid[][]. The VM never sees it:
 * the compiler turns cards[i] into calls to the read/write natives, so the
 * level (Sumin's array engine) checks every access.
 * 레벨이 제공하는 배열 (cards[], grid[][]). VM은 이것을 직접 다루지 않는다:
 * 컴파일러가 cards[i]를 읽기/쓰기 네이티브 호출로 바꾸므로 모든 접근을
 * 레벨(수민의 배열 엔진)이 검사한다. */
typedef struct {
    char name[VM_NAME_MAX];
    int  dims;            /* 1 = cards[i], 2 = grid[i][j]                   */
    int  read_native;     /* native(row, col) -> value                      */
    int  write_native;    /* native(row, col, value) -> value               */
} ProgArray;

/* Level-provided constant such as N, ROWS, COLS (like a #define).
 * 레벨이 제공하는 상수 (N, ROWS, COLS 등, #define과 비슷하다). */
typedef struct {
    char    name[VM_NAME_MAX];
    int32_t value;
} ProgConst;

typedef struct {
    Instr      code[VM_CODE_MAX];
    int        code_len;
    Function   funcs[VM_FUNCS_MAX];
    int        func_count;
    Native     natives[VM_NATIVES_MAX];
    int        native_count;
    DebugLocal locals[VM_DEBUG_LOCALS_MAX];
    int        local_count;
    ProgArray  arrays[VM_ARRAYS_MAX];   /* compile-time only / 컴파일 전용 */
    int        array_count;
    ProgConst  consts[VM_CONSTS_MAX];   /* compile-time only / 컴파일 전용 */
    int        const_count;
    int        entry_func;
    bool       broken;    /* builder ran out of space or compile failed
                             공간 부족 또는 컴파일 실패 시 true */
} Program;

typedef struct {
    int     func;
    int     return_pc;
    int32_t fp;          /* address of this frame's first local
                            이 프레임 첫 지역 변수의 주소 */
    int     op_base;     /* operand stack height at entry
                            진입 시점의 피연산자 스택 높이 */
} Frame;

typedef enum {
    VM_PAUSED,    /* ready to execute more    / 더 실행할 준비 완료   */
    VM_YIELDED,   /* waiting for the host     / 호스트(애니메이션) 대기 */
    VM_HALTED,    /* finished normally        / 정상 종료             */
    VM_ERROR      /* runtime error            / 실행 중 오류          */
} VMStatus;

typedef struct VM {
    const Program *prog;
    void          *user;

    uint8_t  mem[VM_MEM_SIZE];
    uint8_t  init[VM_MEM_SIZE];   /* 1 = byte has been written
                                     1 = 값이 기록된 바이트 */

    int32_t  ops[VM_OPSTACK_MAX];
    int      sp;
    Frame    frames[VM_FRAMES_MAX];
    int      frame_count;
    int32_t  sp_mem;              /* next free byte on the call stack
                                     호출 스택의 다음 빈 바이트 */

    int      pc;
    int      exec_line;           /* line of the executing instruction
                                     실행 중인 명령의 줄 번호 */
    long     steps;
    long     step_limit;
    int32_t  exit_value;

    VMStatus status;
    char     error[VM_ERROR_MAX];
    int      error_line;
} VM;

/* ---- Program building (used by the compiler and hand-built tests)
 * ---- 프로그램 구성 (컴파일러와 수동 테스트에서 사용) */
void prog_init(Program *p);
int  prog_add_native(Program *p, const char *name, int argc,
                     bool returns_value, NativeFn fn);
int  prog_find_native(const Program *p, const char *name);
int  prog_find_function(const Program *p, const char *name);
int  prog_begin_function(Program *p, const char *name, bool returns_value);
int  prog_add_param(Program *p, int func, const char *name); /* before locals
                                                                지역 변수보다 먼저 */
int  prog_add_local(Program *p, int func, const char *name);
int  prog_add_array(Program *p, const char *name, int dims, int read_native,
                    int write_native);
int  prog_find_array(const Program *p, const char *name);
int  prog_add_const(Program *p, const char *name, int32_t value);
int  prog_find_const(const Program *p, const char *name);
int  prog_emit(Program *p, OpCode op, int32_t a, int32_t b, int line);
void prog_patch(Program *p, int at, int32_t a);
int  prog_here(const Program *p);
int  prog_func_at(const Program *p, int pc);
const char *prog_local_name(const Program *p, int func, int offset);
/* Debug-local index of the variable called name that is visible at pc in
 * func (innermost wins), or -1.
 * func 안에서 pc 위치에 보이는 name 변수의 디버그 정보 번호 (가장 안쪽
 * 우선). 없으면 -1. */
int  prog_visible_local(const Program *p, int func, int pc, const char *name);

/* ---- Execution / 실행 */
void     vm_init(VM *vm, const Program *prog, void *user);
VMStatus vm_run(VM *vm, int max_instructions);
void     vm_resume(VM *vm);
void     vm_error(VM *vm, const char *fmt, ...);
int      vm_current_line(const VM *vm);
bool     vm_peek_i32(const VM *vm, int32_t addr, int32_t *out);

/* ---- Debugging helpers / 디버깅 도우미 */
const char *vm_op_name(int op);
const char *vm_status_name(VMStatus s);
void        vm_disasm(const Program *p, int pc, char *buf, size_t n);

#endif
