#include "SettingsVM.h"
#include "system/SystemChecker.h"
#include "system/Logger.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFile>
#include <QProcess>

namespace BlueDrop {

SettingsVM::SettingsVM(const SystemCheckResult& result, QObject* parent)
    : QObject(parent)
    , m_osVersion(result.osVersionString)
    , m_osVersionOk(result.osVersionOk)
    , m_bluetoothAvailable(result.bluetoothAvailable)
    , m_bluetoothAdapterName(result.bluetoothAdapterName)
    , m_vbCableInstalled(result.vbCableInstalled)
    , m_vbCableDeviceName(result.vbCableDeviceName)
    , m_closeAction(QSettings().value("behavior/closeAction", "").toString())
    , m_loggingEnabled(QSettings().value("debug/loggingEnabled", false).toBool())
    , m_updateChecker(new UpdateChecker(this))
{
    // Restore logging state from previous session
    if (m_loggingEnabled && !Logger::instance().isEnabled()) {
        Logger::instance().enable(logFilePath());
        LOG_INFOF("=== BlueDrop v%s session resumed (logging restored from settings) ===",
                  qPrintable(appVersion()));
        LOG_INFOF("OS: %s", qPrintable(m_osVersion));
    }

    // Wire up UpdateChecker signals
    connect(m_updateChecker, &UpdateChecker::updateAvailable,
            this, [this](const QString& latestVer, const QUrl& url) {
        m_updateChecking = false;
        m_updateAvailable = true;
        m_latestVersion = latestVer;
        m_downloadUrl = url;
        setUpdateStatusText(QString::fromUtf8(u8"发现新版本 v%1").arg(latestVer));
        emit updateCheckingChanged();
        emit updateAvailableChanged();
        LOG_INFOF("Update available: v%s", qPrintable(latestVer));
    });

    connect(m_updateChecker, &UpdateChecker::upToDate, this, [this]() {
        m_updateChecking = false;
        setUpdateStatusText(QString::fromUtf8(u8"已是最新版本"));
        emit updateCheckingChanged();
        LOG_INFO("UpdateChecker: already up to date");
    });

    connect(m_updateChecker, &UpdateChecker::checkError,
            this, [this](const QString& msg) {
        m_updateChecking = false;
        setUpdateStatusText(QString::fromUtf8(u8"检查更新失败，请稍后再试"));
        emit updateCheckingChanged();
        LOG_WARN(QString("UpdateChecker error: %1").arg(msg));
    });

    connect(m_updateChecker, &UpdateChecker::downloadProgress,
            this, [this](qint64 received, qint64 total) {
        if (total > 0)
            m_downloadProgress = static_cast<int>(received * 100 / total);
        emit downloadProgressChanged();
    });

    connect(m_updateChecker, &UpdateChecker::downloadFinished,
            this, [this](const QString& zipPath) {
        m_updateDownloading = false;
        emit updateDownloadingChanged();
        LOG_INFOF("Download finished: %s — launching Updater", qPrintable(zipPath));

        QStringList args = {
            "--pid=" + QString::number(QCoreApplication::applicationPid()),
            "--zip=" + zipPath,
            "--dir=" + QCoreApplication::applicationDirPath()
        };
        QString updaterPath = QCoreApplication::applicationDirPath() + "/Updater.exe";
        if (!QProcess::startDetached(updaterPath, args)) {
            setUpdateStatusText(QString::fromUtf8(u8"启动更新程序失败"));
            LOG_WARN("Failed to launch Updater.exe");
            return;
        }
        emit requestAppExit();
    });

    connect(m_updateChecker, &UpdateChecker::downloadError,
            this, [this](const QString& msg) {
        m_updateDownloading = false;
        setUpdateStatusText(QString::fromUtf8(u8"下载失败：") + msg);
        emit updateDownloadingChanged();
        LOG_WARN(QString("Download error: %1").arg(msg));
    });
}

bool SettingsVM::consumeFirstLaunch()
{
    QSettings s;
    if (s.value("behavior/firstLaunchDone", false).toBool())
        return false;
    s.setValue("behavior/firstLaunchDone", true);
    return true;
}

void SettingsVM::setCloseAction(const QString& action)
{
    if (m_closeAction == action) return;
    m_closeAction = action;
    QSettings().setValue("behavior/closeAction", action);
    emit closeActionChanged();
}

QString SettingsVM::logFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/bluedrop_debug.log";
}

void SettingsVM::setLoggingEnabled(bool enabled)
{
    if (m_loggingEnabled == enabled) return;
    m_loggingEnabled = enabled;
    QSettings().setValue("debug/loggingEnabled", enabled);

    if (enabled) {
        Logger::instance().enable(logFilePath());
        LOG_INFOF("=== BlueDrop v%s logging enabled from settings ===", qPrintable(appVersion()));
        LOG_INFOF("OS: %s", qPrintable(m_osVersion));
        LOG_INFOF("BT adapter: %s (available=%s)",
                  qPrintable(m_bluetoothAdapterName),
                  m_bluetoothAvailable ? "yes" : "no");
        LOG_INFOF("VB-Cable: %s (installed=%s)",
                  qPrintable(m_vbCableDeviceName),
                  m_vbCableInstalled ? "yes" : "no");
    } else {
        LOG_INFO("Logging disabled from settings");
        Logger::instance().disable();
    }

    emit loggingEnabledChanged();
}

void SettingsVM::clearLog()
{
    bool wasEnabled = m_loggingEnabled;
    if (wasEnabled) {
        Logger::instance().disable();
    }
    QFile::remove(logFilePath());
    if (wasEnabled) {
        Logger::instance().enable(logFilePath());
        LOG_INFOF("=== BlueDrop v%s log cleared ===", qPrintable(appVersion()));
    }
}

void SettingsVM::checkForUpdates()
{
    if (m_updateChecking || m_updateDownloading) return;
    m_updateChecking = true;
    m_updateAvailable = false;
    m_downloadProgress = 0;
    setUpdateStatusText(QString::fromUtf8(u8"检查中…"));
    emit updateCheckingChanged();
    emit updateAvailableChanged();
    emit downloadProgressChanged();
    m_updateChecker->checkForUpdates();
}

void SettingsVM::downloadAndInstall()
{
    if (!m_updateAvailable || m_updateDownloading) return;
    m_updateDownloading = true;
    m_downloadProgress = 0;
    setUpdateStatusText(QString::fromUtf8(u8"下载中…"));
    emit updateDownloadingChanged();
    emit downloadProgressChanged();
    m_updateChecker->downloadUpdate(m_downloadUrl);
}

void SettingsVM::setUpdateStatusText(const QString& text)
{
    if (m_updateStatusText == text) return;
    m_updateStatusText = text;
    emit updateStatusTextChanged();
}

} // namespace BlueDrop
