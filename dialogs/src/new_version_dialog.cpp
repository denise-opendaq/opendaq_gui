#include "dialogs/new_version_dialog.h"
#include "context/gui_constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>

NewVersionDialog::NewVersionDialog(const QString& version,
                                   const QString& changelog,
                                   const QString& releaseUrl,
                                   const QList<ReleaseAsset>& assets,
                                   QWidget* parent)
    : QDialog(parent)
    , m_releaseUrl(releaseUrl)
    , m_assets(assets)
    , m_network(new QNetworkAccessManager(this))
{
    setWindowTitle(tr("New Version Available"));
    resize(GUIConstants::NEW_VERSION_DIALOG_WIDTH, GUIConstants::NEW_VERSION_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::NEW_VERSION_DIALOG_MIN_WIDTH, GUIConstants::NEW_VERSION_DIALOG_MIN_HEIGHT);
    setupUI();

    if (!m_releaseUrl.isEmpty())
    {
        m_versionLabel->setTextFormat(Qt::RichText);
        m_versionLabel->setOpenExternalLinks(true);
        m_versionLabel->setText(tr("Version <a href=\"%1\">%2</a> is available for download.")
                                    .arg(QString(m_releaseUrl).toHtmlEscaped())
                                    .arg(QString(version).toHtmlEscaped()));
    }
    else
    {
        m_versionLabel->setText(tr("Version %1 is available for download.").arg(version));
    }
    m_changelogEdit->setMarkdown(changelog.isEmpty() ? tr("(No release notes.)") : changelog);

    for (const ReleaseAsset& asset : m_assets)
    {
        QString label = asset.name;
        if (asset.size > 0)
            label += QStringLiteral(" (%1)").arg(formatSize(asset.size));
        m_assetCombo->addItem(label, asset.downloadUrl);
    }
    int recommended = recommendedAssetIndex(m_assets);
    if (recommended >= 0 && recommended < m_assetCombo->count())
        m_assetCombo->setCurrentIndex(recommended);
    m_downloadButton->setEnabled(!m_assets.isEmpty());
    m_progressBar->setVisible(false);
    m_downloadStatusLabel->setVisible(false);
}

void NewVersionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);
    mainLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                  GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                  GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                  GUIConstants::DEFAULT_LAYOUT_MARGIN);

    m_versionLabel = new QLabel(this);
    m_versionLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_versionLabel->setWordWrap(true);
    mainLayout->addWidget(m_versionLabel);

    auto* changelogLabel = new QLabel(tr("Release notes:"), this);
    changelogLabel->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(changelogLabel);

    m_changelogEdit = new QTextEdit(this);
    m_changelogEdit->setReadOnly(true);
    m_changelogEdit->setPlaceholderText(tr("(No release notes.)"));
    mainLayout->addWidget(m_changelogEdit, 1);

    auto* assetLabel = new QLabel(tr("Download:"), this);
    assetLabel->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(assetLabel);
    m_assetCombo = new QComboBox(this);
    m_assetCombo->setMinimumWidth(220);
    mainLayout->addWidget(m_assetCombo);

    m_downloadStatusLabel = new QLabel(this);
    m_downloadStatusLabel->setVisible(false);
    mainLayout->addWidget(m_downloadStatusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%p%"));
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_downloadButton = new QPushButton(tr("Download"), this);
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setDefault(true);
    m_closeButton->setAutoDefault(true);
    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_downloadButton, &QPushButton::clicked, this, &NewVersionDialog::onDownloadClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

NewVersionDialog::~NewVersionDialog()
{
    if (m_downloadReply)
    {
        m_downloadReply->disconnect(this);
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
}

int NewVersionDialog::recommendedAssetIndex(const QList<ReleaseAsset>& assets)
{
    if (assets.isEmpty())
        return 0;
    QString nameLower;
    auto matches = [&nameLower](const ReleaseAsset& a, const QStringList& extensions, const QStringList& keywords) {
        nameLower = a.name.toLower();
        bool extOk = false;
        for (const QString& ext : extensions)
            if (nameLower.endsWith(ext))
            { extOk = true; break; }
        if (!extOk)
            return false;
        for (const QString& kw : keywords)
            if (nameLower.contains(kw))
                return true;
        return keywords.isEmpty();
    };
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".dmg"), QStringLiteral(".pkg") },
                   { QStringLiteral("mac"), QStringLiteral("macos"), QStringLiteral("darwin"), QStringLiteral("osx"), QStringLiteral("apple") }))
            return i;
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".dmg"), QStringLiteral(".pkg") }, {}))
            return i;
#elif defined(Q_OS_WIN)
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".exe"), QStringLiteral(".msi") },
                   { QStringLiteral("win"), QStringLiteral("windows") }))
            return i;
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".exe"), QStringLiteral(".msi") }, {}))
            return i;
#elif defined(Q_OS_LINUX)
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".appimage") }, { QStringLiteral("linux") }))
            return i;
    for (int i = 0; i < assets.size(); ++i)
        if (matches(assets[i], { QStringLiteral(".deb"), QStringLiteral(".rpm"), QStringLiteral(".appimage") }, {}))
            return i;
#endif
    return 0;
}

QString NewVersionDialog::formatSize(qint64 bytes) const
{
    if (bytes < 1024)
        return tr("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr("%1 KB").arg(bytes / 1024);
    return tr("%1 MB").arg(bytes / (1024 * 1024));
}

void NewVersionDialog::setDownloadInProgress(bool inProgress)
{
    m_downloadButton->setEnabled(!inProgress);
    m_assetCombo->setEnabled(!inProgress);
    m_downloadStatusLabel->setVisible(inProgress);
    m_downloadStatusLabel->setText(inProgress ? tr("Downloading...") : QString());
    m_progressBar->setVisible(inProgress);
    if (!inProgress)
        m_progressBar->setRange(0, 100);
}

void NewVersionDialog::onDownloadClicked()
{
    if (m_assets.isEmpty() || m_downloadReply)
        return;

    int idx = m_assetCombo->currentIndex();
    if (idx < 0 || idx >= m_assets.size())
        return;

    const ReleaseAsset& asset = m_assets.at(idx);
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (downloadDir.isEmpty())
        downloadDir = QDir::tempPath();
    m_downloadedFilePath = QDir(downloadDir).absoluteFilePath(asset.name);

    QNetworkRequest request(QUrl(asset.downloadUrl));
    request.setRawHeader("User-Agent", "OpenDAQ-Qt-GUI-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_network->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &NewVersionDialog::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &NewVersionDialog::onDownloadFinished);

    setDownloadInProgress(true);
    m_progressBar->setRange(0, 0); // indeterminate until we get content length
}

void NewVersionDialog::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(static_cast<int>((received * 100) / total));
        m_downloadStatusLabel->setText(tr("Downloading... %1 / %2")
                                          .arg(formatSize(received))
                                          .arg(formatSize(total)));
    }
    else
    {
        m_downloadStatusLabel->setText(tr("Downloading... %1").arg(formatSize(received)));
    }
}

void NewVersionDialog::onDownloadFinished()
{
    if (!m_downloadReply)
        return;

    setDownloadInProgress(false);

    if (m_downloadReply->error() != QNetworkReply::NoError)
    {
        QMessageBox::warning(this, tr("Download failed"),
                             tr("Could not download file: %1").arg(m_downloadReply->errorString()));
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    QFile file(m_downloadedFilePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("Download failed"),
                             tr("Could not save file: %1").arg(file.errorString()));
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }
    file.write(m_downloadReply->readAll());
    file.close();

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    if (QFile::exists(m_downloadedFilePath))
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
}
