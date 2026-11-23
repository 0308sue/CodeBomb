# Code Bomb - Makefile for macOS, Linux and Windows (w64devkit / MSYS2 MinGW)
# Code Bomb - macOS, Linux, Windows(w64devkit / MSYS2 MinGW)용 Makefile
#
#   make            build the game / 게임 빌드         -> build/codebomb(.exe)
#   make run        build and launch / 빌드 후 실행
#   make test       headless tests, no raylib needed / 헤드리스 테스트 (raylib 불필요)
#   make test-asan  tests with sanitizers (macOS/Linux) / 새니타이저 테스트
#   make clean
#
# Sumin's array module is used in place from ../array_module_SOO (not copied),
# so her updates are picked up automatically.
# 수민의 배열 모듈은 ../array_module_SOO에서 그대로 사용한다 (복사하지 않음).
# 그래서 수민이 고친 내용이 자동으로 반영된다.
#
# macOS: uses pkg-config like Makefile_SOO (brew install raylib pkg-config),
#        falling back to Homebrew's folder. / Makefile_SOO처럼 pkg-config 사용.
# Windows: run from C:\raylib\w64devkit\w64devkit.exe, or double-click
#          run_windows.bat. / w64devkit 셸에서 실행하거나 run_windows.bat 실행.
#   Other raylib folder / raylib가 다른 위치에 있으면:
#     make RAYLIB_PATH=D:/libs/raylib/src

ifeq ($(origin CC),default)
  ifeq ($(OS),Windows_NT)
    CC := gcc
  endif
endif

CSTD  := -std=c11
WARN  := -Wall -Wextra -Wshadow
OPT   ?= -O2 -g

SOO_DIR := ../array_module_SOO
BUILD   := build
INCS    := -Isrc -I$(SOO_DIR)

# Core: no raylib, shared with the tests / 핵심: raylib 미사용, 테스트와 공유
CORE_SRC := $(addprefix src/, vm.c levels.c programs.c lexer.c parser.c \
            compiler.c editor.c array_bridge.c bomb_modules.c) \
            $(SOO_DIR)/array_core_SOO.c
# Game: raylib screens / 게임: raylib 화면
GAME_SRC := $(addprefix src/, main.c ui.c editor_ui.c scene_mailroom.c \
            screen_mailroom.c screen_arrays.c screen_pointers.c \
            bomb3d.c) \
            $(SOO_DIR)/pointer_SOO.c
TEST_SRC := $(addprefix tests/, test_main.c test_vm.c test_compiler.c \
            test_editor.c test_bridge.c)

# Objects go in build/; Sumin's files in build/soo/.
# 오브젝트는 build/에, 수민의 파일은 build/soo/에 만든다.
obj = $(patsubst src/%.c,$(BUILD)/%.o,$(patsubst $(SOO_DIR)/%.c,$(BUILD)/soo/%.o,$(1)))
CORE_OBJ := $(call obj,$(CORE_SRC))
GAME_OBJ := $(call obj,$(GAME_SRC))

# ---- platform and raylib / 플랫폼과 raylib ---------------------------------
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   ?= $(shell pkg-config --libs raylib 2>/dev/null)

ifeq ($(OS),Windows_NT)
  EXE := .exe
  RAYLIB_PATH ?= C:/raylib/raylib/src
  ifeq ($(strip $(RAYLIB_LIBS)),)
    RAYLIB_CFLAGS := -I$(RAYLIB_PATH)
    # Some installers put libraylib.a inside w64devkit instead.
    # 일부 설치 프로그램은 libraylib.a를 w64devkit 안에 둔다.
    RAYLIB_LIBS   := -L$(RAYLIB_PATH) \
                     -LC:/raylib/w64devkit/x86_64-w64-mingw32/lib -lraylib
  endif
  RAYLIB_LIBS += -lopengl32 -lgdi32 -lwinmm
else
  EXE :=
  UNAME_S := $(shell uname -s)
  ifeq ($(strip $(RAYLIB_LIBS)),)
    ifeq ($(UNAME_S),Darwin)
      BREW_PREFIX   := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
      RAYLIB_CFLAGS := -I$(BREW_PREFIX)/include
      RAYLIB_LIBS   := -L$(BREW_PREFIX)/lib -lraylib
    else
      RAYLIB_LIBS := -lraylib -lGL -lpthread -ldl -lrt -lX11
    endif
  endif
  ifeq ($(UNAME_S),Darwin)
    RAYLIB_LIBS += -framework OpenGL -framework Cocoa -framework IOKit \
                   -framework CoreVideo
  endif
endif

TARGET   := $(BUILD)/codebomb$(EXE)
TEST_BIN := $(BUILD)/tests$(EXE)

# ---- targets / 타깃 ----------------------------------------------------------
.PHONY: all run test test-asan clean

all: $(TARGET)

$(TARGET): $(CORE_OBJ) $(GAME_OBJ)
	$(CC) $^ -o $@ $(RAYLIB_LIBS) -lm

$(BUILD)/%.o: src/%.c | $(BUILD)/soo
	$(CC) $(CSTD) $(WARN) $(OPT) $(INCS) $(RAYLIB_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/soo/%.o: $(SOO_DIR)/%.c | $(BUILD)/soo
	$(CC) $(CSTD) $(WARN) $(OPT) $(INCS) -MMD -MP -c $< -o $@

$(BUILD)/soo:
	mkdir -p $(BUILD)/soo

run: $(TARGET)
	./$(TARGET)

$(TEST_BIN): $(TEST_SRC) $(CORE_SRC) $(wildcard src/*.h) $(wildcard tests/*.h) \
             $(wildcard $(SOO_DIR)/*.h) | $(BUILD)/soo
	$(CC) $(CSTD) $(WARN) $(OPT) $(INCS) $(TEST_SRC) $(CORE_SRC) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

test-asan: | $(BUILD)/soo
	$(CC) $(CSTD) $(WARN) -O1 -g -fsanitize=address,undefined \
	    -fno-omit-frame-pointer $(INCS) $(TEST_SRC) $(CORE_SRC) \
	    -o $(BUILD)/tests_asan
	./$(BUILD)/tests_asan

clean:
	rm -rf $(BUILD)

-include $(CORE_OBJ:.o=.d) $(GAME_OBJ:.o=.d)
