#include "logger/qt_text_edit_sink.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QMenu>
#include <QTimer>
#include <QString>
#include <QColor>
#include <QDateTime>
#include <QTimeZone>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QPainter>
#include <QApplication>
#include <QMetaObject>
#include <spdlog/details/log_msg.h>
#include <spdlog/fmt/chrono.h>
#include <coretypes/ctutils.h>

using namespace daq;

// Column indices - order: [tid] [time] [loggername] [level] [message]
enum class LogColumn {
    ThreadId = 0,
    Time = 1,
    LoggerName = 2,
    Level = 3,
    Message = 4
};

// Delegate for Message column to enable word wrap
class MessageColumnDelegate : public QStyledItemDelegate
{
public:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() == static_cast<int>(LogColumn::Message))
        {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            
            // Use plain text with word wrap
            QTextDocument doc;
            doc.setPlainText(opt.text);
            doc.setTextWidth(opt.rect.width());
            
            painter->save();
            painter->translate(opt.rect.topLeft());
            doc.drawContents(painter);
            painter->restore();
        }
        else
        {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
    
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() == static_cast<int>(LogColumn::Message))
        {
            QTextDocument doc;
            doc.setPlainText(index.data().toString());
            int width = option.rect.width() > 0 ? option.rect.width() : 200; // Default width if not set
            doc.setTextWidth(width);
            int height = static_cast<int>(doc.size().height());
            return QSize(width, qMax(height, option.fontMetrics.height() + 4)); // Minimum height
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }
};

// QTableWidgetSpdlogSink implementation
QTableWidgetSpdlogSink::QTableWidgetSpdlogSink(QTableWidget* tableWidget)
    : tableWidget(tableWidget)
    , containerWidget(nullptr)
    , searchEdit(nullptr)
    , currentFilterText()
{
    setupTableWidget();
    setupSearchWidget();
}

void QTableWidgetSpdlogSink::setupTableWidget()
{
    if (!tableWidget)
        return;

    // Set up columns - order: [tid] [time] [loggername] [level] [message]
    tableWidget->setColumnCount(5);
    tableWidget->setHorizontalHeaderLabels({
        "Thread ID",
        "Time",
        "Logger Name",
        "Level",
        "Message"
    });
    
    // Hide Thread ID column by default
    tableWidget->horizontalHeader()->setSectionHidden(static_cast<int>(LogColumn::ThreadId), true);

    // Make header context menu enabled
    tableWidget->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Connect context menu signal
    QObject::connect(tableWidget->horizontalHeader(), &QHeaderView::customContextMenuRequested,
                     [this](const QPoint& pos) {
                         QMenu menu;
                         
                         // Add menu items for each column
                         for (int i = 0; i < tableWidget->columnCount(); ++i) {
                             QString headerText = tableWidget->horizontalHeaderItem(i)->text();
                             QAction* action = menu.addAction(headerText);
                             action->setCheckable(true);
                             action->setChecked(!tableWidget->horizontalHeader()->isSectionHidden(i));
                             
                             QObject::connect(action, &QAction::triggered, [this, i](bool checked) {
                                 tableWidget->horizontalHeader()->setSectionHidden(i, !checked);
                             });
                         }
                         
                         menu.exec(tableWidget->horizontalHeader()->mapToGlobal(pos));
                     });

    // Set table properties
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setSortingEnabled(true);
    
    // Allow manual column resizing
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // Message column can stretch to fill remaining space
    tableWidget->horizontalHeader()->setSectionResizeMode(static_cast<int>(LogColumn::Message), QHeaderView::Stretch);
    
    // Set delegate for Message column to enable word wrap
    tableWidget->setItemDelegateForColumn(static_cast<int>(LogColumn::Message), new MessageColumnDelegate());
    
    // Auto-resize rows to fit content (for word wrap in Message column)
    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    
    // Recalculate row heights when Message column is resized.
    // Do NOT call processEvents() here: it can cause reentrant paint on macOS and trigger
    // QIOSurfaceGraphicsBuffer "!isLocked()" assert in Qt Cocoa plugin.
    QObject::connect(tableWidget->horizontalHeader(), &QHeaderView::sectionResized,
                     [this](int logicalIndex, int, int) {
                         if (logicalIndex == static_cast<int>(LogColumn::Message)) {
                             tableWidget->setUpdatesEnabled(false);
                             const int rowCount = tableWidget->rowCount();
                             for (int row = 0; row < rowCount; ++row)
                                 tableWidget->resizeRowToContents(row);
                             tableWidget->setUpdatesEnabled(true);
                         }
                     });
}

