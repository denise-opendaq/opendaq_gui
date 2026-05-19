#include "dialogs/add_function_block_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include <QSplitter>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <opendaq/custom_log.h>
#include "context/AppContext.h"
#include <opendaq/logger_component_ptr.h>

namespace
{
constexpr int kDescriptionRole = Qt::UserRole + 1;
} // namespace

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
    
    // === LEFT PANEL (styled like component tree view) ===
    auto* leftPanel = new QWidget(this);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setAttribute(Qt::WA_StyledBackground, true);
    leftPanel->setStyleSheet(GUIStyles::sidebarPanelStyle("leftPanel"));
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    searchBox = new QLineEdit(leftPanel);
    searchBox->setPlaceholderText(tr("Search function blocks..."));
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(GUIStyles::searchLineEditStyle());
    connect(searchBox, &QLineEdit::textChanged, this, &AddFunctionBlockDialog::onSearchFilterChanged);
    leftLayout->addWidget(searchBox);

    functionBlockList = new QTreeWidget(leftPanel);
    functionBlockList->setHeaderLabel(tr("Function Blocks"));
    functionBlockList->setRootIsDecorated(false);
    functionBlockList->setAlternatingRowColors(false);
    functionBlockList->setSelectionBehavior(QAbstractItemView::SelectRows);
    functionBlockList->setAllColumnsShowFocus(false);
    functionBlockList->setSelectionMode(QAbstractItemView::SingleSelection);
    functionBlockList->setAttribute(Qt::WA_StyledBackground, true);
    functionBlockList->setStyleSheet(GUIStyles::sidebarTreeStyle());
    connect(functionBlockList, &QTreeWidget::itemSelectionChanged, this, &AddFunctionBlockDialog::onFunctionBlockSelected);
    connect(functionBlockList, &QTreeWidget::itemDoubleClicked, this, &AddFunctionBlockDialog::onFunctionBlockDoubleClicked);
    leftLayout->addWidget(functionBlockList);

    splitter->addWidget(leftPanel);

    // === RIGHT PANEL ===
    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN);
    rightLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    auto* headerBlock = new QWidget(rightPanel);
    auto* headerLayout = new QVBoxLayout(headerBlock);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    nameLabel = new QLabel(headerBlock);
    nameLabel->setStyleSheet(GUIStyles::HEADER_TITLE_LABEL);
    nameLabel->hide();
    headerLayout->addWidget(nameLabel);

    descriptionLabel = new QLabel(headerBlock);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    descriptionLabel->setStyleSheet(GUIStyles::DESCRIPTION_LABEL);
    descriptionLabel->hide();
    headerLayout->addWidget(descriptionLabel);

    rightLayout->addWidget(headerBlock);

    configContainer = new QWidget(rightPanel);
    auto* configLayout = new QVBoxLayout(configContainer);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(0);

    auto emptyConfig = daq::PropertyObject();
    configView = new PropertyObjectView(emptyConfig, configContainer);
    configLayout->addWidget(configView, 1);
    rightLayout->addWidget(configContainer, 1);

    auto* buttonRowLayout = new QHBoxLayout();
    buttonRowLayout->addStretch();

    addButton = new QPushButton(tr("Add Function Block"), rightPanel);
    addButton->setDefault(true);
    addButton->setEnabled(false);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setMinimumHeight(44);
    addButton->setStyleSheet(GUIStyles::primaryButtonStyle());
    connect(addButton, &QPushButton::clicked, this, &AddFunctionBlockDialog::onAddClicked);
    buttonRowLayout->addWidget(addButton);
    rightLayout->addLayout(buttonRowLayout);

    rightPanel->setLayout(rightLayout);
    splitter->addWidget(rightPanel);

    // Set splitter proportions (left: 1, right: 2)
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({350, 650});

    mainLayout->addWidget(splitter);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
}

