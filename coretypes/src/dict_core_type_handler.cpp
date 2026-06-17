#include "coretypes/dict_core_type_handler.h"
#include "coretypes/core_type_factory.h"
#include "context/AppContext.h"
#include <QTreeWidgetItem>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

DictCoreTypeHandler::DictCoreTypeHandler(daq::CoreType keyType, daq::CoreType itemType)
    : keyType(keyType)
    , itemType(itemType)
{
}

QString DictCoreTypeHandler::valueToString(const daq::BaseObjectPtr& value) const
{
    if (!value.assigned())
        return QStringLiteral("Dict (null)");

    try
    {
        const auto dict = value.asPtrOrNull<daq::IDict>(true);
        if (!dict.assigned())
            return QStringLiteral("Dict (null)");

        return QStringLiteral("Dict (%1 items)").arg(dict.getCount());
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error converting dict to string: {}", e.what());
        return QStringLiteral("Error");
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error converting dict to string");
        return QStringLiteral("Error");
    }
}

daq::BaseObjectPtr DictCoreTypeHandler::stringToValue(const QString& /*str*/) const
{
    return daq::Dict<daq::IBaseObject, daq::IBaseObject>().detach();
}

bool DictCoreTypeHandler::hasSelection() const
{
    return false;
}

QStringList DictCoreTypeHandler::getSelectionValues() const
{
    return QStringList();
}

bool DictCoreTypeHandler::isEditable() const
{
    return false;
}

bool DictCoreTypeHandler::hasSubtree(const daq::BaseObjectPtr& value) const
{
    if (!value.assigned())
        return false;

    try
    {
        const auto dict = value.asPtrOrNull<daq::IDict>(true);
        return dict.assigned() && dict.getCount() > 0;
    }
    catch (...)
    {
        return false;
    }
}

void DictCoreTypeHandler::buildSubtree(QTreeWidgetItem* parent, const daq::BaseObjectPtr& value) const
{
    if (!parent || !value.assigned())
        return;

    const auto dict = value.asPtrOrNull<daq::IDict>(true);
    if (!dict.assigned())
        return;

    for (const auto& [dictKey, dictValue] : dict)
    {
        auto* entryItem = new QTreeWidgetItem();
        auto keyHandler = CoreTypeFactory::createHandler(dictKey, dictKey.getCoreType());
        auto valHandler = CoreTypeFactory::createHandler(dictValue, dictValue.getCoreType());
        const QString keyStr = keyHandler->valueToString(dictKey);
        const QString valStr = valHandler->valueToString(dictValue);
        entryItem->setText(0, keyStr);
        entryItem->setToolTip(0, keyStr);
        entryItem->setText(1, valStr);
        entryItem->setToolTip(1, valStr);
        parent->addChild(entryItem);
        valHandler->buildSubtree(entryItem, dictValue);
    }
}

daq::CoreType DictCoreTypeHandler::getCoreType() const
{
    return daq::CoreType::ctDict;
}
