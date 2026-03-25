#include "DeviceEnumerator.h"

#include <functiondiscoverykeys_devpkey.h>
#include <QMetaObject>

namespace BlueDrop {

DeviceEnumerator::DeviceEnumerator(QObject* parent)
    : QObject(parent)
{
}

DeviceEnumerator::~DeviceEnumerator()
{
    if (m_registered && m_enumerator) {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
    }
}

bool DeviceEnumerator::initialize()
{
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(m_enumerator.GetAddressOf()));

    if (FAILED(hr)) {
        return false;
    }

    // Register for device change notifications
    hr = m_enumerator->RegisterEndpointNotificationCallback(this);
    if (SUCCEEDED(hr)) {
        m_registered = true;
    }

    // Initial enumeration
    refresh();
    return true;
}

QList<AudioDevice> DeviceEnumerator::inputDevices() const
{
    return m_inputDevices;
}

QList<AudioDevice> DeviceEnumerator::outputDevices() const
{
    return m_outputDevices;
}

bool DeviceEnumerator::isVBCableInstalled() const
{
    for (const auto& dev : m_inputDevices) {
        if (dev.isVirtual) return true;
    }
    for (const auto& dev : m_outputDevices) {
        if (dev.isVirtual) return true;
    }
    return false;
}

QString DeviceEnumerator::findVBCableInputId() const
{
    // Look for VB-Cable in input (capture) devices - this is "CABLE Output"
    // which appears as a recording device that captures what's sent to CABLE Input
    for (const auto& dev : m_inputDevices) {
        if (dev.isVirtual) return dev.id;
    }
    return {};
}

void DeviceEnumerator::refresh()
{
    m_inputDevices = enumerateDevices(eCapture);
    m_outputDevices = enumerateDevices(eRender);
}

QList<AudioDevice> DeviceEnumerator::enumerateDevices(EDataFlow flow) const
{
    QList<AudioDevice> devices;

    if (!m_enumerator) return devices;

    // Get default device ID for this flow
    QString defaultId;
    {
        Microsoft::WRL::ComPtr<IMMDevice> defaultDevice;
        HRESULT hr = m_enumerator->GetDefaultAudioEndpoint(flow, eConsole, defaultDevice.GetAddressOf());
        if (SUCCEEDED(hr) && defaultDevice) {
            LPWSTR id = nullptr;
            if (SUCCEEDED(defaultDevice->GetId(&id)) && id) {
                defaultId = QString::fromWCharArray(id);
                CoTaskMemFree(id);
            }
        }
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = m_enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
    if (FAILED(hr)) return devices;

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        hr = collection->Item(i, device.GetAddressOf());
        if (FAILED(hr)) continue;

        AudioDevice audioDevice;
        audioDevice.type = (flow == eCapture) ? DeviceType::Input : DeviceType::Output;

        // Get device ID
        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id)) && id) {
            audioDevice.id = QString::fromWCharArray(id);
            CoTaskMemFree(id);
        }

        // Get friendly name
        Microsoft::WRL::ComPtr<IPropertyStore> props;
        hr = device->OpenPropertyStore(STGM_READ, props.GetAddressOf());
        if (SUCCEEDED(hr)) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
            if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
                audioDevice.displayName = QString::fromWCharArray(varName.pwszVal);
            }
            PropVariantClear(&varName);
        }

        audioDevice.isDefault = (audioDevice.id == defaultId);
        audioDevice.isVirtual = isVirtualDevice(audioDevice.displayName);

        devices.append(audioDevice);
    }

    return devices;
}

bool DeviceEnumerator::isVirtualDevice(const QString& name)
{
    return name.contains("CABLE", Qt::CaseInsensitive)
        || name.contains("VB-Audio", Qt::CaseInsensitive)
        || name.contains("Virtual Cable", Qt::CaseInsensitive);
}

// IMMNotificationClient - COM ref counting
ULONG DeviceEnumerator::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

ULONG DeviceEnumerator::Release()
{
    ULONG ref = InterlockedDecrement(&m_refCount);
    // Don't delete - Qt manages our lifetime
    return ref;
}

HRESULT DeviceEnumerator::QueryInterface(REFIID riid, void** ppvObject)
{
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
        *ppvObject = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

// Notification callbacks - forward to Qt main thread
HRESULT DeviceEnumerator::OnDeviceStateChanged(LPCWSTR, DWORD)
{
    QMetaObject::invokeMethod(this, [this]() {
        refresh();
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

HRESULT DeviceEnumerator::OnDeviceAdded(LPCWSTR)
{
    QMetaObject::invokeMethod(this, [this]() {
        refresh();
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

HRESULT DeviceEnumerator::OnDeviceRemoved(LPCWSTR)
{
    QMetaObject::invokeMethod(this, [this]() {
        refresh();
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

HRESULT DeviceEnumerator::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
    if (role != eConsole) return S_OK;

    QString deviceId = pwstrDefaultDeviceId ? QString::fromWCharArray(pwstrDefaultDeviceId) : QString();
    auto type = (flow == eCapture) ? DeviceType::Input : DeviceType::Output;

    QMetaObject::invokeMethod(this, [this, type, deviceId]() {
        refresh();
        emit defaultDeviceChanged(type, deviceId);
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

HRESULT DeviceEnumerator::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY)
{
    // Property changes can include name changes; refresh
    QMetaObject::invokeMethod(this, [this]() {
        refresh();
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

} // namespace BlueDrop
