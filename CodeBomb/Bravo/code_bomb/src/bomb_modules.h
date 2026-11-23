/*
 * bomb_modules.h - the harder levels: a 6-sided cube bomb with one coding
 * module on each of five faces (the sixth face holds the display)
 * bomb_modules.h - 더 어려운 레벨: 다섯 면에 코딩 모듈이 하나씩 있는 6면
 * 큐브 폭탄 (여섯 번째 면에는 표시창이 있다)
 *
 * Each module is an ordinary SooLevel, so Sumin's engine checks it exactly
 * like her own levels: bounds, the operation budget, 0/1 values and the
 * target pattern. Only the targets are new.
 * 각 모듈은 보통 SooLevel이므로 수민의 엔진이 수민의 레벨과 똑같이 검사한다:
 * 범위, 연산 예산, 0/1 값, 목표 패턴. 새로운 것은 목표뿐이다.
 */
#ifndef BOMB_MODULES_H
#define BOMB_MODULES_H

#include "array_core_SOO.h"

#define CUBE_MODULE_COUNT 5

extern const SooLevel CUBE_MODULES[CUBE_MODULE_COUNT];

/* Short name for module chips ("Invert", ...). / 모듈 버튼용 짧은 이름. */
const char *cube_module_name(int module);

/* A working solution (tests and screenshots). / 동작하는 정답 (테스트용). */
const char *cube_example_solution(int module);

#endif
