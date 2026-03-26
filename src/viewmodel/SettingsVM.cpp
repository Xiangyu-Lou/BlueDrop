#include "SettingsVM.h"
#include "system/SystemChecker.h"
#include "system/Logger.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFile>

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
{
    // Restore logging state from previous session
    if (m_loggingEnabled && !Logger::instance().isEnabled()) {
        Logger::instance().enable(logFilePath());
        LOG_INFOF("=== BlueDrop v%s session resumed (logging restored from settings) ===",
                  qPrintable(appVersion()));
        LOG_INFOF("OS: %s", qPrintable(m_osVersion));
    }
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

} // namespace BlueDrop
