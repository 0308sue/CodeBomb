/*
 * test_main.c - runs every headless test (no raylib needed)
 * test_main.c - 모든 헤드리스 테스트를 실행한다 (raylib 불필요)
 */
#include "test_util.h"

int g_checks;
int g_failures;

int main(void)
{
    printf("== VM ==\n");
    run_vm_tests();
    printf("== Compiler ==\n");
    run_compiler_tests();
    printf("== Editor ==\n");
    run_editor_tests();
    printf("== Array bridge (editor + Sumin's engine) ==\n");
    run_bridge_tests();
    printf("== Mailroom levels ==\n");
    run_level_tests();

    printf("\n%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures ? 1 : 0;
}
