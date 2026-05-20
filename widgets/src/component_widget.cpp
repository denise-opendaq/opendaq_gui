#include "widgets/component_widget.h"
#include "coretypes/string_ptr.h"
#include "widgets/property_object_view.h"
#include "widgets/status_message_stack.h"
#include "context/AppContext.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include "context/icon_provider.h"
#include "context/QueuedEventHandler.h"
#include <qt_widget_interface/qt_widget_interface.h>

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

ComponentWidget::ComponentWidget(const daq::ComponentPtr& comp, QWidget* parent, const QString& treeIcon)
    : ComponentWidget(comp, parent, Qt::Uninitialized, treeIcon)
{
    setupUI();
    if (component.assigned())
        *AppContext::DaqEvent() += daq::event(this, &ComponentWidget::onCoreEvent);
}

ComponentWidget::ComponentWidget(const daq::ComponentPtr& comp, QWidget* parent, Qt::Initialization, const QString& treeIcon)
    : QWidget(parent)
    , component(comp)
    , treeIconName(treeIcon)
{}

ComponentWidget::~ComponentWidget()
{
    if (component.assigned())
        *AppContext::DaqEvent() -= daq::event(this, &ComponentWidget::onCoreEvent);
}

void ComponentWidget::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 0);
    root->setSpacing(12);

    root->addWidget(buildHeaderCard());
    root->addWidget(tabs = new QTabWidget(this), 1);

    populateTabs();

    tabs->setStyleSheet(GUIStyles::underlineTabBarAccentStyle());

    // Double-click on a tab pops it out into its own top-level window.
    if (auto* bar = tabs->tabBar())
    {
        bar->setToolTip(tr("Double-click a tab to open it in a separate window"));
        bar->installEventFilter(this);
    }
}

void ComponentWidget::addTreeIconToHeaderLayout(QHBoxLayout* cardLayout, QWidget* card)
{
    auto* iconLabel = new QLabel(card);
    iconLabel->setFixedSize(64, 64);
    iconLabel->setAlignment(Qt::AlignCenter);
    const QIcon ico = treeIconName.isEmpty() ? QIcon() : IconProvider::instance().icon(treeIconName);
    if (ico.availableSizes().size() > 0)
        iconLabel->setPixmap(ico.pixmap(52, 52));
    else if (!treeIconName.isEmpty())
        iconLabel->setText(QStringLiteral("\u25C7"));
    cardLayout->addWidget(iconLabel, 0, Qt::AlignTop);
}

