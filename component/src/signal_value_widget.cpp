#include "component/signal_value_widget.h"
#include "AppContext.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDateTime>

SignalValueWidget::SignalValueWidget(const daq::SignalPtr& sig, QWidget* parent)
    : QWidget(parent)
    , signal(sig)
{
    setupUI();

    // Register with global update scheduler
    AppContext::instance()->updateScheduler()->registerUpdatable(this);

    // Initial update
    onScheduledUpdate();
}

SignalValueWidget::~SignalValueWidget()
{
    // Unregister from global update scheduler
    AppContext::instance()->updateScheduler()->unregisterUpdatable(this);
}

void SignalValueWidget::onScheduledUpdate()
{
    try
    {
        // Update last value
        auto lastValue = getLastValue();

        // Get unit symbol if available
        QString unitSymbol;
        if (signal.assigned())
        {
            auto descriptor = signal.getDescriptor();
            if (descriptor.assigned())
            {
                auto unit = descriptor.getUnit();
                if (unit.assigned())
                {
                    unitSymbol = QString::fromStdString(unit.getSymbol().toStdString());
                }
            }
        }

        // Display value with unit
        if (!unitSymbol.isEmpty() && lastValue != "N/A" && !lastValue.startsWith("Error"))
        {
            valueLabel->setText(QString("%1 %2").arg(lastValue, unitSymbol));
        }
        else
        {
            valueLabel->setText(lastValue);
        }

        // Update signal info
        updateSignalInfo();
    }
    catch (const std::exception& e)
    {
        valueLabel->setText(QString("Error: %1").arg(e.what()));
    }
}

void SignalValueWidget::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);

    // Value group
    auto valueGroup = new QGroupBox("Current Value", this);
    auto valueLayout = new QVBoxLayout(valueGroup);
    valueLabel = new QLabel("N/A", valueGroup);
    valueLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    valueLabel->setAlignment(Qt::AlignCenter);
    valueLayout->addWidget(valueLabel);

    // Signal info group
    auto infoGroup = new QGroupBox("Signal Information", this);
    auto infoLayout = new QVBoxLayout(infoGroup);

    signalNameLabel = new QLabel(infoGroup);
    signalUnitLabel = new QLabel(infoGroup);
    signalTypeLabel = new QLabel(infoGroup);
    signalOriginLabel = new QLabel(infoGroup);

    infoLayout->addWidget(signalNameLabel);
    infoLayout->addWidget(signalUnitLabel);
    infoLayout->addWidget(signalTypeLabel);
    infoLayout->addWidget(signalOriginLabel);

    mainLayout->addWidget(valueGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addStretch();

    setLayout(mainLayout);
}

void SignalValueWidget::updateSignalInfo()
{
    try
    {
        // Update signal name
        if (signal.assigned())
        {
            signalNameLabel->setText(QString("Name: %1").arg(QString::fromStdString(signal.getName().toStdString())));

            auto descriptor = signal.getDescriptor();
            if (descriptor.assigned())
            {
                // Update unit
                auto unit = descriptor.getUnit();
                if (unit.assigned())
                {
                    QString unitSymbol = QString::fromStdString(unit.getSymbol().toStdString());
                    QString unitQuantity = QString::fromStdString(unit.getQuantity().toStdString());
                    signalUnitLabel->setText(QString("Unit: %1 (%2)").arg(unitSymbol, unitQuantity));
                }
                else
                {
                    signalUnitLabel->setText("Unit: N/A");
                }

                // Check if it's a time-domain signal
                if (isTimeDomainSignal())
                {
                    signalTypeLabel->setText("Type: Time Domain Signal");

                    // Show origin if available
                    auto origin = descriptor.getOrigin();
                    if (!origin.toStdString().empty())
                    {
                        signalOriginLabel->setText(QString("Origin: %1").arg(QString::fromStdString(origin.toStdString())));
                    }
                    else
                    {
                        signalOriginLabel->setText("Origin: N/A");
                    }
                }
                else
                {
                    signalTypeLabel->setText("Type: Regular Signal");
                    signalOriginLabel->setText("");
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error updating signal info:" << e.what();
    }
}

bool SignalValueWidget::isTimeDomainSignal()
{
    try
    {
        if (!signal.assigned())
            return false;

        auto descriptor = signal.getDescriptor();
        if (!descriptor.assigned())
            return false;

        auto tickResolution = descriptor.getTickResolution();
        if (!tickResolution.assigned())
            return false;

        auto unit = descriptor.getUnit();
        if (!unit.assigned())
            return false;

        QString unitQuantity = QString::fromStdString(unit.getQuantity().toStdString()).toLower();
        QString unitSymbol = QString::fromStdString(unit.getSymbol().toStdString()).toLower();

        return (unitQuantity == "time" && unitSymbol == "s");
    }
    catch (const std::exception&)
    {
        return false;
    }
}

QString SignalValueWidget::getLastValue()
{
    if (!signal.assigned())
        return "N/A";

    try
    {
        auto lastValue = signal.getLastValue();

        // Check if this is a time-domain signal
        if (isTimeDomainSignal())
        {
            auto descriptor = signal.getDescriptor();
            auto origin = descriptor.getOrigin();

            if (!origin.toStdString().empty() && lastValue.assigned())
            {
                // Convert ticks to timestamp
                auto tickResolution = descriptor.getTickResolution();
                int64_t ticks = lastValue;
                double seconds = static_cast<double>(ticks) *
                               static_cast<double>(tickResolution.getNumerator()) /
                               static_cast<double>(tickResolution.getDenominator());

                // Parse origin timestamp and add seconds
                QString originStr = QString::fromStdString(origin.toStdString());
                QDateTime originTime = QDateTime::fromString(originStr, Qt::ISODate);

                if (originTime.isValid())
                {
                    QDateTime resultTime = originTime.addMSecs(static_cast<qint64>(seconds * 1000));
                    return resultTime.toString(Qt::ISODate);
                }
            }
        }

        // For regular signals or if time conversion fails
        if (lastValue.assigned())
        {
            return QString::fromStdString(lastValue.toString());
        }

        return "N/A";
    }
    catch (const std::exception& e)
    {
        return QString("Error: %1").arg(e.what());
    }
}
