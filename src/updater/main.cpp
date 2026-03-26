// Updater.exe — replaces BlueDrop installation files after the main process exits.
//
// Usage:
//   Updater.exe --pid=<PID> --zip=<zipPath> --dir=<installDir>
//
// Flow:
//   1. Wait for the main process (PID) to exit (up to 30s)
//   2. Extract zip to a temp directory using PowerShell
//   3. Copy all files from the extracted subdirectory to installDir
//   4. Launch BlueDrop.exe from installDir
//   5. Clean up temp files and exit

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QTextStream>

#include <windows.h>

static QFile g_logFile;

static void log(const QString& msg)
{
    QString line = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " " + msg;
    QTextStream out(stdout);
    out << line << "\n";
    out.flush();

    if (g_logFile.isOpen()) {
        QTextStream fs(&g_logFile);
        fs << line << "\n";
        fs.flush();
    }
}

static bool waitForProcess(DWORD pid, int timeoutMs)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) {
        // Process already gone
        return true;
    }
    DWORD result = WaitForSingleObject(h, static_cast<DWORD>(timeoutMs));
    CloseHandle(h);
    return result == WAIT_OBJECT_0;
}

static bool extractZip(const QString& zipPath, const QString& destDir)
{
    // Use PowerShell Expand-Archive (available on Windows 10+).
    // Pass program and args separately so Qt does not try to parse a single
    // command string — nested quotes in a single string are mangled on Windows.
    QString psCmd = QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                        .arg(zipPath, destDir);
    QStringList args = {"-NoProfile", "-NonInteractive", "-Command", psCmd};

    log("Extracting zip: " + zipPath + " -> " + destDir);
    log("PowerShell command: " + psCmd);

    QProcess proc;
    proc.setProgram("powershell.exe");
    proc.setArguments(args);
    proc.start();
    if (!proc.waitForFinished(120000)) {
        log("ERROR: PowerShell timed out");
        proc.kill();
        return false;
    }
    QString psOut = proc.readAllStandardOutput().trimmed();
    QString psErr = proc.readAllStandardError().trimmed();
    if (!psOut.isEmpty()) log("PS stdout: " + psOut);
    if (!psErr.isEmpty()) log("PS stderr: " + psErr);
    int ret = proc.exitCode();
    log("PowerShell exit code: " + QString::number(ret));
    return ret == 0;
}

// Copy all files from srcDir into dstDir, preserving relative paths.
static bool copyDir(const QString& srcDir, const QString& dstDir)
{
    QDir src(srcDir);
    QDir dst(dstDir);

    QDirIterator it(srcDir, QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = src.relativeFilePath(it.filePath());
        QString dstPath = dst.filePath(relPath);

        // Ensure parent directory exists
        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        // Remove existing file (can't overwrite on Windows otherwise).
        // Retry up to 10 times with 500ms delay — Windows may hold a brief
        // image-loader lock on BlueDrop.exe even after the process has exited.
        bool copied = false;
        for (int attempt = 1; attempt <= 10; ++attempt) {
            if (QFile::exists(dstPath))
                QFile::remove(dstPath);

            if (QFile::copy(it.filePath(), dstPath)) {
                copied = true;
                break;
            }

            log(QString("Copy attempt %1 failed for %2, retrying in 500ms...")
                    .arg(attempt).arg(dstPath));
            QThread::msleep(500);
        }

        if (!copied) {
            log("ERROR: failed to copy " + it.filePath() + " -> " + dstPath);
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addOption({"pid", "PID of the main process to wait for", "pid"});
    parser.addOption({"zip", "Path to the downloaded update zip", "zip"});
    parser.addOption({"dir", "Installation directory to update", "dir"});
    parser.process(app);

    DWORD pid = parser.value("pid").toULong();
    QString zipPath = parser.value("zip");
    QString installDir = parser.value("dir");

    // Open log file in the install dir for diagnostics (visible even when console is hidden)
    g_logFile.setFileName(installDir + "/BlueDrop-updater.log");
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    if (!pid || zipPath.isEmpty() || installDir.isEmpty()) {
        log("Usage: Updater.exe --pid=<PID> --zip=<path> --dir=<path>");
        return 1;
    }

    log("Updater started. PID=" + QString::number(pid)
        + " zip=" + zipPath + " dir=" + installDir);

    // Wait up to 30 seconds for the main app to exit
    if (!waitForProcess(pid, 30000)) {
        log("WARNING: timed out waiting for main process — continuing anyway");
    }

    log("Main process exited. Proceeding with update.");

    // Extract zip to a unique temp directory
    QString tempExtract = QDir::tempPath() + "/BlueDrop-update-extract";
    QDir().mkpath(tempExtract);

    if (!extractZip(zipPath, tempExtract)) {
        log("ERROR: failed to extract zip");
        return 1;
    }

    // The zip contains a single subdirectory (e.g. BlueDrop-v0.1.5/)
    QDir extractDir(tempExtract);
    QStringList subdirs = extractDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subdirs.isEmpty()) {
        log("ERROR: zip contained no subdirectory");
        return 1;
    }

    QString sourceDir = tempExtract + "/" + subdirs.first();
    log("Copying files from " + sourceDir + " to " + installDir);

    if (!copyDir(sourceDir, installDir)) {
        log("ERROR: file copy failed");
        return 1;
    }

    log("Files copied successfully. Launching BlueDrop...");

    QString exePath = installDir + "/BlueDrop.exe";
    if (!QProcess::startDetached(exePath, {}, installDir)) {
        log("ERROR: failed to launch " + exePath);
        return 1;
    }

    // Clean up temp files
    QDir(tempExtract).removeRecursively();
    QFile::remove(zipPath);

    log("Update complete.");
    return 0;
}
