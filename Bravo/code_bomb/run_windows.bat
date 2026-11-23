@echo off
rem English: Double-click to build and run on Windows. Needs raylib from the
rem          official installer (C:\raylib) which includes w64devkit (gcc + make).
rem Korean comments are in README.md (this file stays ASCII for cmd.exe).
cd /d "%~dp0"
set "W64=C:\raylib\w64devkit\bin"
if exist "%W64%\gcc.exe" set "PATH=%W64%;%PATH%"
where make >nul 2>nul
if errorlevel 1 (
    echo make was not found. Install raylib from https://raysan5.itch.io/raylib
    echo or open C:\raylib\w64devkit\w64devkit.exe and run: make run
    pause
    exit /b 1
)
make
if errorlevel 1 (
    echo Build failed. See the error above.
    pause
    exit /b 1
)
start "" "build\codebomb.exe"
