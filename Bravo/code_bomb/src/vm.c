/*
 * vm.c - Code Foundry virtual machine implementation
 * vm.c - Code Foundry 가상 머신 구현
 */
#include "vm.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const OP_NAMES[OP_COUNT] = {
    [OP_NOP] = "NOP",
    [OP_PUSH] = "PUSH",
    [OP_POP] = "POP",
    [OP_DUP] = "DUP",
    [OP_DUP2] = "DUP2",
    [OP_LOAD_LOCAL] = "LOAD_LOCAL",
    [OP_STORE_LOCAL] = "STORE_LOCAL",
    [OP_ADDR_LOCAL] = "ADDR_LOCAL",
    [OP_LOAD_GLOBAL] = "LOAD_GLOBAL",
    [OP_STORE_GLOBAL] = "STORE_GLOBAL",
    [OP_LOAD] = "LOAD",
    [OP_STORE] = "STORE",
    [OP_ADD] = "ADD",
    [OP_SUB] = "SUB",
    [OP_MUL] = "MUL",
    [OP_DIV] = "DIV",
    [OP_MOD] = "MOD",
    [OP_NEG] = "NEG",
    [OP_EQ] = "EQ",
    [OP_NE] = "NE",
    [OP_LT] = "LT",
    [OP_LE] = "LE",
    [OP_GT] = "GT",
    [OP_GE] = "GE",
    [OP_NOT] = "NOT",
    [OP_JMP] = "JMP",
    [OP_JMP_FALSE] = "JMP_FALSE",
    [OP_JMP_TRUE] = "JMP_TRUE",
    [OP_CALL] = "CALL",
    [OP_CALL_NATIVE] = "CALL_NATIVE",
    [OP_RET] = "RET",
    [OP_RET_VAL] = "RET_VAL",
    [OP_HALT] = "HALT",
};

/* ========================================================================
 * Program builder / 프로그램 빌더
 * ====================================================================== */

void prog_init(Program *p)
{
    memset(p, 0, sizeof *p);
    p->entry_func = -1;
}

int prog_add_native(Program *p, const char *name, int argc,
                    bool returns_value, NativeFn fn)
{
    if (p->native_count >= VM_NATIVES_MAX || argc < 0 ||
        argc > VM_NATIVE_ARGS_MAX || !fn) {
        p->broken = true;
        return -1;
    }
    Native *n = &p->natives[p->native_count];
    snprintf(n->name, sizeof n->name, "%s", name);
    n->argc = argc;
    n->returns_value = returns_value;
    n->fn = fn;
    return p->native_count++;
}

int prog_find_native(const Program *p, const char *name)
{
    for (int i = 0; i < p->native_count; ++i)
        if (strcmp(p->natives[i].name, name) == 0) return i;
    return -1;
}

int prog_find_function(const Program *p, const char *name)
{
    for (int i = 0; i < p->func_count; ++i)
        if (strcmp(p->funcs[i].name, name) == 0) return i;
    return -1;
}

int prog_begin_function(Program *p, const char *name, bool returns_value)
{
    if (p->func_count >= VM_FUNCS_MAX) {
        p->broken = true;
        return -1;
    }
    Function *f = &p->funcs[p->func_count];
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "%s", name);
    f->entry = p->code_len;
    f->returns_value = returns_value;
    return p->func_count++;
}

/* Every parameter/local gets its own 4-byte slot in the frame.
 * 모든 매개변수와 지역 변수는 프레임 안에 4바이트 칸을 하나씩 갖는다. */
static int add_slot(Program *p, int func, const char *name, bool is_param)
{
    if (func < 0 || func >= p->func_count ||
        p->local_count >= VM_DEBUG_LOCALS_MAX) {
        p->broken = true;
        return -1;
    }
    Function *f = &p->funcs[func];
    if (is_param && f->frame_size != f->nparams * 4) {
        /* parameters must come before locals / 매개변수가 지역 변수보다 먼저 */
        p->broken = true;
        return -1;
    }
    int offset = f->frame_size;
    f->frame_size += 4;
    if (is_param) f->nparams++;

    DebugLocal *d = &p->locals[p->local_count++];
    d->func = func;
    d->offset = offset;
    d->is_param = is_param;
    snprintf(d->name, sizeof d->name, "%s", name);
    /* Visible everywhere unless the compiler narrows it.
     * 컴파일러가 좁히지 않으면 함수 전체에서 보인다. */
    d->scope_start = 0;
    d->scope_end = INT_MAX;
    return offset;
}

