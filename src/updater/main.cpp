// Updater.exe — replaces BlueDrop installation files after the main process exits.
//
// Usage:
//   Updater.exe --pid=<PID> --zip=<zipPath> --dir=<installDir>
//
// Progress is shown via a small topmost Win32 window (no Qt Widgets, to avoid
// locking Qt DLLs in the install directory during the copy phase).
//
// Flow:
//   1. Show progress window
//   2. Wait for the main process (PID) to exit (up to 30s)
//   3. Extract zip to a temp directory using PowerShell
//   4. Copy all files (skip Updater.exe + any file still locked after retries)
//   5. Launch BlueDrop.exe from installDir
//   6. Clean up temp files and exit

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QThread>
#include <QTextStream>

#include <windows.h>
#include <thread>

// ─── Log ─────────────────────────────────────────────────────────────────────

static QFile  g_logFile;
static QMutex g_logMutex;

static void log(const QString& msg)
{
    QMutexLocker lock(&g_logMutex);
    QString line = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " " + msg;
    if (g_logFile.isOpen()) {
        QTextStream fs(&g_logFile);
        fs << line << "\n";
        fs.flush();
    }
}

// ─── Win32 progress window ───────────────────────────────────────────────────

static HWND g_hwndMain   = nullptr;
static HWND g_hwndStatus = nullptr;

#define WM_SET_STATUS (WM_USER + 1)  // lParam = wchar_t* (heap, caller frees after post)
#define WM_CLOSE_WINDOW (WM_USER + 2)

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SET_STATUS: {
        auto* text = reinterpret_cast<wchar_t*>(lParam);
        SetWindowTextW(g_hwndStatus, text);
        delete[] text;
        return 0;
    }
    case WM_CLOSE_WINDOW:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Call from any thread — posts a status text update to the window.
static void setStatus(const wchar_t* text)
{
    if (!g_hwndMain) return;
    // Allocate on heap; WndProc deletes after processing
    size_t len = wcslen(text) + 1;
    auto* buf = new wchar_t[len];
    wcscpy_s(buf, len, text);
    PostMessageW(g_hwndMain, WM_SET_STATUS, 0, reinterpret_cast<LPARAM>(buf));
}

static void setStatus(const QString& text)
{
    auto ws = text.toStdWString();
    setStatus(ws.c_str());
}

static void closeWindow()
{
    if (g_hwndMain)
        PostMessageW(g_hwndMain, WM_CLOSE_WINDOW, 0, 0);
}

static void createProgressWindow(HINSTANCE hInst)
{
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"BlueDrop_Updater_Wnd";
    RegisterClassExW(&wc);

    // Window size
    const int W = 360, H = 110;

    // Center on primary monitor
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sx - W) / 2;
    int y  = (sy - H) / 2;

    g_hwndMain = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"BlueDrop_Updater_Wnd",
        L"聚音 BlueDrop 正在更新",
        WS_POPUP | WS_BORDER,
        x, y, W, H,
        nullptr, nullptr, hInst, nullptr);

    // Title label
    HFONT hFontTitle = CreateFontW(
        -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HWND hwndTitle = CreateWindowExW(
        0, L"STATIC", L"聚音 BlueDrop 正在更新",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 18, 320, 24,
        g_hwndMain, nullptr, hInst, nullptr);
    SendMessageW(hwndTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFontTitle), TRUE);

    // Status label
    HFONT hFontStatus = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    g_hwndStatus = CreateWindowExW(
        0, L"STATIC", L"正在准备…",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 52, 320, 40,
        g_hwndMain, nullptr, hInst, nullptr);
    SendMessageW(g_hwndStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStatus), TRUE);

    ShowWindow(g_hwndMain, SW_SHOW);
    UpdateWindow(g_hwndMain);
}

// ─── Update worker (runs on a background std::thread) ────────────────────────

static bool waitForProcess(DWORD pid, int timeoutMs)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return true;  // already gone
    DWORD r = WaitForSingleObject(h, static_cast<DWORD>(timeoutMs));
    CloseHandle(h);
    return r == WAIT_OBJECT_0;
}

