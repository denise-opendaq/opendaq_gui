#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMessageBox>
#include <opendaq/opendaq.h>

class PropertyObjectView;

class AddFunctionBlockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFunctionBlockDialog(const daq::ComponentPtr& parent, QWidget* parentWidget = nullptr);
    ~AddFunctionBlockDialog() override = default;

    QString getFunctionBlockType() const { return selectedFunctionBlockType; }
    daq::PropertyObjectPtr getConfig() const { return config; }

private Q_SLOTS:
    void onFunctionBlockSelected();
    void onFunctionBlockDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddClicked();
    void onSearchFilterChanged(const QString& text);

private:
    void setupUI();
    void initAvailableFunctionBlocks();
    void updateConfigView();
    void replaceConfigView(const daq::PropertyObjectPtr& newConfig);
    void updateSelectionHeader();
    void applySearchFilter(const QString& filter);

    daq::ComponentPtr parentComponent;
    daq::PropertyObjectPtr config;
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> availableTypes;
    QString selectedFunctionBlockType;

    // UI widgets
    QLineEdit* searchBox = nullptr;
    QTreeWidget* functionBlockList = nullptr;
    QLabel* nameLabel = nullptr;
    QLabel* descriptionLabel = nullptr;
    QWidget* configContainer = nullptr;
    PropertyObjectView* configView = nullptr;
    QPushButton* addButton = nullptr;
};

