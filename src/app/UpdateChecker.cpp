#include "UpdateChecker.h"
#include "system/Logger.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace BlueDrop {

static const char* kApiUrl =
    "https://api.github.com/repos/Xiangyu-Lou/BlueDrop/releases/latest";

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
}

void UpdateChecker::checkForUpdates()
{
    LOG_INFO("UpdateChecker: checking for updates...");

    QNetworkRequest req{QUrl(kApiUrl)};
    req.setRawHeader("User-Agent", "BlueDrop");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString msg = reply->errorString();
            LOG_WARN(QString("UpdateChecker: network error: %1").arg(msg));
            emit checkError(msg);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            LOG_WARN(QString("UpdateChecker: JSON parse error: %1").arg(parseErr.errorString()));
            emit checkError(parseErr.errorString());
            return;
        }

        QJsonObject root = doc.object();
        QString tagName = root.value("tag_name").toString();  // e.g. "v0.1.5"
        if (tagName.isEmpty()) {
            emit checkError("invalid response: no tag_name");
            return;
        }

        // Strip leading "v"
        QString latestVersion = tagName.startsWith('v') ? tagName.mid(1) : tagName;
        QString currentVersion = QCoreApplication::applicationVersion();

        LOG_INFOF("UpdateChecker: latest=%s current=%s",
                  qPrintable(latestVersion), qPrintable(currentVersion));

        if (latestVersion == currentVersion) {
            emit upToDate();
            return;
        }

        // Find win64.zip asset
        QUrl downloadUrl;
        QJsonArray assets = root.value("assets").toArray();
        for (const QJsonValue& v : assets) {
            QJsonObject asset = v.toObject();
            QString name = asset.value("name").toString();
            if (name.endsWith("-win64.zip")) {
                downloadUrl = QUrl(asset.value("browser_download_url").toString());
                break;
            }
        }

        if (!downloadUrl.isValid()) {
            emit checkError("no win64.zip asset found in release");
            return;
        }

        LOG_INFOF("UpdateChecker: update available — v%s, url=%s",
                  qPrintable(latestVersion), qPrintable(downloadUrl.toString()));
        emit updateAvailable(latestVersion, downloadUrl);
    });
}

void UpdateChecker::downloadUpdate(const QUrl& url)
{
    QString zipPath = QDir::tempPath() + "/BlueDrop-update.zip";
    LOG_INFOF("UpdateChecker: downloading to %s", qPrintable(zipPath));

    QNetworkRequest req{url};
    req.setRawHeader("User-Agent", "BlueDrop");
    // Follow redirects (GitHub asset URLs redirect to CDN)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                emit downloadProgress(received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, zipPath]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString msg = reply->errorString();
            LOG_WARN(QString("UpdateChecker: download error: %1").arg(msg));
            emit downloadError(msg);
            return;
        }

        // Write to disk atomically
        QSaveFile file(zipPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadError("cannot write to temp file: " + zipPath);
            return;
        }
        file.write(reply->readAll());
        if (!file.commit()) {
            emit downloadError("failed to commit temp file");
            return;
        }

        LOG_INFOF("UpdateChecker: download complete — %s", qPrintable(zipPath));
        emit downloadFinished(zipPath);
    });
}

} // namespace BlueDrop
