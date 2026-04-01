#include "BluetoothManager.h"
#include "system/Logger.h"

#include <QMetaObject>
#include <thread>
#include <mutex>
#include <condition_variable>

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

    // Keep connection thread alive to maintain MTA apartment
    std::thread connectionThread;
    std::atomic<bool> disconnectRequested{false};
    std::mutex connectionMutex;
    std::condition_variable connectionCv;

    winrt::event_token addedToken;
    winrt::event_token removedToken;
    winrt::event_token updatedToken;
    winrt::event_token completedToken;

    bool watcherRunning = false;
};

BluetoothManager::BluetoothManager(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    LOG_INFO("BluetoothManager created");
}

BluetoothManager::~BluetoothManager()
{
    LOG_INFO("BluetoothManager destroying");
    stopListening();
}

QString BluetoothManager::getDeviceSelector()
{
    try {
        auto selector = winrt_audio::AudioPlaybackConnection::GetDeviceSelector();
        return QString::fromStdString(winrt::to_string(selector));
    } catch (const winrt::hresult_error& e) {
        LOG_ERRORF("GetDeviceSelector failed: 0x%08X", static_cast<uint32_t>(e.code()));
        return {};
    }
}

void BluetoothManager::startListening()
{
    LOG_INFO("startListening() called");
    if (m_impl->watcherRunning) {
        LOG_WARN("Watcher already running, ignoring");
        return;
    }

    try {
        LOG_DEBUG("Getting device selector for watcher");
        auto selector = winrt_audio::AudioPlaybackConnection::GetDeviceSelector();
        LOG_DEBUG("Creating DeviceWatcher");
        m_impl->watcher = winrt_enum::DeviceInformation::CreateWatcher(selector);

        // Device discovered
        m_impl->addedToken = m_impl->watcher.Added(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformation& info) {
                try {
                    QString id = QString::fromStdString(winrt::to_string(info.Id()));
                    QString name = QString::fromStdString(winrt::to_string(info.Name()));
                    LOG_INFOF("Device discovered: %s (%s)", qPrintable(name), qPrintable(id));
                    QMetaObject::invokeMethod(this, [this, id, name]() {
                        onDeviceAdded(id, name);
                    }, Qt::QueuedConnection);
                } catch (...) { LOG_ERROR("Exception in Added callback"); }
            });

        // Device removed
        m_impl->removedToken = m_impl->watcher.Removed(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformationUpdate& update) {
                try {
                    QString id = QString::fromStdString(winrt::to_string(update.Id()));
                    LOG_INFOF("Device removed: %s", qPrintable(id));
                    QMetaObject::invokeMethod(this, [this, id]() {
                        onDeviceRemoved(id);
                    }, Qt::QueuedConnection);
                } catch (...) { LOG_ERROR("Exception in Removed callback"); }
            });

        // Updated
        m_impl->updatedToken = m_impl->watcher.Updated(
            [this](const winrt_enum::DeviceWatcher&, const winrt_enum::DeviceInformationUpdate& update) {
                try {
                    QString id = QString::fromStdString(winrt::to_string(update.Id()));
                    LOG_DEBUGF("Device updated: %s", qPrintable(id));
                    QMetaObject::invokeMethod(this, [this, id]() {
                        if (m_state == BluetoothConnectionState::Reconnecting && id == m_connectedDeviceId) {
                            connectToDevice(m_connectedDeviceId, m_connectedDeviceName);
                        }
                    }, Qt::QueuedConnection);
                } catch (...) { LOG_ERROR("Exception in Updated callback"); }
            });

        // Enumeration complete
        m_impl->completedToken = m_impl->watcher.EnumerationCompleted(
            [](const winrt_enum::DeviceWatcher&, const winrt::Windows::Foundation::IInspectable&) {
                try { LOG_INFO("Device enumeration completed"); } catch (...) {}
            });

        LOG_DEBUG("Starting DeviceWatcher");
        m_impl->watcher.Start();
        m_impl->watcherRunning = true;
        LOG_INFO("DeviceWatcher started");
        setState(BluetoothConnectionState::WaitingPair);

    } catch (const winrt::hresult_error& e) {
        LOG_ERRORF("startListening failed: 0x%08X", static_cast<uint32_t>(e.code()));
        setState(BluetoothConnectionState::Disconnected);
    } catch (...) {
        LOG_ERROR("startListening unknown exception");
        setState(BluetoothConnectionState::Disconnected);
    }
}

