qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(BLUEDROP_SOURCES
    main.cpp
    system/SystemChecker.h
    system/SystemChecker.cpp
)

# Qt6 QML module
qt_add_executable(BlueDrop ${BLUEDROP_SOURCES})

# QML module with QML files
qt_add_qml_module(BlueDrop
    URI BlueDrop
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

target_include_directories(BlueDrop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(BlueDrop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    # Windows system libraries
    ole32
    oleaut32
    mmdevapi
    WindowsApp.lib
)

# MSVC specific options
if(MSVC)
    target_compile_options(BlueDrop PRIVATE
        /bigobj         # Required for C++/WinRT large headers
        /utf-8          # Source and execution charset
        /W4             # Warning level 4
        /wd4100         # Unreferenced formal parameter (common in WinRT callbacks)
    )
endif()

# Win32 application (no console window)
set_target_properties(BlueDrop PROPERTIES
    WIN32_EXECUTABLE TRUE
)
