#pragma once

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>

namespace BlueDrop {

/// Checks GitHub Releases for a newer version and downloads the update zip.
///
/// Usage:
///   UpdateChecker checker;
///   connect(&checker, &UpdateChecker::updateAvailable, ...);
///   checker.checkForUpdates();
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    /// Query GitHub API for the latest release. Emits updateAvailable or upToDate.
    void checkForUpdates();

    /// Download the zip asset to %TEMP%/BlueDrop-update.zip.
    /// Emits downloadProgress, then downloadFinished or downloadError.
    void downloadUpdate(const QUrl& url);

signals:
    void updateAvailable(const QString& latestVersion, const QUrl& downloadUrl);
    void upToDate();
    void checkError(const QString& msg);

    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString& zipPath);
    void downloadError(const QString& msg);

private:
    QNetworkAccessManager m_nam;
};

} // namespace BlueDrop