void AddFunctionBlockDialog::initAvailableFunctionBlocks()
{
    functionBlockList->clear();
    selectedFunctionBlockType.clear();
    updateSelectionHeader();

    replaceConfigView(daq::PropertyObject());
    addButton->setEnabled(false);

    try
    {
        // Try to get available function block types from parent
        // Both IDevice and IFunctionBlock have getAvailableFunctionBlockTypes()
        if (const auto parentDevice = parentComponent.asPtrOrNull<daq::IDevice>(true); parentDevice.assigned())
            availableTypes = parentDevice.getAvailableFunctionBlockTypes();
        else if (const auto parentFb = parentComponent.asPtrOrNull<daq::IFunctionBlock>(true); parentFb.assigned())
            availableTypes = parentFb.getAvailableFunctionBlockTypes();

        if (!availableTypes.assigned() || availableTypes.getCount() == 0)
            return;

        // Populate list with available function block types
        for (const auto& [id, type] : availableTypes)
        {
            QString typeId = QString::fromStdString(id.toStdString());
            QString typeName = QString::fromStdString(type.getName().toStdString());
            QString description = QString::fromStdString(type.getDescription().toStdString());

            auto* item = new QTreeWidgetItem(functionBlockList);
            item->setText(0, typeName);
            item->setData(0, Qt::UserRole, typeId);
            item->setData(0, kDescriptionRole, description);
            item->setToolTip(0, description);
        }

        applySearchFilter(searchBox ? searchBox->text() : QString());
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error getting available function blocks: {}", e.what());
    }
}

void AddFunctionBlockDialog::onSearchFilterChanged(const QString& text)
{
    applySearchFilter(text);
}

void AddFunctionBlockDialog::applySearchFilter(const QString& filter)
{
    if (!functionBlockList)
        return;

    if (filter.isEmpty())
    {
        for (int i = 0; i < functionBlockList->topLevelItemCount(); ++i)
            functionBlockList->topLevelItem(i)->setHidden(false);
        return;
    }

    for (int i = 0; i < functionBlockList->topLevelItemCount(); ++i)
    {
        auto* item = functionBlockList->topLevelItem(i);
        const QString name = item->text(0);
        const QString description = item->data(0, kDescriptionRole).toString();
        const bool matches = name.contains(filter, Qt::CaseInsensitive)
            || description.contains(filter, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }

    if (functionBlockList->currentItem() && functionBlockList->currentItem()->isHidden())
        functionBlockList->clearSelection();
}

void AddFunctionBlockDialog::updateSelectionHeader()
{
    auto* selectedItem = functionBlockList ? functionBlockList->currentItem() : nullptr;
    const QString name = selectedItem ? selectedItem->text(0) : QString();
    const QString description = selectedItem
        ? selectedItem->data(0, kDescriptionRole).toString()
        : QString();

    if (nameLabel)
    {
        if (name.isEmpty())
        {
            nameLabel->clear();
            nameLabel->hide();
        }
        else
        {
            nameLabel->setText(name);
            nameLabel->show();
        }
    }

    if (descriptionLabel)
    {
        if (description.isEmpty())
        {
            descriptionLabel->clear();
            descriptionLabel->hide();
        }
        else
        {
            descriptionLabel->setText(description);
            descriptionLabel->show();
        }
    }
}

void AddFunctionBlockDialog::onFunctionBlockSelected()
{
    auto* selectedItem = functionBlockList->currentItem();
    if (selectedItem)
        selectedFunctionBlockType = selectedItem->data(0, Qt::UserRole).toString();
    else
        selectedFunctionBlockType.clear();

    updateSelectionHeader();
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

void AddFunctionBlockDialog::replaceConfigView(const daq::PropertyObjectPtr& newConfig)
{
    if (!configContainer || !configView)
        return;

    auto* layout = qobject_cast<QVBoxLayout*>(configContainer->layout());
    if (!layout)
        return;

    layout->removeWidget(configView);
    delete configView;
    configView = new PropertyObjectView(newConfig, configContainer);
    layout->addWidget(configView, 1);
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

        replaceConfigView(config);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
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

