#include "widgets/input_port_signal_selector.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include "context/icon_provider.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <opendaq/instance_ptr.h>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

namespace
{

const char* kComboStyle =
    "QComboBox {"
    "  background: #f9fafb;"
    "  border: 1px solid #e5e7eb;"
    "  border-radius: 10px;"
    "  padding: 9px 12px;"
    "  color: #111827;"
    "  font-size: 13px;"
    "}"
    "QComboBox:focus {"
    "  border-color: #cbd5e1;"
    "  background: #ffffff;"
    "}"
    "QComboBox::drop-down {"
    "  border: none;"
    "  width: 24px;"
    "}";

const char* kCardStyle =
    "QFrame#inputPortCard {"
    "  background: #ffffff;"
    "  border: 1px solid #e5e7eb;"
    "  border-radius: 12px;"
    "}";

} // namespace

InputPortSignalSelector::InputPortSignalSelector(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , inputPort(inputPort)
    , componentTree(componentTree)
    , signalComboBox(nullptr)
    , disconnectButton(nullptr)
    , showGroupBox(true)
{
    setupSignalSelection();

    if (inputPort.assigned())
    {
        try
        {
            *AppContext::DaqEvent() += daq::event(this, &InputPortSignalSelector::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to subscribe to input port events: {}", e.what());
        }
    }

    populateSignals();
}

InputPortSignalSelector::~InputPortSignalSelector()
{
    if (inputPort.assigned())
    {
        try
        {
            *AppContext::DaqEvent() -= daq::event(this, &InputPortSignalSelector::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to unsubscribe from input port events: {}", e.what());
        }
    }
}

void InputPortSignalSelector::setShowGroupBox(bool show)
{
    if (showGroupBox == show)
        return;

    showGroupBox = show;

    if (cardFrame)
        cardFrame->setVisible(show);
}

void InputPortSignalSelector::setGroupBoxTitle(const QString& title)
{
    if (cardFrame)
    {
        if (auto* titleLbl = cardFrame->findChild<QLabel*>(QStringLiteral("inputPortTitle")))
            titleLbl->setText(title);
    }
}

void InputPortSignalSelector::setupSignalSelection()
{
    QWidget* rowHost = this;

    if (showGroupBox)
    {
        cardFrame = new QFrame(this);
        cardFrame->setObjectName(QStringLiteral("inputPortCard"));
        cardFrame->setAttribute(Qt::WA_StyledBackground, true);
        cardFrame->setStyleSheet(QLatin1String(kCardStyle));
        rowHost = cardFrame;

        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->addWidget(cardFrame);
    }

    auto* row = new QHBoxLayout(rowHost);
    row->setContentsMargins(showGroupBox ? 16 : 0, showGroupBox ? 12 : 0, showGroupBox ? 16 : 0, showGroupBox ? 12 : 0);
    row->setSpacing(12);

    if (showGroupBox)
    {
        const QString portName = inputPort.assigned()
            ? QString::fromStdString(inputPort.getName().toStdString())
            : QStringLiteral("Input Port");

        auto* titleLbl = new QLabel(portName, rowHost);
        titleLbl->setObjectName(QStringLiteral("inputPortTitle"));
        titleLbl->setMinimumWidth(120);
        titleLbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        titleLbl->setStyleSheet(
            QStringLiteral("color: #111827; font-size: 13px; font-weight: 600;"
                           " background: transparent; border: none;"));
        row->addWidget(titleLbl, 0);
    }

    signalComboBox = new QComboBox(rowHost);
    signalComboBox->setEditable(false);
    signalComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    signalComboBox->setStyleSheet(QLatin1String(kComboStyle));
    connect(signalComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InputPortSignalSelector::onSignalSelected);

    disconnectButton = new QToolButton(rowHost);
    disconnectButton->setIcon(IconProvider::instance().icon(QStringLiteral("unlink")));
    disconnectButton->setToolTip(tr("Disconnect signal"));
    disconnectButton->setAutoRaise(true);
    disconnectButton->setFocusPolicy(Qt::NoFocus);
    disconnectButton->setFixedSize(36, 36);
    disconnectButton->setStyleSheet(
        QStringLiteral("QToolButton {"
                       "  border: 1px solid #e5e7eb;"
                       "  border-radius: 10px;"
                       "  background: #f9fafb;"
                       "  padding: 6px;"
                       "}"
                       "QToolButton:hover { background: #f3f4f6; }"
                       "QToolButton:pressed { background: #e5e7eb; }"
                       "QToolButton:disabled { color: #d1d5db; background: #f9fafb; }"));
    disconnectButton->setEnabled(false);
    connect(disconnectButton, &QToolButton::clicked, this, [this]() { disconnectSignal(); });

    row->addWidget(signalComboBox, 1);
    row->addWidget(disconnectButton, 0);
}

void InputPortSignalSelector::populateSignals()
{
    if (!signalComboBox)
        return;

    signalComboBox->blockSignals(true);

    signalComboBox->clear();

    try
    {
        auto instance = AppContext::Instance()->daqInstance();
        if (!instance.assigned())
        {
            signalComboBox->addItem(tr("No instance available"));
            signalComboBox->blockSignals(false);
            return;
        }

        signalComboBox->addItem(tr("(Disconnect)"));

        auto allSignals = instance.getSignalsRecursive();
        if (!allSignals.assigned())
        {
            signalComboBox->addItem(tr("No signals available"));
            signalComboBox->blockSignals(false);
            return;
        }

        QString currentSignalPath;
        if (inputPort.assigned())
        {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned())
                currentSignalPath = getSignalPath(currentSignal);
        }
        if (disconnectButton)
            disconnectButton->setEnabled(!currentSignalPath.isEmpty());

        for (const auto& signal : allSignals)
        {
            try
            {
                QString path = getSignalPath(signal);
                signalComboBox->addItem(path);

                if (!currentSignalPath.isEmpty() && path == currentSignalPath)
                    signalComboBox->setCurrentIndex(signalComboBox->count() - 1);
            }
            catch (const std::exception&)
            {
            }
        }

        if (signalComboBox->currentIndex() < 0 && currentSignalPath.isEmpty())
            signalComboBox->setCurrentIndex(0);
    }
    catch (const std::exception& e)
    {
        signalComboBox->addItem(tr("Error: %1").arg(e.what()));
    }

    signalComboBox->blockSignals(false);
}

void InputPortSignalSelector::onSignalSelected(int index)
{
    if (index < 0 || index >= signalComboBox->count())
        return;

    if (index == 0)
    {
        if (inputPort.assigned())
        {
            auto currentSignal = inputPort.getSignal();
            if (!currentSignal.assigned())
                return;
        }
        disconnectSignal();
        return;
    }

    QString path = signalComboBox->itemText(index);

    try
    {
        auto instance = AppContext::Instance()->daqInstance();
        if (!instance.assigned())
            return;

        auto component = instance.findComponent(path.toStdString());
        if (!component.assigned())
            return;

        auto signal = component.asPtrOrNull<daq::ISignal>(true);
        if (!signal.assigned())
            return;

        if (inputPort.assigned())
        {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned())
            {
                try
                {
                    if (currentSignal == signal)
                        return;
                }
                catch (const std::exception&)
                {
                }
            }
        }

        connectSignal(signal);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Failed to find signal by path: {} Error: {}", path.toStdString(), e.what());
        populateSignals();
    }
}

void InputPortSignalSelector::connectSignal(const daq::SignalPtr& signal)
{
    try
    {
        if (!inputPort.assigned())
            return;

        inputPort.connect(signal);
        populateSignals();
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to connect signal: %1").arg(e.what()));
        populateSignals();
    }
}

void InputPortSignalSelector::disconnectSignal()
{
    try
    {
        if (!inputPort.assigned())
            return;

        inputPort.disconnect();
        populateSignals();
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to disconnect signal: %1").arg(e.what()));
        populateSignals();
    }
}

void InputPortSignalSelector::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != inputPort)
        return;

    try
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        if (eventId == daq::CoreEventId::SignalConnected || eventId == daq::CoreEventId::SignalDisconnected)
            QMetaObject::invokeMethod(this, "populateSignals", Qt::QueuedConnection);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling core event: {}", e.what());
    }
}

QString InputPortSignalSelector::getSignalPath(const daq::SignalPtr& signal) const
{
    if (!signal.assigned())
        return QStringLiteral("N/A");

    try
    {
        auto globalId = signal.getGlobalId();
        return QString::fromStdString(globalId.toStdString());
    }
    catch (const std::exception& e)
    {
        return tr("Error: %1").arg(e.what());
    }
}
