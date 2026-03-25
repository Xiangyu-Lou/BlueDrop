cmake_minimum_required(VERSION 3.20)
project(BlueDrop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt MOC/RCC/UIC automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# x64 only guard
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "BlueDrop only supports 64-bit (x64) builds.")
endif()

# Minimum Windows version: Win10 2004 (Build 19041)
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000007
)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

# Source subdirectory
add_subdirectory(src)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
