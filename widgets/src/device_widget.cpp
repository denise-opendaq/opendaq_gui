#include "widgets/device_widget.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include "context/icon_provider.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QUrl>
#include <QTabWidget>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

// ============================================================================
// DeviceWidget
// ============================================================================

DeviceWidget::DeviceWidget(const daq::DevicePtr& dev, QWidget* parent)
    : QWidget(parent)
    , device(dev)
{
    setupUI();

    if (device.assigned())
        *AppContext::DaqEvent() += daq::event(this, &DeviceWidget::onCoreEvent);
}

DeviceWidget::~DeviceWidget()
{
    if (device.assigned())
        *AppContext::DaqEvent() -= daq::event(this, &DeviceWidget::onCoreEvent);
}

// ============================================================================
// UI construction
// ============================================================================

void DeviceWidget::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 0);
    root->setSpacing(12);

    root->addWidget(buildHeaderCard());
    root->addWidget(tabs = new QTabWidget(this), 1);

    populateTabs();

    // Style the tab bar to match the design
    tabs->setObjectName("deviceTabs");
    tabs->setStyleSheet(
        "QTabWidget#deviceTabs { background: transparent; }"
        "QTabWidget#deviceTabs::pane { border: none; background: transparent; }"
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

    // Timer for current-time field
    timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &DeviceWidget::updateCurrentTime);
    timer->start();
}

