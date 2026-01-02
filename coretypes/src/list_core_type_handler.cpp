#include "coretypes/list_core_type_handler.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

ListCoreTypeHandler::ListCoreTypeHandler(daq::CoreType itemType)
    : itemType(itemType)
{
}

QString ListCoreTypeHandler::valueToString(const daq::BaseObjectPtr& value) const
{
    if (!value.assigned())
        return QString("[]");

    try
    {
        const auto list = value.asPtrOrNull<daq::IList>(true);
        if (list.assigned())
        {
            QStringList items;
            for (const auto & item : list)
            {
                if (item.assigned())
                {
                    try
                    {
                        items.append(QString::fromStdString(item.toString()));
                    }
                    catch (...)
                    {
                        items.append("?");
                    }
                }
            }
            return QString("[%1]").arg(items.join(", "));
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error converting list to string: {}", e.what());
        return QString("Error");
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error converting list to string");
        return QString("Error");
    }

    return QString("[]");
}

daq::BaseObjectPtr ListCoreTypeHandler::stringToValue(const QString& str) const
{
    try
    {
        // Parse comma-separated values
        QString trimmed = str.trimmed();
        
        // Remove brackets if present
        if (trimmed.startsWith('[') && trimmed.endsWith(']'))
            trimmed = trimmed.mid(1, trimmed.length() - 2).trimmed();
        
        QStringList values = trimmed.split(',', Qt::SkipEmptyParts);
        auto list = daq::List<daq::IBaseObject>();
        
        for (const QString& val : values)
        {
            QString itemStr = val.trimmed();
            daq::BaseObjectPtr item = parseItem(itemStr);
            if (item.assigned())
                list.pushBack(item);
        }
        
        return list.detach();
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error parsing list from string: {}", e.what());
        return daq::List<daq::IBaseObject>().detach();
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error parsing list from string");
        return daq::List<daq::IBaseObject>().detach();
    }
}

daq::BaseObjectPtr ListCoreTypeHandler::parseItem(const QString& str) const
{
    try
    {
        daq::StringPtr strValue = str.toStdString();
        return strValue.convertTo(itemType);
    }
    catch (...)
    {
        // If conversion fails, try to create default value based on item type
        switch (itemType)
        {
            case daq::CoreType::ctInt:
                return daq::Integer(0);
            case daq::CoreType::ctFloat:
                return daq::Float(0.0);
            case daq::CoreType::ctBool:
                return daq::Boolean(false);
            case daq::CoreType::ctString:
            default:
                return daq::String(str.toStdString());
        }
    }
}

bool ListCoreTypeHandler::hasSelection() const
{
    return false;  // Lists don't have selection
}

QStringList ListCoreTypeHandler::getSelectionValues() const
{
    return QStringList();  // No selection values
}

bool ListCoreTypeHandler::isEditable() const
{
    return true;  // Editable via text input (comma-separated values)
}

daq::CoreType ListCoreTypeHandler::getCoreType() const
{
    return daq::CoreType::ctList;
}