void BluetoothManager::stopListening()
{
    LOG_INFO("stopListening() called");
    disconnect();

    if (m_impl->watcherRunning && m_impl->watcher) {
        try {
            m_impl->watcher.Stop();
            LOG_INFO("DeviceWatcher stopped");
        } catch (...) {
            LOG_WARN("Exception stopping watcher (ignored)");
        }
        m_impl->watcherRunning = false;
    }
    m_impl->watcher = nullptr;
    setState(BluetoothConnectionState::Disconnected);
}

void BluetoothManager::disconnect()
{
    LOG_DEBUG("disconnect() called");

    // Signal the connection thread to stop
    {
        std::lock_guard<std::mutex> lock(m_impl->connectionMutex);
        m_impl->disconnectRequested.store(true);
    }
    m_impl->connectionCv.notify_all();

    // Wait for connection thread to finish
    if (m_impl->connectionThread.joinable()) {
        m_impl->connectionThread.join();
        LOG_DEBUG("Connection thread joined");
    }

    // Connection is closed by the thread, but ensure cleanup
    if (m_impl->connection) {
        try {
            m_impl->connection.Close();
            LOG_INFO("Connection closed");
        } catch (...) {
            LOG_WARN("Exception closing connection (ignored)");
        }
        m_impl->connection = nullptr;
    }
    m_connectedDeviceId.clear();
    m_connectedDeviceName.clear();
    emit deviceNameChanged({});
}


void BluetoothManager::setState(BluetoothConnectionState state)
{
    if (m_state != state) {
        static const char* names[] = {
            "Disconnected", "WaitingPair", "Connecting", "Connected", "Reconnecting"
        };
        LOG_INFOF("State: %s -> %s",
                  names[static_cast<int>(m_state)], names[static_cast<int>(state)]);
        m_state = state;
        emit stateChanged(state);
    }
}

void BluetoothManager::onDeviceAdded(const QString& deviceId, const QString& deviceName)
{
    LOG_INFOF("onDeviceAdded: %s", qPrintable(deviceName));
    emit deviceDiscovered(deviceId, deviceName);

    if (m_state == BluetoothConnectionState::WaitingPair ||
        m_state == BluetoothConnectionState::Reconnecting) {
        connectToDevice(deviceId, deviceName);
    }
}

void BluetoothManager::onDeviceRemoved(const QString& deviceId)
{
    if (deviceId == m_connectedDeviceId) {
        LOG_WARNF("Connected device removed: %s (%s) — will reconnect",
                  qPrintable(m_connectedDeviceName), qPrintable(deviceId));
        if (m_impl->connection) {
            try { m_impl->connection.Close(); } catch (...) {}
            m_impl->connection = nullptr;
        }
        setState(BluetoothConnectionState::Reconnecting);
    } else {
        LOG_INFOF("onDeviceRemoved (non-connected): %s", qPrintable(deviceId));
    }
}

