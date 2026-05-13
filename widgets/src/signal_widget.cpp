#include "widgets/signal_widget.h"
#include "opendaq/signal.h"
#include "widgets/property_object_view.h"
#include "widgets/signal_value_widget.h"
#include "widgets/signal_descriptor_widget.h"
#include "widgets/signal_plot_widget.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include "context/icon_provider.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFrame>
#include <QTimeZone>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

SignalWidget::SignalWidget(const daq::SignalPtr& sig, QWidget* parent)
    : ComponentWidget(sig, parent, Qt::Uninitialized)
    , signal(sig)
{
    setupUI();

    timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &SignalWidget::updateLastValue);
    timer->start();

    if (signal.assigned())
        *AppContext::DaqEvent() += daq::event(this, &SignalWidget::onCoreEvent);
}

SignalWidget::~SignalWidget()
{
    if (signal.assigned())
        *AppContext::DaqEvent() -= daq::event(this, &SignalWidget::onCoreEvent);
}

// ============================================================================
// UI construction
// ============================================================================

QWidget* SignalWidget::buildHeaderCard()
{
    auto* card = new QWidget(this);
    card->setObjectName("signalHeaderCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#signalHeaderCard {"
        "  background: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 16px;"
        "}"
    );

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(16);

    // — Signal icon —
    auto* iconLabel = new QLabel(card);
    iconLabel->setFixedSize(64, 64);
    iconLabel->setAlignment(Qt::AlignCenter);
    {
        const QIcon ico = IconProvider::instance().icon("signal");
        if (!ico.isNull())
            iconLabel->setPixmap(ico.pixmap(52, 52));
        else
            iconLabel->setText("⚡");
    }
    cardLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    // — Left info block (name / tags / desc / status / public) —
    auto* leftBlock = new QWidget(card);
    auto* leftLayout = new QVBoxLayout(leftBlock);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    nameLabel = new QLabel(leftBlock);
    nameLabel->setObjectName("signalNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#signalNameLabel {"
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
    descLabel->setObjectName("signalDescLabel");
    descLabel->setStyleSheet(
        "QLabel#signalDescLabel {"
        "  font-size: 13px;"
        "  color: #6b7280;"
        "}"
    );
    leftLayout->addWidget(descLabel);

    // Status + Public toggle row
    auto* statusRow = new QWidget(leftBlock);
    auto* statusRowLayout = new QHBoxLayout(statusRow);
    statusRowLayout->setContentsMargins(0, 4, 0, 0);
    statusRowLayout->setSpacing(16);

    statusLabel = new QLabel(statusRow);
    statusLabel->setObjectName("signalStatusLabel");
    statusLabel->setCursor(Qt::PointingHandCursor);
    statusLabel->installEventFilter(this);

    auto* publicCaption = new QLabel(tr("Public:"), statusRow);
    publicCaption->setStyleSheet("color: #6b7280; font-size: 13px;");

    publicCheckBox = new QCheckBox(statusRow);
    publicCheckBox->setStyleSheet(
        "QCheckBox { color: #1d4ed8; font-size: 13px; font-weight: 600; }"
    );

    statusRowLayout->addWidget(statusLabel);
    statusRowLayout->addWidget(publicCaption);
    statusRowLayout->addWidget(publicCheckBox);
    statusRowLayout->addStretch();
    leftLayout->addWidget(statusRow);

    cardLayout->addWidget(leftBlock, 1, Qt::AlignTop);

    // — Vertical separator —
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

    // — Vertical separator before right block —
    {
        auto* vLine = new QFrame(card);
        vLine->setFrameShape(QFrame::VLine);
        vLine->setFrameShadow(QFrame::Plain);
        vLine->setStyleSheet("background: #e5e7eb; border: none; max-width: 1px;");
        cardLayout->addWidget(vLine);
    }

    // — Right block: last value —
    auto* rightBlock = new QWidget(card);
    auto* rightLayout = new QVBoxLayout(rightBlock);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(6);
    rightLayout->setAlignment(Qt::AlignTop);

    auto makeInfoRow = [&](const QString& caption, QLabel*& valueOut) {
        auto* row = new QWidget(rightBlock);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(12);

        auto* lbl = new QLabel(caption, row);
        lbl->setStyleSheet("color: #6b7280; font-size: 13px; min-width: 80px;");

        valueOut = new QLabel(row);
        valueOut->setStyleSheet("color: #111827; font-size: 13px; font-weight: 500;");
        valueOut->setTextInteractionFlags(Qt::TextSelectableByMouse);

        rl->addWidget(lbl);
        rl->addWidget(valueOut);
        rl->addStretch();
        rightLayout->addWidget(row);
    };

    makeInfoRow(tr("Last Value"),   lastValueLabel);
    makeInfoRow(tr("Domain Value"), domainValueLabel);
    cardLayout->addWidget(rightBlock, 0, Qt::AlignTop);

    updateStatus();
    updateTags();
    updateStatusContainer();
    updatePublicToggle();
    updateLastValue();

    connect(publicCheckBox, &QCheckBox::clicked, this, [this](bool checked) {
        if (!signal.assigned())
            return;
        try
        {
            signal.setPublic(checked);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("SignalWidget: failed to set public: {}", e.what());
            updatingFromSignal++;
            updatePublicToggle();
            updatingFromSignal--;
        }
    });

    return card;
}

void SignalWidget::populateTabs()
{
    auto* propView = new PropertyObjectView(signal, nullptr, signal);
    if (signal.asPtr<daq::ISignal>(true).getDomainSignal().assigned())
        tabs->addTab(new SignalPlotWidget(signal), tr("Overview"));
    tabs->addTab(propView, tr("Attributes"));

    tabs->addTab(new SignalDescriptorWidget(signal), tr("Descriptor"));
}

// ============================================================================
// Live updates
// ============================================================================

void SignalWidget::updatePublicToggle()
{
    if (!publicCheckBox || !signal.assigned())
        return;

    bool isPublic = false;
    try { isPublic = signal.getPublic(); } catch (...) {}

    updatingFromSignal++;
    publicCheckBox->setChecked(isPublic);
    updatingFromSignal--;
}

void SignalWidget::updateLastValue()
{
    if (lastValueLabel)
        lastValueLabel->setText(formatSignalValue(signal));

    if (domainValueLabel)
    {
        daq::SignalPtr domain;
        try { domain = signal.getDomainSignal(); } catch (...) {}
        domainValueLabel->setText(domain.assigned() ? formatSignalValue(domain) : tr("—"));
    }
}

QString SignalWidget::formatSignalValue(const daq::SignalPtr& sig) const
{
    if (!sig.assigned())
        return tr("—");

    try
    {
        auto lastValue = sig.getLastValue();
        if (!lastValue.assigned())
            return tr("—");

        auto descriptor = sig.getDescriptor();
        if (!descriptor.assigned())
            return QString::fromStdString(lastValue.toString());

        // Time-domain signal: convert ticks to timestamp
        auto tickResolution = descriptor.getTickResolution();
        auto unit           = descriptor.getUnit();
        if (tickResolution.assigned() && unit.assigned())
        {
            const QString qty = QString::fromStdString(unit.getQuantity().toStdString()).toLower();
            const QString sym = QString::fromStdString(unit.getSymbol().toStdString()).toLower();
            if (qty == "time" && sym == "s")
            {
                auto origin = descriptor.getOrigin();
                if (!origin.toStdString().empty())
                {
                    const int64_t ticks = lastValue;
                    const int64_t num   = tickResolution.getNumerator();
                    const int64_t den   = tickResolution.getDenominator();
                    const double  secs  = static_cast<double>(ticks) * static_cast<double>(num)
                                         / static_cast<double>(den);

                    QString isoStr = QString::fromStdString(origin.toStdString());
                    const bool isUtc = isoStr.endsWith('Z') || isoStr.endsWith('z');
                    if (isUtc) isoStr.chop(1);
                    isoStr.replace(' ', 'T');

                    QDateTime originDt = QDateTime::fromString(isoStr, Qt::ISODate);
                    if (!originDt.isValid())
                        originDt = QDateTime::fromString(isoStr, "yyyy-MM-ddTHH:mm:ss");
                    if (originDt.isValid())
                    {
                        if (isUtc) originDt.setTimeZone(QTimeZone::UTC);
                        QDateTime result = originDt.addMSecs(static_cast<qint64>(secs * 1000.0));
                        return result.toString("yyyy-MM-dd HH:mm:ss");
                    }
                }
            }
        }

        // Regular signal: append unit symbol if present
        QString valueStr = QString::fromStdString(lastValue.toString());
        if (unit.assigned())
        {
            const QString sym = QString::fromStdString(unit.getSymbol().toStdString());
            if (!sym.isEmpty())
                valueStr += ' ' + sym;
        }
        return valueStr;
    }
    catch (...)
    {
        return tr("—");
    }
}

// ============================================================================
// Event handling
// ============================================================================

void SignalWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != signal)
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
            else if (name == "Public")
                updatePublicToggle();
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