QWidget* ComponentWidget::buildHeaderCard()
{
    auto* card = new QWidget(this);
    card->setObjectName("componentHeaderCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(GUIStyles::cardStyle("componentHeaderCard"));

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(16);

    addTreeIconToHeaderLayout(cardLayout, card);

    // — Left info block (name / tags / desc / status) —
    auto* leftBlock = new QWidget(card);
    auto* leftLayout = new QVBoxLayout(leftBlock);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    nameLabel = new QLabel(leftBlock);
    nameLabel->setStyleSheet(GUIStyles::HEADER_TITLE_LABEL);
    leftLayout->addWidget(nameLabel);

    tagsRow = new QWidget(leftBlock);
    auto* tagsLayout = new QHBoxLayout(tagsRow);
    tagsLayout->setContentsMargins(0, 2, 0, 2);
    tagsLayout->setSpacing(6);
    leftLayout->addWidget(tagsRow);

    descLabel = new QLabel(leftBlock);
    descLabel->setStyleSheet(GUIStyles::DESCRIPTION_LABEL);
    leftLayout->addWidget(descLabel);

    auto* statusRow = new QWidget(leftBlock);
    auto* statusRowLayout = new QHBoxLayout(statusRow);
    statusRowLayout->setContentsMargins(0, 4, 0, 0);
    statusRowLayout->setSpacing(16);

    statusLabel = new QLabel(statusRow);
    statusLabel->setObjectName("componentStatusLabel");
    statusLabel->setCursor(Qt::PointingHandCursor);
    statusLabel->installEventFilter(this);
    statusRowLayout->addWidget(statusLabel);
    statusRowLayout->addStretch();
    leftLayout->addWidget(statusRow);

    cardLayout->addWidget(leftBlock, 1, Qt::AlignTop);

    // — Vertical separator (hidden until statuses are present) —
    statusSep = new QFrame(card);
    statusSep->setFrameShape(QFrame::VLine);
    statusSep->setFrameShadow(QFrame::Plain);
    statusSep->setStyleSheet(GUIStyles::hairlineVerticalStyle());
    statusSep->setVisible(false);
    cardLayout->addWidget(statusSep);

    // — Status container block —
    statusContainerBlock = new QWidget(card);
    statusContainerBlock->setLayout(new QVBoxLayout());
    statusContainerBlock->layout()->setContentsMargins(8, 0, 8, 0);
    static_cast<QVBoxLayout*>(statusContainerBlock->layout())->setSpacing(6);
    static_cast<QVBoxLayout*>(statusContainerBlock->layout())->setAlignment(Qt::AlignTop);
    statusContainerBlock->setVisible(false);
    cardLayout->addWidget(statusContainerBlock, 0, Qt::AlignTop);

    updateStatus();
    updateTags();
    updateStatusContainer();

    return card;
}

void ComponentWidget::populateTabs()
{
    const auto widgetComponent = component.asPtrOrNull<IQTWidget>(true);
    if (widgetComponent.assigned())
    {
        QWidget* qtWidget;
        widgetComponent->getWidget(&qtWidget);
        daq::StringPtr name;
        widgetComponent->getName(&name);
        if (qtWidget && name.assigned())
            tabs->addTab(qtWidget, QString::fromStdString(name.toStdString()));
    }

    auto* propView = new PropertyObjectView(component, nullptr, component);
    tabs->addTab(propView, tr("Attributes"));
}

void ComponentWidget::updateStatus()
{
    if (!nameLabel)
        return;

    try
    {
        nameLabel->setText(QString::fromStdString(component.getName()));
    }
    catch (...) {}

    try
    {
        const QString desc = QString::fromStdString(component.getDescription());
        descLabel->setText(desc.isEmpty() ? tr("—") : desc);
    }
    catch (...)
    {
        descLabel->setText(tr("—"));
    }

    bool active = true;
    try { active = component.getActive(); } catch (...) {}

    if (active)
    {
        statusLabel->setText(tr("● Active"));
        statusLabel->setStyleSheet(GUIStyles::coloredStatusLabelStyle(GUIConstants::Colors::SUCCESS));
    }
    else
    {
        statusLabel->setText(tr("● Inactive"));
        statusLabel->setStyleSheet(GUIStyles::coloredStatusLabelStyle(GUIConstants::Colors::NEUTRAL));
    }
}

void ComponentWidget::updateStatusContainer()
{
    if (!statusContainerBlock)
        return;

    auto* layout = static_cast<QVBoxLayout*>(statusContainerBlock->layout());
    while (layout->count())
    {
        auto* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }

    try
    {
        const auto container = component.getStatusContainer();
        if (!container.assigned())
        {
            statusContainerBlock->setVisible(false);
            if (statusSep) statusSep->setVisible(false);
            return;
        }

        const auto statuses = container.getStatuses();
        if (!statuses.assigned())
        {
            statusContainerBlock->setVisible(false);
            if (statusSep) statusSep->setVisible(false);
            return;
        }

        auto* title = new QLabel(tr("Status"), statusContainerBlock);
        title->setStyleSheet(GUIStyles::SMALL_CAPS_LABEL);
        layout->addWidget(title);

        for (const auto& [statusName, statusEnum] : statuses)
        {
            const QString name  = QString::fromStdString(statusName.toStdString());
            const QString value = QString::fromStdString(statusEnum.getValue().toStdString());

            QString dotColor;

            if (value == "Ok")              dotColor = GUIConstants::Colors::SUCCESS;
            else if (value == "Warning")    dotColor = GUIConstants::Colors::WARNING;
            else if (value == "Error")      dotColor = GUIConstants::Colors::ERROR_FG;
            else                            dotColor = GUIConstants::Colors::NEUTRAL;

            QString statusMessage;
            try
            {
                const auto msg = container.getStatusMessage(statusName);
                if (msg.assigned())
                    statusMessage = QString::fromStdString(msg.toStdString()).trimmed();
            }
            catch (...) {}

            auto* block = new QWidget(statusContainerBlock);
            auto* vl    = new QVBoxLayout(block);
            vl->setContentsMargins(0, 0, 0, 0);
            vl->setSpacing(4);

            auto* lineRow = new QWidget(block);
            auto* rl      = new QHBoxLayout(lineRow);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(6);

            auto* nameLbl = new QLabel(name, lineRow);
            nameLbl->setStyleSheet(GUIStyles::CAPTION_LABEL);

            auto* dot = new QLabel("●", lineRow);
            dot->setStyleSheet(GUIStyles::statusDotStyle(dotColor));

            auto* valueLbl = new QLabel(value, lineRow);
            valueLbl->setStyleSheet(GUIStyles::VALUE_LABEL);

            rl->addWidget(nameLbl);
            rl->addWidget(dot);
            rl->addWidget(valueLbl);
            rl->addStretch();

            vl->addWidget(lineRow);

            if (!statusMessage.isEmpty())
                addStatusMessageToLayout(vl, statusMessage, block);

            layout->addWidget(block);
        }
    }
    catch (...) {}

    const bool hasStatuses = layout->count() > 0;
    statusContainerBlock->setVisible(hasStatuses);
    if (statusSep) statusSep->setVisible(hasStatuses);
}

void ComponentWidget::updateTags()
{
    if (!tagsRow)
        return;

    auto* layout = static_cast<QHBoxLayout*>(tagsRow->layout());
    while (layout->count())
    {
        auto* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }

    int idx = 0;
    try
    {
        auto tags = component.getTags();
        for (const auto& tagObj : tags.getList())
        {
            const QString tagName = QString::fromStdString(tagObj.toStdString());
            const auto& c = GUIConstants::Colors::TAG_PALETTE[idx % GUIConstants::Colors::TAG_PALETTE_SIZE];

            auto* bubble = new QWidget(tagsRow);
            bubble->setAttribute(Qt::WA_StyledBackground, true);
            bubble->setStyleSheet(GUIStyles::tagBubbleStyle(c.bg));

            auto* bl = new QHBoxLayout(bubble);
            bl->setContentsMargins(8, 3, 8, 3);
            bl->setSpacing(4);

            auto* nameLbl = new QLabel(tagName, bubble);
            nameLbl->setStyleSheet(GUIStyles::tagBubbleLabelStyle(c.fg));

            bl->addWidget(nameLbl);
            layout->addWidget(bubble);
            ++idx;
        }
    }
    catch (...) {}

    layout->addStretch();
}

bool ComponentWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == statusLabel && event->type() == QEvent::MouseButtonDblClick)
    {
        try { component.setActive(!component.getActive()); }
        catch (...) {}
        return true;
    }

    if (tabs && obj == tabs->tabBar() && event->type() == QEvent::MouseButtonDblClick)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            const int idx = tabs->tabBar()->tabAt(mouseEvent->pos());
            if (idx >= 0)
            {
                detachTabToWindow(idx);
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ComponentWidget::detachTabToWindow(int index)
{
    if (!tabs || index < 0 || index >= tabs->count())
        return;

    const QString tabTitle = tabs->tabText(index);
    QWidget* page = tabs->widget(index);
    if (!page)
        return;

    tabs->removeTab(index);

    auto* window = new QMainWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);

    const QString componentName = nameLabel ? nameLabel->text() : QString();
    window->setWindowTitle(componentName.isEmpty()
                               ? tabTitle
                               : QStringLiteral("%1 \u2014 %2").arg(componentName, tabTitle));

    if (!treeIconName.isEmpty())
    {
        const QIcon ico = IconProvider::instance().icon(treeIconName);
        if (ico.availableSizes().size() > 0)
            window->setWindowIcon(ico);
    }

    page->setParent(window);
    window->setCentralWidget(page);
    page->show();
    window->resize(800, 600);
    window->show();
}

void ComponentWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != component)
        return;

    try
    {
        const auto eventId = static_cast<daq::CoreEventId>(args.getEventId());

        if (eventId == daq::CoreEventId::AttributeChanged)
        {
            const daq::StringPtr attrName = args.getParameters().get("AttributeName");
            const std::string name = attrName.toStdString();
            if (name == "Name" || name == "Active" || name == "Description")
                updateStatus();
        }
        else if (eventId == daq::CoreEventId::StatusChanged)
        {
            updateStatusContainer();
        }
        else if (eventId == daq::CoreEventId::TagsChanged)
        {
            updateTags();
        }
    }
    catch (...) {}
}
