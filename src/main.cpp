#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <Windows.h>

using namespace Qt::StringLiterals;

int main(int argc, char* argv[])
{
    // Initialize COM for WASAPI and other COM-based APIs
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    QGuiApplication app(argc, argv);
    app.setApplicationName("BlueDrop");
    app.setApplicationDisplayName(u"聚音 BlueDrop"_s);
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("BlueDrop");

    QQmlApplicationEngine engine;

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