static bool extractZip(const QString& zipPath, const QString& destDir)
{
    QString psCmd = QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                        .arg(zipPath, destDir);
    log("PowerShell: " + psCmd);

    QProcess proc;
    proc.setProgram("powershell.exe");
    proc.setArguments({"-NoProfile", "-NonInteractive", "-Command", psCmd});
    proc.start();
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        log("ERROR: PowerShell timed out");
        return false;
    }
    QString err = proc.readAllStandardError().trimmed();
    if (!err.isEmpty()) log("PS stderr: " + err);
    int code = proc.exitCode();
    log("PowerShell exit code: " + QString::number(code));
    return code == 0;
}

// Copy srcDir -> dstDir.
// Files that are still locked after retries are skipped (logged as warnings).
// Returns false only on unexpected hard errors; skipped files do not fail the update.
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

        // Skip Updater.exe — it is the running executable and is always locked.
        if (QFileInfo(it.filePath()).fileName().compare("Updater.exe",
                Qt::CaseInsensitive) == 0) {
            log("Skip (self): " + relPath);
            continue;
        }

        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        // Retry up to 10× with 500ms delay for BlueDrop.exe (image-loader lock).
        // Other DLLs loaded by Updater.exe (Qt6Core, icuuc, etc.) are also skipped
        // gracefully after retries — they are the same version and don't need replacing.
        bool copied = false;
        for (int attempt = 1; attempt <= 10; ++attempt) {
            if (QFile::exists(dstPath))
                QFile::remove(dstPath);
            if (QFile::copy(it.filePath(), dstPath)) {
                copied = true;
                break;
            }
            if (attempt < 10)
                QThread::msleep(500);
        }

        if (!copied) {
            log("WARN: skipping locked file: " + relPath);
        }
    }
    return true;
}

static void runUpdate(DWORD pid, const QString& zipPath, const QString& installDir)
{
    // 1. Wait for main process
    setStatus(L"正在等待主程序退出…");
    log("Waiting for PID " + QString::number(pid));
    waitForProcess(pid, 30000);
    log("Main process exited");

    // 2. Extract zip
    setStatus(L"正在解压更新包…");
    QString tempExtract = QDir::tempPath() + "/BlueDrop-update-extract";
    QDir().mkpath(tempExtract);

    if (!extractZip(zipPath, tempExtract)) {
        setStatus(L"更新失败：解压出错，请重试");
        log("ERROR: extraction failed");
        QThread::msleep(6000);
        closeWindow();
        return;
    }

    // 3. Find subdirectory in zip
    QDir extractDir(tempExtract);
    QStringList subdirs = extractDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subdirs.isEmpty()) {
        setStatus(L"更新失败：压缩包结构错误");
        log("ERROR: no subdir in zip");
        QThread::msleep(6000);
        closeWindow();
        return;
    }
    QString sourceDir = tempExtract + "/" + subdirs.first();

    // 4. Copy files
    setStatus(L"正在安装新版本文件…");
    log("Copying from " + sourceDir + " to " + installDir);
    copyDir(sourceDir, installDir);
    log("Copy phase done");

    // 5. Launch BlueDrop
    setStatus(L"正在启动新版本…");
    QString exePath = QDir::toNativeSeparators(installDir + "/BlueDrop.exe");
    log("Launching: " + exePath);
    if (!QProcess::startDetached(exePath, {}, installDir)) {
        setStatus(L"更新完成，请手动启动 BlueDrop");
        log("WARN: startDetached failed, update files are in place");
        QThread::msleep(4000);
        closeWindow();
        return;
    }

    // 6. Clean up
    QDir(tempExtract).removeRecursively();
    QFile::remove(zipPath);
    log("Update complete");

    setStatus(L"更新完成，正在启动…");
    QThread::msleep(800);
    closeWindow();
}

// ─── main ────────────────────────────────────────────────────────────────────
// Qt provides its own WinMain (for WIN32_EXECUTABLE) that calls main() with
// the real argc/argv, so we use standard main() here.

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Parse command line via Qt (handles Unicode)
    QCommandLineParser parser;
    parser.addOption({"pid", "PID of the main process", "pid"});
    parser.addOption({"zip", "Path to update zip", "zip"});
    parser.addOption({"dir", "Installation directory", "dir"});
    parser.process(QCoreApplication::arguments());

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

    // Show progress window on main thread
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    createProgressWindow(hInst);

    // Run update on background thread so the Win32 message loop stays responsive
    std::thread worker([&]() {
        runUpdate(pid, zipPath, installDir);
        // Quit the Qt/Win32 event loop after worker finishes
        PostQuitMessage(0);
    });

    // Win32 message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    worker.join();
    return 0;
}
