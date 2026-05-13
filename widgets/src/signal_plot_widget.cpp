#include "widgets/signal_plot_widget.h"

#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QTimer>
#include <QValueAxis>
#include <QVBoxLayout>

#include <opendaq/reader_factory.h>

SignalPlotWidget::SignalPlotWidget(const daq::SignalPtr& sig, QWidget* parent)
    : QWidget(parent)
    , signal(sig)
{
    // ── Top bar ──────────────────────────────────────────────────────────────
    auto* topBar    = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    auto* rangeLabel = new QLabel(tr("Window:"), topBar);
    rangeLabel->setStyleSheet("color: #6b7280; font-size: 13px;");

    rangeCombo = new QComboBox(topBar);
    rangeCombo->addItem("1 ms",   0.001);
    rangeCombo->addItem("10 ms",  0.01);
    rangeCombo->addItem("100 ms", 0.1);
    rangeCombo->addItem("1 s",    1.0);
    rangeCombo->addItem("10 s",   10.0);
    rangeCombo->setCurrentIndex(3);  // default: 1 s
    rangeCombo->setStyleSheet(
        "QComboBox { font-size: 13px; padding: 2px 6px; }"
    );

    topLayout->addWidget(rangeLabel);
    topLayout->addWidget(rangeCombo);
    topLayout->addStretch();

    // ── Chart ────────────────────────────────────────────────────────────────
    series = new QLineSeries();
    series->setUseOpenGL(false);

    chart = new QChart();
    chart->addSeries(series);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->setBackgroundRoundness(0);

    axisX = new QValueAxis();
    axisX->setLabelFormat("%g s");
    axisX->setTitleText(tr("Time"));
    axisX->setTickCount(3);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    

    axisY = new QValueAxis();
    {
        QString signalName;
        try { signalName = QString::fromStdString(signal.getName().toStdString()); } catch (...) {}
        axisY->setTitleText(signalName.isEmpty() ? tr("Value") : signalName);
    }
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // ── Error label ───────────────────────────────────────────────────────────
    errorLabel = new QLabel();
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet("color: #9ca3af; font-size: 14px;");
    errorLabel->hide();

    // ── Card wrapping chart + top bar ─────────────────────────────────────────
    auto* card = new QWidget(this);
    card->setObjectName("signalPlotCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        "QWidget#signalPlotCard {"
        "  background: #ffffff;"
        "  border: 1px solid #e5e7eb;"
        "  border-radius: 16px;"
        "}"
    );
    // ── Stats row (Last / Min / Max) ─────────────────────────────────────────
    auto makeStatCard = [&](const QString& title, QLabel*& valueOut) -> QWidget* {
        auto* cell = new QWidget();
        cell->setAttribute(Qt::WA_StyledBackground, true);
        cell->setStyleSheet(
            "QWidget { background: #f9fafb; border: 1px solid #e5e7eb; border-radius: 10px; }"
        );
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* titleLbl = new QLabel(title, cell);
        titleLbl->setStyleSheet("color: #6b7280; font-size: 12px; font-weight: 600;"
                                " background: transparent; border: none;");

        valueOut = new QLabel("—", cell);
        valueOut->setStyleSheet("color: #111827; font-size: 15px; font-weight: 700;"
                                " background: transparent; border: none;");

        cl->addWidget(titleLbl);
        cl->addWidget(valueOut);
        return cell;
    };

    auto* statsRow    = new QWidget();
    auto* statsLayout = new QHBoxLayout(statsRow);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(8);
    statsLayout->addWidget(makeStatCard(tr("Current"), statLastLabel), 1);
    statsLayout->addWidget(makeStatCard(tr("Min"),     statMinLabel),  1);
    statsLayout->addWidget(makeStatCard(tr("Max"),     statMaxLabel),  1);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(8);

    auto* titleLabel = new QLabel(tr("Signal preview"), card);
    titleLabel->setStyleSheet(
        "font-size: 15px; font-weight: 700; color: #111827;"
    );
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(topBar);
    cardLayout->addWidget(chartView, 1);
    cardLayout->addWidget(errorLabel, 1);
    cardLayout->addWidget(statsRow);

    // ── Root layout ──────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->addWidget(card, 1);

    // ── Reader ───────────────────────────────────────────────────────────────
    setupReader();

    // ── Timer ────────────────────────────────────────────────────────────────
    timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, &SignalPlotWidget::updatePlot);
    timer->start();

    connect(rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        timeRangeSecs = rangeCombo->itemData(idx).toDouble();
        points.clear();
    });
}

SignalPlotWidget::~SignalPlotWidget() = default;

void SignalPlotWidget::setupReader()
{
    reader  = nullptr;
    tickToMs = 1.0;

    try
    {
        reader = daq::StreamReader(signal, daq::SampleType::Float64, daq::SampleType::Int64);

        try
        {
            auto domain = signal.getDomainSignal();
            if (domain.assigned())
            {
                auto desc = domain.getDescriptor();
                if (desc.assigned())
                {
                    auto res = desc.getTickResolution();
                    if (res.assigned())
                        tickToMs = 1000.0 * res.getNumerator()
                                   / static_cast<double>(res.getDenominator());
                }
            }
        }
        catch (...) {}

        chartView->show();
        errorLabel->hide();
    }
    catch (const std::exception& e)
    {
        chartView->hide();
        errorLabel->setText(tr("Cannot read signal: %1").arg(e.what()));
        errorLabel->show();
    }
}

void SignalPlotWidget::updatePlot()
{
    if (!reader.assigned())
        return;

    try
    {
        const size_t available = reader.getAvailableCount();
        size_t count = available;
        if (available > 0)
        {
            valBuf.resize(available);
            domainBuf.resize(available);

            reader.readWithDomain(valBuf.data(), domainBuf.data(), &count, 0);

            for (size_t i = 0; i < count; ++i)
                points.append({domainBuf[i] * tickToMs, valBuf[i]});
        }
        else
        {
            auto status = reader.read(nullptr, &count);
            
        }
    }
    catch (...)
    {
        setupReader();
        return;
    }

    if (points.isEmpty())
        return;

    // Anchor window to latest data timestamp, not wall clock
    const qint64 latestMs = static_cast<qint64>(points.last().x());
    const qint64 windowMs = static_cast<qint64>(timeRangeSecs * 1000.0);
    const qint64 cutoffMs = latestMs - windowMs;

    // Trim old points
    int trimTo = 0;
    while (trimTo < points.size() && static_cast<qint64>(points[trimTo].x()) < cutoffMs)
        ++trimTo;
    if (trimTo > 0)
        points.remove(0, trimTo);

    // Convert to relative seconds for display (0 = latest point)
    const double latestSec = latestMs / 1000.0;
    QVector<QPointF> displayPoints;
    displayPoints.reserve(points.size());
    for (const auto& p : points)
        displayPoints.append({p.x() / 1000.0 - latestSec, p.y()});

    series->replace(displayPoints);
    axisX->setRange(-timeRangeSecs, 0.0);

    double yMin = points[0].y(), yMax = points[0].y();
    for (const auto& p : points)
    {
        if (p.y() < yMin) yMin = p.y();
        if (p.y() > yMax) yMax = p.y();
    }
    const double margin = (yMax - yMin) * 0.1 + 1e-9;
    axisY->setRange(yMin - margin, yMax + margin);

    auto fmt = [](double v) { return QString::number(v, 'g', 6); };
    if (statLastLabel) statLastLabel->setText(fmt(points.last().y()));
    if (statMinLabel)  statMinLabel->setText(fmt(yMin));
    if (statMaxLabel)  statMaxLabel->setText(fmt(yMax));
}