void QTableWidgetSpdlogSink::setupSearchWidget()
{
    if (!tableWidget)
        return;

    // Create container widget
    containerWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(containerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // Create search line edit
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(5, 5, 5, 0);
    
    QLabel* searchLabel = new QLabel("Search:");
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search in logs...");
    searchEdit->setClearButtonEnabled(true);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit);
    
    layout->addLayout(searchLayout);
    layout->addWidget(tableWidget);

    // Connect search signal
    QObject::connect(searchEdit, &QLineEdit::textChanged, 
                     [this](const QString& text) {
                         currentFilterText = text;
                         filterTable(text);
                     });
}

void QTableWidgetSpdlogSink::filterTable(const QString& searchText)
{
    if (!tableWidget)
        return;

    QString searchLower = searchText.toLower();
    
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        bool match = rowMatchesFilter(row, searchText);
        
        // Show or hide row based on match
        tableWidget->setRowHidden(row, !match && !searchText.isEmpty());
    }
}

bool QTableWidgetSpdlogSink::rowMatchesFilter(int row, const QString& searchText) const
{
    if (!tableWidget || searchText.isEmpty())
        return true;

    QString searchLower = searchText.toLower();
    
    // Search in all columns
    for (int col = 0; col < tableWidget->columnCount(); ++col) {
        QTableWidgetItem* item = tableWidget->item(row, col);
        if (item && item->text().toLower().contains(searchLower)) {
            return true;
        }
    }
    
    return false;
}

QString QTableWidgetSpdlogSink::formatTime(const spdlog::log_clock::time_point& time) const
{
    // spdlog::log_clock is typically an alias for std::chrono::system_clock
    // Try to convert directly assuming it's system_clock
    try {
        // Cast the duration to system_clock duration
        auto duration = time.time_since_epoch();
        auto system_duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(duration);
        auto system_time = std::chrono::system_clock::time_point(system_duration);
        
        auto time_t = std::chrono::system_clock::to_time_t(system_time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            system_time.time_since_epoch()
        ) % 1000;
        
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(time_t, QTimeZone::systemTimeZone());
        QString timeStr = dateTime.toString("yyyy-MM-dd HH:mm:ss");
        timeStr += QString(".%1").arg(ms.count(), 3, 10, QChar('0'));
        return timeStr;
    } catch (...) {
        // Fallback: use current time if conversion fails
        QDateTime dateTime = QDateTime::currentDateTime();
        return dateTime.toString("yyyy-MM-dd HH:mm:ss.zzz");
    }
}

QString QTableWidgetSpdlogSink::levelToString(spdlog::level::level_enum level) const
{
    switch (level)
    {
        case spdlog::level::trace:
            return "TRACE";
        case spdlog::level::debug:
            return "DEBUG";
        case spdlog::level::info:
            return "INFO";
        case spdlog::level::warn:
            return "WARN";
        case spdlog::level::err:
            return "ERROR";
        case spdlog::level::critical:
            return "CRITICAL";
        case spdlog::level::off:
            return "OFF";
        default:
            return "UNKNOWN";
    }
}

QColor QTableWidgetSpdlogSink::getColorForLevel(spdlog::level::level_enum level) const
{
    switch (level)
    {
        case spdlog::level::trace:
            return QColor(128, 128, 128); // gray
        case spdlog::level::debug:
            return QColor(0, 102, 204); // light blue
        case spdlog::level::info:
            return QColor(0, 153, 0); // green
        case spdlog::level::warn:
            return QColor(204, 204, 0); // dark yellow
        case spdlog::level::err:
            return QColor(255, 0, 0); // red
        case spdlog::level::critical:
            return QColor(204, 0, 0); // dark red
        default:
            return QColor(); // default system color
    }
}

void QTableWidgetSpdlogSink::sink_it_(const spdlog::details::log_msg& msg)
{
    if (!tableWidget)
        return;

    // Copy message data (string_view may become invalid)
    // Store in shared_ptr to pass to GUI thread safely
    auto msgData = std::make_shared<LogMsgData>();
    msgData->time = msg.time;
    msgData->level = msg.level;
    msgData->logger_name = std::string(msg.logger_name.data(), msg.logger_name.size());
    msgData->payload = std::string(msg.payload.data(), msg.payload.size());
    msgData->thread_id = msg.thread_id;

    // Use QTimer::singleShot to safely add log from any thread to GUI thread
    QTimer::singleShot(0, tableWidget, [this, msgData]() {
        addLogRow(*msgData);
    });
}

