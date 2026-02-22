#pragma once

#include <QDialog>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QString>
#include <QList>

#include "context/release_asset.h"

class NewVersionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewVersionDialog(const QString& version,
                              const QString& changelog,
                              const QString& releaseUrl,
                              const QList<ReleaseAsset>& assets,
                              QWidget* parent = nullptr);
    ~NewVersionDialog() override;

private Q_SLOTS:
    void onDownloadClicked();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    void setupUI();
    QString formatSize(qint64 bytes) const;
    void setDownloadInProgress(bool inProgress);
    // Returns index of the asset best suited for the current platform, or 0 if none matched.
    static int recommendedAssetIndex(const QList<ReleaseAsset>& assets);

    QString m_releaseUrl;
    QList<ReleaseAsset> m_assets;
    QLabel* m_versionLabel = nullptr;
    QTextEdit* m_changelogEdit = nullptr;
    QComboBox* m_assetCombo = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_downloadStatusLabel = nullptr;
    QString m_downloadedFilePath;
    class QNetworkAccessManager* m_network = nullptr;
    class QNetworkReply* m_downloadReply = nullptr;
};
