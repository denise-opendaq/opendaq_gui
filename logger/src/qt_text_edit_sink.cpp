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
#include <QScrollBar>
#include <QFrame>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QAction>
#include <QStyle>
#include <QCoreApplication>

#include "logger/qt_text_edit_sink.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include "context/icon_provider.h"
#include "coretypes/ctutils.h"
#include <spdlog/details/log_msg.h>
#include <spdlog/fmt/chrono.h>

// Column indices - order: [tid] [time] [loggername] [level] [message]
enum class LogColumn 
{
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

// Pill-style level badges (INFO / WARN / ERROR, …)
class LevelBadgeDelegate final : public QStyledItemDelegate
{
public:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString text = opt.text;

        // Choose a single accent color per level; render foreground at full
        // opacity and background as the same color with low alpha. This keeps
        // the badges legible against both light and dark surfaces.
        QColor accent(GUIConstants::Colors::NEUTRAL);
        if (text == QStringLiteral("INFO"))
            accent = QColor(GUIConstants::Colors::SUCCESS);
        else if (text == QStringLiteral("DEBUG") || text == QStringLiteral("TRACE"))
            accent = QColor(GUIConstants::Colors::DEBUG_FG);
        else if (text == QStringLiteral("WARN") || text == QStringLiteral("WARNING"))
            accent = QColor(GUIConstants::Colors::WARNING);
        else if (text == QStringLiteral("ERROR") || text == QStringLiteral("CRITICAL"))
            accent = QColor(GUIConstants::Colors::ERROR_FG);

        QColor bg = accent;
        bg.setAlpha(50);