void BluetoothManager::connectToDevice(const QString& deviceId, const QString& deviceName)
{
    LOG_INFOF("connectToDevice: %s (%s)", qPrintable(deviceName), qPrintable(deviceId));
    setState(BluetoothConnectionState::Connecting);

    // Clean up previous connection thread if any
    {
        std::lock_guard<std::mutex> lock(m_impl->connectionMutex);
        m_impl->disconnectRequested.store(true);
    }
    m_impl->connectionCv.notify_all();
    if (m_impl->connectionThread.joinable()) {
        m_impl->connectionThread.join();
    }
    m_impl->disconnectRequested.store(false);

    // Run connection on a dedicated MTA thread.
    // Thread stays alive to keep the MTA apartment active for the connection.
    m_impl->connectionThread = std::thread([this, id = deviceId, name = deviceName]() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            LOG_DEBUG("MTA thread started for connection");

            auto winrtId = winrt::to_hstring(id.toStdString());

            LOG_DEBUG("TryCreateFromId...");
            auto connection = winrt_audio::AudioPlaybackConnection::TryCreateFromId(winrtId);
            if (!connection) {
                LOG_WARN("TryCreateFromId returned null");
                QMetaObject::invokeMethod(this, [this]() {
                    setState(BluetoothConnectionState::WaitingPair);
                }, Qt::QueuedConnection);
                winrt::uninit_apartment();
                return;
            }

            // Monitor state changes
            connection.StateChanged([this](
                const winrt_audio::AudioPlaybackConnection& sender,
                const winrt::Windows::Foundation::IInspectable&) {
                try {
                    auto state = sender.State();
                    LOG_INFOF("Connection state changed: %d", static_cast<int>(state));
                    if (state == winrt_audio::AudioPlaybackConnectionState::Closed) {
                        // Wake up the thread so it can exit
                        {
                            std::lock_guard<std::mutex> lock(m_impl->connectionMutex);
                            m_impl->disconnectRequested.store(true);
                        }
                        m_impl->connectionCv.notify_all();

                        QMetaObject::invokeMethod(this, [this]() {
                            m_impl->connection = nullptr;
                            setState(BluetoothConnectionState::Reconnecting);
                        }, Qt::QueuedConnection);
                    }
                } catch (...) {
                    LOG_ERROR("Exception in StateChanged callback");
                }
            });

            LOG_DEBUG("StartAsync...");
            connection.StartAsync().get();
            LOG_DEBUG("OpenAsync...");
            auto result = connection.OpenAsync().get();

            auto status = result.Status();
            const char* statusName = "Unknown";
            switch (status) {
            case winrt_audio::AudioPlaybackConnectionOpenResultStatus::Success:       statusName = "Success"; break;
            case winrt_audio::AudioPlaybackConnectionOpenResultStatus::RequestTimedOut: statusName = "RequestTimedOut"; break;
            case winrt_audio::AudioPlaybackConnectionOpenResultStatus::DeniedBySystem:  statusName = "DeniedBySystem"; break;
            case winrt_audio::AudioPlaybackConnectionOpenResultStatus::UnknownFailure:  statusName = "UnknownFailure"; break;
            }
            LOG_INFOF("OpenAsync result: %s (%d)", statusName, static_cast<int>(status));

            if (status != winrt_audio::AudioPlaybackConnectionOpenResultStatus::Success) {
                LOG_WARNF("OpenAsync failed with status: %d", static_cast<int>(status));
                connection.Close();
                QMetaObject::invokeMethod(this, [this]() {
                    setState(BluetoothConnectionState::WaitingPair);
                }, Qt::QueuedConnection);
            } else {
                m_impl->connection = std::move(connection);

                QMetaObject::invokeMethod(this, [this, id, name]() {
                    m_connectedDeviceId = id;
                    m_connectedDeviceName = name;
                    emit deviceNameChanged(name);
                    setState(BluetoothConnectionState::Connected);
                    LOG_INFOF("Connected to: %s", qPrintable(name));
                }, Qt::QueuedConnection);

                // Keep this thread alive to maintain the MTA apartment.
                LOG_DEBUG("Connection thread waiting (keeping MTA alive)...");
                std::unique_lock<std::mutex> lock(m_impl->connectionMutex);
                m_impl->connectionCv.wait(lock, [this]() {
                    return m_impl->disconnectRequested.load();
                });
                LOG_DEBUG("Connection thread woke up, exiting");
            }

        } catch (const winrt::hresult_error& e) {
            LOG_ERRORF("connectToDevice failed: 0x%08X", static_cast<uint32_t>(e.code()));
            QMetaObject::invokeMethod(this, [this]() {
                setState(BluetoothConnectionState::WaitingPair);
            }, Qt::QueuedConnection);
        } catch (...) {
            LOG_ERROR("connectToDevice unknown exception");
            QMetaObject::invokeMethod(this, [this]() {
                setState(BluetoothConnectionState::WaitingPair);
            }, Qt::QueuedConnection);
        }

        LOG_DEBUG("Connection thread finished");
        winrt::uninit_apartment();
    });
}

} // namespace BlueDrop

