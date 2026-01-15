#include "logger/qt_text_edit_sink.h"
#include <QTextEdit>
#include <QMetaObject>
#include <QString>
#include <QColor>
#include <spdlog/details/log_msg.h>
#include <coretypes/ctutils.h>

using namespace daq;

// QTextEditSpdlogSink implementation
QTextEditSpdlogSink::QTextEditSpdlogSink(QTextEdit* textEdit)
    : textEdit(textEdit)
{
}

QString getColorForLevel(spdlog::level::level_enum level)
{
    switch (level)
    {
        case spdlog::level::trace:
            return "#808080"; // gray
        case spdlog::level::debug:
            return "#0066CC"; // light blue
        case spdlog::level::info: 
            return "#009900"; // green
        case spdlog::level::warn:
            return "#CCCC00"; // dark yellow
        case spdlog::level::err:
            return "#FF0000"; // red
        case spdlog::level::critical:
            return "#CC0000"; // dark red
        default:
            return ""; // default system color
    }
}

void QTextEditSpdlogSink::sink_it_(const spdlog::details::log_msg& msg)
{
    if (!textEdit)
        return;

    // Format the message
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    QString message = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));

    QString coloredMessage;
    
    // Check if color_range_start and color_range_end are set by the formatter
    if (msg.color_range_start < msg.color_range_end && 
        msg.color_range_end <= static_cast<size_t>(message.length()))
    {
        QString color = getColorForLevel(msg.level);
        if (!color.isEmpty())
        {
            // Apply color only to the specified range
            QString beforeColor = message.left(static_cast<int>(msg.color_range_start));
            QString colorRange = message.mid(static_cast<int>(msg.color_range_start), 
                                            static_cast<int>(msg.color_range_end - msg.color_range_start));
            QString afterColor = message.mid(static_cast<int>(msg.color_range_end));

            coloredMessage = beforeColor + 
                               QString("<span style=\"color:%1;\">%2</span>").arg(color, colorRange) + 
                               afterColor;
        }
    }

    if (coloredMessage.isEmpty())
        coloredMessage = message;

    // Use QMetaObject::invokeMethod to ensure thread-safe GUI updates
    QMetaObject::invokeMethod(textEdit, [this, coloredMessage]() {
        textEdit->append(coloredMessage);
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