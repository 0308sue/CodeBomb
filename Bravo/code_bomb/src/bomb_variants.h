/*
 * bomb_variants.h - random bombs that follow the same rule, and hints
 * bomb_variants.h - 같은 규칙을 따르는 무작위 폭탄과 힌트
 *
 * Every bomb (Sumin's six levels and the five cube modules) has a rule,
 * such as "every FAULT becomes SAFE" or "copy the left half onto the right
 * half". A variant is a new bomb with the same rule but different cards
 * and a different size (N, ROWS, COLS), so only code that follows the rule
 * defuses it; code that sets cards one by one for the bomb on screen does
 * not. Variants lean on edge cases: all FAULT, nothing to fix, odd sizes,
 * one-row grids.
 * 모든 폭탄(수민의 6개 레벨과 큐브 모듈 5개)에는 "모든 FAULT를 SAFE로"나
 * "왼쪽 절반을 오른쪽에 복사" 같은 규칙이 있다. 변형은 같은 규칙에 카드와
 * 크기(N, ROWS, COLS)만 다른 새 폭탄이므로, 규칙을 따르는 코드만 해체할 수
 * 있다. 화면의 폭탄에 맞춰 카드를 하나씩 바꾼 코드는 실패한다. 변형은 경계
 * 상황을 자주 만든다: 모두 FAULT, 고칠 것 없음, 홀수 크기.
 *
 * No raylib here: the tests use it too. / raylib 미사용: 테스트도 쓴다.
 */
#ifndef BOMB_VARIANTS_H
#define BOMB_VARIANTS_H

#include "array_core_SOO.h"

#include <stdbool.h>

typedef enum {
    BOMB_L1, BOMB_L2, BOMB_L3, BOMB_L4, BOMB_L5, BOMB_L6,   /* Sumin's levels */
    BOMB_INVERT, BOMB_EVENS, BOMB_MIRROR, BOMB_CHECKER, BOMB_BORDER,
    BOMB_KIND_COUNT
} BombKind;

#define BOMB_HINTS 3

typedef struct {
    SooLevel level;          /* a normal SooLevel for Sumin's engine */
    BombKind kind;
    unsigned seed;
} BombVariant;

BombKind bomb_kind_for_level(int level);     /* 0-5 */
BombKind bomb_kind_for_module(int module);   /* 0-4 */

/* Build random bomb number `seed` of this kind. Seeds 1, 2, 3, ... each
 * give a different bomb. / 이 종류의 무작위 폭탄 seed번을 만든다. */
void bomb_variant_make(BombVariant *v, BombKind kind, unsigned seed);

/* Hints from gentle to almost the answer. / 가벼운 힌트부터 거의 정답까지. */
const char *bomb_hint(BombKind kind, int n);

#endif
