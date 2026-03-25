#pragma once

#include <QObject>
#include <memory>
#include "system/SystemChecker.h"

namespace BlueDrop {

class BluetoothManager;
class DeviceEnumerator;
class AudioEngine;

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    bool initialize();

    BluetoothManager* bluetooth() const { return m_bluetooth.get(); }
    DeviceEnumerator* deviceEnumerator() const { return m_deviceEnumerator.get(); }
    AudioEngine* audioEngine() const { return m_audioEngine.get(); }

    SystemCheckResult checkResult() const;

signals:
    void initializationFailed(const QString& message);

private:
    std::unique_ptr<BluetoothManager> m_bluetooth;
    std::unique_ptr<DeviceEnumerator> m_deviceEnumerator;
    std::unique_ptr<AudioEngine> m_audioEngine;
    SystemCheckResult m_checkResult;
};

} // namespace BlueDrop
