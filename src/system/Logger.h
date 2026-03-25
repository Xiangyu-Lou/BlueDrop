#pragma once

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

namespace BlueDrop {

/// Simple file logger with runtime enable/disable.
/// Usage:
///   Logger::instance().enable("bluedrop.log");
///   LOG_INFO("something happened");
///   LOG_DEBUG("value = %d", 42);
///   Logger::instance().disable();
///
/// Controlled by environment variable BLUEDROP_LOG=1 for auto-enable,
/// or call enable()/disable() at runtime.

class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    static Logger& instance() {
        static Logger s;
        return s;
    }

    /// Enable logging to file. If path is empty, uses "bluedrop.log".
    void enable(const QString& filePath = {}) {
        QMutexLocker lock(&m_mutex);
        if (m_enabled) return;
        m_file.setFileName(filePath.isEmpty() ? QStringLiteral("bluedrop.log") : filePath);
        m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        m_enabled = true;
        writeRaw("=== Log started " +
                 QDateTime::currentDateTime().toString(Qt::ISODate) + " ===\n");
    }

    void disable() {
        QMutexLocker lock(&m_mutex);
        if (!m_enabled) return;
        writeRaw("=== Log ended " +
                 QDateTime::currentDateTime().toString(Qt::ISODate) + " ===\n");
        m_file.close();
        m_enabled = false;
    }

    bool isEnabled() const { return m_enabled; }

    void setLevel(Level level) { m_level = level; }

    void write(Level level, const char* file, int line, const QString& msg) {
        if (!m_enabled || level < m_level) return;
        QMutexLocker lock(&m_mutex);

        static const char* tags[] = { "DBG", "INF", "WRN", "ERR" };
        QTextStream out(&m_file);
        out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
            << " [" << tags[level] << "] "
            << msg;

        // Include source location for debug/warning/error
        if (level != Info) {
            // Extract just the filename from full path
            QString src = QString::fromUtf8(file);
            int slash = src.lastIndexOf('/');
            if (slash < 0) slash = src.lastIndexOf('\\');
            if (slash >= 0) src = src.mid(slash + 1);
            out << "  (" << src << ":" << line << ")";
        }
        out << "\n";
        out.flush();
    }

    /// Install as Qt message handler (captures qDebug/qWarning/qCritical)
    static void installQtHandler() {
        qInstallMessageHandler(qtMessageHandler);
    }

private:
    Logger() = default;
    ~Logger() { disable(); }

    void writeRaw(const QString& text) {
        QTextStream out(&m_file);
        out << text;
        out.flush();
    }

    static void qtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
        Level level = Info;
        switch (type) {
        case QtDebugMsg:    level = Debug; break;
        case QtInfoMsg:     level = Info; break;
        case QtWarningMsg:  level = Warning; break;
        case QtCriticalMsg: level = Error; break;
        case QtFatalMsg:    level = Error; break;
        }
        instance().write(level, ctx.file ? ctx.file : "qt", ctx.line, msg);

        if (type == QtFatalMsg)
            abort();
    }

    QFile m_file;
    QMutex m_mutex;
    Level m_level = Debug;
    bool m_enabled = false;
};

} // namespace BlueDrop

// Convenience macros - zero overhead when logging is disabled
#define LOG_DEBUG(msg) BlueDrop::Logger::instance().write(BlueDrop::Logger::Debug, __FILE__, __LINE__, msg)
#define LOG_INFO(msg)  BlueDrop::Logger::instance().write(BlueDrop::Logger::Info, __FILE__, __LINE__, msg)
#define LOG_WARN(msg)  BlueDrop::Logger::instance().write(BlueDrop::Logger::Warning, __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) BlueDrop::Logger::instance().write(BlueDrop::Logger::Error, __FILE__, __LINE__, msg)

// Format variants
#define LOG_DEBUGF(fmt, ...) LOG_DEBUG(QString::asprintf(fmt, __VA_ARGS__))
#define LOG_INFOF(fmt, ...)  LOG_INFO(QString::asprintf(fmt, __VA_ARGS__))
#define LOG_WARNF(fmt, ...)  LOG_WARN(QString::asprintf(fmt, __VA_ARGS__))
#define LOG_ERRORF(fmt, ...) LOG_ERROR(QString::asprintf(fmt, __VA_ARGS__))
