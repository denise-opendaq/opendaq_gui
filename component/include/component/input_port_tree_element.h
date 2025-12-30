#pragma once
#include "component_tree_element.h"
#include <opendaq/input_port_ptr.h>

class InputPortTreeElement : public ComponentTreeElement
{
    Q_OBJECT
    using Super = ComponentTreeElement;

public:
    InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QObject* parent = nullptr);

    void onSelected(QWidget* mainContent) override;
    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName, QWidget* mainContent) override;

    daq::InputPortPtr getInputPort() const;
};
