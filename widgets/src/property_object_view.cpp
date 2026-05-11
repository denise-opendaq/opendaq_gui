#include "widgets/property_object_view.h"
#include "widgets/property_inspector.h"
#include "property/property_factory.h"
#include "property/base_property_item.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QKeyEvent>
#include <coreobjects/property_object_internal_ptr.h>

// ============================================================================
// ValueEditRoleDelegate
// ============================================================================

namespace
{

class ValueEditRoleDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        const QString raw = index.data(Qt::UserRole).toString();
        if (auto* lineEdit = qobject_cast<QLineEdit*>(editor))
        {
            lineEdit->setText(raw.isEmpty() ? index.data(Qt::DisplayRole).toString() : raw);
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto* lineEdit = qobject_cast<QLineEdit*>(editor))
        {
            // Store edited value as raw in UserRole; display formatting is restored by BasePropertyItem::commitEdit
            model->setData(index, lineEdit->text(), Qt::UserRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};

// ============================================================================
// PropertyTreeWidget — private QTreeWidget subclass that controls editing
// ============================================================================

class PropertyTreeWidget : public QTreeWidget
{
public:
    explicit PropertyTreeWidget(PropertyObjectView* view, QWidget* parent = nullptr)
        : QTreeWidget(parent)
        , view(view)
    {}

protected:
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override
    {
        auto* item = itemFromIndex(index);
        if (!item)
            return false;

        auto* logic = PropertyObjectView::getLogic(item);
        if (!logic)
        {
            auto* parent = item->parent();
            if (!parent)
                return false;
            logic = PropertyObjectView::getLogic(parent);
            if (!logic)
                return false;
        }

        if (index.column() == 0)
            return logic->isKeyEditable() && QTreeWidget::edit(index, trigger, event);
        if (index.column() == 1)
            return logic->isValueEditable() && QTreeWidget::edit(index, trigger, event);
        return false;
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_F5)
        {
            view->refresh();
            event->accept();
        }
        else
        {
            QTreeWidget::keyPressEvent(event);
        }
    }

private:
    PropertyObjectView* view;
};

} // namespace

// ============================================================================
// PropertyObjectView implementation
// ============================================================================

