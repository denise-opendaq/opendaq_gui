#include "dialogs/add_function_block_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include <QSplitter>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <opendaq/custom_log.h>
#include "context/AppContext.h"
#include <opendaq/logger_component_ptr.h>

namespace
{
class SidebarSelectionDelegate final : public QStyledItemDelegate
{
public:
    explicit SidebarSelectionDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);

        if ((selected || hovered) && opt.widget)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor("#dfe9ff") : QColor("#ececec"));

            QRect r = opt.rect.adjusted(6, 1, -6, -1);

            if (auto* view = qobject_cast<const QAbstractItemView*>(opt.widget))
            {
                const int viewportW = view->viewport() ? view->viewport()->width() : r.width();
                r.setLeft(6);
                r.setRight(viewportW - 6);
            }

            painter->drawRoundedRect(r, 10.0, 10.0);
            painter->restore();
        }

        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

const char* kComponentTreeStyleSheet =
    "QTreeWidget {"
    "  background-color: #f2f2f2;"
    "  border: none;"
    "  font-size: 13px;"
    "  outline: 0;"
    "  show-decoration-selected: 1;"
    "  selection-background-color: transparent;"
    "  selection-color: #1f2a44;"
    "}"
    "QTreeWidget::item {"
    "  padding: 6px 8px;"
    "  border-radius: 8px;"
    "}"
    "QTreeWidget::item:selected, QTreeWidget::item:selected:active, QTreeWidget::item:selected:!active {"
    "  color: #1f2a44;"
    "}"
    "QTreeView::branch {"
    "  background: transparent;"
    "}"
    "QTreeView::branch:selected {"
    "  background: transparent;"
    "}"
    "QHeaderView::section {"
    "  background-color: #f2f2f2;"
    "  border: none;"
    "  padding: 6px 8px;"
    "  font-size: 11px;"
    "  font-weight: 600;"
    "  text-transform: uppercase;"
    "  color: #6b6b6b;"
    "}"
    "QHeaderView {"
    "  background-color: #f2f2f2;"
    "}";

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
    leftPanel->setStyleSheet(
        "QWidget#leftPanel {"
        "  background-color: #f2f2f2;"
        "  border: 1px solid #e0e0e0;"
        "}"
    );
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    searchBox = new QLineEdit(leftPanel);
    searchBox->setPlaceholderText(tr("Search function blocks..."));
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(
        "QLineEdit {"
        "  background-color: #f7f7f7;"
        "  border: 1px solid #e5e5e5;"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #c8d6ff;"
        "  background-color: #ffffff;"
        "}"
    );
    connect(searchBox, &QLineEdit::textChanged, this, &AddFunctionBlockDialog::onSearchFilterChanged);
    leftLayout->addWidget(searchBox);

    functionBlockList = new QTreeWidget(leftPanel);
    functionBlockList->setHeaderLabel(tr("Function Blocks"));
    functionBlockList->setRootIsDecorated(false);
    functionBlockList->setAlternatingRowColors(false);
    functionBlockList->setSelectionBehavior(QAbstractItemView::SelectRows);
    functionBlockList->setAllColumnsShowFocus(false);
    functionBlockList->setSelectionMode(QAbstractItemView::SingleSelection);
    functionBlockList->setItemDelegate(new SidebarSelectionDelegate(functionBlockList));
    functionBlockList->setAttribute(Qt::WA_StyledBackground, true);
    functionBlockList->setStyleSheet(kComponentTreeStyleSheet);
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
    nameLabel->setObjectName("componentNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#componentNameLabel {"
        "  font-size: 20px;"
        "  font-weight: 700;"
        "  color: #111827;"
        "}"
    );
    nameLabel->hide();
    headerLayout->addWidget(nameLabel);

    descriptionLabel = new QLabel(headerBlock);
    descriptionLabel->setObjectName("componentDescLabel");
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    descriptionLabel->setStyleSheet(
        "QLabel#componentDescLabel {"
        "  font-size: 13px;"
        "  color: #6b7280;"
        "}"
    );
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
    addButton->setStyleSheet(
        "QPushButton {"
        "  border: 1.5px solid #1d4ed8;"
        "  border-radius: 10px;"
        "  padding: 10px 10px;"
        // "  font-size: 14px;"
        // "  font-weight: 600;"
        "  color: #ffffff;"
        "  background-color: #1d4ed8;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1e40af;"
        "  border: 1.5px solid #1e40af;"
        "  border-radius: 12px;"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1e3a8a;"
        "  border: 1.5px solid #1e3a8a;"
        "  border-radius: 12px;"
        "  color: #ffffff;"
        "}"
        "QPushButton:disabled {"
        "  border: 1.5px solid #e5e7eb;"
        "  border-radius: 10px;"
        "  padding: 10px 10px;"
        // "  font-size: 14px;"
        // "  font-weight: 600;"
        "  color: #9ca3af;"
        "  background-color: #e5e7eb;"
        "}"
    );
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

