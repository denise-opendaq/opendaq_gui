#include "widgets/signal_plot_widget.h"
#include "context/gui_styles.h"

#include <QApplication>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QPalette>
#include <QPen>
#include <QTimer>
#include <QValueAxis>
#include <QVector>
#include <QPointF>
#include <QVBoxLayout>

#include <opendaq/reader_factory.h>
#include <widgets/time_tail_reader/time_tail_reader_factory.h>

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
    rangeLabel->setStyleSheet(GUIStyles::CAPTION_LABEL);

    rangeCombo = new QComboBox(topBar);
    rangeCombo->addItem("0.001");
    rangeCombo->addItem("0.01");
    rangeCombo->addItem("0.1");
    rangeCombo->addItem("1");
    rangeCombo->addItem("10");
    rangeCombo->setEditable(true);
    rangeCombo->setInsertPolicy(QComboBox::NoInsert);
    rangeCombo->setCurrentIndex(3);
    rangeCombo->setStyleSheet(
        "QComboBox { font-size: 13px; padding: 2px 6px; }"
    );
    rangeCombo->lineEdit()->setValidator(
        new QDoubleValidator(0.0001, 86400.0, 4, rangeCombo));

    auto* secLabel = new QLabel(tr("sec"), topBar);
    secLabel->setStyleSheet(GUIStyles::CAPTION_LABEL);

    topLayout->addWidget(rangeLabel);
    topLayout->addWidget(rangeCombo);
    topLayout->addWidget(secLabel);
    topLayout->addStretch();

    // ── Chart ────────────────────────────────────────────────────────────────
    series = new QLineSeries();
    series->setUseOpenGL(false);

    chart = new QChart();
    chart->addSeries(series);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->setBackgroundRoundness(0);

    // Theme chart background / axis labels to follow current palette.
    const QPalette pal = QApplication::palette();
    QBrush bgBrush(pal.color(QPalette::Base));
    chart->setBackgroundBrush(bgBrush);
    chart->setPlotAreaBackgroundBrush(bgBrush);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setBackgroundPen(QPen(Qt::NoPen));

    const QColor axisTextColor = pal.color(QPalette::Text);
    const QColor axisGridColor = pal.color(QPalette::Mid);

    axisX = new QValueAxis();
    axisX->setLabelFormat("%g s");
    axisX->setTitleText(tr("Time"));
    axisX->setTickCount(3);
    axisX->setLabelsColor(axisTextColor);
    axisX->setTitleBrush(axisTextColor);
    axisX->setLinePenColor(axisGridColor);
    axisX->setGridLineColor(axisGridColor);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    axisY = new QValueAxis();
    {
        QString signalName;
        try 
        { 
            signalName = QString::fromStdString(signal.getName().toStdString()); 
        } 
        catch (...) 
        {
        }
        axisY->setTitleText(signalName.isEmpty() ? tr("Value") : signalName);
    }
    axisY->setLabelsColor(axisTextColor);
    axisY->setTitleBrush(axisTextColor);
    axisY->setLinePenColor(axisGridColor);
    axisY->setGridLineColor(axisGridColor);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // ── Error label ───────────────────────────────────────────────────────────
    errorLabel = new QLabel();
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet(GUIStyles::EMPTY_STATE_LABEL);
    errorLabel->hide();

    // ── Card wrapping chart + top bar ─────────────────────────────────────────
    auto* card = new QWidget(this);
    card->setObjectName("signalPlotCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(GUIStyles::cardStyle("signalPlotCard"));
    // ── Stats row (Last / Min / Max) ─────────────────────────────────────────
    auto makeStatCard = [&](const QString& title, QLabel*& valueOut) -> QWidget* 
    {
        auto* cell = new QWidget();
        cell->setAttribute(Qt::WA_StyledBackground, true);
        // Soft card with the muted alt-base bg, used for inline stat cells.
        cell->setStyleSheet(
            QStringLiteral(
                "QWidget { background: palette(alternate-base); border: 1px solid %1; border-radius: 10px; }")
                .arg(GUIStyles::borderColor()));
        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* titleLbl = new QLabel(title, cell);
        titleLbl->setStyleSheet("color: palette(placeholder-text); font-size: 12px; font-weight: 600;"
                                " background: transparent; border: none;");

        valueOut = new QLabel("—", cell);
        valueOut->setStyleSheet("color: palette(text); font-size: 15px; font-weight: 700;"
                                " background: transparent; border: none;");

        cl->addWidget(titleLbl);
        cl->addWidget(valueOut);
        return cell;
    };

    auto* statsRow    = new QWidget();
    auto* statsLayout = new QHBoxLayout(statsRow);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(8);
    statsLayout->addWidget(makeStatCard(tr("Samples"), statSamplesLabel), 1);
    statsLayout->addWidget(makeStatCard(tr("Current"), statLastLabel), 1);
    statsLayout->addWidget(makeStatCard(tr("Min"),     statMinLabel),  1);
    statsLayout->addWidget(makeStatCard(tr("Max"),     statMaxLabel),  1);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(8);

    auto* titleLabel = new QLabel(tr("Signal preview"), card);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: 700; color: palette(text);");
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

    connect(rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) 
    {
        if (idx < 0)
            return;

        bool ok = false;
        const double val = rangeCombo->currentText().toDouble(&ok);
        if (ok && val > 0.0)
        {
            timeRangeSecs = val;
            if (reader.assigned())
                reader.setHistoryDuration(timeRangeSecs);
        }
    });

    connect(rangeCombo->lineEdit(), &QLineEdit::editingFinished, this, [this]() 
    {
        bool ok = false;
        const double val = rangeCombo->currentText().toDouble(&ok);
        if (ok && val > 0.0) 
        {
            timeRangeSecs = val;
            if (reader.assigned())
                reader.setHistoryDuration(timeRangeSecs);
        }
    });
}

