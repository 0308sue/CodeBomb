#!/bin/sh
# 한글: Finder에서 더블 클릭하면 이 폴더에서 빌드하고 실행한다.
# English: Double-click in Finder to build and run from this directory.
cd "$(dirname "$0")" || exit 1
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists raylib; then
    echo "Raylib or pkg-config is missing. See README_SOO.md."
    read -r reply
    exit 1
fi
if ! make -f Makefile_SOO all; then
    echo "Build failed. See the error above."
    read -r reply
    exit 1
fi
exec ./array_circuit_SOO
