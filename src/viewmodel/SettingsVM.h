#pragma once

#include <QObject>
#include <QString>

namespace BlueDrop {

struct SystemCheckResult;

class SettingsVM : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString osVersion READ osVersion CONSTANT)
    Q_PROPERTY(bool osVersionOk READ osVersionOk CONSTANT)
    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable CONSTANT)
    Q_PROPERTY(QString bluetoothAdapterName READ bluetoothAdapterName CONSTANT)
    Q_PROPERTY(bool vbCableInstalled READ vbCableInstalled CONSTANT)
    Q_PROPERTY(QString vbCableDeviceName READ vbCableDeviceName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    // "": ask on close  "minimize": minimize to tray  "close": exit
    Q_PROPERTY(QString closeAction READ closeAction WRITE setCloseAction NOTIFY closeActionChanged)
public:
    explicit SettingsVM(const SystemCheckResult& result, QObject* parent = nullptr);

    QString osVersion() const { return m_osVersion; }
    bool osVersionOk() const { return m_osVersionOk; }
    bool bluetoothAvailable() const { return m_bluetoothAvailable; }
    QString bluetoothAdapterName() const { return m_bluetoothAdapterName; }
    bool vbCableInstalled() const { return m_vbCableInstalled; }
    QString vbCableDeviceName() const { return m_vbCableDeviceName; }
    QString appVersion() const { return "0.1.2"; }

    QString closeAction() const { return m_closeAction; }
    void setCloseAction(const QString& action);

    // Returns true the first time ever called, false thereafter (persisted via QSettings)
    Q_INVOKABLE bool consumeFirstLaunch();

signals:
    void closeActionChanged();

private:
    QString m_osVersion;
    bool m_osVersionOk;
    bool m_bluetoothAvailable;
    QString m_bluetoothAdapterName;
    bool m_vbCableInstalled;
    QString m_vbCableDeviceName;
    QString m_closeAction;
};

} // namespace BlueDrop
