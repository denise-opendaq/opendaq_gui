#pragma once

#include <QObject>
#include <QString>
#include <QList>

#include "context/release_asset.h"

class QNetworkAccessManager;
class QNetworkReply;

// Checks GitHub releases for a newer version and emits updateAvailable(version, changelog, url, assets).
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(const QString& currentVersion, QObject* parent = nullptr);
    ~UpdateChecker() override;

    // Starts async check. Emits updateAvailable() if a newer release exists.
    void checkForUpdates();

    static const char* GitHubReleasesLatestUrl;

Q_SIGNALS:
    void updateAvailable(const QString& version, const QString& changelog, const QString& releaseUrl,
                         const QList<ReleaseAsset>& assets);

private Q_SLOTS:
    void onReplyFinished();

private:
    // Returns true if remoteVersion is strictly greater than currentVersion.
    static bool isNewerVersion(const QString& currentVersion, const QString& remoteVersion);
    static QList<int> parseVersion(const QString& version);

    QString m_currentVersion;
    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
};