int prog_add_param(Program *p, int func, const char *name)
{
    return add_slot(p, func, name, true);
}

int prog_add_local(Program *p, int func, const char *name)
{
    return add_slot(p, func, name, false);
}

int prog_add_array(Program *p, const char *name, int dims, int read_native,
                   int write_native)
{
    if (p->array_count >= VM_ARRAYS_MAX || dims < 1 || dims > 2 ||
        read_native < 0 || write_native < 0) {
        p->broken = true;
        return -1;
    }
    ProgArray *a = &p->arrays[p->array_count];
    snprintf(a->name, sizeof a->name, "%s", name);
    a->dims = dims;
    a->read_native = read_native;
    a->write_native = write_native;
    return p->array_count++;
}

int prog_find_array(const Program *p, const char *name)
{
    for (int i = 0; i < p->array_count; ++i)
        if (strcmp(p->arrays[i].name, name) == 0) return i;
    return -1;
}

int prog_add_const(Program *p, const char *name, int32_t value)
{
    if (p->const_count >= VM_CONSTS_MAX) {
        p->broken = true;
        return -1;
    }
    ProgConst *c = &p->consts[p->const_count];
    snprintf(c->name, sizeof c->name, "%s", name);
    c->value = value;
    return p->const_count++;
}

int prog_find_const(const Program *p, const char *name)
{
    for (int i = 0; i < p->const_count; ++i)
        if (strcmp(p->consts[i].name, name) == 0) return i;
    return -1;
}

int prog_emit(Program *p, OpCode op, int32_t a, int32_t b, int line)
{
    if (p->code_len >= VM_CODE_MAX) {
        p->broken = true;
        return -1;
    }
    Instr *in = &p->code[p->code_len];
    in->op = (uint8_t)op;
    in->a = a;
    in->b = b;
    in->line = line;
    return p->code_len++;
}

void prog_patch(Program *p, int at, int32_t a)
{
    if (at >= 0 && at < p->code_len) p->code[at].a = a;
}

int prog_here(const Program *p) { return p->code_len; }

int prog_func_at(const Program *p, int pc)
{
    int best = -1;
    for (int i = 0; i < p->func_count; ++i)
        if (p->funcs[i].entry <= pc &&
            (best < 0 || p->funcs[i].entry > p->funcs[best].entry))
            best = i;
    return best;
}

int prog_visible_local(const Program *p, int func, int pc, const char *name)
{
    int best = -1;
    for (int i = 0; i < p->local_count; ++i) {
        const DebugLocal *d = &p->locals[i];
        if (d->func != func || strcmp(d->name, name) != 0) continue;
        if (pc < d->scope_start || pc >= d->scope_end) continue;
        if (best < 0 || d->scope_start >= p->locals[best].scope_start)
            best = i;
    }
    return best;
}

const char *prog_local_name(const Program *p, int func, int offset)
{
    for (int i = 0; i < p->local_count; ++i)
        if (p->locals[i].func == func && p->locals[i].offset == offset)
            return p->locals[i].name;
    return NULL;
}

/* ========================================================================
 * VM helpers / VM 도우미 함수
 * ====================================================================== */

void vm_error(VM *vm, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->error, sizeof vm->error, fmt, ap);
    va_end(ap);
    vm->error_line = vm->exec_line;
    vm->status = VM_ERROR;
}

static Frame *top_frame(VM *vm) { return &vm->frames[vm->frame_count - 1]; }

static bool is_init(const VM *vm, int32_t addr, int size)
{
    for (int i = 0; i < size; ++i)
        if (!vm->init[addr + i]) return false;
    return true;
}

static int32_t raw_read(const VM *vm, int32_t addr)
{
    int32_t v;
    memcpy(&v, &vm->mem[addr], sizeof v);
    return v;
}

