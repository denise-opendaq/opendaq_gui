#include "dialogs/add_server_dialog.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/gui_constants.h"
#include <QSplitter>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

AddServerDialog::AddServerDialog(QWidget* parentWidget)
    : QDialog(parentWidget)
    , config(nullptr)
    , availableTypes(nullptr)
    , serverList(nullptr)
    , configView(nullptr)
    , addButton(nullptr)
{
    setWindowTitle("Add Server");
    resize(GUIConstants::ADD_SERVER_DIALOG_WIDTH, GUIConstants::ADD_SERVER_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_SERVER_DIALOG_MIN_WIDTH, GUIConstants::ADD_SERVER_DIALOG_MIN_HEIGHT);

    setupUI();
    initAvailableServers();
}

void AddServerDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create horizontal splitter for left and right panels
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    
    // === LEFT PANEL ===
    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(5);

    // Servers list label
    auto* listLabel = new QLabel("Available Servers:", this);
    listLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(listLabel);
    
    // Servers tree (with Name and Description columns)
    serverList = new QTreeWidget(this);
    serverList->setHeaderLabels(QStringList() << "Name" << "Description");
    serverList->setRootIsDecorated(false);
    serverList->setAlternatingRowColors(true);
    serverList->setSelectionMode(QAbstractItemView::SingleSelection);
    serverList->header()->setStretchLastSection(true);
    serverList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    serverList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(serverList, &QTreeWidget::itemSelectionChanged, this, &AddServerDialog::onServerSelected);
    connect(serverList, &QTreeWidget::itemDoubleClicked, this, &AddServerDialog::onServerDoubleClicked);
    leftLayout->addWidget(serverList);

    leftPanel->setLayout(leftLayout);
    splitter->addWidget(leftPanel);

    // === RIGHT PANEL ===
    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(5, 5, 5, 5);
    rightLayout->setSpacing(5);

    // Config label
    auto* configLabel = new QLabel("Configuration:", this);
    configLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(configLabel);

    // Config view (will be created when server is selected)
    // Create empty PropertyObject for initial state
    auto emptyConfig = daq::PropertyObject();
    configView = new PropertyObjectView(emptyConfig, this);
    rightLayout->addWidget(configView);

    rightPanel->setLayout(rightLayout);
    splitter->addWidget(rightPanel);

    // Set splitter proportions (left: 1, right: 2)
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({350, 650});

    mainLayout->addWidget(splitter);

    // Button layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(5, 0, 5, 0);
    buttonLayout->addStretch();
    
    addButton = new QPushButton("Add Server", this);
    addButton->setDefault(true);
    addButton->setEnabled(false);
    connect(addButton, &QPushButton::clicked, this, &AddServerDialog::onAddClicked);
    buttonLayout->addWidget(addButton);
    
    mainLayout->addLayout(buttonLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
}

void AddServerDialog::initAvailableServers()
{
    // Clear config view
    auto* parentWidget = configView->parentWidget();
    auto* parentLayout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
    if (parentLayout)
    {
        parentLayout->removeWidget(configView);
        delete configView;
        auto emptyConfig = daq::PropertyObject();
        configView = new PropertyObjectView(emptyConfig, parentWidget);
        parentLayout->insertWidget(1, configView);
    }

    try
    {
        // Get instance from AppContext
        auto instance = AppContext::instance()->daqInstance();
        if (!instance.assigned())
        {
            const auto loggerComponent = AppContext::getLoggerComponent();
            LOG_W("No openDAQ instance available");
            return;
        }

        // Get available server types from instance
        availableTypes = instance.getAvailableServerTypes();

        if (!availableTypes.assigned() || availableTypes.getCount() == 0)
            return;

        // Populate list with available server types
        for (const auto& [id, type] : availableTypes)
        {
            QString typeId = QString::fromStdString(id.toStdString());
            QString typeName = QString::fromStdString(type.getName().toStdString());
            QString description = QString::fromStdString(type.getDescription().toStdString());

            auto* item = new QTreeWidgetItem(serverList);
            item->setText(0, typeName);
            item->setText(1, description);
            item->setData(0, Qt::UserRole, typeId);
            item->setToolTip(0, description);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error getting available servers: {}", e.what());
    }
}

void AddServerDialog::onServerSelected()
{
    auto* selectedItem = serverList->currentItem();
    if (selectedItem)
        selectedServerType = selectedItem->data(0, Qt::UserRole).toString();
    else
        selectedServerType.clear();

    // Create and display config
    updateConfigView();
}

void AddServerDialog::onServerDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;

    // Select the item first
    serverList->setCurrentItem(item);
    
    // Call onServerSelected to update the selection and config
    onServerSelected();
    
    // Accept the dialog if a server type was selected
    if (!selectedServerType.isEmpty())
        accept();
}

void AddServerDialog::updateConfigView()
{
    try
    {
        if (selectedServerType.isEmpty())
        {
            config = daq::PropertyObject();
            addButton->setEnabled(false);
        }
        else
        {
            daq::ServerTypePtr serverType = availableTypes[selectedServerType.toStdString()];
            config = serverType.createDefaultConfig();
            if (!config.assigned())
                config = daq::PropertyObject();
            addButton->setEnabled(true);
        }

        auto* parentWidget = configView->parentWidget();
        auto* parentLayout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
        if (parentLayout)
        {
            parentLayout->removeWidget(configView);
            delete configView;
            configView = new PropertyObjectView(config, parentWidget);
            parentLayout->insertWidget(1, configView);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error creating configuration: {}", e.what());
        addButton->setEnabled(false);
    }
}

void AddServerDialog::onAddClicked()
{
    if (selectedServerType.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please select a server type.");
        return;
    }

    accept();
}

