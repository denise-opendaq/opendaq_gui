#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <opendaq/opendaq.h>

class AddDeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDeviceDialog(const daq::DevicePtr& parentDevice, QWidget* parent = nullptr);
    ~AddDeviceDialog() override = default;

    QString getConnectionString() const;
    daq::PropertyObjectPtr getConfig() const { return config; }

private Q_SLOTS:
    void onDeviceSelected();
    void onAddClicked();
    void onConnectionStringChanged();
    void onDeviceTreeDoubleClicked(QTreeWidgetItem* item, int column);
    void onDeviceTreeContextMenu(const QPoint& pos);
    void onAddFromContextMenu();
    void onAddWithConfigFromContextMenu();

private:
    void setupUI();
    void updateAvailableDevices();
    void updateConnectionString();

    daq::DevicePtr parentDevice;
    daq::PropertyObjectPtr config;

    QLineEdit* connectionStringEdit;
    QTreeWidget* deviceTree;
    QPushButton* addButton;
    QLabel* statusLabel;
    QMenu* contextMenu;
    QAction* addAction;
    QAction* addWithConfigAction;
};