static void raw_write(VM *vm, int32_t addr, int32_t v)
{
    memcpy(&vm->mem[addr], &v, sizeof v);
    memset(&vm->init[addr], 1, sizeof v);
}

/* Validate an access through a pointer and report a friendly error.
 * 포인터를 통한 접근을 검사하고 이해하기 쉬운 오류를 보고한다. */
static bool check_access(VM *vm, int32_t addr, int size, bool write)
{
    const char *verb = write ? "write to" : "read from";
    if (addr >= 0 && addr < VM_NULL_GUARD) {
        if (addr == 0)
            vm_error(vm, "Tried to %s a NULL pointer", verb);
        else
            vm_error(vm, "Tried to %s address %d, which is next to NULL. "
                         "Was the pointer ever set?", verb, (int)addr);
        return false;
    }
    if (addr < 0 || addr > VM_MEM_SIZE - size) {
        vm_error(vm, "Tried to %s address %d, outside the machine's memory",
                 verb, (int)addr);
        return false;
    }
    /* Above the current stack top = a frame that has already returned.
     * 현재 스택 꼭대기보다 위쪽 = 이미 반환된 함수의 프레임. */
    if (addr >= VM_STACK_BASE && addr < VM_STACK_LIMIT &&
        addr + size > vm->sp_mem) {
        vm_error(vm, "Tried to %s address %d, a local variable of a function "
                     "that has already returned (dangling pointer)",
                 verb, (int)addr);
        return false;
    }
    return true;
}

static bool push(VM *vm, int32_t v)
{
    if (vm->sp >= VM_OPSTACK_MAX) {
        vm_error(vm, "Expression too complex: the operand stack overflowed");
        return false;
    }
    vm->ops[vm->sp++] = v;
    return true;
}

static bool pop(VM *vm, int32_t *out)
{
    int base = vm->frame_count > 0 ? top_frame(vm)->op_base : 0;
    if (vm->sp <= base) {
        vm_error(vm, "Internal error: operand stack underflow (bad bytecode)");
        return false;
    }
    *out = vm->ops[--vm->sp];
    return true;
}

/* ========================================================================
 * Public execution API / 공개 실행 API
 * ====================================================================== */

void vm_init(VM *vm, const Program *prog, void *user)
{
    memset(vm, 0, sizeof *vm);
    vm->prog = prog;
    vm->user = user;
    vm->step_limit = VM_DEFAULT_STEP_LIMIT;
    vm->sp_mem = VM_STACK_BASE;
    vm->status = VM_PAUSED;

    /* Globals start as zero in C, so they count as initialised.
     * C에서 전역 변수는 0으로 시작하므로 초기화된 것으로 표시한다. */
    memset(&vm->init[VM_GLOBALS_BASE], 1, VM_STACK_BASE - VM_GLOBALS_BASE);

    if (prog->broken) {
        vm_error(vm, "The program has not been compiled successfully");
        return;
    }
    if (prog->entry_func < 0 || prog->entry_func >= prog->func_count) {
        vm_error(vm, "No entry function: write a run() function to start");
        return;
    }
    const Function *f = &prog->funcs[prog->entry_func];
    if (f->nparams != 0) {
        vm_error(vm, "%s() must not take parameters", f->name);
        return;
    }
    Frame *fr = &vm->frames[vm->frame_count++];
    fr->func = prog->entry_func;
    fr->return_pc = -1;
    fr->fp = vm->sp_mem;
    fr->op_base = 0;
    vm->sp_mem += f->frame_size;
    vm->pc = f->entry;
}

void vm_resume(VM *vm)
{
    if (vm->status == VM_YIELDED) vm->status = VM_PAUSED;
}

int vm_current_line(const VM *vm)
{
    if (vm->status == VM_ERROR) return vm->error_line;
    if (vm->prog && vm->pc >= 0 && vm->pc < vm->prog->code_len)
        return vm->prog->code[vm->pc].line;
    return 0;
}

bool vm_peek_i32(const VM *vm, int32_t addr, int32_t *out)
{
    if (addr < 0 || addr > VM_MEM_SIZE - 4) return false;
    if (!is_init(vm, addr, 4)) return false;
    *out = raw_read(vm, addr);
    return true;
}