QWidget* DeviceWidget::buildHeaderCard()
{
    auto* card = new QWidget(this);
    card->setObjectName("deviceHeaderCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#deviceHeaderCard {"
        "  background: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 16px;"
        "}"
    );

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(16);

    // — Device icon —
    auto* iconLabel = new QLabel(card);
    iconLabel->setFixedSize(64, 64);
    iconLabel->setAlignment(Qt::AlignCenter);
    {
        QIcon ico = IconProvider::instance().icon("device");
        if (!ico.isNull())
            iconLabel->setPixmap(ico.pixmap(52, 52));
        else
            iconLabel->setText("🖥");
    }
    cardLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    // — Left info block (name / desc / status / opMode) —
    auto* leftBlock = new QWidget(card);
    auto* leftLayout = new QVBoxLayout(leftBlock);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    // Name row
    auto* nameRow = new QWidget(leftBlock);
    auto* nameRowLayout = new QHBoxLayout(nameRow);
    nameRowLayout->setContentsMargins(0, 0, 0, 0);
    nameRowLayout->setSpacing(8);

    nameLabel = new QLabel(leftBlock);
    nameLabel->setObjectName("deviceNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#deviceNameLabel {"
        "  font-size: 20px;"
        "  font-weight: 700;"
        "  color: #111827;"
        "}"
    );
    nameRowLayout->addWidget(nameLabel);
    nameRowLayout->addStretch();
    leftLayout->addWidget(nameRow);

    // Tags row
    tagsRow = new QWidget(leftBlock);
    auto* tagsLayout = new QHBoxLayout(tagsRow);
    tagsLayout->setContentsMargins(0, 2, 0, 2);
    tagsLayout->setSpacing(6);
    leftLayout->addWidget(tagsRow);
    updateTags();

    // Description
    descLabel = new QLabel(leftBlock);
    descLabel->setObjectName("deviceDescLabel");
    descLabel->setStyleSheet(
        "QLabel#deviceDescLabel {"
        "  font-size: 13px;"
        "  color: #6b7280;"
        "}"
    );
    leftLayout->addWidget(descLabel);

    // Status + Operation mode row
    auto* statusRow = new QWidget(leftBlock);
    auto* statusRowLayout = new QHBoxLayout(statusRow);
    statusRowLayout->setContentsMargins(0, 4, 0, 0);
    statusRowLayout->setSpacing(16);

    statusLabel = new QLabel(statusRow);
    statusLabel->setObjectName("deviceStatusLabel");
    statusLabel->setCursor(Qt::PointingHandCursor);
    statusLabel->installEventFilter(this);

    // Operation mode label
    auto* opModeCaption = new QLabel(tr("Operation mode:"), statusRow);
    opModeCaption->setStyleSheet("color: #6b7280; font-size: 13px;");

    opModeCombo = new QComboBox(statusRow);
    opModeCombo->setObjectName("opModeCombo");
    opModeCombo->addItem("Unknown",      static_cast<int>(daq::OperationModeType::Unknown));
    opModeCombo->addItem("Idle",         static_cast<int>(daq::OperationModeType::Idle));
    opModeCombo->addItem("Operation",    static_cast<int>(daq::OperationModeType::Operation));
    opModeCombo->addItem("SafeOperation",static_cast<int>(daq::OperationModeType::SafeOperation));
    opModeCombo->setStyleSheet(
        "QComboBox#opModeCombo {"
        "  color: #1d4ed8;"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  padding: 0 4px;"
        "}"
        "QComboBox#opModeCombo::drop-down { border: none; width: 16px; }"
    );

    statusRowLayout->addWidget(statusLabel);
    statusRowLayout->addWidget(opModeCaption);
    statusRowLayout->addWidget(opModeCombo);
    statusRowLayout->addStretch();
    leftLayout->addWidget(statusRow);

    cardLayout->addWidget(leftBlock, 1, Qt::AlignTop);

    // — Vertical separator —
    {
        auto* vLine = new QFrame(card);
        vLine->setFrameShape(QFrame::VLine);
        vLine->setFrameShadow(QFrame::Plain);
        vLine->setStyleSheet("background: #e5e7eb; border: none; max-width: 1px;");
        cardLayout->addWidget(vLine);
    }

    // — Connection status block —
    connectionStatusBlock = new QWidget(card);
    connectionStatusBlock->setLayout(new QVBoxLayout());
    connectionStatusBlock->layout()->setContentsMargins(8, 0, 8, 0);
    static_cast<QVBoxLayout*>(connectionStatusBlock->layout())->setSpacing(6);
    static_cast<QVBoxLayout*>(connectionStatusBlock->layout())->setAlignment(Qt::AlignTop);
    cardLayout->addWidget(connectionStatusBlock, 0, Qt::AlignTop);
    updateConnectionStatus();

    // — Vertical separator —
    {
        auto* vLine = new QFrame(card);
        vLine->setFrameShape(QFrame::VLine);
        vLine->setFrameShadow(QFrame::Plain);
        vLine->setStyleSheet("background: #e5e7eb; border: none; max-width: 1px;");
        cardLayout->addWidget(vLine);
    }

    // — Right info block (domain / tick freq / current time) —
    auto* rightBlock = new QWidget(card);
    auto* rightLayout = new QVBoxLayout(rightBlock);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(6);
    rightLayout->setAlignment(Qt::AlignTop);

    auto makeInfoRow = [&](const QString& labelText, QLabel*& valueOut) {
        auto* row = new QWidget(rightBlock);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto* lbl = new QLabel(labelText, row);
        lbl->setStyleSheet("color: #6b7280; font-size: 13px; min-width: 110px;");

        valueOut = new QLabel(row);
        valueOut->setStyleSheet("color: #111827; font-size: 13px; font-weight: 500;");
        valueOut->setTextInteractionFlags(Qt::TextSelectableByMouse);

        rowLayout->addWidget(lbl);
        rowLayout->addWidget(valueOut);
        rowLayout->addStretch();
        rightLayout->addWidget(row);
    };

    makeInfoRow(tr("Domain"),         domainLabel);
    makeInfoRow(tr("Tick Frequency"), tickFreqLabel);
    makeInfoRow(tr("Current Time"),   currentTimeLbl);

    cardLayout->addWidget(rightBlock, 0, Qt::AlignTop);

    // Populate header values
    updateStatus();
    updateOpModeCombo();

    // Domain / tick frequency (static — only needs to be read once)
    try
    {
        auto domain = device.getDomain();
        if (domain.assigned())
        {
            // Unit name (e.g. "Time")
            try
            {
                auto unit = domain.getUnit();
                if (unit.assigned())
                {
                    QString unitName;
                    try { unitName = QString::fromStdString(unit.getName()); } catch (...) {}
                    if (unitName.isEmpty())
                        try { unitName = QString::fromStdString(unit.getSymbol()); } catch (...) {}
                    domainLabel->setText(unitName.isEmpty() ? tr("—") : unitName);
                }
            }
            catch (...) {}

            // Tick frequency
            try
            {
                auto res = domain.getTickResolution();
                if (res.assigned())
                {
                    const int64_t num  = res.getNumerator();
                    const int64_t den  = res.getDenominator();
                    if (num > 0)
                    {
                        double freq = static_cast<double>(den) / static_cast<double>(num);
                        tickFreqLabel->setText(formatFrequency(freq));
                    }
                }
            }
            catch (...) {}
        }
    }
    catch (...) {}

    if (domainLabel->text().isEmpty())
        domainLabel->setText(tr("—"));
    if (tickFreqLabel->text().isEmpty())
        tickFreqLabel->setText(tr("—"));

    // Connect op-mode combo
    connect(opModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceWidget::onOpModeChanged);

    return card;
}