PropertyObjectView::PropertyObjectView(const daq::PropertyObjectPtr& root,
                                       QWidget* parent,
                                       const daq::ComponentPtr& owner)
    : QWidget(parent)
    , owner(owner)
    , root(root)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "PropertyObjectView { background: transparent; }"
        "QWidget#propertyPanel {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QLabel#propertiesTitle {"
        "  color: #111827;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "}"
        "QLineEdit#propertySearch {"
        "  background: #f9fafb;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 10px;"
        "  padding: 9px 12px;"
        "  color: #111827;"
        "}"
        "QLineEdit#propertySearch:focus {"
        "  border-color: #cbd5e1;"
        "  background: #ffffff;"
        "}"
        "QTreeWidget#propertyTree {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QTreeWidget#propertyTree::item {"
        "  padding: 7px 8px;"
        "  color: #111827;"
        "}"
        "QTreeWidget#propertyTree::item:selected {"
        "  background: #eff6ff;"
        "  color: #111827;"
        "}"
        "QTreeWidget#propertyTree::branch {"
        "  background: transparent;"
        "}"
        "QHeaderView::section {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 1px solid #e5e7eb;"
        "  padding: 10px 8px 8px 8px;"
        "  color: #6b7280;"
        "  font-weight: 600;"
        "}"
        "QSplitter::handle {"
        "  background: transparent;"
        // "  width: 12px;"
        "}"
    );

    if (const auto internal = root.asPtrOrNull<daq::IPropertyObjectInternal>(true); internal.assigned())
    {
        if (const auto path = internal.getPath(); path.assigned())
            rootPath = path.toStdString();
    }

    // Create tree widget
    tree = new PropertyTreeWidget(this);
    tree->setObjectName("propertyTree");
    tree->setColumnCount(2);
    tree->setHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
    tree->setHeaderHidden(false);
    tree->header()->setStretchLastSection(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree->setRootIsDecorated(true);
    tree->setUniformRowHeights(true);
    tree->setExpandsOnDoubleClick(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->setItemDelegateForColumn(1, new ValueEditRoleDelegate(tree));
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setIndentation(20);
    tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    searchEdit = new QLineEdit(this);
    searchEdit->setObjectName("propertySearch");
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setPlaceholderText(QStringLiteral("Search properties..."));

    // Create inspector (hidden until a property is selected)
    m_inspector = new PropertyInspector();
    m_inspector->setVisible(false);
    m_inspector->setMinimumWidth(300);

    auto* propertyPanel = new QWidget(this);
    propertyPanel->setObjectName("propertyPanel");
    auto* propertyPanelLayout = new QVBoxLayout(propertyPanel);
    propertyPanelLayout->setContentsMargins(16, 16, 16, 16);
    propertyPanelLayout->setSpacing(12);

    auto* propertiesTitle = new QLabel(tr("Properties"), propertyPanel);
    propertiesTitle->setObjectName("propertiesTitle");
    propertyPanelLayout->addWidget(propertiesTitle);
    propertyPanelLayout->addWidget(searchEdit);
    propertyPanelLayout->addWidget(tree, 1);

    // Layout: splitter with tree (left) and inspector (right)
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(14);
    splitter->addWidget(propertyPanel);
    splitter->addWidget(m_inspector);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);

    // Connect tree signals to view slots
    connect(tree, &QTreeWidget::itemChanged,      this, &PropertyObjectView::onItemChanged);
    connect(tree, &QTreeWidget::itemExpanded,     this, &PropertyObjectView::onItemExpanded);
    connect(tree, &QTreeWidget::itemCollapsed,    this, &PropertyObjectView::onItemCollapsed);
    connect(tree, &QTreeWidget::itemDoubleClicked,this, &PropertyObjectView::onItemDoubleClicked);
    connect(tree, &QWidget::customContextMenuRequested, this, &PropertyObjectView::onContextMenu);
    connect(searchEdit, &QLineEdit::textChanged, this, &PropertyObjectView::onSearchTextChanged);

    connect(tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem*)
    {
        BasePropertyItem* item = current ? getLogic(current) : nullptr;
        m_inspector->setVisible(item != nullptr);
        Q_EMIT propertySelected(item);
    });
    connect(this, &PropertyObjectView::propertySelected, m_inspector, &PropertyInspector::showProperty);

    refresh();
    if (owner.assigned())
        *AppContext::DaqEvent() += daq::event(this, &PropertyObjectView::componentCoreEventCallback);

    connect(AppContext::Instance(), &AppContext::showInvisibleChanged, this, &PropertyObjectView::refresh);
    connect(AppContext::Instance(), &AppContext::expandAllPropertiesChanged, this, [this](bool)
    {
        applyExpandState();
        applyFilter();
    });
}

PropertyObjectView::~PropertyObjectView()
{
    if (owner.assigned())
        *AppContext::DaqEvent() -= daq::event(this, &PropertyObjectView::componentCoreEventCallback);
}

void PropertyObjectView::refresh()
{
    {
        QSignalBlocker b(tree);
        propertyObjectToLogic[root] = nullptr;
        PropertySubtreeBuilder builder(*this);
        builder.buildFromPropertyObject(nullptr, root);
        applyExpandState();
    }

    applyFilter();
}

void PropertyObjectView::addTopLevelItem(QTreeWidgetItem* item)
{
    tree->addTopLevelItem(item);
}

QRect PropertyObjectView::visualItemRect(QTreeWidgetItem* item) const
{
    return tree->visualItemRect(item);
}

int PropertyObjectView::columnViewportPosition(int column) const
{
    return tree->columnViewportPosition(column);
}

int PropertyObjectView::currentColumn() const
{
    return tree->currentColumn();
}

QWidget* PropertyObjectView::viewport() const
{
    return tree->viewport();
}

daq::PropertyObjectPtr PropertyObjectView::getChildObject(std::string path)
{
    if (!rootPath.empty())
    {
        if (path.find(rootPath) != 0)
            return nullptr;
        if (path.length() == rootPath.length())
            path = "";
        else
            path = path.substr(rootPath.length() + 1);
    }

    if (path.empty())
        return root;

    if (root.hasProperty(path))
        return root.getPropertyValue(path);

    return nullptr;
}

