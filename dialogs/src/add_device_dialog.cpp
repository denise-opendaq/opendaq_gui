#include "dialogs/add_device_dialog.h"
#include "dialogs/add_device_config_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QShortcut>
#include <QKeySequence>

AddDeviceDialog::AddDeviceDialog(const daq::DevicePtr& parentDevice, QWidget* parent)
    : QDialog(parent)
    , parentDevice(parentDevice)
    , config(nullptr)
    , contextMenu(nullptr)
    , addAction(nullptr)
    , addWithConfigAction(nullptr)
{
    setWindowTitle("Add Device");
    resize(GUIConstants::ADD_DEVICE_DIALOG_WIDTH, GUIConstants::ADD_DEVICE_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_DEVICE_DIALOG_MIN_WIDTH, GUIConstants::ADD_DEVICE_DIALOG_MIN_HEIGHT);

    setupUI();
    updateAvailableDevices();
}

void AddDeviceDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Connection string input
    auto* connectionLayout = new QHBoxLayout();
    connectionLayout->addWidget(new QLabel("Connection string:"));
    
    connectionStringEdit = new QLineEdit(this);
    connectionStringEdit->setPlaceholderText("e.g., daq.nd://127.0.0.1");
    connect(connectionStringEdit, &QLineEdit::textChanged, this, &AddDeviceDialog::onConnectionStringChanged);
    connect(connectionStringEdit, &QLineEdit::returnPressed, this, &AddDeviceDialog::onAddClicked);
    connectionLayout->addWidget(connectionStringEdit);
    
    mainLayout->addLayout(connectionLayout);

    // Available devices tree
    auto* treeLabel = new QLabel("Available devices:", this);
    mainLayout->addWidget(treeLabel);

    deviceTree = new QTreeWidget(this);
    deviceTree->setHeaderLabels(QStringList() << "Name" << "Connection String");
    deviceTree->setRootIsDecorated(false);
    deviceTree->setAlternatingRowColors(true);
    deviceTree->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTree->setContextMenuPolicy(Qt::CustomContextMenu);
    deviceTree->header()->setStretchLastSection(true);
    deviceTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    
    connect(deviceTree, &QTreeWidget::itemSelectionChanged, this, &AddDeviceDialog::onDeviceSelected);
    connect(deviceTree, &QTreeWidget::itemDoubleClicked, this, &AddDeviceDialog::onAddFromContextMenu);
    connect(deviceTree, &QTreeWidget::customContextMenuRequested, this, &AddDeviceDialog::onDeviceTreeContextMenu);
    
    // Create context menu for device tree
    contextMenu = new QMenu(this);
    addAction = contextMenu->addAction("Add");
    connect(addAction, &QAction::triggered, this, &AddDeviceDialog::onAddFromContextMenu);
    addWithConfigAction = contextMenu->addAction("Add with config");
    connect(addWithConfigAction, &QAction::triggered, this, &AddDeviceDialog::onAddWithConfigFromContextMenu);
    contextMenu->addSeparator();
    auto* showDeviceInfoAction = contextMenu->addAction("Show Device Info");
    connect(showDeviceInfoAction, &QAction::triggered, this, &AddDeviceDialog::onShowDeviceInfo);
    
    // Add F5 shortcut for refresh
    auto* refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(refreshShortcut, &QShortcut::activated, this, &AddDeviceDialog::updateAvailableDevices);
    
    mainLayout->addWidget(deviceTree);

    // Status label
    statusLabel = new QLabel("", this);
    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    addButton = new QPushButton("Add Device", this);
    addButton->setDefault(true);
    addButton->setEnabled(false);
    connect(addButton, &QPushButton::clicked, this, &AddDeviceDialog::onAddClicked);
    buttonLayout->addWidget(addButton);
    
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void AddDeviceDialog::updateAvailableDevices()
{
    deviceTree->clear();

    if (!parentDevice.assigned())
    {
        statusLabel->setText("No parent device available.");
        return;
    }

    try
    {
        availableDevices = parentDevice.getAvailableDevices();
        if (availableDevices.getCount() == 0)
        {
            statusLabel->setText("No devices discovered. Enter connection string manually.");
            return;
        }

        for (const auto & deviceInfo : availableDevices)
        {
            QString name = QString::fromStdString(deviceInfo.getName().toStdString());
            QString connectionString = QString::fromStdString(deviceInfo.getConnectionString().toStdString());

            auto* item = new QTreeWidgetItem(deviceTree);
            item->setText(0, name);
            item->setText(1, connectionString);
            item->setData(1, Qt::UserRole, connectionString);
        }

        statusLabel->setText(QString("Found %1 device(s). Select one or enter connection string manually.").arg(availableDevices.getCount()));
    }
    catch (const std::exception& e)
    {
        statusLabel->setText(QString("Error discovering devices: %1").arg(e.what()));
    }
}

void AddDeviceDialog::onDeviceSelected()
{
    auto* selectedItem = deviceTree->currentItem();
    if (selectedItem)
    {
        QString connectionString = selectedItem->data(1, Qt::UserRole).toString();
        connectionStringEdit->setText(connectionString);
        updateConnectionString();
    }
}

void AddDeviceDialog::onConnectionStringChanged()
{
    updateConnectionString();
}

