#pragma once
#include "base_tree_element.h"

// Example derived class for Device elements
class DeviceTreeElement : public BaseTreeElement
{
    Q_OBJECT

public:
    DeviceTreeElement(QTreeWidget* tree, const QString& deviceId, const QString& deviceName, QObject* parent = nullptr)
        : BaseTreeElement(tree, parent)
    {
        this->localId = deviceId;
        this->globalId = "/" + deviceId;
        this->name = deviceName;
        this->type = "Device";
        this->iconName = "device";  // Will load :/icons/device.png
    }

    // Override onSelected to show device-specific content
    void onSelected(QWidget* mainContent) override
    {
        // Clear previous content
        QLayout* layout = mainContent->layout();
        if (layout)
        {
            QLayoutItem* item;
            while ((item = layout->takeAt(0)) != nullptr)
            {
                delete item->widget();
                delete item;
            }
        }
        else
        {
            layout = new QVBoxLayout(mainContent);
            mainContent->setLayout(layout);
        }

        // Add device-specific widgets
        QLabel* titleLabel = new QLabel(QString("Device: %1").arg(name));
        titleLabel->setFont(QFont("Arial", 16, QFont::Bold));
        layout->addWidget(titleLabel);

        QLabel* idLabel = new QLabel(QString("ID: %1").arg(globalId));
        layout->addWidget(idLabel);

        QLabel* typeLabel = new QLabel(QString("Type: %1").arg(type));
        layout->addWidget(typeLabel);

        // Add stretch to push content to top
        static_cast<QVBoxLayout*>(layout)->addStretch();
    }

    // Override to add device-specific context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override
    {
        QMenu* menu = new QMenu(parent);

        QAction* refreshAction = menu->addAction(IconProvider::instance().icon("refresh"), "Refresh");
        QAction* settingsAction = menu->addAction(IconProvider::instance().icon("settings"), "Settings");
        menu->addSeparator();
        QAction* removeAction = menu->addAction("Remove Device");

        // Connect actions (you would implement these handlers)
        // connect(refreshAction, &QAction::triggered, this, &DeviceTreeElement::onRefresh);
        // connect(settingsAction, &QAction::triggered, this, &DeviceTreeElement::onSettings);
        // connect(removeAction, &QAction::triggered, this, &DeviceTreeElement::onRemove);

        return menu;
    }
};