void DeviceWidget::populateTabs()
{
    // Overview — PropertyObjectView of device.getInfo()
    {
        try
        {
            auto info = device.getInfo();
            auto* propView = new PropertyObjectView(info);
            tabs->addTab(propView, tr("Overview"));
        }
        catch (...)
        {
            auto* page = new QWidget();
            auto* layout = new QVBoxLayout(page);
            layout->setAlignment(Qt::AlignCenter);
            auto* lbl = new QLabel(tr("No device info available"), page);
            lbl->setStyleSheet("color: #9ca3af; font-size: 14px;");
            layout->addWidget(lbl);
            tabs->addTab(page, tr("Overview"));
        }
    }

    // Attributes tab — full device PropertyObjectView
    {
        auto* propView = new PropertyObjectView(device, nullptr, device);
        tabs->addTab(propView, tr("Attributes"));
        tabs->setCurrentIndex(1); // default to Attributes
    }

    // Signals placeholder
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->setAlignment(Qt::AlignCenter);
        auto* lbl = new QLabel(tr("Signals — coming soon"), page);
        lbl->setStyleSheet("color: #9ca3af; font-size: 14px;");
        layout->addWidget(lbl);
        tabs->addTab(page, tr("Signals"));
    }

    // Logs tab
    {
        auto* page = new QWidget();
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(16, 12, 16, 16);
        pageLayout->setSpacing(8);

        // Single container card (fills remaining space)
        auto* card = new QFrame(page);
        card->setObjectName("logsCard");
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setStyleSheet(
            "QFrame#logsCard {"
            "  background: #ffffff;"
            "  border: 1px solid #e5e7eb;"
            "  border-radius: 12px;"
            "}");

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);

        // Card header: count + last updated + refresh button
        auto* cardHeader = new QWidget(card);
        cardHeader->setAttribute(Qt::WA_StyledBackground, true);
        cardHeader->setStyleSheet("background: transparent; border: none;");
        auto* cardHeaderLayout = new QHBoxLayout(cardHeader);
        cardHeaderLayout->setContentsMargins(16, 12, 12, 12);
        cardHeaderLayout->setSpacing(12);

        auto* countLbl = new QLabel(card);
        countLbl->setStyleSheet(
            "color: #6b7280; font-size: 11px; font-weight: 600;"
            " text-transform: uppercase; background: transparent; border: none;");
        cardHeaderLayout->addWidget(countLbl);
        cardHeaderLayout->addStretch();

        auto* updatedLbl = new QLabel(card);
        updatedLbl->setStyleSheet(
            "color: #9ca3af; font-size: 12px; background: transparent; border: none;");
        cardHeaderLayout->addWidget(updatedLbl);

        auto* refreshBtn = new QToolButton(cardHeader);
        refreshBtn->setCursor(Qt::PointingHandCursor);
        refreshBtn->setIcon(IconProvider::instance().icon("refresh"));
        refreshBtn->setIconSize(QSize(16, 16));
        refreshBtn->setToolTip(tr("Refresh"));
        refreshBtn->setStyleSheet(
            "QToolButton { border: 1px solid #e5e7eb; border-radius: 6px;"
            "  padding: 4px; background: transparent; }"
            "QToolButton:hover { background: #f3f4f6; }");
        cardHeaderLayout->addWidget(refreshBtn);

        cardLayout->addWidget(cardHeader);

        // Header separator
        auto* headerSep = new QFrame(card);
        headerSep->setFrameShape(QFrame::HLine);
        headerSep->setStyleSheet("background: #e5e7eb; border: none; max-height: 1px;");
        cardLayout->addWidget(headerSep);

        // Scrollable list inside card
        auto* scroll = new QScrollArea(card);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

        auto* listWidget = new QWidget();
        listWidget->setStyleSheet("background: transparent;");
        auto* listLayout = new QVBoxLayout(listWidget);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(0);
        listLayout->setAlignment(Qt::AlignTop);

        scroll->setWidget(listWidget);
        cardLayout->addWidget(scroll, 1);

        pageLayout->addWidget(card, 1);

        auto buildList = [this, listLayout, listWidget, countLbl, updatedLbl]()
        {
            // Clear
            while (listLayout->count())
            {
                auto* item = listLayout->takeAt(0);
                delete item->widget();
                delete item;
            }

            updatedLbl->setText(
                tr("Last updated: %1").arg(QTime::currentTime().toString("HH:mm:ss")));

            try
            {
                const auto infos = device.getLogFileInfos();
                if (!infos.assigned() || infos.getCount() == 0)
                {
                    countLbl->setText(tr("No log files"));
                    auto* empty = new QLabel(tr("No log files available"), listWidget);
                    empty->setStyleSheet(
                        "color: #9ca3af; font-size: 13px; padding: 20px 16px;"
                        " background: transparent; border: none;");
                    listLayout->addWidget(empty);
                    return;
                }

                const int count = static_cast<int>(infos.getCount());
                countLbl->setText(tr("%1 log file%2").arg(count).arg(count == 1 ? "" : "s"));

                bool first = true;
                for (const auto& info : infos)
                {
                    // Row separator (skip before first)
                    if (!first)
                    {
                        auto* sep = new QFrame(listWidget);
                        sep->setFrameShape(QFrame::HLine);
                        sep->setStyleSheet(
                            "background: #e5e7eb; border: none; max-height: 1px;");
                        listLayout->addWidget(sep);
                    }
                    first = false;

                    const QString id   = QString::fromStdString(info.getId().toStdString());
                    const QString name = QString::fromStdString(info.getName().toStdString());

                    QString sizeStr;
                    try
                    {
                        const int64_t sz = info.getSize();
                        if (sz < 1024)           sizeStr = QString("%1 B").arg(sz);
                        else if (sz < 1024*1024) sizeStr = QString("%1 KB").arg(sz / 1024);
                        else                     sizeStr = QString("%1 MB").arg(sz / (1024*1024));
                    }
                    catch (...) {}

                    auto* row = new QWidget(listWidget);
                    row->setAttribute(Qt::WA_StyledBackground, true);
                    row->setStyleSheet(
                        "QWidget { background: transparent; border: none; }"
                        "QWidget:hover { background: #f9fafb; }");

                    auto* rowLayout = new QHBoxLayout(row);
                    rowLayout->setContentsMargins(16, 12, 12, 12);
                    rowLayout->setSpacing(12);

                    auto* nameLbl = new QLabel(name, row);
                    nameLbl->setStyleSheet(
                        "color: #111827; font-size: 13px; font-weight: 600;"
                        " background: transparent; border: none;");

                    auto* sizeLbl = new QLabel(sizeStr, row);
                    sizeLbl->setStyleSheet(
                        "color: #9ca3af; font-size: 12px;"
                        " background: transparent; border: none;");

                    auto* openBtn = new QToolButton(row);
                    openBtn->setCursor(Qt::PointingHandCursor);
                    openBtn->setIcon(IconProvider::instance().icon("folder"));
                    openBtn->setIconSize(QSize(16, 16));
                    openBtn->setToolTip(tr("Open in system editor"));
                    openBtn->setStyleSheet(
                        "QToolButton { border: 1px solid #e5e7eb; border-radius: 6px;"
                        "  padding: 5px; background: #f9fafb; }"
                        "QToolButton:hover { background: #eff6ff; border-color: #1d4ed8; }");

                    connect(openBtn, &QToolButton::clicked, this, [this, id, name]()
                    {
                        try
                        {
                            const QString content = QString::fromStdString(
                                device.getLog(id.toStdString()).toStdString());
                            const QString path = QDir::tempPath() + "/" + name;
                            QFile file(path);
                            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                            {
                                file.write(content.toUtf8());
                                file.close();
                                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                            }
                        }
                        catch (const std::exception& e)
                        {
                            const auto loggerComponent = AppContext::LoggerComponent();
                            LOG_W("Failed to open log '{}': {}", id.toStdString(), e.what());
                        }
                    });

                    rowLayout->addWidget(nameLbl);
                    rowLayout->addWidget(sizeLbl);
                    rowLayout->addStretch();
                    rowLayout->addWidget(openBtn);
                    listLayout->addWidget(row);
                }
            }
            catch (...) {}
        };

        buildList();

        connect(refreshBtn, &QToolButton::clicked, page, [buildList]() { buildList(); });

        tabs->addTab(page, tr("Logs"));
    }

    // Diagnostics placeholder
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->setAlignment(Qt::AlignCenter);
        auto* lbl = new QLabel(tr("Diagnostics — coming soon"), page);
        lbl->setStyleSheet("color: #9ca3af; font-size: 14px;");
        layout->addWidget(lbl);
        tabs->addTab(page, tr("Diagnostics"));
    }
}

