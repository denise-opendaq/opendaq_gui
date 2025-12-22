#include "dialogs/add_device_dialog.h"
#include "dialogs/add_device_config_dialog.h"
#include <QHeaderView>
#include <QTreeWidgetItem>

AddDeviceDialog::AddDeviceDialog(const daq::DevicePtr& parentDevice, QWidget* parent)
    : QDialog(parent)
    , parentDevice(parentDevice)
    , config(nullptr)
    , contextMenu(nullptr)
    , addAction(nullptr)
    , addWithConfigAction(nullptr)
{
    setWindowTitle("Add Device");
    resize(800, 500);
    setMinimumSize(600, 400);

    setupUI();
    initAvailableDevices();
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
    connect(deviceTree, &QTreeWidget::itemDoubleClicked, this, &AddDeviceDialog::onDeviceTreeDoubleClicked);
    connect(deviceTree, &QTreeWidget::customContextMenuRequested, this, &AddDeviceDialog::onDeviceTreeContextMenu);
    
    // Create context menu for device tree
    contextMenu = new QMenu(this);
    addAction = contextMenu->addAction("Add");
    connect(addAction, &QAction::triggered, this, &AddDeviceDialog::onAddFromContextMenu);
    addWithConfigAction = contextMenu->addAction("Add with config");
    connect(addWithConfigAction, &QAction::triggered, this, &AddDeviceDialog::onAddWithConfigFromContextMenu);
    
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

void AddDeviceDialog::initAvailableDevices()
{
    deviceTree->clear();

    if (!parentDevice.assigned())
    {
        statusLabel->setText("No parent device available.");
        return;
    }

    try
    {
        auto availableDevices = parentDevice.getAvailableDevices();
        if (availableDevices.getCount() == 0)
        {
            statusLabel->setText("No devices discovered. Enter connection string manually.");
            return;
        }

        for (size_t i = 0; i < availableDevices.getCount(); ++i)
        {
            auto deviceInfo = availableDevices[i];
            if (!deviceInfo.assigned())
                continue;

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

void AddDeviceDialog::onDeviceTreeDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (item)
    {
        QString connectionString = item->data(1, Qt::UserRole).toString();
        connectionStringEdit->setText(connectionString);
        updateConnectionString();
        
        // Open config dialog on double click
        AddDeviceConfigDialog configDialog(parentDevice, connectionString, this);
        if (configDialog.exec() == QDialog::Accepted)
        {
            // Store the config
            config = configDialog.getConfig();
            // Update connection string in case it was changed in config dialog
            QString finalConnectionString = configDialog.getConnectionString();
            if (!finalConnectionString.isEmpty())
            {
                connectionStringEdit->setText(finalConnectionString);
            }
            accept();
        }
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
        {
            connectionStringEdit->setText(finalConnectionString);
        }
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
    {
        return;
    }
    
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
    {
        return;
    }
    
    // Open config dialog
    AddDeviceConfigDialog configDialog(parentDevice, connectionString, this);
    if (configDialog.exec() == QDialog::Accepted)
    {
        // Store the config
        config = configDialog.getConfig();
        // Update connection string in case it was changed in config dialog
        QString finalConnectionString = configDialog.getConnectionString();
        if (!finalConnectionString.isEmpty())
        {
            connectionStringEdit->setText(finalConnectionString);
        }
        accept();
    }
}

QString AddDeviceDialog::getConnectionString() const
{
    return connectionStringEdit->text().trimmed();
}

