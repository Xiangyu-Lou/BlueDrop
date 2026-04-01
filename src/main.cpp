#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QFont>
#include <QIcon>
#include <QTimer>
#include <Windows.h>
#include <csignal>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

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

// Force-initialize the audio engine on a BT render endpoint.
// Windows lazily starts the endpoint's audio engine; without this, audio may
// be silent until the user manually switches the output device and back.
static void wakeUpAudioEndpoint(const QString& endpointId)
{
    if (endpointId.isEmpty()) return;

    using Microsoft::WRL::ComPtr;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("wakeUpAudioEndpoint: CoCreateInstance failed: 0x%08X", hr);
        return;
    }

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDevice(endpointId.toStdWString().c_str(), device.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERRORF("wakeUpAudioEndpoint: GetDevice failed: 0x%08X", hr);
        return;
    }

    ComPtr<IAudioClient> audioClient;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(audioClient.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("wakeUpAudioEndpoint: Activate IAudioClient failed: 0x%08X", hr);
        return;
    }

    // Use the endpoint's native mix format to guarantee Initialize succeeds
    WAVEFORMATEX* mixFmt = nullptr;
    hr = audioClient->GetMixFormat(&mixFmt);
    if (FAILED(hr) || !mixFmt) {
        LOG_ERRORF("wakeUpAudioEndpoint: GetMixFormat failed: 0x%08X", hr);
        return;
    }

    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                 1000000 /* 100ms */, 0, mixFmt, nullptr);
    CoTaskMemFree(mixFmt);
    if (FAILED(hr)) {
        LOG_ERRORF("wakeUpAudioEndpoint: Initialize failed: 0x%08X", hr);
        return;
    }

    audioClient->Start();
    audioClient->Stop();
    LOG_INFO("wakeUpAudioEndpoint: audio engine primed");
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
    app.setApplicationVersion("0.2.0");
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

    // Scan output devices to find the phone's named BT endpoint and the PC's
    // default output. Used both on initial connection and on lazy re-scan.
    // Returns the phone's named BT endpoint ID (empty if not found by name).
    auto scanBtAudioSession = [&]() -> QString {
        auto outputs = bluedrop.deviceEnumerator()->outputDevices();
        auto btName = bluedrop.bluetooth()->connectedDeviceName();

        // AudioPlaybackConnection creates a named render endpoint for the phone.
        // Scan that endpoint for sessions; fall back to the default endpoint.
        QString btEndpointId;
        QString defaultEndpointId;
        for (const auto& dev : outputs) {
            if (!btName.isEmpty() &&
                dev.displayName.contains(btName, Qt::CaseInsensitive)) {
                btEndpointId = dev.id;
                LOG_INFOF("BT named endpoint found: '%s'", qPrintable(dev.displayName));
            }
            if (dev.isDefault) {
                defaultEndpointId = dev.id;
            }
        }
        QString sessionEndpointId = btEndpointId.isEmpty() ? defaultEndpointId : btEndpointId;
        QString monitorEndpointId = defaultEndpointId;

        if (!sessionEndpointId.isEmpty()) {
            mixerVM.scanBtSession(sessionEndpointId, monitorEndpointId, btName);
        }
        return btEndpointId;
    };

    QObject::connect(bluedrop.bluetooth(), &BluetoothManager::stateChanged,
        &mixerVM, [&](BluetoothConnectionState state) {
            if (state == BluetoothConnectionState::Connecting) {
                endpointBeforeConnect = SessionVolumeController::getDefaultPlaybackEndpoint();
                LOG_INFOF("BT connecting — saved default endpoint: %s",
                          qPrintable(endpointBeforeConnect));
            }

            if (state == BluetoothConnectionState::Connected) {
                // Scan for BT audio session after a short delay to let Windows
                // register the audio endpoint.
                QTimer::singleShot(1500, &mixerVM, [&]() {
                    QString btEndpointId = scanBtAudioSession();
                    // Prime the BT endpoint's audio engine to prevent silent audio
                    // on first connection (Windows lazily initializes the render pipeline).
                    wakeUpAudioEndpoint(btEndpointId);

                    // Restore default endpoint if Windows changed it during BT connection
                    QString currentDefault = SessionVolumeController::getDefaultPlaybackEndpoint();
                    if (!endpointBeforeConnect.isEmpty() &&
                        !currentDefault.isEmpty() &&
                        currentDefault != endpointBeforeConnect) {
                        LOG_INFOF("Endpoint changed during BT connect (%s → %s), restoring",
                                  qPrintable(currentDefault), qPrintable(endpointBeforeConnect));
                        SessionVolumeController::setDefaultPlaybackEndpoint(endpointBeforeConnect);
                    }
                });
            }
        });

    // Re-scan for BT audio session whenever audio devices change.
    // This handles the case where the phone was silent at connection time
    // (no session existed yet) and starts playing audio later.
    QObject::connect(bluedrop.deviceEnumerator(), &DeviceEnumerator::devicesChanged,
        &mixerVM, [&]() {
            if (bluedrop.bluetooth()->state() == BluetoothConnectionState::Connected
                && !mixerVM.btSessionFound()) {
                LOG_DEBUG("devicesChanged: BT connected but no session — re-scanning");
                QString btEndpointId = scanBtAudioSession();
                wakeUpAudioEndpoint(btEndpointId);
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