#define POP(v)    do { if (!pop(vm, &(v))) return vm->status; } while (0)
#define PUSH(v)   do { if (!push(vm, (v))) return vm->status; } while (0)
#define FAIL(...) do { vm_error(vm, __VA_ARGS__); return vm->status; } while (0)

VMStatus vm_run(VM *vm, int max_instructions)
{
    const Program *prog = vm->prog;

    for (int n = 0; n < max_instructions; ++n) {
        if (vm->status != VM_PAUSED) return vm->status;

        if (vm->pc < 0 || vm->pc >= prog->code_len)
            FAIL("Internal error: program counter %d out of range", vm->pc);

        const Instr *in = &prog->code[vm->pc];
        vm->exec_line = in->line;

        /* Catch infinite loops before they freeze the game.
         * 무한 루프가 게임을 멈추기 전에 잡아낸다. */
        if (vm->steps >= vm->step_limit)
            FAIL("The program ran for %ld steps without finishing. "
                 "Is there a loop that never ends?", vm->steps);

        vm->pc++;
        vm->steps++;

        switch ((OpCode)in->op) {
        case OP_NOP:
            break;

        case OP_PUSH:
            PUSH(in->a);
            break;

        case OP_POP: {
            int32_t v;
            POP(v);
            break;
        }

        case OP_DUP: {
            int32_t v;
            POP(v);
            PUSH(v);
            PUSH(v);
            break;
        }

        case OP_DUP2: {
            int32_t b, a;
            POP(b);
            POP(a);
            PUSH(a);
            PUSH(b);
            PUSH(a);
            PUSH(b);
            break;
        }

        case OP_LOAD_LOCAL: {
            Frame *fr = top_frame(vm);
            if (in->a < 0 || in->a + 4 > prog->funcs[fr->func].frame_size)
                FAIL("Internal error: local slot %d out of frame", (int)in->a);
            int32_t addr = fr->fp + in->a;
            /* Reading an unset local is undefined behaviour in C.
             * 값이 없는 지역 변수를 읽는 것은 C에서 정의되지 않은 동작이다. */
            if (!is_init(vm, addr, 4)) {
                const char *name = prog_local_name(prog, fr->func, in->a);
                if (name)
                    FAIL("Variable '%s' is used before it has been given a "
                         "value", name);
                FAIL("A variable is used before it has been given a value");
            }
            PUSH(raw_read(vm, addr));
            break;
        }

        case OP_STORE_LOCAL: {
            Frame *fr = top_frame(vm);
            if (in->a < 0 || in->a + 4 > prog->funcs[fr->func].frame_size)
                FAIL("Internal error: local slot %d out of frame", (int)in->a);
            int32_t v;
            POP(v);
            raw_write(vm, fr->fp + in->a, v);
            break;
        }

        case OP_ADDR_LOCAL:
            PUSH(top_frame(vm)->fp + in->a);
            break;

        case OP_LOAD_GLOBAL: {
            int32_t addr = VM_GLOBALS_BASE + in->a;
            if (in->a < 0 || addr > VM_STACK_BASE - 4)
                FAIL("Internal error: global slot %d out of range", (int)in->a);
            PUSH(raw_read(vm, addr));
            break;
        }

        case OP_STORE_GLOBAL: {
            int32_t addr = VM_GLOBALS_BASE + in->a;
            if (in->a < 0 || addr > VM_STACK_BASE - 4)
                FAIL("Internal error: global slot %d out of range", (int)in->a);
            int32_t v;
            POP(v);
            raw_write(vm, addr, v);
            break;
        }

        case OP_LOAD: {
            int32_t addr;
            POP(addr);
            if (!check_access(vm, addr, 4, false)) return vm->status;
            if (!is_init(vm, addr, 4))
                FAIL("Read memory at address %d that was never given a value",
                     (int)addr);
            PUSH(raw_read(vm, addr));
            break;
        }

        case OP_STORE: {
            int32_t v, addr;
            POP(v);
            POP(addr);
            if (!check_access(vm, addr, 4, true)) return vm->status;
            raw_write(vm, addr, v);
            break;
        }

        case OP_ADD:
        case OP_SUB:
        case OP_MUL: {
            /* Compute in 64 bits to detect signed overflow (UB in C).
             * 부호 있는 오버플로(C의 UB)를 찾기 위해 64비트로 계산한다. */
            int32_t b, a;
            POP(b);
            POP(a);
            int64_t r;
            char sym;
            if (in->op == OP_ADD)      { r = (int64_t)a + b; sym = '+'; }
            else if (in->op == OP_SUB) { r = (int64_t)a - b; sym = '-'; }
            else                       { r = (int64_t)a * b; sym = '*'; }
            if (r < INT32_MIN || r > INT32_MAX)
                FAIL("Integer overflow: %d %c %d does not fit in an int "
                     "(undefined behaviour in C)", (int)a, sym, (int)b);
            PUSH((int32_t)r);
            break;
        }

        case OP_DIV:
        case OP_MOD: {
            int32_t b, a;
            POP(b);
            POP(a);
            if (b == 0)
                FAIL(in->op == OP_DIV ? "Division by zero"
                                      : "Remainder (%%) by zero");
            if (a == INT32_MIN && b == -1)
                FAIL("Integer overflow: %d / -1 does not fit in an int",
                     (int)a);
            /* C integer division truncates toward zero.
             * C의 정수 나눗셈은 0 쪽으로 버림한다. */
            PUSH(in->op == OP_DIV ? a / b : a % b);
            break;
        }

        case OP_NEG: {
            int32_t a;
            POP(a);
            if (a == INT32_MIN)
                FAIL("Integer overflow: -(%d) does not fit in an int", (int)a);
            PUSH(-a);
            break;
        }

        case OP_EQ: case OP_NE: case OP_LT:
        case OP_LE: case OP_GT: case OP_GE: {
            int32_t b, a;
            POP(b);
            POP(a);
            int r = 0;
            switch (in->op) {
            case OP_EQ: r = a == b; break;
            case OP_NE: r = a != b; break;
            case OP_LT: r = a < b;  break;
            case OP_LE: r = a <= b; break;
            case OP_GT: r = a > b;  break;
            default:    r = a >= b; break;
            }
            PUSH(r);
            break;
        }

        case OP_NOT: {
            int32_t a;
            POP(a);
            PUSH(!a);
            break;
        }

        case OP_JMP:
            vm->pc = in->a;
            break;

        case OP_JMP_FALSE: {
            int32_t c;
            POP(c);
            if (!c) vm->pc = in->a;
            break;
        }

        case OP_JMP_TRUE: {
            int32_t c;
            POP(c);
            if (c) vm->pc = in->a;
            break;
        }

        case OP_CALL: {
            if (in->a < 0 || in->a >= prog->func_count)
                FAIL("Internal error: bad function index %d", (int)in->a);
            const Function *f = &prog->funcs[in->a];
            if (in->b != f->nparams)
                FAIL("%s() needs %d argument(s) but was given %d",
                     f->name, f->nparams, (int)in->b);
            if (vm->frame_count >= VM_FRAMES_MAX)
                FAIL("Stack overflow: more than %d nested calls. "
                     "Does a function call itself forever?", VM_FRAMES_MAX);
            int32_t fp = vm->sp_mem;
            if (fp + f->frame_size > VM_STACK_LIMIT)
                FAIL("Stack overflow: out of stack memory. "
                     "Does a function call itself forever?");

            /* Fresh locals hold old garbage: mark them uninitialised.
             * 새 지역 변수에는 이전 쓰레기 값이 남아 있으므로 미초기화로 표시. */
            memset(&vm->init[fp], 0, (size_t)f->frame_size);
            for (int i = f->nparams - 1; i >= 0; --i) {
                int32_t v;
                POP(v);
                raw_write(vm, fp + 4 * i, v);
            }
            Frame *fr = &vm->frames[vm->frame_count++];
            fr->func = in->a;
            fr->return_pc = vm->pc;
            fr->fp = fp;
            fr->op_base = vm->sp;
            vm->sp_mem = fp + f->frame_size;
            vm->pc = f->entry;
            break;
        }

        case OP_CALL_NATIVE: {
            if (in->a < 0 || in->a >= prog->native_count)
                FAIL("Internal error: bad native index %d", (int)in->a);
            const Native *nat = &prog->natives[in->a];
            if (in->b != nat->argc)
                FAIL("%s() needs %d argument(s) but was given %d",
                     nat->name, nat->argc, (int)in->b);
            int32_t args[VM_NATIVE_ARGS_MAX] = {0};
            for (int i = nat->argc - 1; i >= 0; --i) POP(args[i]);

            int32_t ret = 0;
            NativeResult r = nat->fn(vm, args, &ret, vm->user);
            if (r == NATIVE_ERROR) {
                if (vm->status != VM_ERROR) FAIL("%s() failed", nat->name);
                return vm->status;
            }
            if (nat->returns_value) PUSH(ret);
            if (r == NATIVE_YIELD) {
                /* Pause so the scene can animate this action.
                 * 장면이 이 동작을 애니메이션할 수 있도록 일시정지. */
                vm->status = VM_YIELDED;
                return vm->status;
            }
            break;
        }

        case OP_RET:
        case OP_RET_VAL: {
            Frame fr = *top_frame(vm);
            const Function *f = &prog->funcs[fr.func];
            int32_t rv = 0;
            bool has_value = in->op == OP_RET_VAL;
            if (has_value) POP(rv);
            if (f->returns_value && !has_value)
                FAIL("%s() reached its end without returning a value",
                     f->name);
            vm->frame_count--;
            vm->sp = fr.op_base;
            vm->sp_mem = fr.fp;
            if (vm->frame_count == 0) {
                vm->exit_value = rv;
                vm->status = VM_HALTED;
                return vm->status;
            }
            vm->pc = fr.return_pc;
            if (f->returns_value) PUSH(rv);
            break;
        }

        case OP_HALT:
            vm->status = VM_HALTED;
            return vm->status;

        default:
            FAIL("Internal error: unknown opcode %d", (int)in->op);
        }
    }
    return vm->status;
}

