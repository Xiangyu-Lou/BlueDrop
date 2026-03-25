@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 (
    echo ERROR: vcvarsall.bat failed
    exit /b 1
)

set CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

cd /d F:\Project\BlueDrop

echo.
echo === CMake Configure ===
"%CMAKE_BIN%" --preset default -DCMAKE_MAKE_PROGRAM="%NINJA_BIN%"
if errorlevel 1 (
    echo ERROR: CMake configure failed
    exit /b 1
)

echo.
echo === CMake Build ===
"%CMAKE_BIN%" --build build
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo === SUCCESS ===
