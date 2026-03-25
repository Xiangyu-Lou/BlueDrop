#include "BluetoothVM.h"
#include "bluetooth/BluetoothManager.h"

namespace BlueDrop {

BluetoothVM::BluetoothVM(BluetoothManager* manager, QObject* parent)
    : QObject(parent)
    , m_manager(manager)
{
    connect(m_manager, &BluetoothManager::stateChanged,
            this, &BluetoothVM::connectionStateChanged);
    connect(m_manager, &BluetoothManager::deviceNameChanged,
            this, &BluetoothVM::deviceNameChanged);
}

int BluetoothVM::connectionState() const
{
    return static_cast<int>(m_manager->state());
}

QString BluetoothVM::deviceName() const
{
    return m_manager->connectedDeviceName();
}

QString BluetoothVM::statusText() const
{
    switch (m_manager->state()) {
    case BluetoothConnectionState::Disconnected:
        return QString::fromUtf8(u8"未连接蓝牙设备");
    case BluetoothConnectionState::WaitingPair:
        return QString::fromUtf8(u8"等待 iPhone 连接...");
    case BluetoothConnectionState::Connecting:
        return QString::fromUtf8(u8"正在连接...");
    case BluetoothConnectionState::Connected:
        return QString::fromUtf8(u8"已连接：%1").arg(m_manager->connectedDeviceName());
    case BluetoothConnectionState::Reconnecting:
        return QString::fromUtf8(u8"连接已断开，等待重新连接...");
    }
    return {};
}

QString BluetoothVM::statusColor() const
{
    switch (m_manager->state()) {
    case BluetoothConnectionState::Disconnected:
        return "#86868B"; // gray
    case BluetoothConnectionState::WaitingPair:
    case BluetoothConnectionState::Connecting:
        return "#4A90D9"; // blue
    case BluetoothConnectionState::Connected:
        return "#34C759"; // green
    case BluetoothConnectionState::Reconnecting:
        return "#FF9500"; // orange
    }
    return "#86868B";
}

void BluetoothVM::startListening()
{
    m_manager->startListening();
}

void BluetoothVM::stopListening()
{
    m_manager->stopListening();
}

void BluetoothVM::disconnect()
{
    m_manager->disconnect();
}

} // namespace BlueDrop
