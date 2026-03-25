@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64

set PATH=A:\Qt\6.11.0\msvc2022_64\bin;%PATH%

cd /d F:\Project\BlueDrop

echo === Launching BlueDrop (with logging) ===
build\src\BlueDrop.exe --log 2>&1
echo.
echo === Exit code: %ERRORLEVEL% ===
echo === Log output: ===
type bluedrop_debug.log