void QTableWidgetSpdlogSink::addLogRow(const LogMsgData& msg)
{
    if (!tableWidget)
        return;

    // Remove old rows if we exceed MAX_ROWS
    int currentRowCount = tableWidget->rowCount();
    if (currentRowCount >= MAX_ROWS) {
        tableWidget->removeRow(0);  // Remove oldest row
    }

    // Convert message to display format
    QString loggerName = QString::fromStdString(msg.logger_name);
    QString level = levelToString(msg.level);
    QString time = formatTime(msg.time);
    QString threadId = QString::number(msg.thread_id);
    QString message = QString::fromStdString(msg.payload);
    QColor levelColor = getColorForLevel(msg.level);

    // Insert new row
    int row = tableWidget->rowCount();
    tableWidget->insertRow(row);

    // Set items in order: [tid] [time] [loggername] [level] [message]
    QTableWidgetItem* threadItem = new QTableWidgetItem(threadId);
    threadItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    tableWidget->setItem(row, static_cast<int>(LogColumn::ThreadId), threadItem);

    QTableWidgetItem* timeItem = new QTableWidgetItem(time);
    timeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    tableWidget->setItem(row, static_cast<int>(LogColumn::Time), timeItem);

    QTableWidgetItem* loggerItem = new QTableWidgetItem(loggerName);
    loggerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    tableWidget->setItem(row, static_cast<int>(LogColumn::LoggerName), loggerItem);

    QTableWidgetItem* levelItem = new QTableWidgetItem(level);
    levelItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (levelColor.isValid()) {
        levelItem->setForeground(levelColor);
    }
    tableWidget->setItem(row, static_cast<int>(LogColumn::Level), levelItem);

    // Message column with word wrap enabled (via delegate)
    QTableWidgetItem* messageItem = new QTableWidgetItem(message);
    messageItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    tableWidget->setItem(row, static_cast<int>(LogColumn::Message), messageItem);
    
    // Check if new row matches current filter and hide if it doesn't
    if (!currentFilterText.isEmpty()) {
        bool matches = rowMatchesFilter(row, currentFilterText);
        tableWidget->setRowHidden(row, !matches);
    }
    
    // Don't resize row immediately - do it in batch at the end for better performance
}

void QTableWidgetSpdlogSink::flush_()
{
    // QTableWidget doesn't need explicit flushing
}

// QTableWidgetLoggerSink implementation
QTableWidgetLoggerSink::QTableWidgetLoggerSink()
{
    auto* tableWidget = new QTableWidget();
    
    qtSink = std::make_shared<QTableWidgetSpdlogSink>(tableWidget);
    sink = qtSink;

    // Pattern is not used for table view, but keep for compatibility
    sink->set_pattern("%+");
}

ErrCode QTableWidgetLoggerSink::setLevel(LogLevel level)
{
    sink->set_level(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

ErrCode QTableWidgetLoggerSink::getLevel(LogLevel* level)
{
    if (level == nullptr)
    {
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");
    }

    *level = static_cast<LogLevel>(sink->level());
    return OPENDAQ_SUCCESS;
}

ErrCode QTableWidgetLoggerSink::shouldLog(LogLevel level, Bool* willLog)
{
    if (willLog == nullptr)
    {
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");
    }

    *willLog = sink->should_log(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

ErrCode QTableWidgetLoggerSink::setPattern(IString* pattern)
{
    OPENDAQ_PARAM_NOT_NULL(pattern);
    const ErrCode errCode = daqTry([&]()
    {
        sink->set_pattern(toStdString(pattern));
    });
    OPENDAQ_RETURN_IF_FAILED(errCode, "Failed to set pattern for logger sink");
    return errCode;
}

ErrCode QTableWidgetLoggerSink::flush()
{
    const ErrCode errCode = daqTry([&]()
    {
        sink->flush();
    });
    OPENDAQ_RETURN_IF_FAILED(errCode, "Failed to flush logger sink");
    return errCode;
}

ErrCode QTableWidgetLoggerSink::getSinkImpl(SinkPtr* sinkImp)
{
    if (sinkImp == nullptr)
       return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "SinkImp out-parameter must not be null");
    *sinkImp = sink;
    return OPENDAQ_SUCCESS;
}

ErrCode QTableWidgetLoggerSink::getTableWidget(QTableWidget** tableWidget)
{
    if (tableWidget == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "TableWidget out-parameter must not be null");

    *tableWidget = qtSink ? qtSink->getTableWidget() : nullptr;
    return OPENDAQ_SUCCESS;
}

ErrCode QTableWidgetLoggerSink::getContainerWidget(QWidget** containerWidget)
{
    if (containerWidget == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "ContainerWidget out-parameter must not be null");

    *containerWidget = qtSink ? qtSink->getContainerWidget() : nullptr;
    return OPENDAQ_SUCCESS;
}

LoggerSinkPtr createQTableWidgetLoggerSink()
{
    return daq::createWithImplementation<ILoggerSink, QTableWidgetLoggerSink>();
}