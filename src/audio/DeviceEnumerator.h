#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace BlueDrop {

enum class DeviceType {
    Input,  // Capture / Recording
    Output  // Render / Playback
};

struct AudioDevice {
    QString id;
    QString displayName;
    bool isDefault = false;
    bool isVirtual = false;
    DeviceType type = DeviceType::Input;

    bool operator==(const AudioDevice& other) const {
        return id == other.id;
    }
};

class DeviceEnumerator : public QObject, public IMMNotificationClient {
    Q_OBJECT
public:
    explicit DeviceEnumerator(QObject* parent = nullptr);
    ~DeviceEnumerator() override;

    bool initialize();

    QList<AudioDevice> inputDevices() const;
    QList<AudioDevice> outputDevices() const;

    bool isVBCableInstalled() const;
    QString findVBCableInputId() const;

    // Force refresh device lists
    void refresh();

    // IMMNotificationClient (COM) - ref counting
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    // IMMNotificationClient callbacks
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

signals:
    void devicesChanged();
    void defaultDeviceChanged(DeviceType type, const QString& deviceId);

private:
    QList<AudioDevice> enumerateDevices(EDataFlow flow) const;
    static bool isVirtualDevice(const QString& name);

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;
    QList<AudioDevice> m_inputDevices;
    QList<AudioDevice> m_outputDevices;
    LONG m_refCount = 1;
    bool m_registered = false;
};

} // namespace BlueDrop
