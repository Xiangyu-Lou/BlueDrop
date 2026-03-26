@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1

set CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
set QT_BIN=A:\Qt\6.11.0\msvc2022_64\bin
set RELEASE_DIR=F:\Project\BlueDrop\release\BlueDrop-v0.1.6
set BUILD_DIR=F:\Project\BlueDrop\build-release

set PATH=%QT_BIN%;%PATH%

cd /d F:\Project\BlueDrop

echo === Configure Release ===
"%CMAKE_BIN%" --preset release -DCMAKE_MAKE_PROGRAM="%NINJA_BIN%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo === Build Release ===
"%CMAKE_BIN%" --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo === Package ===
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

copy "%BUILD_DIR%\src\BlueDrop.exe" "%RELEASE_DIR%\"
copy "%BUILD_DIR%\src\Updater.exe" "%RELEASE_DIR%\"

echo === windeployqt ===
"%QT_BIN%\windeployqt.exe" --release --qmldir F:\Project\BlueDrop\src\qml --no-translations "%RELEASE_DIR%\BlueDrop.exe"
if errorlevel 1 exit /b 1

echo === Create ZIP ===
cd /d F:\Project\BlueDrop\release
powershell -Command "Compress-Archive -Path 'BlueDrop-v0.1.6' -DestinationPath 'BlueDrop-v0.1.6-win64.zip' -Force"

echo === DONE: release\BlueDrop-v0.1.6-win64.zip ===
