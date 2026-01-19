#pragma once

#include <memory>
#include <QMutex>
#include <QString>
#include <QColor>

class QTableWidget;
class QWidget;
class QLineEdit;

#include <spdlog/sinks/base_sink.h>
#include <coretypes/baseobject.h>
#include <coretypes/intfs.h>
#include <opendaq/logger_sink.h>
#include <opendaq/logger_sink_base_private.h>
#include <opendaq/logger_sink_ptr.h>

// spdlog sink that writes to QTableWidget
class QTableWidgetSpdlogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit QTableWidgetSpdlogSink(QTableWidget* tableWidget);

    QTableWidget* getTableWidget() const { return tableWidget; }
    QWidget* getContainerWidget() const { return containerWidget; }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    QTableWidget* tableWidget;
    QWidget* containerWidget;
    QLineEdit* searchEdit;
    QString currentFilterText;
    void setupTableWidget();
    void setupSearchWidget();
    void filterTable(const QString& searchText);
    bool rowMatchesFilter(int row, const QString& searchText) const;
    QString formatTime(const spdlog::log_clock::time_point& time) const;
    QString levelToString(spdlog::level::level_enum level) const;
    QColor getColorForLevel(spdlog::level::level_enum level) const;
};

// Custom interface for QTableWidget logger sink
DECLARE_OPENDAQ_INTERFACE(IQTableWidgetLoggerSink, daq::ILoggerSink)
{
    virtual daq::ErrCode INTERFACE_FUNC getTableWidget(QTableWidget** tableWidget) = 0;
    virtual daq::ErrCode INTERFACE_FUNC getContainerWidget(QWidget** containerWidget) = 0;
};

// openDAQ logger sink wrapper with custom interface
class QTableWidgetLoggerSink : public daq::ImplementationOf<IQTableWidgetLoggerSink, daq::ILoggerSinkBasePrivate>
{
public:
    using Sink = spdlog::sinks::sink;
    using SinkPtr = std::shared_ptr<Sink>;

    QTableWidgetLoggerSink();

    // ILoggerSink interface
    daq::ErrCode INTERFACE_FUNC setLevel(daq::LogLevel level) override;
    daq::ErrCode INTERFACE_FUNC getLevel(daq::LogLevel* level) override;
    daq::ErrCode INTERFACE_FUNC shouldLog(daq::LogLevel level, daq::Bool* willLog) override;
    daq::ErrCode INTERFACE_FUNC setPattern(daq::IString* pattern) override;
    daq::ErrCode INTERFACE_FUNC flush() override;

    // ILoggerSinkBasePrivate interface
    daq::ErrCode INTERFACE_FUNC getSinkImpl(SinkPtr* sinkImp) override;

    // IQTableWidgetLoggerSink interface
    daq::ErrCode INTERFACE_FUNC getTableWidget(QTableWidget** tableWidget) override;
    daq::ErrCode INTERFACE_FUNC getContainerWidget(QWidget** containerWidget) override;

private:
    std::shared_ptr<QTableWidgetSpdlogSink> qtSink;
    SinkPtr sink;
};

// Factory function to create QTableWidget logger sink
daq::LoggerSinkPtr createQTableWidgetLoggerSink();
