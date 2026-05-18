#pragma once

#include <QString>

#include "widgets/component_widget.h"
#include <opendaq/function_block_ptr.h>

class ComponentTreeWidget;
class InputPortFolderSelector;
class QCheckBox;
class QHBoxLayout;
class QVBoxLayout;

class FunctionBlockWidget : public ComponentWidget
{
    Q_OBJECT

public:
    explicit FunctionBlockWidget(const daq::FunctionBlockPtr& functionBlock,
                                 ComponentTreeWidget* componentTree,
                                 QWidget* parent = nullptr,
                                 const QString& treeIconName = {});
    ~FunctionBlockWidget() override;

private:
    void populateTabs() override;
    void setupRecorderControls(QWidget* page, QVBoxLayout* layout);

private Q_SLOTS:
    void onRecordingToggled(bool checked);
    void updateRecordingStatus();

private:
    daq::FunctionBlockPtr functionBlock;
    ComponentTreeWidget* componentTree = nullptr;
    InputPortFolderSelector* inputPortFolderSelector = nullptr;

    QCheckBox* recordingToggle = nullptr;
    QHBoxLayout* recorderLayout = nullptr;
};