void PropertyObjectView::componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs)
{
    if (component != owner)
        return;

    const auto eventId = static_cast<daq::CoreEventId>(eventArgs.getEventId());
    if (eventId == daq::CoreEventId::PropertyValueChanged || eventId == daq::CoreEventId::PropertyAdded)
    {
        const auto obj = getChildObject(eventArgs.getParameters()["Path"]);
        if (obj.assigned())
            onPropertyValueChanged(obj, true);
    }
    else if (eventId == daq::CoreEventId::PropertyRemoved)
    {
        const auto obj = getChildObject(eventArgs.getParameters()["Path"]);
        if (!obj.assigned())
            return;

        auto parentIt = propertyObjectToLogic.find(obj);
        if (parentIt == propertyObjectToLogic.end())
            return;

        const daq::StringPtr propName = eventArgs.getParameters().get("Name");
        ObjectPropertyItem* objLogic = parentIt->second;
        removeChildProperty(objLogic ? objLogic->getWidgetItem() : nullptr, propName);
        applyFilter();
    }
    else if (eventId == daq::CoreEventId::PropertyObjectUpdateEnd)
    {
        refresh();
    }
}

void PropertyObjectView::onPropertyValueChanged(const daq::PropertyObjectPtr& obj, bool force)
{
    if (!force && owner.assigned())
        return;

    auto it = propertyObjectToLogic.find(obj);
    if (it == propertyObjectToLogic.end())
        return;

    ObjectPropertyItem* objLogic = it->second;
    if (!objLogic)
    {
        refresh();
        return;
    }

    {
        QSignalBlocker blocker(tree);
        PropertySubtreeBuilder builder(*this);
        objLogic->refresh(builder);
    }

    applyFilter();
}

BasePropertyItem* PropertyObjectView::store(std::unique_ptr<BasePropertyItem> item)
{
    BasePropertyItem* itemPtr = item.get();
    PropertyKey key(itemPtr->getOwner(), itemPtr->getName());
    items.insert_or_assign(key, std::move(item));
    return itemPtr;
}

BasePropertyItem* PropertyObjectView::getLogic(QTreeWidgetItem* it)
{
    return reinterpret_cast<BasePropertyItem*>(it->data(0, Qt::UserRole).toULongLong());
}

void PropertyObjectView::setLogic(QTreeWidgetItem* it, BasePropertyItem* logic)
{
    it->setData(0, Qt::UserRole,
                QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(logic)));
}

void PropertyObjectView::onItemExpanded(QTreeWidgetItem* item)
{
    auto* logic = getLogic(item);
    if (!logic || !logic->hasSubtree())
        return;

    PropertySubtreeBuilder builder(*this);
    logic->build_subtree(builder, item);
    logic->setExpanded(true);
}

void PropertyObjectView::onItemCollapsed(QTreeWidgetItem* item)
{
    auto* logic = getLogic(item);
    if (logic)
        logic->setExpanded(false);
}

void PropertyObjectView::onItemChanged(QTreeWidgetItem* item, int column)
{
    BasePropertyItem* logic = getLogic(item);
    if (!logic && item->parent())
        logic = getLogic(item->parent());

    if (!logic)
        return;

    if ((column == 0 && !logic->isKeyEditable()) || (column == 1 && !logic->isValueEditable()))
        return;

    try
    {
        logic->commitEdit(item, column);
    }
    catch (const std::exception& e)
    {
        handleEditError(item, column, logic, e.what());
    }
    catch (...)
    {
        handleEditError(item, column, logic, "unknown error");
    }
}

void PropertyObjectView::handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg)
{
    QSignalBlocker b(tree);
    if (column == 1 && getLogic(item) == logic)
        item->setText(1, logic->showValue());

    QMessageBox::warning(this, "Property Update Error",
                         QString("Failed to update property: %1").arg(errorMsg));
}

void PropertyObjectView::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    auto* logic = getLogic(item);
    if (!logic)
    {
        auto* parent = item->parent();
        if (parent)
            logic = getLogic(parent);
    }

    if (logic && !logic->isReadOnly())
        logic->handle_double_click(this, item);
}

void PropertyObjectView::onContextMenu(const QPoint& pos)
{
    auto* item = tree->itemAt(pos);
    if (!item)
        return;

    auto* logic = getLogic(item);
    if (!logic)
    {
        auto* parent = item->parent();
        if (parent)
            logic = getLogic(parent);
    }

    if (logic)
        logic->handle_right_click(this, item, tree->viewport()->mapToGlobal(pos));
}

