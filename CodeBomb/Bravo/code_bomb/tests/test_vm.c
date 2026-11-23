/*
 * test_vm.c - VM tests using hand-built bytecode
 * test_vm.c - 손으로 만든 바이트코드로 VM을 테스트한다
 */
#include "level.h"
#include "programs.h"
#include "test_util.h"
#include "vm.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ---- test natives / 테스트용 네이티브 ----------------------------------- */
static int32_t g_reported[64];
static int g_report_count;

static NativeResult nat_report(VM *vm, const int32_t *args, int32_t *ret,
                               void *user)
{
    (void)vm; (void)ret; (void)user;
    if (g_report_count < 64) g_reported[g_report_count++] = args[0];
    return NATIVE_OK;
}

static NativeResult nat_yield7(VM *vm, const int32_t *args, int32_t *ret,
                               void *user)
{
    (void)vm; (void)args; (void)user;
    *ret = 7;
    return NATIVE_YIELD;
}

static void setup(Program *p)
{
    prog_init(p);
    prog_add_native(p, "report", 1, false, nat_report);   /* native 0 */
    prog_add_native(p, "yield7", 0, true, nat_yield7);    /* native 1 */
    g_report_count = 0;
}

static VMStatus run_all(VM *vm, const Program *p)
{
    vm_init(vm, p, NULL);
    while (vm->status == VM_PAUSED || vm->status == VM_YIELDED) {
        vm_resume(vm);
        vm_run(vm, 1000);
    }
    return vm->status;
}

#define E(op, a, b) prog_emit(p, op, a, b, 1)

static void test_arithmetic(Program *p, VM *vm)
{
    setup(p);
    int f = prog_begin_function(p, "run", false);
    E(OP_PUSH, 2, 0); E(OP_PUSH, 3, 0); E(OP_PUSH, 4, 0);
    E(OP_MUL, 0, 0); E(OP_ADD, 0, 0); E(OP_CALL_NATIVE, 0, 1);
    E(OP_PUSH, 17, 0); E(OP_PUSH, 5, 0); E(OP_MOD, 0, 0);
    E(OP_CALL_NATIVE, 0, 1);
    E(OP_PUSH, -7, 0); E(OP_PUSH, 2, 0); E(OP_DIV, 0, 0);
    E(OP_CALL_NATIVE, 0, 1);
    E(OP_RET, 0, 0);
    p->entry_func = f;

    CHECK(run_all(vm, p) == VM_HALTED, "arithmetic halts");
    CHECK(g_reported[0] == 14, "2 + 3 * 4 == 14");
    CHECK(g_reported[1] == 2, "17 % 5 == 2");
    CHECK(g_reported[2] == -3, "-7 / 2 == -3 (truncates toward zero)");
}

static void expect_error(Program *p, VM *vm, const char *needle,
                         const char *what)
{
    VMStatus s = run_all(vm, p);
    char buf[512];
    snprintf(buf, sizeof buf, "%s (got: %s)", what, vm->error);
    CHECK(s == VM_ERROR && strstr(vm->error, needle) != NULL, buf);
}

static void test_errors(Program *p, VM *vm)
{
    int f, g, x;

    setup(p);
    f = prog_begin_function(p, "run", false);
    E(OP_PUSH, 0, 0); E(OP_LOAD, 0, 0); E(OP_RET, 0, 0);
    p->entry_func = f;
    expect_error(p, vm, "NULL", "NULL dereference");

    setup(p);
    g = prog_begin_function(p, "leak_address", true);
    x = prog_add_local(p, g, "x");
    E(OP_PUSH, 5, 0); E(OP_STORE_LOCAL, x, 0);
    E(OP_ADDR_LOCAL, x, 0); E(OP_RET_VAL, 0, 0);
    f = prog_begin_function(p, "run", false);
    E(OP_CALL, g, 0); E(OP_LOAD, 0, 0); E(OP_CALL_NATIVE, 0, 1);
    E(OP_RET, 0, 0);
    p->entry_func = f;
    expect_error(p, vm, "dangling", "dangling pointer to a dead local");

    setup(p);
    f = prog_begin_function(p, "run", false);
    E(OP_JMP, 0, 0);
    p->entry_func = f;
    expect_error(p, vm, "never ends", "infinite loop hits the step limit");

    setup(p);
    f = prog_begin_function(p, "run", false);
    x = prog_add_local(p, f, "x");
    E(OP_ADDR_LOCAL, x, 0); E(OP_PUSH, 9, 0); E(OP_STORE, 0, 0);
    E(OP_LOAD_LOCAL, x, 0); E(OP_CALL_NATIVE, 0, 1); E(OP_RET, 0, 0);
    p->entry_func = f;
    CHECK(run_all(vm, p) == VM_HALTED && g_reported[0] == 9,
          "*(&x) = 9 writes through a pointer");
}

static void test_yield(Program *p, VM *vm)
{
    setup(p);
    int f = prog_begin_function(p, "run", false);
    E(OP_CALL_NATIVE, 1, 0); E(OP_CALL_NATIVE, 0, 1); E(OP_RET, 0, 0);
    p->entry_func = f;

    vm_init(vm, p, NULL);
    CHECK(vm_run(vm, 100) == VM_YIELDED, "native yields");
    CHECK(vm_run(vm, 100) == VM_YIELDED, "stays yielded until resumed");
    vm_resume(vm);
    CHECK(vm_run(vm, 100) == VM_HALTED, "resumes to completion");
    CHECK(g_reported[0] == 7, "yielded native's return value is kept");
}

static void test_level1(Program *p, VM *vm)
{
    SubmitResult r;
    demo_build(demo_get(0), &LEVEL_MAILROOM, p);
    level_submit(&LEVEL_MAILROOM, p, 5, &r);
    CHECK(r.passed, "hand-built solution passes");
    CHECK(r.avg_steps == LEVEL_MAILROOM.par_speed, "solution meets speed par");

    /* Visible runs must yield on every take/put.
     * 화면에 보이는 실행은 take/put마다 YIELD 해야 한다. */
    LevelRun run;
    level_run_init(&run, &LEVEL_MAILROOM, 0, false);
    vm_init(vm, p, &run);
    int yields = 0;
    while (vm->status == VM_PAUSED || vm->status == VM_YIELDED) {
        if (vm->status == VM_YIELDED) {
            yields++;
            vm_resume(vm);
        }
        vm_run(vm, 1000);
    }
    CHECK(vm->status == VM_HALTED, "visible run halts");
    CHECK(yields == 6, "3 takes + 3 puts each yield once");
}

void run_vm_tests(void)
{
    Program *p = malloc(sizeof *p);
    VM *vm = malloc(sizeof *vm);
    if (!p || !vm) return;
    test_arithmetic(p, vm);
    test_errors(p, vm);
    test_yield(p, vm);
    test_level1(p, vm);
    free(p);
    free(vm);
}
