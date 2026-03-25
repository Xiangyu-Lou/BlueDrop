#include "BluetoothManager.h"

#include <QMetaObject>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Media.Audio.h>

namespace winrt_enum = winrt::Windows::Devices::Enumeration;
namespace winrt_audio = winrt::Windows::Media::Audio;

namespace BlueDrop {

struct BluetoothManager::Impl {
    winrt_enum::DeviceWatcher watcher{ nullptr };
    winrt_audio::AudioPlaybackConnection connection{ nullptr };

    winrt::event_token addedToken;
    winrt::event_token removedToken;
    winrt::event_token updatedToken;
    winrt::event_token completedToken;
    winrt::event_token stateChangedToken;

    bool watcherRunning = false;
};

BluetoothManager::BluetoothManager(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

BluetoothManager::~BluetoothManager()
{
    stopListening();
}

QString BluetoothManager::getDeviceSelector()
{
    try {
        auto selector = winrt_audio::AudioPlaybackConnection::GetDeviceSelector();
        return QString::fromStdString(winrt::to_string(selector));
    } catch (const winrt::hresult_error&) {
        return {};
    }
}

void BluetoothManager::startListening()
{
    if (m_impl->watcherRunning) return;

    try {
        auto selector = winrt_audio::AudioPlaybackConnection::GetDeviceSelector();
        m_impl->watcher = winrt_enum::DeviceInformation::CreateWatcher(selector);

        // Device discovered
        m_impl->addedToken = m_impl->watcher.Added(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformation& info) {
                QString id = QString::fromStdString(winrt::to_string(info.Id()));
                QString name = QString::fromStdString(winrt::to_string(info.Name()));
                QMetaObject::invokeMethod(this, [this, id, name]() {
                    onDeviceAdded(id, name);
                }, Qt::QueuedConnection);
            });

        // Device removed
        m_impl->removedToken = m_impl->watcher.Removed(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformationUpdate& update) {
                QString id = QString::fromStdString(winrt::to_string(update.Id()));
                QMetaObject::invokeMethod(this, [this, id]() {
                    onDeviceRemoved(id);
                }, Qt::QueuedConnection);
            });

        // Updated - handle reconnection
        m_impl->updatedToken = m_impl->watcher.Updated(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformationUpdate& update) {
                // A device update while in Reconnecting state may indicate reconnection
                QString id = QString::fromStdString(winrt::to_string(update.Id()));
                QMetaObject::invokeMethod(this, [this, id]() {
                    if (m_state == BluetoothConnectionState::Reconnecting && id == m_connectedDeviceId) {
                        connectToDevice(m_connectedDeviceId, m_connectedDeviceName);
                    }
                }, Qt::QueuedConnection);
            });

        // Enumeration complete
        m_impl->completedToken = m_impl->watcher.EnumerationCompleted(
            [](const winrt_enum::DeviceWatcher& sender, const winrt::Windows::Foundation::IInspectable&) {
                // Continue watching for new devices after initial enumeration
                // DeviceWatcher remains active
            });

        m_impl->watcher.Start();
        m_impl->watcherRunning = true;
        setState(BluetoothConnectionState::WaitingPair);

    } catch (const winrt::hresult_error& e) {
        qWarning("BluetoothManager: Failed to start watcher: %ls (0x%08X)",
                 e.message().c_str(), static_cast<uint32_t>(e.code()));
        setState(BluetoothConnectionState::Disconnected);
    }
}

void BluetoothManager::stopListening()
{
    // Close active connection
    disconnect();

    // Stop watcher
    if (m_impl->watcherRunning && m_impl->watcher) {
        try {
            m_impl->watcher.Stop();
        } catch (...) {}
        m_impl->watcherRunning = false;
    }
    m_impl->watcher = nullptr;

    setState(BluetoothConnectionState::Disconnected);
}

void BluetoothManager::disconnect()
{
    if (m_impl->connection) {
        try {
            m_impl->connection.Close();
        } catch (...) {}
        m_impl->connection = nullptr;
    }

    m_connectedDeviceId.clear();
    m_connectedDeviceName.clear();
    emit deviceNameChanged({});
}

void BluetoothManager::setState(BluetoothConnectionState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void BluetoothManager::onDeviceAdded(const QString& deviceId, const QString& deviceName)
{
    emit deviceDiscovered(deviceId, deviceName);

    // Auto-connect to the first discovered device
    if (m_state == BluetoothConnectionState::WaitingPair ||
        m_state == BluetoothConnectionState::Reconnecting) {
        connectToDevice(deviceId, deviceName);
    }
}

void BluetoothManager::onDeviceRemoved(const QString& deviceId)
{
    if (deviceId == m_connectedDeviceId) {
        // Device was disconnected - enter reconnecting state
        if (m_impl->connection) {
            try {
                m_impl->connection.Close();
            } catch (...) {}
            m_impl->connection = nullptr;
        }

        setState(BluetoothConnectionState::Reconnecting);
    }
}

void BluetoothManager::connectToDevice(const QString& deviceId, const QString& deviceName)
{
    setState(BluetoothConnectionState::Connecting);

    // Run async connection in a separate thread
    auto id = deviceId;
    auto name = deviceName;

    // Use fire_and_forget pattern for async WinRT calls
    [this, id, name]() -> winrt::fire_and_forget {
        try {
            auto winrtId = winrt::to_hstring(id.toStdString());

            auto connection = winrt_audio::AudioPlaybackConnection::TryCreateFromId(winrtId);
            if (!connection) {
                QMetaObject::invokeMethod(this, [this]() {
                    setState(BluetoothConnectionState::WaitingPair);
                }, Qt::QueuedConnection);
                co_return;
            }

            // Monitor state changes
            connection.StateChanged([this, id, name](
                const winrt_audio::AudioPlaybackConnection& sender,
                const winrt::Windows::Foundation::IInspectable&) {
                if (sender.State() == winrt_audio::AudioPlaybackConnectionState::Closed) {
                    QMetaObject::invokeMethod(this, [this]() {
                        if (m_impl->connection) {
                            try { m_impl->connection.Close(); } catch (...) {}
                            m_impl->connection = nullptr;
                        }
                        // Enter reconnecting state - keep name for display
                        setState(BluetoothConnectionState::Reconnecting);
                    }, Qt::QueuedConnection);
                }
            });

            co_await connection.StartAsync();
            auto result = co_await connection.OpenAsync();

            auto status = result.Status();
            if (status == winrt_audio::AudioPlaybackConnectionOpenResultStatus::Success) {
                QMetaObject::invokeMethod(this, [this, id, name, conn = std::move(connection)]() mutable {
                    m_impl->connection = std::move(conn);
                    m_connectedDeviceId = id;
                    m_connectedDeviceName = name;
                    emit deviceNameChanged(name);
                    setState(BluetoothConnectionState::Connected);
                }, Qt::QueuedConnection);
            } else {
                connection.Close();
                QMetaObject::invokeMethod(this, [this]() {
                    setState(BluetoothConnectionState::WaitingPair);
                }, Qt::QueuedConnection);
            }

        } catch (const winrt::hresult_error& e) {
            qWarning("BluetoothManager: Connection failed: %ls (0x%08X)",
                     e.message().c_str(), static_cast<uint32_t>(e.code()));
            QMetaObject::invokeMethod(this, [this]() {
                setState(BluetoothConnectionState::WaitingPair);
            }, Qt::QueuedConnection);
        }
    }();
}

} // namespace BlueDrop
