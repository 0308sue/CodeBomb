#!/bin/sh
# 한글: Finder에서 더블 클릭하면 이 폴더에서 빌드하고 실행한다 (run_SOO.command와 같은 방식).
# English: Double-click in Finder to build and run from this folder (same as run_SOO.command).
cd "$(dirname "$0")" || exit 1
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists raylib; then
    if [ ! -f /opt/homebrew/include/raylib.h ] && [ ! -f /usr/local/include/raylib.h ]; then
        echo "Raylib is missing. Install it with: brew install raylib pkg-config"
        echo "Raylib가 없습니다. 설치: brew install raylib pkg-config"
        read -r reply
        exit 1
    fi
fi
if ! make; then
    echo "Build failed. See the error above. / 빌드 실패. 위의 오류를 확인하세요."
    read -r reply
    exit 1
fi
exec ./build/codebomb
