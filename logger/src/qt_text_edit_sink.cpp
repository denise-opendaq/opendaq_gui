#include "logger/qt_text_edit_sink.h"
#include <QTextEdit>
#include <QMetaObject>
#include <QString>
#include <spdlog/details/log_msg.h>
#include <coretypes/ctutils.h>

using namespace daq;

// QTextEditSpdlogSink implementation
QTextEditSpdlogSink::QTextEditSpdlogSink(QTextEdit* textEdit)
    : textEdit(textEdit)
{
}

void QTextEditSpdlogSink::sink_it_(const spdlog::details::log_msg& msg)
{
    if (!textEdit)
        return;

    // Format the message
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    QString message = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));

    // Remove trailing newline if present
    if (message.endsWith('\n'))
        message.chop(1);

    // Use QMetaObject::invokeMethod to ensure thread-safe GUI updates
    QMetaObject::invokeMethod(textEdit, [this, message]() {
        textEdit->append(message);
    }, Qt::QueuedConnection);
}

void QTextEditSpdlogSink::flush_()
{
    // QTextEdit doesn't need explicit flushing
}

// QTextEditLoggerSink implementation
QTextEditLoggerSink::QTextEditLoggerSink()
{
    auto* textEdit = new QTextEdit();
    textEdit->setReadOnly(true);

    qtSink = std::make_shared<QTextEditSpdlogSink>(textEdit);
    sink = qtSink;

    // Set default pattern
    sink->set_pattern("[tid: %t]%+");
}

ErrCode QTextEditLoggerSink::setLevel(LogLevel level)
{
    sink->set_level(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

ErrCode QTextEditLoggerSink::getLevel(LogLevel* level)
{
    if (level == nullptr)
    {
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");
    }

    *level = static_cast<LogLevel>(sink->level());
    return OPENDAQ_SUCCESS;
}

ErrCode QTextEditLoggerSink::shouldLog(LogLevel level, Bool* willLog)
{
    if (willLog == nullptr)
    {
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");
    }

    *willLog = sink->should_log(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

ErrCode QTextEditLoggerSink::setPattern(IString* pattern)
{
    OPENDAQ_PARAM_NOT_NULL(pattern);
    const ErrCode errCode = daqTry([&]()
    {
        sink->set_pattern(toStdString(pattern));
    });
    OPENDAQ_RETURN_IF_FAILED(errCode, "Failed to set pattern for logger sink");
    return errCode;
}

ErrCode QTextEditLoggerSink::flush()
{
    const ErrCode errCode = daqTry([&]()
    {
        sink->flush();
    });
    OPENDAQ_RETURN_IF_FAILED(errCode, "Failed to flush logger sink");
    return errCode;
}

ErrCode QTextEditLoggerSink::getSinkImpl(SinkPtr* sinkImp)
{
    if (sinkImp == nullptr)
       return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "SinkImp out-parameter must not be null");
    *sinkImp = sink;
    return OPENDAQ_SUCCESS;
}

ErrCode QTextEditLoggerSink::getTextEdit(QTextEdit** textEdit)
{
    if (textEdit == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "TextEdit out-parameter must not be null");

    *textEdit = qtSink ? qtSink->getTextEdit() : nullptr;
    return OPENDAQ_SUCCESS;
}

LoggerSinkPtr createQTextEditLoggerSink()
{
    return daq::createWithImplementation<ILoggerSink, QTextEditLoggerSink>();
}