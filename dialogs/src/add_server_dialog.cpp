#include "dialogs/add_server_dialog.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include <QSplitter>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

namespace
{
constexpr int kDescriptionRole = Qt::UserRole + 1;
} // namespace

AddServerDialog::AddServerDialog(QWidget* parentWidget)
    : QDialog(parentWidget)
    , config(nullptr)
    , availableTypes(nullptr)
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
    searchBox->setPlaceholderText(tr("Search servers..."));
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(GUIStyles::searchLineEditStyle());
    connect(searchBox, &QLineEdit::textChanged, this, &AddServerDialog::onSearchFilterChanged);
    leftLayout->addWidget(searchBox);

    serverList = new QTreeWidget(leftPanel);
    serverList->setHeaderLabel(tr("Servers"));
    serverList->setRootIsDecorated(false);
    serverList->setAlternatingRowColors(false);
    serverList->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverList->setAllColumnsShowFocus(false);
    serverList->setSelectionMode(QAbstractItemView::SingleSelection);
    serverList->setAttribute(Qt::WA_StyledBackground, true);
    serverList->setStyleSheet(GUIStyles::sidebarTreeStyle());
    serverList->viewport()->setAttribute(Qt::WA_Hover);
    connect(serverList, &QTreeWidget::itemSelectionChanged, this, &AddServerDialog::onServerSelected);
    connect(serverList, &QTreeWidget::itemDoubleClicked, this, &AddServerDialog::onServerDoubleClicked);
    leftLayout->addWidget(serverList);

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

    addButton = new QPushButton(tr("Add Server"), rightPanel);
    addButton->setDefault(true);
    addButton->setEnabled(false);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setMinimumHeight(44);
    addButton->setStyleSheet(GUIStyles::primaryButtonStyle());
    connect(addButton, &QPushButton::clicked, this, &AddServerDialog::onAddClicked);
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

void AddServerDialog::initAvailableServers()
{
    serverList->clear();
    selectedServerType.clear();
    updateSelectionHeader();

    replaceConfigView(daq::PropertyObject());
    addButton->setEnabled(false);

    try
    {
        // Get instance from AppContext
        auto instance = AppContext::Instance()->daqInstance();
        if (!instance.assigned())
        {
            const auto loggerComponent = AppContext::LoggerComponent();
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
            item->setData(0, Qt::UserRole, typeId);
            item->setData(0, kDescriptionRole, description);
            item->setToolTip(0, description);
        }

        applySearchFilter(searchBox ? searchBox->text() : QString());
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error getting available servers: {}", e.what());
    }
}

void AddServerDialog::onSearchFilterChanged(const QString& text)
{
    applySearchFilter(text);
}

void AddServerDialog::applySearchFilter(const QString& filter)
{
    if (!serverList)
        return;

    if (filter.isEmpty())
    {
        for (int i = 0; i < serverList->topLevelItemCount(); ++i)
            serverList->topLevelItem(i)->setHidden(false);
        return;
    }

    for (int i = 0; i < serverList->topLevelItemCount(); ++i)
    {
        auto* item = serverList->topLevelItem(i);
        const QString name = item->text(0);
        const QString description = item->data(0, kDescriptionRole).toString();
        const bool matches = name.contains(filter, Qt::CaseInsensitive)
            || description.contains(filter, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }

    if (serverList->currentItem() && serverList->currentItem()->isHidden())
        serverList->clearSelection();
}

void AddServerDialog::updateSelectionHeader()
{
    auto* selectedItem = serverList ? serverList->currentItem() : nullptr;
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

void AddServerDialog::onServerSelected()
{
    auto* selectedItem = serverList->currentItem();
    if (selectedItem)
        selectedServerType = selectedItem->data(0, Qt::UserRole).toString();
    else
        selectedServerType.clear();

    updateSelectionHeader();
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

void AddServerDialog::replaceConfigView(const daq::PropertyObjectPtr& newConfig)
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

        replaceConfigView(config);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
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

