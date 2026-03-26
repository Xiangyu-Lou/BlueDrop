#pragma once

#include "BluetoothTypes.h"

#include <QObject>
#include <QString>
#include <QMap>

// Forward declare WinRT types to avoid header pollution
namespace winrt::Windows::Devices::Enumeration {
    struct DeviceWatcher;
    struct DeviceInformation;
    struct DeviceInformationUpdate;
}
namespace winrt::Windows::Media::Audio {
    struct AudioPlaybackConnection;
}

namespace BlueDrop {

struct BluetoothDevice {
    QString id;
    QString name;
    bool connected = false;
};

class BluetoothManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(BluetoothConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY deviceNameChanged)
public:
    explicit BluetoothManager(QObject* parent = nullptr);
    ~BluetoothManager() override;

    // Start advertising as A2DP Sink and watching for devices
    Q_INVOKABLE void startListening();

    // Stop watching and disconnect all
    Q_INVOKABLE void stopListening();

    // Disconnect current device
    Q_INVOKABLE void disconnect();

    BluetoothConnectionState state() const { return m_state; }
    QString connectedDeviceName() const { return m_connectedDeviceName; }

    // Get the device selector string for AudioPlaybackConnection
    static QString getDeviceSelector();

    // Open the BT audio endpoint (calls OpenAsync on the active connection).
    // Must be called before audio routing (Route B or Boost) begins.
    // Safe to call multiple times — no-op if already open.
    Q_INVOKABLE void openAudio();

signals:
    void stateChanged(BluetoothConnectionState newState);
    void deviceNameChanged(const QString& name);
    void deviceDiscovered(const QString& id, const QString& name);
    // Emitted after OpenAsync succeeds — safe to start audio routing / scan sessions
    void audioEndpointOpened();

private:
    void setState(BluetoothConnectionState state);
    void onDeviceAdded(const QString& deviceId, const QString& deviceName);
    void onDeviceRemoved(const QString& deviceId);
    void connectToDevice(const QString& deviceId, const QString& deviceName);

    BluetoothConnectionState m_state = BluetoothConnectionState::Disconnected;
    QString m_connectedDeviceName;
    QString m_connectedDeviceId;

    // Prevent premature destruction - use pointers to avoid WinRT headers in .h
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace BlueDrop
