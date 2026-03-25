qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# Core library (shared between main app and tests)
set(BLUEDROP_CORE_SOURCES
    system/SystemChecker.h
    system/SystemChecker.cpp
    audio/DeviceEnumerator.h
    audio/DeviceEnumerator.cpp
)

add_library(BlueDrop_core STATIC ${BLUEDROP_CORE_SOURCES})

target_include_directories(BlueDrop_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BlueDrop_core PUBLIC
    Qt6::Core
    ole32
    oleaut32
    mmdevapi
)

if(MSVC)
    target_compile_options(BlueDrop_core PRIVATE
        /bigobj
        /utf-8
        /W4
        /wd4100
    )
endif()

# Main executable
qt_add_executable(BlueDrop main.cpp)

qt_add_qml_module(BlueDrop
    URI BlueDrop
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

target_link_libraries(BlueDrop PRIVATE
    BlueDrop_core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    WindowsApp.lib
)

if(MSVC)
    target_compile_options(BlueDrop PRIVATE
        /bigobj
        /utf-8
        /W4
        /wd4100
    )
endif()

set_target_properties(BlueDrop PROPERTIES
    WIN32_EXECUTABLE TRUE
)
