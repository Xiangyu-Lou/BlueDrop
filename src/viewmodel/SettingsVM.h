#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include "app/UpdateChecker.h"

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
    Q_PROPERTY(bool loggingEnabled READ loggingEnabled WRITE setLoggingEnabled NOTIFY loggingEnabledChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
    // Update check
    Q_PROPERTY(bool updateChecking READ updateChecking NOTIFY updateCheckingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool updateDownloading READ updateDownloading NOTIFY updateDownloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString updateStatusText READ updateStatusText NOTIFY updateStatusTextChanged)
public:
    explicit SettingsVM(const SystemCheckResult& result, QObject* parent = nullptr);

    QString osVersion() const { return m_osVersion; }
    bool osVersionOk() const { return m_osVersionOk; }
    bool bluetoothAvailable() const { return m_bluetoothAvailable; }
    QString bluetoothAdapterName() const { return m_bluetoothAdapterName; }
    bool vbCableInstalled() const { return m_vbCableInstalled; }
    QString vbCableDeviceName() const { return m_vbCableDeviceName; }
    QString appVersion() const { return "0.1.6"; }

    QString closeAction() const { return m_closeAction; }
    void setCloseAction(const QString& action);

    bool loggingEnabled() const { return m_loggingEnabled; }
    void setLoggingEnabled(bool enabled);
    QString logFilePath() const;

    // Update state
    bool updateChecking() const { return m_updateChecking; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString latestVersion() const { return m_latestVersion; }
    bool updateDownloading() const { return m_updateDownloading; }
    int downloadProgress() const { return m_downloadProgress; }
    QString updateStatusText() const { return m_updateStatusText; }

    Q_INVOKABLE void clearLog();
    Q_INVOKABLE bool consumeFirstLaunch();
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadAndInstall();

signals:
    void closeActionChanged();
    void loggingEnabledChanged();
    void updateCheckingChanged();
    void updateAvailableChanged();
    void updateDownloadingChanged();
    void downloadProgressChanged();
    void updateStatusTextChanged();
    void requestAppExit();

private:
    void setUpdateStatusText(const QString& text);

    QString m_osVersion;
    bool m_osVersionOk;
    bool m_bluetoothAvailable;
    QString m_bluetoothAdapterName;
    bool m_vbCableInstalled;
    QString m_vbCableDeviceName;
    QString m_closeAction;
    bool m_loggingEnabled = false;

    // Update state
    UpdateChecker* m_updateChecker = nullptr;
    bool m_updateChecking = false;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QUrl m_downloadUrl;
    bool m_updateDownloading = false;
    int m_downloadProgress = 0;
    QString m_updateStatusText;
};

} // namespace BlueDrop
