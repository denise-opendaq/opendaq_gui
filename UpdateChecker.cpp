#include "UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

const char* UpdateChecker::GitHubReleasesLatestUrl =
    "https://api.github.com/repos/denise-opendaq/opendaq_gui/releases/latest";

UpdateChecker::UpdateChecker(const QString& currentVersion, QObject* parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_network(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker()
{
    if (m_reply)
    {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void UpdateChecker::checkForUpdates()
{
    if (m_reply)
        return;

    QNetworkRequest request(QUrl(QString::fromUtf8(GitHubReleasesLatestUrl)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "OpenDAQ-Qt-GUI-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::onReplyFinished()
{
    if (!m_reply)
        return;

    m_reply->deleteLater();
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;

    if (reply->error() != QNetworkReply::NoError)
        return;

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject())
        return;

    QJsonObject obj = doc.object();
    QString tagName = obj.value(QStringLiteral("tag_name")).toString().trimmed();
    QString body = obj.value(QStringLiteral("body")).toString().trimmed();
    QString htmlUrl = obj.value(QStringLiteral("html_url")).toString().trimmed();
    QList<ReleaseAsset> assets;
    QJsonArray assetsArray = obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& v : assetsArray)
    {
        QJsonObject a = v.toObject();
        ReleaseAsset asset;
        asset.name = a.value(QStringLiteral("name")).toString().trimmed();
        asset.downloadUrl = a.value(QStringLiteral("browser_download_url")).toString().trimmed();
        asset.size = a.value(QStringLiteral("size")).toInteger(0);
        if (!asset.name.isEmpty() && !asset.downloadUrl.isEmpty())
            assets.append(asset);
    }

    if (tagName.isEmpty())
        return;

    if (isNewerVersion(m_currentVersion, tagName))
        Q_EMIT updateAvailable(tagName, body, htmlUrl, assets);
}

bool UpdateChecker::isNewerVersion(const QString& currentVersion, const QString& remoteVersion)
{
    QList<int> current = parseVersion(currentVersion);
    QList<int> remote = parseVersion(remoteVersion);

    for (int i = 0; i < qMax(current.size(), remote.size()); ++i)
    {
        int c = i < current.size() ? current[i] : 0;
        int r = i < remote.size() ? remote[i] : 0;
        if (r > c)
            return true;
        if (r < c)
            return false;
    }
    return false;
}

QList<int> UpdateChecker::parseVersion(const QString& version)
{
    QList<int> parts;
    QString s = version.trimmed();
    if (s.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        s = s.mid(1);
    for (const QString& segment : s.split(QLatin1Char('.')))
    {
        bool ok = false;
        int n = segment.trimmed().toInt(&ok);
        parts.append(ok ? n : 0);
    }
    return parts.isEmpty() ? QList<int>{0} : parts;
}
