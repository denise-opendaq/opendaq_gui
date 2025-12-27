#include "dialogs/add_function_block_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include <QSplitter>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <opendaq/custom_log.h>
#include "context/AppContext.h"
#include <opendaq/logger_component_ptr.h>

AddFunctionBlockDialog::AddFunctionBlockDialog(const daq::ComponentPtr& parent, QWidget* parentWidget)
    : QDialog(parentWidget)
    , parentComponent(parent)
    , config(nullptr)
    , availableTypes(nullptr)
    , functionBlockList(nullptr)
    , configView(nullptr)
    , addButton(nullptr)
{
    setWindowTitle("Add Function Block");
    resize(GUIConstants::ADD_FUNCTION_BLOCK_DIALOG_WIDTH, GUIConstants::ADD_FUNCTION_BLOCK_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_FUNCTION_BLOCK_DIALOG_MIN_WIDTH, GUIConstants::ADD_FUNCTION_BLOCK_DIALOG_MIN_HEIGHT);

    setupUI();
    initAvailableFunctionBlocks();
}

void AddFunctionBlockDialog::setupUI()
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

    // Function blocks list label
    auto* listLabel = new QLabel("Available Function Blocks:", this);
    listLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(listLabel);
    
    // Function blocks tree (with Name and Description columns)
    functionBlockList = new QTreeWidget(this);
    functionBlockList->setHeaderLabels(QStringList() << "Name" << "Description");
    functionBlockList->setRootIsDecorated(false);
    functionBlockList->setAlternatingRowColors(true);
    functionBlockList->setSelectionMode(QAbstractItemView::SingleSelection);
    functionBlockList->header()->setStretchLastSection(true);
    functionBlockList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    functionBlockList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(functionBlockList, &QTreeWidget::itemSelectionChanged, this, &AddFunctionBlockDialog::onFunctionBlockSelected);
    connect(functionBlockList, &QTreeWidget::itemDoubleClicked, this, &AddFunctionBlockDialog::onFunctionBlockDoubleClicked);
    leftLayout->addWidget(functionBlockList);

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

    // Config view (will be created when function block is selected)
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
    
    addButton = new QPushButton("Add Function Block", this);
    addButton->setDefault(true);
    addButton->setEnabled(false);
    connect(addButton, &QPushButton::clicked, this, &AddFunctionBlockDialog::onAddClicked);
    buttonLayout->addWidget(addButton);
    
    mainLayout->addLayout(buttonLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
}

void AddFunctionBlockDialog::initAvailableFunctionBlocks()
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
        // Try to get available function block types from parent
        // Both IDevice and IFunctionBlock have getAvailableFunctionBlockTypes()
        if (const auto parentDevice = parentComponent.asPtrOrNull<daq::IDevice>(true); parentDevice.assigned())
            availableTypes = parentDevice.getAvailableFunctionBlockTypes();
        else if (const auto parentFb = parentComponent.asPtrOrNull<daq::IFunctionBlock>(true); parentFb.assigned())
            availableTypes = parentFb.getAvailableFunctionBlockTypes();

        if (!availableTypes.assigned() ||availableTypes.getCount() == 0)
            return;

        // Populate list with available function block types
        for (const auto& [id, type] : availableTypes)
        {
            QString typeId = QString::fromStdString(id.toStdString());
            QString typeName = QString::fromStdString(type.getName().toStdString());
            QString description = QString::fromStdString(type.getDescription().toStdString());

            auto* item = new QTreeWidgetItem(functionBlockList);
            item->setText(0, typeName);
            item->setText(1, description);
            item->setData(0, Qt::UserRole, typeId);
            item->setToolTip(0, description);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error getting available function blocks: {}", e.what());
    }
}

void AddFunctionBlockDialog::onFunctionBlockSelected()
{
    auto* selectedItem = functionBlockList->currentItem();
    if (selectedItem)
        selectedFunctionBlockType = selectedItem->data(0, Qt::UserRole).toString();
    else
        selectedFunctionBlockType.clear();

    // Create and display config
    updateConfigView();
}

void AddFunctionBlockDialog::onFunctionBlockDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;

    // Select the item first
    functionBlockList->setCurrentItem(item);
    
    // Call onFunctionBlockSelected to update the selection and config
    onFunctionBlockSelected();
    
    // Accept the dialog if a function block type was selected
    if (!selectedFunctionBlockType.isEmpty())
        accept();
}

void AddFunctionBlockDialog::updateConfigView()
{
    try
    {
        if (selectedFunctionBlockType.isEmpty())
        {
            config = daq::PropertyObject();
            addButton->setEnabled(false);
        }
        else
        {
            daq::FunctionBlockTypePtr functionBlockType = availableTypes[selectedFunctionBlockType.toStdString()];
            config = functionBlockType.createDefaultConfig();
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

void AddFunctionBlockDialog::onAddClicked()
{
    if (selectedFunctionBlockType.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please select a function block type.");
        return;
    }

    accept();
}

