#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <opendaq/opendaq.h>

class PropertyObjectView;

class CallFunctionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CallFunctionDialog(const daq::PropertyObjectPtr& owner, 
                                const daq::PropertyPtr& prop,
                                QWidget* parent = nullptr);
    ~CallFunctionDialog() override = default;

private Q_SLOTS:
    void onExecuteClicked();
    void onCloseClicked();

private:
    void setupUI();
    void createArgumentsPropertyObject();
    daq::BaseObjectPtr collectArguments();
    QString formatResult(const daq::BaseObjectPtr& result);
    QString coreTypeToString(daq::CoreType type);

    daq::PropertyObjectPtr owner;
    daq::PropertyPtr prop;
    daq::CallableInfoPtr callableInfo;
    daq::PropertyObjectPtr argumentsPropertyObject;
    
    PropertyObjectView* argumentsView;
    QTextEdit* resultTextEdit;
    QPushButton* executeButton;
};

