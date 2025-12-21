#include "component/folder_tree_element.h"
#include "component/component_factory.h"

FolderTreeElement::FolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent)
    : ComponentTreeElement(tree, daqFolder, parent)
{
    this->type = "Folder";
    this->iconName = "folder";
    this->name = getStandardFolderName(this->name);
}

void FolderTreeElement::init(BaseTreeElement* parent)
{
    ComponentTreeElement::init(parent);

    // Add all items from the folder as children
    try
    {
        auto folder = daqComponent.asPtr<daq::IFolder>();
        auto items = folder.getItems();

        for (size_t i = 0; i < items.getCount(); ++i)
        {
            auto item = items[i];
            auto childElement = createTreeElement(tree, item, this);
            if (childElement)
            {
                addChild(childElement);
            }
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error initializing folder children:" << e.what();
    }
}

bool FolderTreeElement::visible() const
{
    if (children.isEmpty())
    {
        return false;
    }
    return ComponentTreeElement::visible();
}

QString FolderTreeElement::getStandardFolderName(const QString& componentName) const
{
    if (componentName == "Sig")
        return "Signals";
    if (componentName == "FB")
        return "Function blocks";
    if (componentName == "Dev")
        return "Devices";
    if (componentName == "IP")
        return "Input ports";
    if (componentName == "IO")
        return "Inputs/Outputs";
    if (componentName == "Srv")
        return "Servers";

    return componentName;
}