SignalPlotWidget::~SignalPlotWidget() = default;

void SignalPlotWidget::setupReader()
{
    reader = daq::TimeTailReaderPtr();
    tickToMs = 1.0;

    try
    {
        reader = daq::TimeTailReader(signal, timeRangeSecs);

        try
        {
            const auto domain = signal.getDomainSignal();
            if (domain.assigned())
            {
                const auto desc = domain.getDescriptor();
                if (desc.assigned())
                {
                    const auto res = desc.getTickResolution();
                    if (res.assigned())
                        tickToMs = 1000.0 * res.getNumerator()
                                   / static_cast<double>(res.getDenominator());
                }
            }
        }
        catch (...) 
        {
        }

        chartView->show();
        errorLabel->hide();
    }
    catch (const std::exception& e)
    {
        reader = daq::TimeTailReaderPtr();
        chartView->hide();
        errorLabel->setText(tr("Cannot read signal: %1").arg(e.what()));
        errorLabel->show();
    }
}

void SignalPlotWidget::updatePlot()
{
    if (!reader.assigned())
        return;

    size_t count = 0;
    try
    {
        const size_t available = reader.getAvailableCount();
        count = available;
        if (available == 0)
            return;

        valBuf.resize(available);
        domainBuf.resize(available);

        reader.readWithDomain(valBuf.data(), domainBuf.data(), &count);
    }
    catch (...)
    {
        setupReader();
        return;
    }

    if (count == 0)
        return;

    const qint64 latestMs = static_cast<qint64>(domainBuf[count - 1] * tickToMs);
    const double latestSec = latestMs / 1000.0;

    QVector<QPointF> displayPoints;
    displayPoints.reserve(static_cast<int>(count));
    for (size_t i = 0; i < count; ++i)
    {
        const double timeSec = domainBuf[i] * tickToMs / 1000.0;
        displayPoints.append({timeSec - latestSec, valBuf[i]});
    }

    series->replace(displayPoints);
    axisX->setRange(-timeRangeSecs, 0.0);

    double yMin = valBuf[0], yMax = valBuf[0];
    for (size_t i = 0; i < count; ++i)
    {
        if (valBuf[i] < yMin) yMin = valBuf[i];
        if (valBuf[i] > yMax) yMax = valBuf[i];
    }
    const double margin = (yMax - yMin) * 0.1 + 1e-9;
    axisY->setRange(yMin - margin, yMax + margin);

    auto fmt = [](double v) 
    { 
        return QString::number(v, 'g', 6); 
    };
    if (statSamplesLabel) statSamplesLabel->setText(QString::number(count));
    if (statLastLabel) statLastLabel->setText(fmt(valBuf[count - 1]));
    if (statMinLabel)  statMinLabel->setText(fmt(yMin));
    if (statMaxLabel)  statMaxLabel->setText(fmt(yMax));
}