/* ========================================================================
 * Debug helpers / 디버그 도우미
 * ====================================================================== */

const char *vm_op_name(int op)
{
    if (op < 0 || op >= OP_COUNT || !OP_NAMES[op]) return "???";
    return OP_NAMES[op];
}

const char *vm_status_name(VMStatus s)
{
    switch (s) {
    case VM_PAUSED:  return "READY";
    case VM_YIELDED: return "WAITING";
    case VM_HALTED:  return "FINISHED";
    case VM_ERROR:   return "ERROR";
    }
    return "?";
}

void vm_disasm(const Program *p, int pc, char *buf, size_t n)
{
    if (pc < 0 || pc >= p->code_len) {
        snprintf(buf, n, "%04d  ---", pc);
        return;
    }
    const Instr *in = &p->code[pc];
    const char *name = vm_op_name(in->op);

    switch ((OpCode)in->op) {
    case OP_CALL:
        snprintf(buf, n, "%04d  %-12s %s/%d", pc, name,
                 (in->a >= 0 && in->a < p->func_count) ? p->funcs[in->a].name
                                                       : "?",
                 (int)in->b);
        break;
    case OP_CALL_NATIVE:
        snprintf(buf, n, "%04d  %-12s %s/%d", pc, name,
                 (in->a >= 0 && in->a < p->native_count)
                     ? p->natives[in->a].name : "?",
                 (int)in->b);
        break;
    case OP_LOAD_LOCAL:
    case OP_STORE_LOCAL:
    case OP_ADDR_LOCAL: {
        const char *local = prog_local_name(p, prog_func_at(p, pc), in->a);
        if (local)
            snprintf(buf, n, "%04d  %-12s %s", pc, name, local);
        else
            snprintf(buf, n, "%04d  %-12s [fp+%d]", pc, name, (int)in->a);
        break;
    }
    case OP_PUSH:
    case OP_JMP:
    case OP_JMP_FALSE:
    case OP_JMP_TRUE:
    case OP_LOAD_GLOBAL:
    case OP_STORE_GLOBAL:
        snprintf(buf, n, "%04d  %-12s %d", pc, name, (int)in->a);
        break;
    default:
        snprintf(buf, n, "%04d  %s", pc, name);
        break;
    }
}