void AddDeviceDialog::updateConnectionString()
{
    QString connectionString = connectionStringEdit->text().trimmed();
    
    if (connectionString.isEmpty())
    {
        addButton->setEnabled(false);
        statusLabel->setText("Enter a connection string to add a device.");
        return;
    }

    // Basic validation: check if it looks like a connection string
    if (!connectionString.contains("://"))
    {
        addButton->setEnabled(false);
        statusLabel->setText("Invalid connection string format. Expected format: protocol://address");
        return;
    }

    addButton->setEnabled(true);
    statusLabel->setText(QString("Ready to add device: %1").arg(connectionString));
}

void AddDeviceDialog::onAddClicked()
{
    QString connectionString = connectionStringEdit->text().trimmed();
    
    if (connectionString.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a connection string.");
        return;
    }

    if (!connectionString.contains("://"))
    {
        QMessageBox::warning(this, "Error", "Invalid connection string format. Expected format: protocol://address");
        return;
    }

    // Open config dialog by default
    AddDeviceConfigDialog configDialog(parentDevice, connectionString, this);
    if (configDialog.exec() == QDialog::Accepted)
    {
        // Store the config
        config = configDialog.getConfig();
        // Update connection string in case it was changed in config dialog
        QString finalConnectionString = configDialog.getConnectionString();
        if (!finalConnectionString.isEmpty())
            connectionStringEdit->setText(finalConnectionString);
        
        accept();
    }
}

void AddDeviceDialog::onDeviceTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = deviceTree->itemAt(pos);
    if (item)
    {
        // Select the item first
        deviceTree->setCurrentItem(item);
        QString connectionString = item->data(1, Qt::UserRole).toString();
        connectionStringEdit->setText(connectionString);
        updateConnectionString();
        
        // Show context menu
        contextMenu->exec(deviceTree->mapToGlobal(pos));
    }
}

void AddDeviceDialog::onAddFromContextMenu()
{
    // Get connection string from selected item
    auto* selectedItem = deviceTree->currentItem();
    QString connectionString;
    if (selectedItem)
    {
        connectionString = selectedItem->data(1, Qt::UserRole).toString();
        connectionStringEdit->setText(connectionString);
        updateConnectionString();
    }
    else
    {
        connectionString = connectionStringEdit->text().trimmed();
    }
    
    if (connectionString.isEmpty() || !connectionString.contains("://"))
        return;
    
    // Add without config (config remains nullptr)
    accept();
}

void AddDeviceDialog::onAddWithConfigFromContextMenu()
{
    // Get connection string from selected item
    auto* selectedItem = deviceTree->currentItem();
    QString connectionString;
    if (selectedItem)
    {
        connectionString = selectedItem->data(1, Qt::UserRole).toString();
        connectionStringEdit->setText(connectionString);
        updateConnectionString();
    }
    else
    {
        connectionString = connectionStringEdit->text().trimmed();
    }
    
    if (connectionString.isEmpty() || !connectionString.contains("://"))
        return;
    
    // Open config dialog
    AddDeviceConfigDialog configDialog(parentDevice, connectionString, this);
    if (configDialog.exec() == QDialog::Accepted)
    {
        // Store the config
        config = configDialog.getConfig();
        // Update connection string in case it was changed in config dialog
        QString finalConnectionString = configDialog.getConnectionString();
        if (!finalConnectionString.isEmpty())
            connectionStringEdit->setText(finalConnectionString);

        accept();
    }
}

QString AddDeviceDialog::getConnectionString() const
{
    return connectionStringEdit->text().trimmed();
}

void AddDeviceDialog::onShowDeviceInfo()
{
    auto* selectedItem = deviceTree->currentItem();
    if (!selectedItem)
        return;

    // Get connection string from selected item
    daq::StringPtr connectionString = selectedItem->data(1, Qt::UserRole).toString().toStdString();
    if (!connectionString.getLength())
        return;

    // Find the deviceInfo by connection string
    daq::DeviceInfoPtr info;
    try
    {
        for (const auto & deviceInfo : availableDevices)
        {
            if (deviceInfo.getConnectionString() == connectionString)
            {
                info = deviceInfo;
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(this, "Error", QString("Failed to get device info: %1").arg(e.what()));
        return;
    }

    if (!info.assigned())
    {
        QMessageBox::warning(this, "Error", "Device info not found.");
        return;
    }

    // Create dialog window to show device info
    // Use stack-based dialog with parent for automatic cleanup
    QDialog infoDialog(this);
    infoDialog.setWindowTitle("Device Info");
    infoDialog.resize(GUIConstants::ADD_DEVICE_INFO_DIALOG_WIDTH, GUIConstants::ADD_DEVICE_INFO_DIALOG_HEIGHT);
    infoDialog.setMinimumSize(GUIConstants::ADD_DEVICE_DIALOG_MIN_WIDTH, GUIConstants::ADD_DEVICE_DIALOG_MIN_HEIGHT);

    auto* layout = new QVBoxLayout(&infoDialog);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create PropertyObjectView with deviceInfo
    // DeviceInfoPtr can be used directly as PropertyObjectPtr
    auto* propertyView = new PropertyObjectView(info, &infoDialog);
    layout->addWidget(propertyView);

    // Add close button
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &infoDialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &infoDialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    infoDialog.exec();
}

