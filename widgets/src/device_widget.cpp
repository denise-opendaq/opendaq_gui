#include "widgets/device_widget.h"
#include "widgets/property_object_view.h"
#include "widgets/device_logs_widget.h"
#include "widgets/status_message_stack.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

DeviceWidget::DeviceWidget(const daq::DevicePtr& dev, QWidget* parent, const QString& treeIcon)
    : ComponentWidget(dev, parent, Qt::Uninitialized, treeIcon)
    , device(dev)
{
    setupUI();

    timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &DeviceWidget::updateCurrentTime);
    timer->start();

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

    addTreeIconToHeaderLayout(cardLayout, card);

    // — Left info block (name / tags / desc / status / opMode) —
    auto* leftBlock = new QWidget(card);
    auto* leftLayout = new QVBoxLayout(leftBlock);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    nameLabel = new QLabel(leftBlock);
    nameLabel->setObjectName("deviceNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#deviceNameLabel {"
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

    auto* opModeCaption = new QLabel(tr("Operation mode:"), statusRow);
    opModeCaption->setStyleSheet("color: #6b7280; font-size: 13px;");

    opModeCombo = new QComboBox(statusRow);
    opModeCombo->setObjectName("opModeCombo");
    opModeCombo->addItem("Unknown",       static_cast<int>(daq::OperationModeType::Unknown));
    opModeCombo->addItem("Idle",          static_cast<int>(daq::OperationModeType::Idle));
    opModeCombo->addItem("Operation",     static_cast<int>(daq::OperationModeType::Operation));
    opModeCombo->addItem("SafeOperation", static_cast<int>(daq::OperationModeType::SafeOperation));
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

    // — Connection status block (reuses inherited statusContainerBlock / statusSep) —
    statusSep = new QFrame(card);
    statusSep->setFrameShape(QFrame::VLine);
    statusSep->setFrameShadow(QFrame::Plain);
    statusSep->setStyleSheet("background: #e5e7eb; border: none; max-width: 1px;");
    statusSep->setVisible(false);
    cardLayout->addWidget(statusSep);

    statusContainerBlock = new QWidget(card);
    statusContainerBlock->setLayout(new QVBoxLayout());
    statusContainerBlock->layout()->setContentsMargins(8, 0, 8, 0);
    static_cast<QVBoxLayout*>(statusContainerBlock->layout())->setSpacing(6);
    static_cast<QVBoxLayout*>(statusContainerBlock->layout())->setAlignment(Qt::AlignTop);
    statusContainerBlock->setVisible(false);
    cardLayout->addWidget(statusContainerBlock, 0, Qt::AlignTop);

    // — Vertical separator before right block —
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
    currentTimeLbl->setStyleSheet(
        "color: #111827; font-size: 13px; font-weight: 500; font-family: monospace;"
    );

    cardLayout->addWidget(rightBlock, 0, Qt::AlignTop);

    // Populate header values
    updateStatus();
    updateTags();
    updateOpModeCombo();
    updateConnectionStatus();

    // Domain / tick frequency (static)
    try
    {
        auto domain = device.getDomain();
        if (domain.assigned())
        {
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

            try
            {
                auto res = domain.getTickResolution();
                if (res.assigned())
                {
                    const int64_t num = res.getNumerator();
                    const int64_t den = res.getDenominator();
                    if (num > 0)
                        tickFreqLabel->setText(formatFrequency(static_cast<double>(den) / static_cast<double>(num)));
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

    // Attributes tab
    {
        auto* propView = new PropertyObjectView(device, nullptr, device);
        tabs->addTab(propView, tr("Attributes"));
        tabs->setCurrentIndex(1);
    }

    // Logs tab
    tabs->addTab(new DeviceLogsWidget(device), tr("Logs"));
}

// ============================================================================
// Live updates
// ============================================================================

void DeviceWidget::updateConnectionStatus()
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
        auto container = device.getConnectionStatusContainer();
        if (!container.assigned())
        {
            statusContainerBlock->setVisible(false);
            if (statusSep) statusSep->setVisible(false);
            return;
        }

        auto statuses = container.getStatuses();
        if (!statuses.assigned())
        {
            statusContainerBlock->setVisible(false);
            if (statusSep) statusSep->setVisible(false);
            return;
        }

        auto* title = new QLabel(tr("Connection Status"), statusContainerBlock);
        title->setStyleSheet(
            "color: #6b7280; font-size: 11px; font-weight: 600;"
            " text-transform: uppercase; letter-spacing: 0.5px;");
        layout->addWidget(title);

        for (const auto& [statusName, statusEnum] : statuses)
        {
            const QString name  = QString::fromStdString(statusName.toStdString());
            const QString value = QString::fromStdString(statusEnum.getValue().toStdString());

            QString dotColor;
            if (value == "Connected")          dotColor = "#16a34a";
            else if (value == "Reconnecting")  dotColor = "#f59e0b";
            else if (value == "Unrecoverable") dotColor = "#dc2626";
            else                               dotColor = "#9ca3af";

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

void DeviceWidget::updateOpModeCombo()
{
    if (!opModeCombo)
        return;

    int currentMode = static_cast<int>(daq::OperationModeType::Unknown);
    try { currentMode = static_cast<int>(device.getOperationMode()); } catch (...) {}

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

        QString originStr;
        try
        {
            auto origin = domain.getOrigin();
            if (origin.assigned())
                originStr = QString::fromStdString(origin.toStdString());
        }
        catch (...) {}

        if (originStr.isEmpty())
            return QString::number(ticks) + tr(" ticks");

        QString isoStr = originStr;
        bool isUtc = isoStr.endsWith('Z') || isoStr.endsWith('z');
        if (isUtc)
            isoStr.chop(1);
        isoStr.replace(' ', 'T');

        QDateTime originDt = QDateTime::fromString(isoStr, Qt::ISODate);
        if (!originDt.isValid())
            originDt = QDateTime::fromString(isoStr, "yyyy-MM-ddTHH:mm:ss");
        if (!originDt.isValid())
            return tr("—");
        if (isUtc)
            originDt.setTimeZone(QTimeZone::UTC);

        const int64_t wholeSeconds = (ticks / den) * num + (ticks % den) * num / den;
        const int64_t nsNum = ((ticks % den) * num % den) * 1'000'000'000LL;
        const int64_t ns    = nsNum / den;

        QDateTime current = originDt.addSecs(wholeSeconds);
        current = current.addMSecs(static_cast<int>(ns / 1'000'000));

        return current.toString("yyyy-MM-dd HH:mm:ss.zzz");
    }
    catch (...)
    {
        return tr("—");
    }
}

QString DeviceWidget::formatFrequency(double hz) const
{
    if (hz >= 1e9) return QString::number(hz / 1e9, 'g', 4) + tr(" GHz");
    if (hz >= 1e6) return QString::number(hz / 1e6, 'g', 4) + tr(" MHz");
    if (hz >= 1e3) return QString::number(hz / 1e3, 'g', 4) + tr(" kHz");
    return QString::number(hz, 'g', 4) + tr(" Hz");
}

// ============================================================================
// Event handling
// ============================================================================

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
