#pragma once

#include <memory>
#include <QMutex>

class QTextEdit;

#include <spdlog/sinks/base_sink.h>
#include <coretypes/baseobject.h>
#include <coretypes/intfs.h>
#include <opendaq/logger_sink.h>
#include <opendaq/logger_sink_base_private.h>
#include <opendaq/logger_sink_ptr.h>

// spdlog sink that writes to QTextEdit
class QTextEditSpdlogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit QTextEditSpdlogSink(QTextEdit* textEdit);

    QTextEdit* getTextEdit() const { return textEdit; }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    QTextEdit* textEdit;
};

// Custom interface for QTextEdit logger sink
DECLARE_OPENDAQ_INTERFACE(IQTextEditLoggerSink, daq::ILoggerSink)
{
    virtual daq::ErrCode INTERFACE_FUNC getTextEdit(QTextEdit** textEdit) = 0;
};

// openDAQ logger sink wrapper with custom interface
class QTextEditLoggerSink : public daq::ImplementationOf<IQTextEditLoggerSink, daq::ILoggerSinkBasePrivate>
{
public:
    using Sink = spdlog::sinks::sink;
    using SinkPtr = std::shared_ptr<Sink>;

    QTextEditLoggerSink();

    // ILoggerSink interface
    daq::ErrCode INTERFACE_FUNC setLevel(daq::LogLevel level) override;
    daq::ErrCode INTERFACE_FUNC getLevel(daq::LogLevel* level) override;
    daq::ErrCode INTERFACE_FUNC shouldLog(daq::LogLevel level, daq::Bool* willLog) override;
    daq::ErrCode INTERFACE_FUNC setPattern(daq::IString* pattern) override;
    daq::ErrCode INTERFACE_FUNC flush() override;

    // ILoggerSinkBasePrivate interface
    daq::ErrCode INTERFACE_FUNC getSinkImpl(SinkPtr* sinkImp) override;

    // IQTextEditLoggerSink interface
    daq::ErrCode INTERFACE_FUNC getTextEdit(QTextEdit** textEdit) override;

private:
    std::shared_ptr<QTextEditSpdlogSink> qtSink;
    SinkPtr sink;
};

// Factory function to create QTextEdit logger sink
daq::LoggerSinkPtr createQTextEditLoggerSink();
