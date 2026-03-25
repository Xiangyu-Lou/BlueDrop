#pragma once

#include <QObject>
#include "bluetooth/BluetoothTypes.h"

namespace BlueDrop {

class BluetoothManager;

class BluetoothVM : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusColor READ statusColor NOTIFY connectionStateChanged)
public:
    explicit BluetoothVM(BluetoothManager* manager, QObject* parent = nullptr);

    int connectionState() const;
    QString deviceName() const;
    QString statusText() const;
    QString statusColor() const;

    Q_INVOKABLE void startListening();
    Q_INVOKABLE void stopListening();
    Q_INVOKABLE void disconnect();

signals:
    void connectionStateChanged();
    void deviceNameChanged();

private:
    BluetoothManager* m_manager;
};

} // namespace BlueDrop
