#pragma once

#include <QObject>
#include <QStringList>

namespace BlueDrop {

class DeviceEnumerator;
class AudioEngine;

class DeviceVM : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList inputDeviceNames READ inputDeviceNames NOTIFY devicesChanged)
    Q_PROPERTY(QStringList outputDeviceNames READ outputDeviceNames NOTIFY devicesChanged)
    Q_PROPERTY(int selectedMicIndex READ selectedMicIndex WRITE setSelectedMicIndex NOTIFY selectedMicChanged)
    Q_PROPERTY(int selectedMonitorIndex READ selectedMonitorIndex WRITE setSelectedMonitorIndex NOTIFY selectedMonitorChanged)
    Q_PROPERTY(int selectedVirtualIndex READ selectedVirtualIndex WRITE setSelectedVirtualIndex NOTIFY selectedVirtualChanged)
    Q_PROPERTY(bool vbCableInstalled READ vbCableInstalled NOTIFY devicesChanged)
public:
    explicit DeviceVM(DeviceEnumerator* enumerator, AudioEngine* engine, QObject* parent = nullptr);

    QStringList inputDeviceNames() const;
    QStringList outputDeviceNames() const;

    int selectedMicIndex() const { return m_selectedMicIndex; }
    int selectedMonitorIndex() const { return m_selectedMonitorIndex; }
    int selectedVirtualIndex() const { return m_selectedVirtualIndex; }

    void setSelectedMicIndex(int index);
    void setSelectedMonitorIndex(int index);
    void setSelectedVirtualIndex(int index);

    bool vbCableInstalled() const;

signals:
    void devicesChanged();
    void selectedMicChanged();
    void selectedMonitorChanged();
    void selectedVirtualChanged();

private:
    void onDevicesChanged();
    void applyDeviceSelection();

    DeviceEnumerator* m_enumerator;
    AudioEngine* m_engine;
    int m_selectedMicIndex = -1;
    int m_selectedMonitorIndex = -1;
    int m_selectedVirtualIndex = -1;
};

} // namespace BlueDrop
