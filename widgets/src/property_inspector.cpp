#include "widgets/property_inspector.h"
#include "property/base_property_item.h"
#include <coretypes/string_ptr.h>

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QToolButton>
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

static QStringList getAllowedValues(const daq::PropertyPtr& prop)
{
    QStringList result;
    try
    {
        const auto sel = prop.getSelectionValues();
        if (sel.assigned())
        {
            if (const auto dict = sel.asPtrOrNull<daq::IDict>(true); dict.assigned())
            {
                for (const auto& v : dict.getValueList())
                    result << QString::fromStdString(v.toString());
                return result;
            }
            if (const auto list = sel.asPtrOrNull<daq::IList>(true); list.assigned())
            {
                for (const auto& v : list)
                    result << QString::fromStdString(v.toString());
                return result;
            }
        }
    }
    catch (...) {}

    try
    {
        const auto sug = prop.getSuggestedValues();
        if (sug.assigned())
            if (const auto list = sug.asPtrOrNull<daq::IList>(true); list.assigned())
                for (const auto& v : list)
                    result << QString::fromStdString(v.toString());
    }
    catch (...) {}

    return result;
}

// ============================================================================

PropertyInspector::PropertyInspector(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("propertyInspector");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QWidget#propertyInspector {"
        "  background: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 16px;"
        "}"
        "QWidget#inspectorHeader {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QLabel#inspectorTitle {"
        "  color: #111827;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  border: none;"
        "}"
        "QToolButton#inspectorCloseButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 10px;"
        "  color: #6b7280;"
        "  font-size: 16px;"
        "  padding: 2px;"
        "}"
        "QToolButton#inspectorCloseButton:hover {"
        "  background: #f3f4f6;"
        "  color: #111827;"
        "}"
        "QScrollArea#inspectorScroll, QWidget#inspectorScrollContent, QWidget#inspectorContent, QWidget#advancedContent {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QLabel#inspectorEmptyState {"
        "  color: #9ca3af;"
        "  border: none;"
        "  padding: 24px 8px;"
        "}"
        "QLabel#inspectorFieldLabel {"
        "  color: #6b7280;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.5px;"
        "  border: none;"
        "  margin-top: 10px;"
        "}"
        "QLabel#inspectorFieldValue {"
        "  color: #374151;"
        "  border: none;"
        "  padding-top: 2px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QToolButton#advancedToggle {"
        "  background: transparent;"
        "  border: none;"
        "  color: #111827;"
        "  font-weight: 600;"
        "  padding: 10px 0 2px 0;"
        "}"
    );
    setupUI();
    clearProperty();
}

static PropertyInspector::Row makeRow(QVBoxLayout* layout)
{
    auto* label = new QLabel();
    label->setObjectName("inspectorFieldLabel");

    auto* value = new QLabel();
    value->setObjectName("inspectorFieldValue");
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
    headerLayout->setContentsMargins(16, 14, 12, 8);

    auto* titleLabel = new QLabel(tr("Property Inspector"), header);
    titleLabel->setObjectName("inspectorTitle");

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto* closeButton = new QToolButton(header);
    closeButton->setObjectName("inspectorCloseButton");
    closeButton->setText(QStringLiteral("x"));
    closeButton->setAutoRaise(true);
    connect(closeButton, &QToolButton::clicked, this, &QWidget::hide);
    headerLayout->addWidget(closeButton);

    // Separator line
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("border: none; background: #f3f4f6; min-height: 1px; max-height: 1px;");

    // Scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("inspectorScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);

    auto* scrollContent = new QWidget();
    scrollContent->setObjectName("inspectorScrollContent");
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(16, 8, 16, 16);
    scrollLayout->setSpacing(0);

    // Empty state
    emptyState = new QLabel(tr("Select a property to inspect"), scrollContent);
    emptyState->setObjectName("inspectorEmptyState");
    emptyState->setAlignment(Qt::AlignCenter);
    QPalette pal = emptyState->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
    emptyState->setPalette(pal);
    scrollLayout->addWidget(emptyState);

    // Content with rows
    content = new QWidget(scrollContent);
    content->setObjectName("inspectorContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    rowName        = makeRow(contentLayout);
    rowPath        = makeRow(contentLayout);
    rowDescription = makeRow(contentLayout);
    rowType        = makeRow(contentLayout);
    rowUnit        = makeRow(contentLayout);
    rowDefault     = makeRow(contentLayout);
    rowCurrent     = makeRow(contentLayout);
    rowAllowed = makeRow(contentLayout);
    rowAllowed.value->hide();

    allowedBubblesWidget = new QWidget(content);
    allowedBubblesLayout = new QHBoxLayout(allowedBubblesWidget);
    allowedBubblesLayout->setContentsMargins(0, 2, 0, 0);
    allowedBubblesLayout->setSpacing(6);
    allowedBubblesWidget->hide();
    contentLayout->addWidget(allowedBubblesWidget);

    rowWritable = makeRow(contentLayout);

    rowMin = makeRow(contentLayout);
    rowMax = makeRow(contentLayout);
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
    rowMin.label->setText(tr("Minimum"));
    rowMax.label->setText(tr("Maximum"));

    scrollLayout->addWidget(content);
    scroll->setWidget(scrollContent);

    outerLayout->addWidget(header);
    outerLayout->addWidget(separator);
    outerLayout->addWidget(scroll, 1);
}

void PropertyInspector::populateAllowedBubbles(const daq::PropertyPtr& prop)
{
    while (allowedBubblesLayout->count())
    {
        auto* item = allowedBubblesLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    const QStringList values = getAllowedValues(prop);

    rowAllowed.label->setVisible(!values.isEmpty());
    allowedBubblesWidget->setVisible(!values.isEmpty());

    for (const QString& val : values)
    {
        auto* bubble = new QWidget(allowedBubblesWidget);
        bubble->setAttribute(Qt::WA_StyledBackground, true);
        bubble->setStyleSheet("QWidget { background: #dbeafe; border-radius: 10px; }");
        auto* bl = new QHBoxLayout(bubble);
        bl->setContentsMargins(8, 3, 8, 3);
        bl->setSpacing(0);

        auto* lbl = new QLabel(val, bubble);
        lbl->setStyleSheet("color: #1d4ed8; font-size: 12px; font-weight: 600; border: none;");
        bl->addWidget(lbl);

        allowedBubblesLayout->addWidget(bubble);
    }

    if (!values.isEmpty())
        allowedBubblesLayout->addStretch();
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

    // Allowed values — bubbles
    populateAllowedBubbles(prop);

    // Writable
    const bool readOnly = item->isReadOnly();
    auto* writableLabel = rowWritable.value;
    writableLabel->setText(readOnly ? tr("No") : tr("Yes"));
    QPalette pal = writableLabel->palette();
    pal.setColor(QPalette::WindowText, readOnly ? QColor(0xCC, 0x44, 0x44) : QColor(0x22, 0xAA, 0x44));
    writableLabel->setPalette(pal);
    setRowVisible(rowWritable, true);

    const QString minValue = item->getMetadataValue(QStringLiteral("Min value"));
    setRowText(rowMin, minValue);
    setRowVisible(rowMin, !minValue.isEmpty());

    const QString maxValue = item->getMetadataValue(QStringLiteral("Max value"));
    setRowText(rowMax, maxValue);
    setRowVisible(rowMax, !maxValue.isEmpty());


    emptyState->setVisible(false);
    content->setVisible(true);
    setVisible(true);
}

void PropertyInspector::clearProperty()
{
    emptyState->setVisible(true);
    content->setVisible(false);
}
