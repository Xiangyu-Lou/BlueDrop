#pragma once

#include <QObject>
#include <QString>

namespace BlueDrop {

enum class BluetoothConnectionState {
    Disconnected,
    WaitingPair,
    Connecting,
    Connected,
    Reconnecting
};

} // namespace BlueDrop
