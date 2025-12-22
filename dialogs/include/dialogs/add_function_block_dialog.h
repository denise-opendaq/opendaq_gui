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
    void onAddClicked();

private:
    void setupUI();
    void initAvailableFunctionBlocks();
    void updateConfigView();

    daq::ComponentPtr parentComponent;
    daq::PropertyObjectPtr config;
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> availableTypes;
    QString selectedFunctionBlockType;

    // UI widgets
    QTreeWidget* functionBlockList;
    PropertyObjectView* configView;
    QPushButton* addButton;
};

