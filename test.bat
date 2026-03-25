@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64

set CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set CTEST_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe

rem Add Qt bin to PATH for DLL loading
set PATH=A:\Qt\6.11.0\msvc2022_64\bin;%PATH%

cd /d F:\Project\BlueDrop\build

echo.
echo === Running Tests ===
"%CTEST_BIN%" --output-on-failure
