#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <Windows.h>

#include "app/Application.h"
#include "viewmodel/BluetoothVM.h"
#include "viewmodel/DeviceVM.h"
#include "viewmodel/MixerVM.h"
#include "viewmodel/SettingsVM.h"

using namespace Qt::StringLiterals;
using namespace BlueDrop;

int main(int argc, char* argv[])
{
    // Initialize COM for WASAPI and other COM-based APIs
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    QGuiApplication app(argc, argv);
    app.setApplicationName("BlueDrop");
    app.setApplicationDisplayName(u"聚音 BlueDrop"_s);
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("BlueDrop");

    // Initialize application core
    Application bluedrop;
    if (!bluedrop.initialize()) {
        // Show error and exit - for now just log
        qCritical("Initialization failed: %s",
                   qPrintable(bluedrop.checkResult().failureMessage));
        CoUninitialize();
        return 1;
    }

    // Create ViewModels
    BluetoothVM bluetoothVM(bluedrop.bluetooth());
    DeviceVM deviceVM(bluedrop.deviceEnumerator(), bluedrop.audioEngine());
    MixerVM mixerVM(bluedrop.audioEngine());
    SettingsVM settingsVM(bluedrop.checkResult());

    // Set up QML engine
    QQmlApplicationEngine engine;

    // Register ViewModels as context properties
    engine.rootContext()->setContextProperty("bluetoothVM", &bluetoothVM);
    engine.rootContext()->setContextProperty("deviceVM", &deviceVM);
    engine.rootContext()->setContextProperty("mixerVM", &mixerVM);
    engine.rootContext()->setContextProperty("settingsVM", &settingsVM);

    // Load main QML
    const QUrl url(u"qrc:/BlueDrop/qml/main.qml"_s);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );
    engine.load(url);

    int result = app.exec();

    CoUninitialize();
    return result;
}
