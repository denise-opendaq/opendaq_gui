#pragma once

#include <QVector>
#include <QPointF>
#include <QWidget>

#include <opendaq/signal_ptr.h>
#include <opendaq/stream_reader_ptr.h>

QT_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QComboBox;
class QLabel;
class QLineSeries;
class QTimer;
class QValueAxis;
QT_END_NAMESPACE

class SignalPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalPlotWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr);
    ~SignalPlotWidget() override;

private:
    void setupReader();
    void updatePlot();

    daq::SignalPtr       signal;
    daq::StreamReaderPtr reader;
    double               tickToMs = 1.0;

    QChart*        chart      = nullptr;
    QChartView*    chartView  = nullptr;
    QLineSeries*   series     = nullptr;
    QValueAxis*    axisX      = nullptr;
    QValueAxis*    axisY      = nullptr;
    QComboBox*     rangeCombo    = nullptr;
    QLabel*        errorLabel    = nullptr;
    QTimer*        timer         = nullptr;

    QLabel*        statLastLabel = nullptr;
    QLabel*        statMinLabel  = nullptr;
    QLabel*        statMaxLabel  = nullptr;

    double          timeRangeSecs = 1.0;
    QVector<QPointF> points;

    std::vector<double>   valBuf;
    std::vector<int64_t>  domainBuf;
};
