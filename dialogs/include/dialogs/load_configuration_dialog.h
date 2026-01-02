#pragma once

#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <opendaq/opendaq.h>

class PropertyObjectView;

class LoadConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadConfigurationDialog(QWidget* parent = nullptr);
    ~LoadConfigurationDialog() override = default;

    daq::UpdateParametersPtr getUpdateParameters() const { return updateParameters; }
    QString getConfigurationFilePath() const { return configFilePath; }

private Q_SLOTS:
    void onBrowseClicked();
    void onLoadClicked();

private:
    void setupUI();
    void updateLoadButton();

    daq::UpdateParametersPtr updateParameters;
    QString configFilePath;

    QLabel* filePathLabel;
    QPushButton* browseButton;
    QPushButton* loadButton;
    PropertyObjectView* propertyView;
};
