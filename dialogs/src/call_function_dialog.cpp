#include "dialogs/call_function_dialog.h"
#include "widgets/property_object_view.h"
#include "coretypes/core_type_factory.h"
#include "coretypes/default_core_type.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <QScrollArea>
#include <QGroupBox>

CallFunctionDialog::CallFunctionDialog(const daq::PropertyObjectPtr& owner, 
                                     const daq::PropertyPtr& prop,
                                     QWidget* parent)
    : QDialog(parent)
    , owner(owner)
    , prop(prop)
    , argumentsPropertyObject(nullptr)
    , argumentsView(nullptr)
    , resultTextEdit(nullptr)
    , executeButton(nullptr)
{
    setWindowTitle(QString("Call %1").arg(QString::fromStdString(prop.getName())));
    resize(600, 500);
    setMinimumSize(400, 300);

    // Get callable info
    try
    {
        callableInfo = prop.getCallableInfo();
    }
    catch (...)
    {
        callableInfo = nullptr;
    }

    createArgumentsPropertyObject();
    setupUI();
}

void CallFunctionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Arguments section
    auto* argsGroupBox = new QGroupBox("Arguments", this);
    auto* argsLayout = new QVBoxLayout(argsGroupBox);
    argsLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create PropertyObjectView for arguments (without owner to avoid core events)
    if (argumentsPropertyObject.assigned())
    {
        argumentsView = new PropertyObjectView(argumentsPropertyObject, argsGroupBox);
        argsLayout->addWidget(argumentsView);
    }
    else
    {
        auto* noArgsLabel = new QLabel("No arguments required", argsGroupBox);
        argsLayout->addWidget(noArgsLabel);
    }
    
    mainLayout->addWidget(argsGroupBox);

    // Result section
    auto* resultLabel = new QLabel("Result:", this);
    mainLayout->addWidget(resultLabel);

    resultTextEdit = new QTextEdit(this);
    resultTextEdit->setReadOnly(true);
    resultTextEdit->setMaximumHeight(150);
    mainLayout->addWidget(resultTextEdit);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    executeButton = buttonBox->button(QDialogButtonBox::Ok);
    executeButton->setText("Execute");
    executeButton->setDefault(true);
    
    connect(executeButton, &QPushButton::clicked, this, &CallFunctionDialog::onExecuteClicked);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &CallFunctionDialog::onCloseClicked);
    
    mainLayout->addWidget(buttonBox);
}

void CallFunctionDialog::createArgumentsPropertyObject()
{
    if (!callableInfo.assigned())
        return;

    const auto argsInfo = callableInfo.getArguments();
    if (!argsInfo.assigned())
        return;

    try
    {
        // Create PropertyObject for arguments
        argumentsPropertyObject = daq::PropertyObject();
        for (const auto& argInfo : argsInfo)
        {
            auto argName = argInfo.getName();
            daq::CoreType argType;
            argInfo->getType(&argType);

            // Create property using PropertyBuilder with setValueType
            daq::PropertyPtr property = daq::PropertyBuilder(argName).setValueType(argType)
                                                                        .setDefaultValue(createDefaultValue(argType))
                                                                        .build();
            
            argumentsPropertyObject.addProperty(property);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error creating arguments property object: {}", e.what());
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Unknown error creating arguments property object");
    }
}

daq::BaseObjectPtr CallFunctionDialog::collectArguments()
{
    if (!argumentsPropertyObject.assigned())
        return nullptr; // No arguments

    auto args = callableInfo.getArguments();
    if (!args.assigned() || args.getCount() == 0)
        return nullptr;

    try
    {
        if (args.getCount() == 1)
        {
            // Single argument - return the value directly
            auto argName = args[0].getName();
            return argumentsPropertyObject.getPropertyValue(argName);
        }
        else
        {
            // Multiple arguments - return as a list
            auto list = daq::List<daq::IBaseObject>();
            for (const auto& argInfo : args)
            {
                auto argName = argInfo.getName();
                auto value = argumentsPropertyObject.getPropertyValue(argName);
                list.pushBack(value);
            }
            return list.detach();
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error collecting arguments: {}", e.what());
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Unknown error collecting arguments");
    }
    
    return nullptr;
}

QString CallFunctionDialog::formatResult(const daq::BaseObjectPtr& result)
{
    if (!result.assigned())
        return "null";
    
    try
    {
        // Use core type handler for formatting
        auto coreType = result.getCoreType();
        auto handler = CoreTypeFactory::createHandler(result, coreType);
        if (handler)
            return handler->valueToString(result);
        
        // Fallback to toString
        return QString::fromStdString(result);
    }
    catch (...)
    {
        return "(unable to format result)";
    }
}

void CallFunctionDialog::onExecuteClicked()
{
    try
    {
        const auto value = owner.getPropertyValue(prop.getName());
        if (!value.assigned())
        {
            resultTextEdit->setPlainText("Error: Property has no assigned value");
            return;
        }

        auto valueType = prop.getValueType();
        daq::BaseObjectPtr params = collectArguments();

        if (auto proc = value.asPtrOrNull<daq::IProcedure>(true); proc.assigned())
        {
            if (params.assigned())
                proc.dispatch(params);
            else
                proc.dispatch();

            resultTextEdit->setPlainText("Procedure executed successfully");
        }
        else if (auto func = value.asPtrOrNull<daq::IFunction>(true); func.assigned())
        {
            daq::BaseObjectPtr resultPtr;
            if (params.assigned())
                resultPtr = func.call(params);
            else
                resultPtr = func.call();
            
            if (resultPtr != nullptr)
            {
                daq::BaseObjectPtr result = daq::BaseObjectPtr::Borrow(resultPtr);
                resultTextEdit->setPlainText(QString("Function executed successfully\nResult: %1").arg(formatResult(result)));
            }
            else
            {
                resultTextEdit->setPlainText("Function executed successfully\nResult: null");
            }
        }
        else
        {
            resultTextEdit->setPlainText("Error: Property does not contain a valid function");
        }
    }
    catch (const std::exception& e)
    {
        resultTextEdit->setPlainText(QString("Error: %1").arg(e.what()));
    }
    catch (...)
    {
        resultTextEdit->setPlainText("Error: Unknown error occurred");
    }
}

void CallFunctionDialog::onCloseClicked()
{
    reject();
}