        painter->save();
        QRect r = opt.rect.adjusted(2, 3, -2, -3);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(r, 6, 6);
        painter->setPen(accent);
        painter->setFont(opt.font);
        painter->drawText(r, Qt::AlignCenter, text);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const int w = qMax(52, option.fontMetrics.horizontalAdvance(opt.text) + 24);
        return QSize(w, 24);
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
    setupChromeWidget();
}

void QTableWidgetSpdlogSink::setupTableWidget()
{
    if (!tableWidget)
        return;

    // Set up columns - order: [tid] [time] [loggername] [level] [message]
    tableWidget->setColumnCount(5);
    tableWidget->setHorizontalHeaderLabels(
    {
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
        [this](const QPoint& pos) 
        {
            QMenu menu;
            
            // Add menu items for each column
            for (int i = 0; i < tableWidget->columnCount(); ++i) 
            {
                QString headerText = tableWidget->horizontalHeaderItem(i)->text();
                QAction* action = menu.addAction(headerText);
                action->setCheckable(true);
                action->setChecked(!tableWidget->horizontalHeader()->isSectionHidden(i));
                
                QObject::connect(action, &QAction::triggered, [this, i](bool checked)
                {
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
    tableWidget->setSortingEnabled(false);

    tableWidget->setStyleSheet(GUIStyles::logTableStyle());
    
    // Allow manual column resizing
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // Message column can stretch to fill remaining space
    tableWidget->horizontalHeader()->setSectionResizeMode(static_cast<int>(LogColumn::Level), QHeaderView::ResizeToContents);
    // Message column can stretch to fill remaining space
    tableWidget->horizontalHeader()->setSectionResizeMode(static_cast<int>(LogColumn::Message), QHeaderView::Stretch);

    // Set delegate for Message column to enable word wrap
    tableWidget->setItemDelegateForColumn(static_cast<int>(LogColumn::Message), new MessageColumnDelegate());
    tableWidget->setItemDelegateForColumn(static_cast<int>(LogColumn::Level), new LevelBadgeDelegate());
    
    // Auto-resize rows to fit content (for word wrap in Message column)
    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableWidget->verticalHeader()->setVisible(false);
    
    // Recalculate row heights when Message column is resized.
    // Do NOT call processEvents() here: it can cause reentrant paint on macOS and trigger
    // QIOSurfaceGraphicsBuffer "!isLocked()" assert in Qt Cocoa plugin.
    QObject::connect(tableWidget->horizontalHeader(), 
        &QHeaderView::sectionResized, [this](int logicalIndex, int, int) 
        {
            if (logicalIndex == static_cast<int>(LogColumn::Message)) 
            {
                tableWidget->setUpdatesEnabled(false);
                const int rowCount = tableWidget->rowCount();
                for (int row = 0; row < rowCount; ++row)
                    tableWidget->resizeRowToContents(row);
                tableWidget->setUpdatesEnabled(true);
            }
        });
}

void QTableWidgetSpdlogSink::setupChromeWidget()
{
    if (!tableWidget)
        return;

    containerWidget = new QWidget();
    auto* outer = new QVBoxLayout(containerWidget);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    cardWidget = new QFrame(containerWidget);
    cardWidget->setObjectName(QStringLiteral("logViewerCard"));
    cardWidget->setAttribute(Qt::WA_StyledBackground, true);
    cardWidget->setStyleSheet(GUIStyles::cardStyle("logViewerCard", "QFrame", 12));

    auto* cardLayout = new QVBoxLayout(cardWidget);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto* header = new QWidget(cardWidget);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 12, 16, 10);
    headerLayout->setSpacing(10);

    auto* title = new QLabel(QCoreApplication::translate("LogViewer", "Logs"), header);
    title->setStyleSheet(
        "font-size: 16px; font-weight: 700; color: palette(text); background: transparent; border: none;");

    searchEdit = new QLineEdit(header);
    searchEdit->setPlaceholderText(QCoreApplication::translate("LogViewer", "Search in logs..."));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    searchEdit->setMinimumWidth(120);
    searchEdit->setStyleSheet(GUIStyles::searchLineEditStyle());

    const QString pillUnchecked = GUIStyles::neutralPillStyle();
    const QString pillChecked   = GUIStyles::pillButtonStyle(
        GUIConstants::Colors::ACCENT_SOFT_BG, GUIConstants::Colors::ACCENT, /*bold=*/true);

    filterErrBtn   = new QPushButton(QCoreApplication::translate("LogViewer", "Error"), header);
    filterWarnBtn  = new QPushButton(QCoreApplication::translate("LogViewer", "Warning"), header);
    filterInfoBtn  = new QPushButton(QCoreApplication::translate("LogViewer", "Info"), header);
    filterDebugBtn = new QPushButton(QCoreApplication::translate("LogViewer", "Debug"), header);
    for (auto* b : {filterErrBtn, filterWarnBtn, filterInfoBtn, filterDebugBtn})
    {
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(true);
    }
    filterErrBtn->setChecked(true);
    filterWarnBtn->setChecked(true);
    filterInfoBtn->setChecked(false);
    filterDebugBtn->setChecked(false);
    filterErrBtn->setStyleSheet(pillChecked);
    filterWarnBtn->setStyleSheet(pillChecked);
    filterInfoBtn->setStyleSheet(pillUnchecked);
    filterDebugBtn->setStyleSheet(pillUnchecked);

    pauseBtn = new QToolButton(header);
    pauseBtn->setCheckable(true);
    pauseBtn->setCursor(Qt::PointingHandCursor);
    pauseBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
    pauseBtn->setIconSize(QSize(16, 16));
    pauseBtn->setToolTip(QCoreApplication::translate("LogViewer", "Pause incoming log lines"));
    pauseBtn->setStyleSheet(GUIStyles::iconToolButtonStyle(/*withCheckedState=*/true));

    autoScrollCheck = new QCheckBox(QCoreApplication::translate("LogViewer", "Auto-scroll"), header);
    autoScrollCheck->setChecked(true);
    autoScrollCheck->setStyleSheet(QStringLiteral("color: palette(text); font-size: 13px; spacing: 6px;"));

    auto* clearBtn = new QToolButton(header);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setIcon(IconProvider::instance().icon(QStringLiteral("clear")));
    clearBtn->setIconSize(QSize(16, 16));
    clearBtn->setToolTip(QCoreApplication::translate("LogViewer", "Clear log"));
    clearBtn->setStyleSheet(GUIStyles::iconToolButtonStyle());

    headerLayout->addWidget(title, 0);
    headerLayout->addWidget(searchEdit, 1);
    headerLayout->addWidget(filterErrBtn, 0);
    headerLayout->addWidget(filterWarnBtn, 0);
    headerLayout->addWidget(filterInfoBtn, 0);
    headerLayout->addWidget(filterDebugBtn, 0);
    headerLayout->addSpacing(4);
    headerLayout->addWidget(pauseBtn, 0);
    headerLayout->addWidget(autoScrollCheck, 0);
    headerLayout->addWidget(clearBtn, 0);

    auto syncLevelPillStyles = [this, pillChecked, pillUnchecked]() {
        filterErrBtn->setStyleSheet(filterErrBtn->isChecked() ? pillChecked : pillUnchecked);
        filterWarnBtn->setStyleSheet(filterWarnBtn->isChecked() ? pillChecked : pillUnchecked);
        filterInfoBtn->setStyleSheet(filterInfoBtn->isChecked() ? pillChecked : pillUnchecked);
        filterDebugBtn->setStyleSheet(filterDebugBtn->isChecked() ? pillChecked : pillUnchecked);
    };

    for (auto* b : {filterErrBtn, filterWarnBtn, filterInfoBtn, filterDebugBtn})
    {
        QObject::connect(b, &QPushButton::toggled, tableWidget, [this, syncLevelPillStyles](bool) {
            syncLevelPillStyles();
            applyFilters();
        });
    }

    QObject::connect(pauseBtn, &QToolButton::toggled, tableWidget, [this](bool on) {
        paused = on;
        pauseBtn->setIcon(QApplication::style()->standardIcon(on ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        pauseBtn->setToolTip(on ? QCoreApplication::translate("LogViewer", "Resume log")
                                : QCoreApplication::translate("LogViewer", "Pause incoming log lines"));
    });

    QObject::connect(clearBtn, &QToolButton::clicked, tableWidget, [this]() {
        tableWidget->setRowCount(0);
        applyFilters();
    });

    QObject::connect(searchEdit, &QLineEdit::textChanged, tableWidget, [this](const QString& text) {
        currentFilterText = text;
        applyFilters();
    });

    auto* headerSep = new QFrame(cardWidget);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setStyleSheet(GUIStyles::HAIRLINE_HORIZONTAL);

    auto* footerBar = new QWidget(cardWidget);
    auto* footerLayout = new QHBoxLayout(footerBar);
    footerLayout->setContentsMargins(16, 0, 16, 10);
    footerLayout->setSpacing(0);
    footerLabel = new QLabel(footerBar);
    footerLabel->setStyleSheet(QStringLiteral("color: palette(placeholder-text); font-size: 12px; background: transparent; border: none;"));
    footerLayout->addStretch(1);
    footerLayout->addWidget(footerLabel);

    cardLayout->addWidget(header);
    cardLayout->addWidget(headerSep);
    cardLayout->addWidget(tableWidget, 1);
    cardLayout->addWidget(footerBar);

    outer->addWidget(cardWidget, 1);

    updateFooter();
}

void QTableWidgetSpdlogSink::applyFilters()
{
    if (!tableWidget)
        return;

    for (int row = 0; row < tableWidget->rowCount(); ++row)
    {
        const bool searchOk = currentFilterText.isEmpty() || rowMatchesFilter(row, currentFilterText);
        const bool levelOk  = rowMatchesLevelFilter(row);
        tableWidget->setRowHidden(row, !(searchOk && levelOk));
    }
    updateFooter();
}

bool QTableWidgetSpdlogSink::rowMatchesLevelFilter(int row) const
{
    if (!tableWidget || !filterInfoBtn)
        return true;

    const bool anyOn = filterErrBtn->isChecked() || filterWarnBtn->isChecked() || filterInfoBtn->isChecked()
        || filterDebugBtn->isChecked();
    if (!anyOn)
        return true;

    QTableWidgetItem* item = tableWidget->item(row, static_cast<int>(LogColumn::Level));
    if (!item)
        return true;
    const QString lvl = item->text();

    if (filterErrBtn->isChecked() && (lvl == QStringLiteral("ERROR") || lvl == QStringLiteral("CRITICAL")))
        return true;
    if (filterWarnBtn->isChecked() && (lvl == QStringLiteral("WARN") || lvl == QStringLiteral("WARNING")))
        return true;
    if (filterInfoBtn->isChecked() && lvl == QStringLiteral("INFO"))
        return true;
    if (filterDebugBtn->isChecked() && (lvl == QStringLiteral("DEBUG") || lvl == QStringLiteral("TRACE")))
        return true;
    return false;
}

void QTableWidgetSpdlogSink::updateFooter()
{
    if (!tableWidget || !footerLabel)
        return;

    const int total = tableWidget->rowCount();
    int       visible = 0;
    for (int r = 0; r < total; ++r)
    {
        if (!tableWidget->isRowHidden(r))
            ++visible;
    }
    footerLabel->setText(
        QCoreApplication::translate("LogViewer", "Showing %1 of %2 log entries").arg(visible).arg(total));
}

bool QTableWidgetSpdlogSink::rowMatchesFilter(int row, const QString& searchText) const
{
    if (!tableWidget || searchText.isEmpty())
        return true;

    QString searchLower = searchText.toLower();
    
    // Search in all columns
    for (int col = 0; col < tableWidget->columnCount(); ++col)
    {
        QTableWidgetItem* item = tableWidget->item(row, col);
        if (item && item->text().toLower().contains(searchLower))
            return true;
    }
    
    return false;
}

QString QTableWidgetSpdlogSink::formatTime(const spdlog::log_clock::time_point& time) const
{
    // spdlog::log_clock is typically an alias for std::chrono::system_clock
    // Try to convert directly assuming it's system_clock
    try 
    {
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
    } 
    catch (...) 
    {
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
    QTimer::singleShot(0, tableWidget, [this, msgData]() 
    {
        if (paused)
            return;
        addLogRow(*msgData);
    });
}

void QTableWidgetSpdlogSink::addLogRow(const LogMsgData& msg)
{
    if (!tableWidget || paused)
        return;

    QScrollBar* verticalScrollBar = tableWidget->verticalScrollBar();
    const bool atBottom = !verticalScrollBar
        || verticalScrollBar->value() >= verticalScrollBar->maximum() - 1;
    const bool wantAutoScroll = atBottom && autoScrollCheck && autoScrollCheck->isChecked();

    // Remove old rows if we exceed MAX_ROWS
    int currentRowCount = tableWidget->rowCount();
    if (currentRowCount >= MAX_ROWS)
        tableWidget->removeRow(0);  // Remove oldest row

    // Convert message to display format
    QString loggerName = QString::fromStdString(msg.logger_name);
    QString level = levelToString(msg.level);
    QString time = formatTime(msg.time);
    QString threadId = QString::number(msg.thread_id);
    QString message = QString::fromStdString(msg.payload);

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
    levelItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    tableWidget->setItem(row, static_cast<int>(LogColumn::Level), levelItem);

    // Message column with word wrap enabled (via delegate)
    QTableWidgetItem* messageItem = new QTableWidgetItem(message);
    messageItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    tableWidget->setItem(row, static_cast<int>(LogColumn::Message), messageItem);

    applyFilters();

    if (wantAutoScroll)
        tableWidget->scrollToBottom();
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

daq::ErrCode QTableWidgetLoggerSink::setLevel(daq::LogLevel level)
{
    sink->set_level(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

daq::ErrCode QTableWidgetLoggerSink::getLevel(daq::LogLevel* level)
{
    if (level == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");

    *level = static_cast<daq::LogLevel>(sink->level());
    return OPENDAQ_SUCCESS;
}

daq::ErrCode QTableWidgetLoggerSink::shouldLog(daq::LogLevel level, daq::Bool* willLog)
{
    if (willLog == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "Cannot save return value to a null pointer.");

    *willLog = sink->should_log(static_cast<spdlog::level::level_enum>(level));
    return OPENDAQ_SUCCESS;
}

daq::ErrCode QTableWidgetLoggerSink::setPattern(daq::IString* pattern)
{
    OPENDAQ_PARAM_NOT_NULL(pattern);
    const auto errCode = daq::daqTry([&]()
    {
        sink->set_pattern(toStdString(pattern));
    });

    OPENDAQ_RETURN_IF_FAILED(errCode, "Cannot save return value to a null pointer.");
    return errCode;
}

daq::ErrCode QTableWidgetLoggerSink::flush()
{
    const auto errCode = daq::daqTry([&]()
    {
        sink->flush();
    });
    OPENDAQ_RETURN_IF_FAILED(errCode, "Failed to flush logger sink");
    return errCode;
}

daq::ErrCode QTableWidgetLoggerSink::getSinkImpl(SinkPtr* sinkImp)
{
    if (sinkImp == nullptr)
       return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "SinkImp out-parameter must not be null");
    *sinkImp = sink;
    return OPENDAQ_SUCCESS;
}

daq::ErrCode QTableWidgetLoggerSink::getTableWidget(QTableWidget** tableWidget)
{
    if (tableWidget == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "TableWidget out-parameter must not be null");

    *tableWidget = qtSink ? qtSink->getTableWidget() : nullptr;
    return OPENDAQ_SUCCESS;
}

daq::ErrCode QTableWidgetLoggerSink::getContainerWidget(QWidget** containerWidget)
{
    if (containerWidget == nullptr)
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_ARGUMENT_NULL, "ContainerWidget out-parameter must not be null");

    *containerWidget = qtSink ? qtSink->getContainerWidget() : nullptr;
    return OPENDAQ_SUCCESS;
}

daq::LoggerSinkPtr createQTableWidgetLoggerSink()
{
    return daq::createWithImplementation<daq::ILoggerSink, QTableWidgetLoggerSink>();
}