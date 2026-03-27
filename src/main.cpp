#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QFont>
#include <QIcon>
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
#include "audio/SessionVolumeController.h"
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
    app.setApplicationVersion("0.1.6");
    app.setOrganizationName("BlueDrop");
    app.setWindowIcon(QIcon(":/icons/icon.png"));

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

    // Track the default audio endpoint before BT connects, so we can detect
    // whether Windows silently changed it during the connection handshake.
    QString endpointBeforeConnect;

    // Snapshot default endpoint before BT connects
    QObject::connect(bluedrop.bluetooth(), &BluetoothManager::stateChanged,
        &mixerVM, [&](BluetoothConnectionState state) {
            if (state == BluetoothConnectionState::Connecting) {
                endpointBeforeConnect = SessionVolumeController::getDefaultPlaybackEndpoint();
                LOG_INFOF("BT connecting — saved default endpoint: %s",
                          qPrintable(endpointBeforeConnect));
            }

            // When BT reconnects (e.g. after auto-reconnect), reopen the audio
            // endpoint if audio routing was already active (Boost or Route B).
            if (state == BluetoothConnectionState::Connected) {
                if (mixerVM.boostEnabled() || mixerVM.engineRunning()) {
                    LOG_INFO("BT reconnected with active audio routing — reopening audio endpoint");
                    bluedrop.bluetooth()->openAudio();
                }
            }
        });

    // After OpenAsync succeeds, scan for the BT audio session and restore the
    // default endpoint if Windows changed it during connection.
    // This replaces the old 1500ms timer on Connected — we now wait for the
    // real audio endpoint to exist before scanning.
    QObject::connect(bluedrop.bluetooth(), &BluetoothManager::audioEndpointOpened,
        &mixerVM, [&]() {
            QTimer::singleShot(500, &mixerVM, [&]() {
                auto outputs = bluedrop.deviceEnumerator()->outputDevices();
                auto btName = bluedrop.bluetooth()->connectedDeviceName();
                for (const auto& dev : outputs) {
                    if (dev.isDefault) {
                        mixerVM.scanBtSession(dev.id, btName);
                        break;
                    }
                }

                // Only restore the default endpoint if Windows changed it
                // during BT connection. Unconditionally re-applying it causes
                // AirPods to re-negotiate their BT profile (A2DP→HFP), which
                // produces crackling/stuttering.
                QString currentDefault = SessionVolumeController::getDefaultPlaybackEndpoint();
                if (!endpointBeforeConnect.isEmpty() &&
                    !currentDefault.isEmpty() &&
                    currentDefault != endpointBeforeConnect) {
                    LOG_INFOF("Endpoint changed during BT connect (%s → %s), restoring",
                              qPrintable(currentDefault), qPrintable(endpointBeforeConnect));
                    SessionVolumeController::setDefaultPlaybackEndpoint(endpointBeforeConnect);
                }
            });
        });

    // Open the BT audio endpoint when audio routing becomes active.
    // This defers OpenAsync (which creates a competing BT A2DP stream) until
    // it is actually needed, preventing idle-time interference with AirPods.
    QObject::connect(&mixerVM, &MixerVM::boostEnabledChanged,
        &mixerVM, [&]() {
            if (mixerVM.boostEnabled()) {
                bluedrop.bluetooth()->openAudio();
            }
        });
    QObject::connect(&mixerVM, &MixerVM::engineRunningChanged,
        &mixerVM, [&]() {
            if (mixerVM.engineRunning()) {
                bluedrop.bluetooth()->openAudio();
            }
        });

    // Exit app when Updater is launched (update flow)
    QObject::connect(&settingsVM, &SettingsVM::requestAppExit,
                     &app, &QCoreApplication::quit);

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
