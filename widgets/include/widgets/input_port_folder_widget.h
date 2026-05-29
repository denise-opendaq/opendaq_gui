#pragma once

#include <QString>

#include "widgets/component_widget.h"
#include <opendaq/folder_ptr.h>

class ComponentTreeWidget;

// ComponentWidget for an Input Ports folder: in addition to the regular
// "Attributes" tab, exposes a "Signal Selector" tab that lets the user wire
// signals into the contained input ports.
class InputPortFolderWidget : public ComponentWidget
{
    Q_OBJECT

public:
    InputPortFolderWidget(const daq::FolderPtr& folder,
                          ComponentTreeWidget* componentTree,
                          QWidget* parent = nullptr,
                          const QString& treeIconName = {});
    ~InputPortFolderWidget() override;

private:
    QStringList availableTabs() const override;
    QWidget* openTab(const QString& title, QWidget* parent) override;

    daq::FolderPtr folder;
    ComponentTreeWidget* componentTree = nullptr;
};
