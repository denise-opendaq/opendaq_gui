#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMessageBox>
#include <opendaq/opendaq.h>

class PropertyObjectView;

class AddServerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddServerDialog(QWidget* parentWidget = nullptr);
    ~AddServerDialog() override = default;

    QString getServerType() const { return selectedServerType; }
    daq::PropertyObjectPtr getConfig() const { return config; }

private Q_SLOTS:
    void onServerSelected();
    void onServerDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddClicked();

private:
    void setupUI();
    void initAvailableServers();
    void updateConfigView();

    daq::PropertyObjectPtr config;
    daq::DictPtr<daq::IString, daq::IServerType> availableTypes;
    QString selectedServerType;

    // UI widgets
    QTreeWidget* serverList;
    PropertyObjectView* configView;
    QPushButton* addButton;
};

