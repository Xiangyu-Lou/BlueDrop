#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QFont>
#include <QTimer>
#include <Windows.h>
#include <csignal>

#include "app/Application.h"
#include "system/Logger.h"
#include "bluetooth/BluetoothManager.h"
#include "audio/DeviceEnumerator.h"
#include "viewmodel/BluetoothVM.h"
#include "viewmodel/DeviceVM.h"
#include "viewmodel/MixerVM.h"
#include "viewmodel/SettingsVM.h"
#include "app/TrayManager.h"

using namespace Qt::StringLiterals;
using namespace BlueDrop;

// Global crash handlers to capture the cause before process dies
static void terminateHandler() {
    LOG_ERROR("std::terminate() called!");
    Logger::instance().disable(); // flush
    std::abort();
}

static void signalHandler(int sig) {
    LOG_ERRORF("Signal caught: %d", sig);
    Logger::instance().disable();
    std::abort();
}

static LONG WINAPI sehHandler(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        LOG_ERRORF("SEH exception: 0x%08X at addr 0x%p",
                   ep->ExceptionRecord->ExceptionCode,
                   ep->ExceptionRecord->ExceptionAddress);
    } else {
        LOG_ERROR("SEH exception (no details)");
    }
    Logger::instance().disable();
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char* argv[])
{
    // Enable logging if env BLUEDROP_LOG=1 or --log argument present
    bool enableLog = qEnvironmentVariableIntValue("BLUEDROP_LOG") == 1;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == "--log") {
            enableLog = true;
            break;
        }
    }
    if (enableLog) {
        Logger::instance().enable("bluedrop_debug.log");
        Logger::installQtHandler();
        LOG_INFO("BlueDrop starting (log enabled)");

        // Install crash handlers
        std::set_terminate(terminateHandler);
        std::signal(SIGSEGV, signalHandler);
        std::signal(SIGABRT, signalHandler);
        SetUnhandledExceptionFilter(sehHandler);
    }

    // Use Basic style for full control customization support
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QApplication app(argc, argv);
    app.setApplicationName("BlueDrop");
    app.setApplicationDisplayName(u"聚音 BlueDrop"_s);
    app.setApplicationVersion("0.1.2");
    app.setOrganizationName("BlueDrop");

    // Load embedded Noto Sans SC font
    int regularId = QFontDatabase::addApplicationFont(u":/fonts/NotoSansSC-Regular.ttf"_s);
    int mediumId = QFontDatabase::addApplicationFont(u":/fonts/NotoSansSC-Medium.ttf"_s);
    QString fontFamily = "Noto Sans CJK SC";
    if (regularId >= 0) {
        auto families = QFontDatabase::applicationFontFamilies(regularId);
        if (!families.isEmpty()) fontFamily = families.first();
    }
    QFont defaultFont(fontFamily, 10);
    defaultFont.setHintingPreference(QFont::PreferNoHinting);
    app.setFont(defaultFont);
    LOG_INFOF("Font loaded: %s (regular=%d, medium=%d)",
              qPrintable(fontFamily), regularId, mediumId);

    // Initialize application core
    Application bluedrop;
    if (!bluedrop.initialize()) {
        LOG_ERRORF("Initialization failed: %s",
                   qPrintable(bluedrop.checkResult().failureMessage));
        return 1;
    }
    LOG_INFO("Application initialized");

    // Create ViewModels
    BluetoothVM bluetoothVM(bluedrop.bluetooth());
    DeviceVM deviceVM(bluedrop.deviceEnumerator(), bluedrop.audioEngine());
    MixerVM mixerVM(bluedrop.audioEngine(), bluedrop.sessionVolume(),
                    bluedrop.deviceEnumerator());
    SettingsVM settingsVM(bluedrop.checkResult());

    // When BT connects, auto-scan for BT audio session on monitor endpoint
    QObject::connect(bluedrop.bluetooth(), &BluetoothManager::stateChanged,
        &mixerVM, [&](BluetoothConnectionState state) {
            if (state == BluetoothConnectionState::Connected) {
                // Delay scan slightly to let Windows create the audio session
                QTimer::singleShot(1500, &mixerVM, [&]() {
                    auto outputs = bluedrop.deviceEnumerator()->outputDevices();
                    auto btName = bluedrop.bluetooth()->connectedDeviceName();
                    // Scan on the default output endpoint (where BT audio plays)
                    for (const auto& dev : outputs) {
                        if (dev.isDefault) {
                            mixerVM.scanBtSession(dev.id, btName);
                            break;
                        }
                    }
                });
            }
        });

    // Create system tray manager
    TrayManager trayManager;
    trayManager.show();

    // Set up QML engine
    QQmlApplicationEngine engine;

    // Register ViewModels as context properties
    engine.rootContext()->setContextProperty("bluetoothVM", &bluetoothVM);
    engine.rootContext()->setContextProperty("deviceVM", &deviceVM);
    engine.rootContext()->setContextProperty("mixerVM", &mixerVM);
    engine.rootContext()->setContextProperty("settingsVM", &settingsVM);
    engine.rootContext()->setContextProperty("trayManager", &trayManager);

    // Load main QML
    const QUrl url(u"qrc:/qt/qml/BlueDrop/qml/main.qml"_s);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR("Failed to load QML");
        return -1;
    }
    LOG_INFO("QML loaded, entering event loop");

    return app.exec();
}
