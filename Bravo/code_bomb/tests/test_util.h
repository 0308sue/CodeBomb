/*
 * test_util.h - tiny test framework shared by all test files
 * test_util.h - 모든 테스트 파일이 함께 쓰는 아주 작은 테스트 프레임워크
 */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond, what)                                                   \
    do {                                                                    \
        g_checks++;                                                         \
        if (!(cond)) {                                                      \
            g_failures++;                                                   \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, what);         \
        }                                                                   \
    } while (0)

void run_vm_tests(void);
void run_compiler_tests(void);
void run_editor_tests(void);
void run_bridge_tests(void);
void run_level_tests(void);
void run_variant_tests(void);

#endif
