/*
 * compiler.h - compiles C source into VM bytecode
 * compiler.h - C 소스를 VM 바이트코드로 컴파일한다
 *
 * Usage / 사용법:
 *     prog_init(&prog);
 *     level->register_natives(&prog);       // take, put, ... first / 먼저 등록
 *     compile_program(src, len, &prog, &result);
 *
 * On failure, result holds the line, column and a friendly message, and the
 * program is marked broken so the VM refuses to run it.
 * 실패하면 result에 줄, 열, 친절한 메시지가 담기고, 프로그램은 broken으로
 * 표시되어 VM이 실행하지 않는다.
 */
#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"
#include "vm.h"

bool compile_program(const char *src, int len, Program *prog,
                     CompileResult *res);

#endif
