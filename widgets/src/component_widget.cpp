#include "widgets/component_widget.h"
#include "widgets/property_object_view.h"
#include "widgets/status_message_stack.h"
#include "context/AppContext.h"
#include "context/icon_provider.h"
#include "context/QueuedEventHandler.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

    tabs->setStyleSheet(
        "QTabWidget { background: transparent; }"
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar::tab {"
        "  padding: 10px 18px;"
        "  color: #6b7280;"
        "  background: transparent;"
        "  font-size: 13px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #1d4ed8;"
        "  border-bottom: 2px solid #1d4ed8;"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  color: #374151;"
        "  border-bottom: 2px solid #d1d5db;"
        "}"
    );
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
    card->setStyleSheet(
        "QWidget#componentHeaderCard {"
        "  background: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 16px;"
        "}"
    );

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
    nameLabel->setObjectName("componentNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#componentNameLabel {"
        "  font-size: 20px;"
        "  font-weight: 700;"
        "  color: #111827;"
        "}"
    );
    leftLayout->addWidget(nameLabel);

    tagsRow = new QWidget(leftBlock);
    auto* tagsLayout = new QHBoxLayout(tagsRow);
    tagsLayout->setContentsMargins(0, 2, 0, 2);
    tagsLayout->setSpacing(6);
    leftLayout->addWidget(tagsRow);

    descLabel = new QLabel(leftBlock);
    descLabel->setObjectName("componentDescLabel");
    descLabel->setStyleSheet(
        "QLabel#componentDescLabel {"
        "  font-size: 13px;"
        "  color: #6b7280;"
        "}"
    );
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
    statusSep->setStyleSheet("background: #e5e7eb; border: none; max-width: 1px;");
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
        statusLabel->setStyleSheet("color: #16a34a; font-size: 13px; font-weight: 600;");
    }
    else
    {
        statusLabel->setText(tr("● Inactive"));
        statusLabel->setStyleSheet("color: #9ca3af; font-size: 13px; font-weight: 600;");
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
        title->setStyleSheet(
            "color: #6b7280; font-size: 11px; font-weight: 600;"
            " text-transform: uppercase; letter-spacing: 0.5px;");
        layout->addWidget(title);

        for (const auto& [statusName, statusEnum] : statuses)
        {
            const QString name  = QString::fromStdString(statusName.toStdString());
            const QString value = QString::fromStdString(statusEnum.getValue().toStdString());

            QString dotColor;
            
            if (value == "Ok")              dotColor = "#16a34a";
            else if (value == "Warning")    dotColor = "#f59e0b";
            else if (value == "Error")      dotColor = "#dc2626";
            else                            dotColor = "#9ca3af";

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
            nameLbl->setStyleSheet("color: #6b7280; font-size: 13px;");

            auto* dot = new QLabel("●", lineRow);
            dot->setStyleSheet(QString("color: %1; font-size: 10px;").arg(dotColor));

            auto* valueLbl = new QLabel(value, lineRow);
            valueLbl->setStyleSheet("color: #111827; font-size: 13px; font-weight: 500;");

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

    static const struct { const char* bg; const char* fg; } palette[] = {
        {"#dbeafe", "#1d4ed8"}, {"#dcfce7", "#16a34a"}, {"#fef9c3", "#ca8a04"},
        {"#fce7f3", "#be185d"}, {"#ede9fe", "#7c3aed"}, {"#ffedd5", "#ea580c"},
    };

    int idx = 0;
    try
    {
        auto tags = component.getTags();
        for (const auto& tagObj : tags.getList())
        {
            const QString tagName = QString::fromStdString(tagObj.toStdString());
            const auto& c = palette[idx % 6];

            auto* bubble = new QWidget(tagsRow);
            bubble->setAttribute(Qt::WA_StyledBackground, true);
            bubble->setStyleSheet(
                QString("QWidget { background: %1; border-radius: 10px; }").arg(c.bg));

            auto* bl = new QHBoxLayout(bubble);
            bl->setContentsMargins(8, 3, 5, 3);
            bl->setSpacing(4);

            auto* nameLbl = new QLabel(tagName, bubble);
            nameLbl->setStyleSheet(
                QString("color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(c.fg));

            auto* removeBtn = new QPushButton("×", bubble);
            removeBtn->setFlat(true);
            removeBtn->setCursor(Qt::PointingHandCursor);
            removeBtn->setFixedSize(14, 14);
            removeBtn->setStyleSheet(
                QString("QPushButton { color: %1; background: transparent; border: none;"
                        " font-size: 13px; font-weight: 700; padding: 0; }"
                        "QPushButton:hover { color: #111827; }").arg(c.fg));

            connect(removeBtn, &QPushButton::clicked, this, [this, tagName]()
            {
                try
                {
                    component.getTags().asPtr<daq::ITagsPrivate>(true).remove(tagName.toStdString());
                }
                catch (...) {}
                updateTags();
            });

            bl->addWidget(nameLbl);
            bl->addWidget(removeBtn);
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
    return QWidget::eventFilter(obj, event);
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
