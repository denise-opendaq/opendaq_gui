#include "property/function_property_item.h"
#include "widgets/property_object_view.h"
#include "dialogs/call_function_dialog.h"
#include <QTreeWidget>
#include <QMessageBox>

namespace
{
    QString coreTypeToString(daq::CoreType type)
    {
        switch (type)
        {
            case daq::CoreType::ctBool: return "Bool";
            case daq::CoreType::ctInt: return "Int";
            case daq::CoreType::ctFloat: return "Float";
            case daq::CoreType::ctString: return "String";
            case daq::CoreType::ctList: return "List";
            case daq::CoreType::ctDict: return "Dict";
            case daq::CoreType::ctObject: return "Object";
            case daq::CoreType::ctFunc: return "Function";
            case daq::CoreType::ctProc: return "Procedure";
            case daq::CoreType::ctStruct: return "Struct";
            case daq::CoreType::ctEnumeration: return "Enumeration";
            case daq::CoreType::ctRatio: return "Ratio";
            case daq::CoreType::ctComplexNumber: return "ComplexNumber";
            case daq::CoreType::ctBinaryData: return "BinaryData";
            default: return "Unknown";
        }
    }
}

FunctionPropertyItem::FunctionPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
{
}

QString FunctionPropertyItem::showValue() const
{
    auto valueType = prop.getValueType();
    QString typeStr = (valueType == daq::CoreType::ctFunc) ? "Function" : "Procedure";
    
    // Try to get callable info to show argument information
    try
    {
        auto callableInfo = prop.getCallableInfo();
        if (callableInfo.assigned())
        {
            QStringList argStrings;
            for (const auto& argInfo :  callableInfo.getArguments())
            {
                auto argName = argInfo.getName();
                daq::CoreType argType;
                argInfo->getType(&argType);
                QString argTypeStr = coreTypeToString(argType);
                argStrings.append(QString("%1: %2").arg(QString::fromStdString(argName)).arg(argTypeStr));
            }
            return QString("%1(%2)").arg(typeStr).arg(argStrings.join(", "));
        }
    }
    catch (...)
    {
        // If we can't get callable info, just return the basic type
    }
    
    return typeStr;
}

bool FunctionPropertyItem::isValueEditable() const
{
    return false; // Functions/procedures are not editable, only callable
}

void FunctionPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    try
    {
        // Open dialog for calling the function/procedure
        CallFunctionDialog dialog(owner, prop, view);
        dialog.exec();
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(view,
                             "Error",
                             QString("Failed to open function dialog: %1").arg(e.what()));
    }
    catch (...)
    {
        QMessageBox::critical(view,
                             "Error",
                             "Failed to open function dialog: unknown error");
    }
}

