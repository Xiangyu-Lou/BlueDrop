// Updater.exe — replaces BlueDrop installation files after the main process exits.
//
// Usage:
//   Updater.exe --pid=<PID> --zip=<zipPath> --dir=<installDir>
//
// Flow:
//   1. Wait for the main process (PID) to exit (up to 30s)
//   2. Extract zip to a temp directory using PowerShell
//   3. Copy all files from the extracted subdirectory to installDir (skips Updater.exe)
//   4. Launch BlueDrop.exe from installDir
//   5. Clean up temp files and exit

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QScreen>
#include <QThread>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <windows.h>

// ─── Log ────────────────────────────────────────────────────────────────────

static QFile g_logFile;
static QMutex g_logMutex;

static void log(const QString& msg)
{
    QMutexLocker locker(&g_logMutex);
    QString line = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " " + msg;
    if (g_logFile.isOpen()) {
        QTextStream fs(&g_logFile);
        fs << line << "\n";
        fs.flush();
    }
}

// ─── Worker thread ──────────────────────────────────────────────────────────

class UpdateWorker : public QThread
{
    Q_OBJECT
public:
    explicit UpdateWorker(DWORD pid, const QString& zip, const QString& dir,
                          QObject* parent = nullptr)
        : QThread(parent), m_pid(pid), m_zip(zip), m_dir(dir) {}

signals:
    void statusChanged(const QString& text);
    void done(bool success, const QString& errorMsg);

protected:
    void run() override
    {
        emit statusChanged("正在等待主程序退出…");
        log("Waiting for PID " + QString::number(m_pid));

        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, m_pid);
        if (h) {
            WaitForSingleObject(h, 30000);
            CloseHandle(h);
        }
        log("Main process exited");

        // Extract
        emit statusChanged("正在解压更新包…");
        QString tempExtract = QDir::tempPath() + "/BlueDrop-update-extract";
        QDir().mkpath(tempExtract);

        QString psCmd = QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                            .arg(m_zip, tempExtract);
        log("PowerShell: " + psCmd);

        QProcess proc;
        proc.setProgram("powershell.exe");
        proc.setArguments({"-NoProfile", "-NonInteractive", "-Command", psCmd});
        proc.start();
        if (!proc.waitForFinished(120000)) {
            proc.kill();
            emit done(false, "解压超时");
            return;
        }
        if (proc.exitCode() != 0) {
            QString err = proc.readAllStandardError().trimmed();
            log("PS error: " + err);
            emit done(false, "解压失败：" + err);
            return;
        }
        log("Extraction done");

        // Find subdirectory
        QDir extractDir(tempExtract);
        QStringList subdirs = extractDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (subdirs.isEmpty()) {
            emit done(false, "压缩包结构错误");
            return;
        }
        QString sourceDir = tempExtract + "/" + subdirs.first();

        // Copy files
        emit statusChanged("正在安装新版本文件…");
        log("Copying from " + sourceDir + " to " + m_dir);

        QDir src(sourceDir);
        QDir dst(m_dir);
        QDirIterator it(sourceDir, QDir::Files | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString relPath = src.relativeFilePath(it.filePath());
            QString dstPath = dst.filePath(relPath);

            // Skip Updater.exe — it's the currently running executable and is locked
            if (QFileInfo(it.filePath()).fileName().compare("Updater.exe",
                    Qt::CaseInsensitive) == 0) {
                log("Skipping (in use): " + it.filePath());
                continue;
            }

            QDir().mkpath(QFileInfo(dstPath).absolutePath());

            // Retry up to 10× / 500ms — Windows may briefly hold an image-loader
            // lock on BlueDrop.exe even after the process has exited.
            bool copied = false;
            for (int attempt = 1; attempt <= 10; ++attempt) {
                if (QFile::exists(dstPath))
                    QFile::remove(dstPath);
                if (QFile::copy(it.filePath(), dstPath)) {
                    copied = true;
                    break;
                }
                log(QString("Copy attempt %1 failed for %2").arg(attempt).arg(dstPath));
                emit statusChanged(QString("正在安装新版本文件… (重试 %1)").arg(attempt));
                msleep(500);
            }

            if (!copied) {
                emit done(false, "文件复制失败：" + relPath);
                return;
            }
        }
        log("All files copied");

        // Launch BlueDrop
        emit statusChanged("正在启动新版本…");
        QString exePath = QDir::toNativeSeparators(m_dir + "/BlueDrop.exe");
        log("Launching: " + exePath);

        if (!QProcess::startDetached(exePath, {}, m_dir)) {
            emit done(false, "无法启动 BlueDrop.exe");
            return;
        }

        // Clean up
        QDir(tempExtract).removeRecursively();
        QFile::remove(m_zip);
        log("Update complete");

        emit done(true, {});
    }

private:
    DWORD   m_pid;
    QString m_zip;
    QString m_dir;
};

#include "main.moc"

// ─── Progress window ─────────────────────────────────────────────────────────

static QWidget* createWindow()
{
    auto* w = new QWidget(nullptr,
        Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    w->setFixedSize(340, 100);
    w->setStyleSheet(
        "QWidget { background: #FFFFFF; border: 1px solid #D0D0D5; border-radius: 12px; }"
        "QLabel#title { font-size: 14px; font-weight: bold; color: #1D1D1F; border: none; }"
        "QLabel#status { font-size: 12px; color: #6E6E73; border: none; }");

    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(8);

    auto* title = new QLabel("聚音 BlueDrop 正在更新", w);
    title->setObjectName("title");

    auto* status = new QLabel("正在准备…", w);
    status->setObjectName("status");
    status->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(status);

    // Center on screen
    if (auto* screen = QApplication::primaryScreen()) {
        auto rect = screen->availableGeometry();
        w->move(rect.center() - w->rect().center());
    }

    w->show();
    return w;
}

// ─── main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("BlueDrop Updater");

    QCommandLineParser parser;
    parser.addOption({"pid", "PID of the main process", "pid"});
    parser.addOption({"zip", "Path to update zip", "zip"});
    parser.addOption({"dir", "Installation directory", "dir"});
    parser.process(app);

    DWORD   pid        = parser.value("pid").toULong();
    QString zipPath    = parser.value("zip");
    QString installDir = parser.value("dir");

    // Open log file
    g_logFile.setFileName(installDir + "/BlueDrop-updater.log");
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    log("Updater started. PID=" + QString::number(pid)
        + " zip=" + zipPath + " dir=" + installDir);

    if (!pid || zipPath.isEmpty() || installDir.isEmpty()) {
        log("ERROR: missing arguments");
        return 1;
    }

    // Create progress window
    QWidget* window = createWindow();
    auto* statusLabel = window->findChild<QLabel*>("status");

    // Start worker
    auto* worker = new UpdateWorker(pid, zipPath, installDir, &app);

    QObject::connect(worker, &UpdateWorker::statusChanged,
                     statusLabel, &QLabel::setText,
                     Qt::QueuedConnection);

    QObject::connect(worker, &UpdateWorker::done, &app,
        [&](bool success, const QString& errorMsg) {
            if (success) {
                statusLabel->setText("更新完成，正在启动…");
                QTimer::singleShot(800, &app, &QApplication::quit);
            } else {
                log("ERROR: " + errorMsg);
                statusLabel->setText("更新失败：" + errorMsg);
                // Keep window visible for 8s so user can read the error
                QTimer::singleShot(8000, &app, &QApplication::quit);
            }
        }, Qt::QueuedConnection);

    worker->start();

    return app.exec();
}