void PropertyObjectView::onSearchTextChanged(const QString&)
{
    applyFilter();
}

void PropertyObjectView::applyExpandState()
{
    if (AppContext::Instance()->expandAllProperties())
        tree->expandAll();
    else
        tree->collapseAll();
}

bool PropertyObjectView::updateItemFilter(QTreeWidgetItem* item, const QString& text)
{
    if (!item)
        return false;

    bool childMatches = false;
    for (int i = 0; i < item->childCount(); ++i)
        childMatches = updateItemFilter(item->child(i), text) || childMatches;

    if (text.isEmpty())
    {
        item->setHidden(false);
        return true;
    }

    const bool selfMatches = item->text(0).contains(text, Qt::CaseInsensitive);
    const bool visible = selfMatches || childMatches;
    item->setHidden(!visible);

    if (childMatches)
        item->setExpanded(true);

    return visible;
}

void PropertyObjectView::applyFilter()
{
    const QString filterText = searchEdit ? searchEdit->text().trimmed() : QString();

    if (filterText.isEmpty())
        applyExpandState();

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        updateItemFilter(tree->topLevelItem(i), filterText);

    if (auto* current = tree->currentItem(); current && current->isHidden())
        tree->setCurrentItem(nullptr);
}

void PropertyObjectView::removeChildProperty(QTreeWidgetItem* parentWidget, const std::string& propName)
{
    int childCount = parentWidget ? parentWidget->childCount() : tree->topLevelItemCount();

    for (int i = 0; i < childCount; ++i)
    {
        QTreeWidgetItem* childWidget = parentWidget ? parentWidget->child(i) : tree->topLevelItem(i);
        auto* childLogic = getLogic(childWidget);

        if (childLogic && childLogic->getName() == propName)
        {
            childLogic->setWidgetItem(nullptr);
            PropertyKey key(childLogic->getOwner(), propName);
            items.erase(key);
            break;
        }
    }
}

// ============================================================================
// PropertySubtreeBuilder implementation
// ============================================================================

QTreeWidgetItem* PropertySubtreeBuilder::addItem(QTreeWidgetItem* parent,
                                                 std::unique_ptr<BasePropertyItem> item)
{
    auto* logic = view.store(std::move(item));

    if (!logic->isVisible() && !AppContext::Instance()->showInvisibleComponents())
    {
        logic->setWidgetItem(nullptr);
        return nullptr;
    }

    QTreeWidgetItem* it = logic->getWidgetItem();
    if (!it)
    {
        it = new QTreeWidgetItem();
        logic->setWidgetItem(it);

        if (parent)
            parent->addChild(it);
        else
            view.addTopLevelItem(it);
        PropertyObjectView::setLogic(it, logic);
    }

    if (logic->isReadOnly())
    {
        QPalette palette = view.palette();
        it->setForeground(0, palette.color(QPalette::Disabled, QPalette::Text));
        it->setForeground(1, palette.color(QPalette::Disabled, QPalette::Text));
    }
    else if (!logic->hasSubtree())
    {
        it->setFlags(it->flags() |= Qt::ItemIsEditable);
    }

    if (logic->hasSubtree())
    {
        it->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        QFont sectionFont = it->font(0);
        sectionFont.setBold(true);
        it->setFont(0, sectionFont);
        it->setFont(1, sectionFont);
        const QColor sectionBackground(0xF8, 0xFA, 0xFC);
        it->setBackground(0, sectionBackground);
        it->setBackground(1, sectionBackground);
        it->setFirstColumnSpanned(true);
        it->setSizeHint(0, QSize(0, 34));
    }
    else
    {
        it->setFirstColumnSpanned(false);
        it->setSizeHint(0, QSize(0, 30));
    }

    PropertySubtreeBuilder builder(*this);
    logic->refresh(builder);
    if (logic->hasSubtree() && logic->isExpanded())
        view.onItemExpanded(it);

    return it;
}

void PropertySubtreeBuilder::buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj)
{
    if (!obj.assigned())
        return;

    for (const auto& prop: obj.getAllProperties())
    {
        PropertyKey key(obj, prop.getName());
        auto it = view.items.find(key);

        if (it != view.items.end())
            addItem(parent, std::move(it->second));
        else
            addItem(parent, createPropertyItem(obj, prop));
    }
}
