# Code Bomb - CMake build (Visual Studio, Xcode, CLion, VS Code, or command line)
# Code Bomb - CMake 빌드 (Visual Studio, Xcode, CLion, VS Code, 명령줄)
#
#   cmake -B build-cmake
#   cmake --build build-cmake --config Release
#   ctest --test-dir build-cmake -C Release        # headless tests / 헤드리스 테스트
#
# If raylib is not installed, CMake downloads and builds raylib 5.5 (needs
# internet the first time). raylib 5.5 and 6.0 both work.
# raylib가 설치되어 있지 않으면 CMake가 raylib 5.5를 내려받아 빌드한다
# (처음 한 번은 인터넷 필요). raylib 5.5와 6.0 모두 동작한다.

cmake_minimum_required(VERSION 3.16)
project(CodeBomb C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sumin's module, used in place (not copied) / 수민의 모듈 (복사하지 않고 사용)
set(SOO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../array_module_SOO)

# ---- raylib ---------------------------------------------------------------
find_package(raylib 5.0 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

# ---- compiler options / 컴파일러 옵션 ---------------------------------------
if (MSVC)
    # /utf-8: source files contain Korean comments in UTF-8.
    # /utf-8: 소스 파일에 UTF-8 한국어 주석이 있다.
    add_compile_options(/utf-8 /W3)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wshadow)
endif()

# ---- core library (no raylib) / 핵심 라이브러리 (raylib 미사용) -------------
add_library(cb_core STATIC
    src/vm.c src/levels.c src/programs.c src/lexer.c src/parser.c
    src/compiler.c src/editor.c src/array_bridge.c src/bomb_modules.c
    ${SOO_DIR}/array_core_SOO.c)
target_include_directories(cb_core PUBLIC src ${SOO_DIR})
if (NOT MSVC)
    target_link_libraries(cb_core PUBLIC m)
endif()

# ---- game / 게임 ------------------------------------------------------------
add_executable(codebomb
    src/main.c src/ui.c src/editor_ui.c src/scene_mailroom.c
    src/screen_mailroom.c src/screen_arrays.c src/screen_pointers.c src/bomb3d.c
    ${SOO_DIR}/pointer_SOO.c)
target_link_libraries(codebomb PRIVATE cb_core raylib)
if (APPLE)
    target_link_libraries(codebomb PRIVATE
        "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Copy assets (optional mono.ttf) next to the executable.
# 에셋(선택 사항인 mono.ttf)을 실행 파일 옆으로 복사한다.
add_custom_command(TARGET codebomb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:codebomb>/assets)

# ---- tests / 테스트 ---------------------------------------------------------
enable_testing()
add_executable(cb_tests
    tests/test_main.c tests/test_vm.c tests/test_compiler.c
    tests/test_editor.c tests/test_bridge.c)
target_link_libraries(cb_tests PRIVATE cb_core)
add_test(NAME cb_tests COMMAND cb_tests)
