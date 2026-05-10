#include "widgets/property_inspector.h"
#include "property/base_property_item.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <coreobjects/property_object_internal_ptr.h>

static QString formatUnit(const daq::PropertyPtr& prop)
{
    try
    {
        const auto unit = prop.getUnit();
        if (!unit.assigned())
            return QString();

        QString result;
        try
        {
            const auto name = unit.getName();
            if (name.assigned() && name.getLength())
                result = QString::fromStdString(name);
        }
        catch (...) {}

        try
        {
            const auto symbol = unit.getSymbol();
            if (symbol.assigned() && symbol.getLength())
            {
                const QString sym = QString::fromStdString(symbol);
                result = result.isEmpty() ? sym : result + QStringLiteral(" (") + sym + QStringLiteral(")");
            }
        }
        catch (...) {}

        return result;
    }
    catch (...)
    {
        return QString();
    }
}

static QString formatAllowedValues(const daq::PropertyPtr& prop)
{
    try
    {
        const auto selectionValues = prop.getSelectionValues();
        if (selectionValues.assigned())
        {
            if (const auto dict = selectionValues.asPtrOrNull<daq::IDict>(true); dict.assigned())
                return QString::fromStdString(dict.getValueList());
            return QString::fromStdString(selectionValues);
        }
    }
    catch (...) {}

    try
    {
        const auto suggested = prop.getSuggestedValues();
        if (suggested.assigned())
            return QString::fromStdString(suggested);
    }
    catch (...) {}

    return QString();
}

// ============================================================================

PropertyInspector::PropertyInspector(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    clearProperty();
}

static PropertyInspector::Row makeRow(QVBoxLayout* layout)
{
    auto* label = new QLabel();
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    label->setContentsMargins(0, 8, 0, 0);

    auto* value = new QLabel();
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(label);
    layout->addWidget(value);

    return {label, value};
}

void PropertyInspector::setupUI()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Header bar
    auto* header = new QWidget(this);
    header->setObjectName("inspectorHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 8, 8);

    auto* titleLabel = new QLabel(tr("Property Inspector"), header);
    QFont f = titleLabel->font();
    f.setBold(true);
    titleLabel->setFont(f);

    headerLayout->addWidget(titleLabel);

    // Separator line
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    // Scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);

    auto* scrollContent = new QWidget();
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(12, 4, 12, 12);
    scrollLayout->setSpacing(0);

    // Empty state
    emptyState = new QLabel(tr("Select a property to inspect"), scrollContent);
    emptyState->setAlignment(Qt::AlignCenter);
    QPalette pal = emptyState->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
    emptyState->setPalette(pal);
    scrollLayout->addWidget(emptyState);

    // Content with rows
    content = new QWidget(scrollContent);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    rowName        = makeRow(contentLayout);
    rowPath        = makeRow(contentLayout);
    rowDescription = makeRow(contentLayout);
    rowType        = makeRow(contentLayout);
    rowUnit        = makeRow(contentLayout);
    rowDefault     = makeRow(contentLayout);
    rowCurrent     = makeRow(contentLayout);
    rowAllowed     = makeRow(contentLayout);
    rowWritable    = makeRow(contentLayout);
    contentLayout->addStretch();

    rowName.label->setText(tr("Name"));
    rowPath.label->setText(tr("Path"));
    rowDescription.label->setText(tr("Description"));
    rowType.label->setText(tr("Type"));
    rowUnit.label->setText(tr("Unit"));
    rowDefault.label->setText(tr("Default value"));
    rowCurrent.label->setText(tr("Current value"));
    rowAllowed.label->setText(tr("Allowed values"));
    rowWritable.label->setText(tr("Writable"));

    scrollLayout->addWidget(content);
    scroll->setWidget(scrollContent);

    outerLayout->addWidget(header);
    outerLayout->addWidget(separator);
    outerLayout->addWidget(scroll, 1);
}

void PropertyInspector::setRowText(Row& row, const QString& text)
{
    row.value->setText(text);
}

void PropertyInspector::setRowVisible(Row& row, bool visible)
{
    row.label->setVisible(visible);
    row.value->setVisible(visible);
}

void PropertyInspector::showProperty(BasePropertyItem* item)
{
    if (!item)
    {
        clearProperty();
        return;
    }

    const auto prop = item->getProperty();
    const auto owner = item->getOwner();

    // Name
    setRowText(rowName, QString::fromStdString(item->getName()));

    // Path
    QString pathStr;
    try
    {
        if (const auto internal = owner.asPtrOrNull<daq::IPropertyObjectInternal>(true); internal.assigned())
        {
            const auto ownerPath = internal.getPath();
            if (ownerPath.assigned() && ownerPath.getLength())
                pathStr = QString::fromStdString(ownerPath) + QStringLiteral(".") + QString::fromStdString(item->getName());
        }
    }
    catch (...) {}
    setRowText(rowPath, pathStr);
    setRowVisible(rowPath, !pathStr.isEmpty());

    // Description
    const QString desc = item->getMetadataValue(QStringLiteral("Description"));
    setRowText(rowDescription, desc);
    setRowVisible(rowDescription, !desc.isEmpty());

    // Type
    const QString type = item->getMetadataValue(QStringLiteral("Value type"));
    setRowText(rowType, type);
    setRowVisible(rowType, !type.isEmpty());

    // Unit
    const QString unit = formatUnit(prop);
    setRowText(rowUnit, unit);
    setRowVisible(rowUnit, !unit.isEmpty());

    // Default value
    const QString defaultVal = item->getMetadataValue(QStringLiteral("Default value"));
    setRowText(rowDefault, defaultVal);
    setRowVisible(rowDefault, !defaultVal.isEmpty());

    // Current value (with unit symbol if available)
    setRowText(rowCurrent, item->showDisplayValue());
    setRowVisible(rowCurrent, true);

    // Allowed values
    const QString allowed = formatAllowedValues(prop);
    setRowText(rowAllowed, allowed);
    setRowVisible(rowAllowed, !allowed.isEmpty());

    // Writable
    const bool readOnly = item->isReadOnly();
    auto* writableLabel = rowWritable.value;
    writableLabel->setText(readOnly ? tr("No") : tr("Yes"));
    QPalette pal = writableLabel->palette();
    pal.setColor(QPalette::WindowText, readOnly ? QColor(0xCC, 0x44, 0x44) : QColor(0x22, 0xAA, 0x44));
    writableLabel->setPalette(pal);
    setRowVisible(rowWritable, true);

    emptyState->setVisible(false);
    content->setVisible(true);
}

void PropertyInspector::clearProperty()
{
    emptyState->setVisible(true);
    content->setVisible(false);
}