// ============================================================================
// Live updates
// ============================================================================

void DeviceWidget::updateStatus()
{
    if (!nameLabel)
        return;

    try
    {
        nameLabel->setText(QString::fromStdString(device.getName()));
    }
    catch (...) {}

    try
    {
        const QString desc = QString::fromStdString(device.getDescription());
        descLabel->setText(desc.isEmpty() ? tr("—") : desc);
    }
    catch (...)
    {
        descLabel->setText(tr("—"));
    }

    bool active = true;
    try { active = device.getActive(); } catch (...) {}

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

void DeviceWidget::updateOpModeCombo()
{
    if (!opModeCombo)
        return;

    int currentMode = static_cast<int>(daq::OperationModeType::Unknown);
    try
    {
        currentMode = static_cast<int>(device.getOperationMode());
    }
    catch (...) {}

    updatingFromDevice++;
    for (int i = 0; i < opModeCombo->count(); ++i)
    {
        if (opModeCombo->itemData(i).toInt() == currentMode)
        {
            opModeCombo->setCurrentIndex(i);
            break;
        }
    }
    updatingFromDevice--;
}

void DeviceWidget::updateTags()
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
        auto tags = device.getTags();
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
                    device.getTags().asPtr<daq::ITagsPrivate>(true).remove(tagName.toStdString());
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

void DeviceWidget::updateConnectionStatus()
{
    if (!connectionStatusBlock)
        return;

    // Clear existing rows
    auto* layout = static_cast<QVBoxLayout*>(connectionStatusBlock->layout());
    while (layout->count())
    {
        auto* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }

    try
    {
        auto container = device.getConnectionStatusContainer();
        if (!container.assigned())
            return;

        auto statuses = container.getStatuses();
        if (!statuses.assigned())
            return;

        auto* title = new QLabel(tr("Connection Status"), connectionStatusBlock);
        title->setStyleSheet("color: #6b7280; font-size: 11px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px;");
        layout->addWidget(title);

        for (const auto& [statusName, statusEnum] : statuses)
        {
            const QString name  = QString::fromStdString(statusName.toStdString());
            const QString value = QString::fromStdString(statusEnum.getValue().toStdString());

            QString dotColor;
            if (value == "Connected")          dotColor = "#16a34a";
            else if (value == "Reconnecting")  dotColor = "#f59e0b";
            else if (value == "Unrecoverable") dotColor = "#dc2626";
            else                               dotColor = "#9ca3af"; // Removed / unknown

            auto* row = new QWidget(connectionStatusBlock);
            auto* rl  = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(6);

            auto* nameLbl = new QLabel(name, row);
            nameLbl->setStyleSheet("color: #6b7280; font-size: 13px;");

            auto* dot = new QLabel("●", row);
            dot->setStyleSheet(QString("color: %1; font-size: 10px;").arg(dotColor));

            auto* valueLbl = new QLabel(value, row);
            valueLbl->setStyleSheet("color: #111827; font-size: 13px; font-weight: 500;");

            rl->addWidget(nameLbl);
            rl->addWidget(dot);
            rl->addWidget(valueLbl);
            rl->addStretch();

            layout->addWidget(row);
        }
    }
    catch (...) {}

    // Hide the whole block if there are no statuses to show
    connectionStatusBlock->setVisible(layout->count() > 0);
}

void DeviceWidget::updateCurrentTime()
{
    if (!currentTimeLbl)
        return;
    currentTimeLbl->setText(computeCurrentTime());
}

QString DeviceWidget::computeCurrentTime() const
{
    try
    {
        auto domain = device.getDomain();
        if (!domain.assigned())
            return tr("—");

        auto res = domain.getTickResolution();
        if (!res.assigned())
            return tr("—");

        const int64_t num = res.getNumerator();
        const int64_t den = res.getDenominator();
        if (num <= 0 || den <= 0)
            return tr("—");

        const int64_t ticks = device.getTicksSinceOrigin();

        // origin is an ISO 8601 string like "2021-01-01T00:00:00Z"
        QString originStr;
        try
        {
            auto origin = domain.getOrigin();
            if (origin.assigned())
                originStr = QString::fromStdString(origin.toStdString());
        }
        catch (...) {}

        if (originStr.isEmpty())
        {
            // No origin: just show ticks
            return QString::number(ticks) + tr(" ticks");
        }

        // Parse origin and add fractional seconds
        // Use Qt's ISO 8601 parser; strip trailing 'Z' and convert to UTC
        QString isoStr = originStr;
        bool isUtc = isoStr.endsWith('Z') || isoStr.endsWith('z');
        if (isUtc)
            isoStr.chop(1);
        isoStr.replace(' ', 'T'); // normalise

        QDateTime originDt = QDateTime::fromString(isoStr, Qt::ISODate);
        if (!originDt.isValid())
            originDt = QDateTime::fromString(isoStr, "yyyy-MM-ddTHH:mm:ss");
        if (!originDt.isValid())
            return tr("—");
        if (isUtc)
            originDt.setTimeZone(QTimeZone::UTC);

        // ticks * (num/den) = seconds since origin
        // Split into whole seconds and nanoseconds
        // Use __int128 arithmetic to avoid overflow with nanosecond precision
        // seconds = ticks * num / den
        const int64_t wholeSeconds = (ticks / den) * num + (ticks % den) * num / den;
        // nanoseconds remainder
        const int64_t nsNum = ((ticks % den) * num % den) * 1'000'000'000LL;
        const int64_t ns    = nsNum / den;

        QDateTime current = originDt.addSecs(wholeSeconds);
        const int ms = static_cast<int>(ns / 1'000'000);
        current = current.addMSecs(ms);

        QString timeStr = current.toString("yyyy-MM-dd HH:mm:ss");
        // Append sub-second precision (nanoseconds)
        const int subSec_ns = static_cast<int>(ns % 1'000'000'000);
        if (subSec_ns > 0)
            timeStr += QStringLiteral(".") + QString::number(subSec_ns).rightJustified(9, '0');

        return timeStr;
    }
    catch (...)
    {
        return tr("—");
    }
}

QString DeviceWidget::formatFrequency(double hz) const
{
    if (hz >= 1e9)
        return QString::number(hz / 1e9, 'g', 4) + tr(" GHz");
    if (hz >= 1e6)
        return QString::number(hz / 1e6, 'g', 4) + tr(" MHz");
    if (hz >= 1e3)
        return QString::number(hz / 1e3, 'g', 4) + tr(" kHz");
    return QString::number(hz, 'g', 4) + tr(" Hz");
}

// ============================================================================
// Event handling
// ============================================================================

bool DeviceWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == statusLabel && event->type() == QEvent::MouseButtonDblClick)
    {
        try { device.setActive(!device.getActive()); }
        catch (...) {}
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void DeviceWidget::onOpModeChanged(int index)
{
    if (updatingFromDevice > 0 || !device.assigned())
        return;

    try
    {
        const int modeInt = opModeCombo->itemData(index).toInt();
        device.setOperationMode(static_cast<daq::OperationModeType>(modeInt));
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("DeviceWidget: failed to set operation mode: {}", e.what());
        // Revert combo to actual mode
        updatingFromDevice++;
        updateOpModeCombo();
        updatingFromDevice--;
    }
}

void DeviceWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != device)
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
        else if (eventId == daq::CoreEventId::DeviceOperationModeChanged)
        {
            updateOpModeCombo();
        }
        else if (eventId == daq::CoreEventId::ConnectionStatusChanged)
        {
            updateConnectionStatus();
        }
        else if (eventId == daq::CoreEventId::TagsChanged)
        {
            updateTags();
        }
    }
    catch (...) {}
}
