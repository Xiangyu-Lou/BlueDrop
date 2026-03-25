#include "DeviceVM.h"
#include "audio/DeviceEnumerator.h"
#include "audio/AudioEngine.h"

namespace BlueDrop {

DeviceVM::DeviceVM(DeviceEnumerator* enumerator, AudioEngine* engine, QObject* parent)
    : QObject(parent)
    , m_enumerator(enumerator)
    , m_engine(engine)
{
    connect(m_enumerator, &DeviceEnumerator::devicesChanged,
            this, &DeviceVM::onDevicesChanged);

    // Auto-select defaults
    onDevicesChanged();
}

QStringList DeviceVM::inputDeviceNames() const
{
    QStringList names;
    for (const auto& dev : m_enumerator->inputDevices()) {
        QString name = dev.displayName;
        if (dev.isVirtual) name += " [Virtual]";
        if (dev.isDefault) name += " *";
        names.append(name);
    }
    return names;
}

QStringList DeviceVM::outputDeviceNames() const
{
    QStringList names;
    names.append(QStringLiteral("跟随系统默认"));
    for (const auto& dev : m_enumerator->outputDevices()) {
        QString name = dev.displayName;
        if (dev.isVirtual) name += " [Virtual]";
        if (dev.isDefault) name += " *";
        names.append(name);
    }
    return names;
}

void DeviceVM::setSelectedMicIndex(int index)
{
    if (m_selectedMicIndex != index) {
        m_selectedMicIndex = index;
        emit selectedMicChanged();
        applyDeviceSelection();
    }
}

void DeviceVM::setSelectedMonitorIndex(int index)
{
    if (m_selectedMonitorIndex != index) {
        m_selectedMonitorIndex = index;
        emit selectedMonitorChanged();
        applyDeviceSelection();
    }
}

void DeviceVM::setSelectedVirtualIndex(int index)
{
    if (m_selectedVirtualIndex != index) {
        m_selectedVirtualIndex = index;
        emit selectedVirtualChanged();
        applyDeviceSelection();
    }
}

bool DeviceVM::vbCableInstalled() const
{
    return m_enumerator->isVBCableInstalled();
}

void DeviceVM::onDevicesChanged()
{
    auto inputs = m_enumerator->inputDevices();
    auto outputs = m_enumerator->outputDevices();

    // Auto-select default mic
    if (m_selectedMicIndex < 0 || m_selectedMicIndex >= inputs.size()) {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i].isDefault && !inputs[i].isVirtual) {
                m_selectedMicIndex = i;
                emit selectedMicChanged();
                break;
            }
        }
    }

    // Auto-select default monitor output: index 0 = "follow system"
    if (m_selectedMonitorIndex < 0) {
        m_selectedMonitorIndex = 0;
        emit selectedMonitorChanged();
    }

    // Auto-select VB-Cable for virtual output (offset +1 for "follow system" item)
    if (m_selectedVirtualIndex < 0 || m_selectedVirtualIndex > outputs.size()) {
        for (int i = 0; i < outputs.size(); i++) {
            if (outputs[i].isVirtual) {
                m_selectedVirtualIndex = i + 1;
                emit selectedVirtualChanged();
                break;
            }
        }
    }

    applyDeviceSelection();
    emit devicesChanged();
}

void DeviceVM::applyDeviceSelection()
{
    auto inputs = m_enumerator->inputDevices();
    auto outputs = m_enumerator->outputDevices();

    // Set mic device
    if (m_selectedMicIndex >= 0 && m_selectedMicIndex < inputs.size()) {
        m_engine->setMicrophoneDevice(inputs[m_selectedMicIndex].id);
    }

    // Set loopback device (monitor output where BT audio plays)
    // index 0 = "follow system default", index >= 1 = specific device (offset by 1)
    if (m_selectedMonitorIndex == 0) {
        // Find system default
        for (const auto& dev : outputs) {
            if (dev.isDefault) {
                m_engine->setLoopbackDevice(dev.id);
                break;
            }
        }
    } else {
        int devIdx = m_selectedMonitorIndex - 1;
        if (devIdx >= 0 && devIdx < outputs.size()) {
            m_engine->setLoopbackDevice(outputs[devIdx].id);
        }
    }

    // Set virtual cable output (also offset by 1 for "follow system" item)
    {
        int devIdx = m_selectedVirtualIndex - 1;
        if (devIdx >= 0 && devIdx < outputs.size()) {
            m_engine->setVirtualCableDevice(outputs[devIdx].id);
        }
    }
}

} // namespace BlueDrop
