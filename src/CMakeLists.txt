qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# Core library (shared between main app and tests)
set(BLUEDROP_CORE_SOURCES
    system/SystemChecker.h
    system/SystemChecker.cpp
    audio/DeviceEnumerator.h
    audio/DeviceEnumerator.cpp
    audio/RingBuffer.h
    audio/Resampler.h
    audio/AudioCaptureStream.h
    audio/AudioCaptureStream.cpp
    audio/AudioRenderStream.h
    audio/AudioRenderStream.cpp
    audio/AudioEngine.h
    audio/AudioEngine.cpp
    audio/SessionVolumeController.h
    audio/SessionVolumeController.cpp
    bluetooth/BluetoothTypes.h
    bluetooth/BluetoothManager.h
    bluetooth/BluetoothManager.cpp
    app/Application.h
    app/Application.cpp
    app/TrayManager.h
    app/TrayManager.cpp
    viewmodel/BluetoothVM.h
    viewmodel/BluetoothVM.cpp
    viewmodel/DeviceVM.h
    viewmodel/DeviceVM.cpp
    viewmodel/MixerVM.h
    viewmodel/MixerVM.cpp
    viewmodel/SettingsVM.h
    viewmodel/SettingsVM.cpp
)

add_library(BlueDrop_core STATIC ${BLUEDROP_CORE_SOURCES})

target_include_directories(BlueDrop_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BlueDrop_core PUBLIC
    Qt6::Core
    Qt6::Widgets
    ole32
    oleaut32
    mmdevapi
    WindowsApp.lib
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
        qml/theme/Theme.qml
        qml/components/NavSidebar.qml
        qml/components/StatusCard.qml
        qml/components/DeviceCard.qml
        qml/components/MixerCard.qml
        qml/components/VolumeSlider.qml
        qml/components/StyledComboBox.qml
        qml/pages/HomePage.qml
        qml/pages/BroadcastPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/HelpPage.qml
    RESOURCES
        qml/theme/qmldir
)

# Embed font files as Qt resources
qt_add_resources(BlueDrop "fonts"
    PREFIX "/fonts"
    BASE "${CMAKE_SOURCE_DIR}/resources/fonts"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/fonts/NotoSansSC-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/resources/fonts/NotoSansSC-Medium.ttf"
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
