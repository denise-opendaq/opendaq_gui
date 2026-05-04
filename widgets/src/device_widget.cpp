#include "widgets/device_widget.h"
#include "context/AppContext.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTableWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>

namespace
{
    QString humanBytes(qulonglong bytes)
    {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double size = static_cast<double>(bytes);
        int u = 0;
        while (size >= 1024.0 && u < 4) { size /= 1024.0; ++u; }
        if (u == 0)
            return QString::number(static_cast<qulonglong>(size)) + " B";
        return QString::number(size, 'f', size >= 10.0 ? 1 : 2) + " " + units[u];
    }

    QString opModeToString(daq::OperationModeType mode)
    {
        switch (mode)
        {
            case daq::OperationModeType::Unknown:       return "Unknown";
            case daq::OperationModeType::Idle:          return "Idle";
            case daq::OperationModeType::Operation:     return "Operation";
            case daq::OperationModeType::SafeOperation: return "SafeOperation";
            default:                                    return "Unknown";
        }
    }

    QString connStatusHtml(const QString& status)
    {
        QString color;
        if      (status == "Connected")    color = "#27ae60";
        else if (status == "Reconnecting") color = "#f59e0b";
        else if (status == "Unrecoverable")color = "#ef4444";
        else                               color = "#9aa4b2";
        return QString("<span style='color:%1'>&#9679;</span>&nbsp;%2").arg(color, status.toHtmlEscaped());
    }

    // 1-px vertical separator
    QFrame* makeVSep(QWidget* parent)
    {
        auto* f = new QFrame(parent);
        f->setObjectName("vSep");
        f->setFixedWidth(1);
        return f;
    }

    // 1-px horizontal separator
    QFrame* makeHSep(QWidget* parent)
    {
        auto* f = new QFrame(parent);
        f->setObjectName("hSep");
        f->setFixedHeight(1);
        return f;
    }

    // Styled card container
    QFrame* makeCard(QWidget* parent = nullptr)
    {
        auto* card = new QFrame(parent);
        card->setObjectName("card");
        return card;
    }

    // Card header row: [title label] [stretch] [optional action widget]
    QWidget* makeCardHeader(const QString& title, QWidget* actionWidget, QWidget* parent)
    {
        auto* w = new QWidget(parent);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        auto* lbl = new QLabel(title, w);
        lbl->setObjectName("cardTitle");
        h->addWidget(lbl);
        h->addStretch();

        if (actionWidget)
        {
            actionWidget->setParent(w);
            h->addWidget(actionWidget);
        }
        return w;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

DeviceWidget::DeviceWidget(const daq::DevicePtr& device, QWidget* parent)
    : QWidget(parent)
    , device(device)
    , overviewNameLabel(nullptr)
    , overviewTypeLabel(nullptr)
    , overviewActiveLabel(nullptr)
    , overviewOpModeValue(nullptr)
    , overviewTicksSinceOriginValue(nullptr)
    , overviewCurrentTimeValue(nullptr)
    , overviewConnStatusContainer(nullptr)
    , deviceInfoSerialNumberValue(nullptr)
    , deviceInfoLocationValue(nullptr)
    , deviceInfoConnectionStringValue(nullptr)
    , deviceInfoVendorValue(nullptr)
    , deviceInfoModelValue(nullptr)
    , deviceInfoFirmwareVersionValue(nullptr)
    , deviceInfoHardwareVersionValue(nullptr)
    , deviceInfoDriverVersionValue(nullptr)
    , deviceInfoDomainValue(nullptr)
    , logFilesTable(nullptr)
    , logFilesCountLabel(nullptr)
    , logFilesTotalSizeLabel(nullptr)
{
    setupUi();
    applyStyle();
    refresh();

    AppContext::Instance()->updateScheduler()->registerUpdatable(this);
}

DeviceWidget::~DeviceWidget()
{
    AppContext::Instance()->updateScheduler()->unregisterUpdatable(this);
}

void DeviceWidget::setDevice(const daq::DevicePtr& newDevice)
{
    device = newDevice;
    refresh();
}

void DeviceWidget::onScheduledUpdate()
{
    if (!device.assigned() || !overviewTicksSinceOriginValue || !overviewCurrentTimeValue)
        return;

    try
    {
        const auto ticks = static_cast<qulonglong>(device.getTicksSinceOrigin());
        overviewTicksSinceOriginValue->setText(QString::number(ticks));

        const auto domain = device.getDomain();
        if (!domain.assigned())
            return;

        const auto origin = domain.getOrigin();
        const auto res    = domain.getTickResolution();
        const double resNum = static_cast<double>(res.getNumerator());
        const double resDen = static_cast<double>(res.getDenominator());

        if (!origin.assigned() || origin.getLength() == 0 || resNum <= 0.0 || resDen <= 0.0)
            return;

        auto dt = QDateTime::fromString(
            QString::fromStdString(origin.toStdString()), Qt::ISODate);
        if (!dt.isValid())
            return;

        const double secondsPerTick = resNum / resDen;
        dt = dt.addMSecs(static_cast<qint64>(static_cast<double>(ticks) * secondsPerTick * 1000.0));
        overviewCurrentTimeValue->setText(dt.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    }
    catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

void DeviceWidget::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Row 0: full-width overview card
    root->addWidget(createOverviewCard());

    // Row 1: device info (fixed) | log files (stretches)
    auto* midRow = new QWidget(content);
    auto* midH = new QHBoxLayout(midRow);
    midH->setContentsMargins(0, 0, 0, 0);
    midH->setSpacing(12);
    midH->addWidget(createDeviceInfoCard(), 0);
    midH->addWidget(createLogFilesCard(), 1);
    root->addWidget(midRow);

    root->addStretch();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cards
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DeviceWidget::createOverviewCard()
{
    auto* card = makeCard(this);
    auto* h = new QHBoxLayout(card);
    h->setContentsMargins(20, 16, 20, 16);
    h->setSpacing(24);

    // ── Device image ──────────────────────────────────────────────────────────
    auto* imgLbl = new QLabel(card);
    imgLbl->setObjectName("deviceImage");
    imgLbl->setFixedSize(110, 88);
    imgLbl->setAlignment(Qt::AlignCenter);
    {
        QPixmap pm(":/icons/device.png");
        if (!pm.isNull())
            imgLbl->setPixmap(pm.scaled(80, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    h->addWidget(imgLbl);

    // ── Name / type / status section ─────────────────────────────────────────
    auto* infoW = new QWidget(card);
    auto* infoV = new QVBoxLayout(infoW);
    infoV->setContentsMargins(0, 0, 0, 0);
    infoV->setSpacing(4);
    infoV->addStretch();

    overviewNameLabel = new QLabel("-", infoW);
    overviewNameLabel->setObjectName("deviceName");
    infoV->addWidget(overviewNameLabel);

    overviewTypeLabel = new QLabel("-", infoW);
    overviewTypeLabel->setObjectName("deviceType");
    infoV->addWidget(overviewTypeLabel);

    // "● Active   Operation Mode: SafeOperation ▾"
    auto* statusRow = new QWidget(infoW);
    auto* statusH = new QHBoxLayout(statusRow);
    statusH->setContentsMargins(0, 8, 0, 0);
    statusH->setSpacing(20);

    overviewActiveLabel = new QLabel(statusRow);
    overviewActiveLabel->setObjectName("activeLabel");
    overviewActiveLabel->setTextFormat(Qt::RichText);
    statusH->addWidget(overviewActiveLabel);

    auto* opRow = new QWidget(statusRow);
    auto* opH = new QHBoxLayout(opRow);
    opH->setContentsMargins(0, 0, 0, 0);
    opH->setSpacing(5);
    auto* opKey = new QLabel("Operation Mode:", opRow);
    opKey->setObjectName("opModeKey");
    overviewOpModeValue = new QLabel("-", opRow);
    overviewOpModeValue->setObjectName("opModeValue");
    overviewOpModeValue->setTextFormat(Qt::RichText);
    overviewOpModeValue->setCursor(Qt::PointingHandCursor);
    overviewOpModeValue->installEventFilter(this);
    opH->addWidget(opKey);
    opH->addWidget(overviewOpModeValue);
    statusH->addWidget(opRow);
    statusH->addStretch();

    infoV->addWidget(statusRow);
    infoV->addStretch();
    h->addWidget(infoW, 1);

    // ── Separator ─────────────────────────────────────────────────────────────
    h->addWidget(makeVSep(card));

    // ── Domain (Time) section ─────────────────────────────────────────────────
    auto makeKVSection = [&](QWidget* parent, const QString& sectionTitle,
                             QList<QPair<QString, QLabel**>> kvPairs) -> QWidget*
    {
        auto* w = new QWidget(parent);
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(8);
        v->addStretch();

        auto* titleLbl = new QLabel(sectionTitle, w);
        titleLbl->setObjectName("sectionTitle");
        v->addWidget(titleLbl);

        for (auto& [key, outLabel] : kvPairs)
        {
            auto* row = new QWidget(w);
            auto* rh = new QHBoxLayout(row);
            rh->setContentsMargins(0, 0, 0, 0);
            rh->setSpacing(12);

            auto* k = new QLabel(key, row);
            k->setObjectName("kvKey");
            k->setMinimumWidth(130);
            *outLabel = new QLabel("-", row);
            (*outLabel)->setObjectName("kvValue");
            (*outLabel)->setTextInteractionFlags(Qt::TextSelectableByMouse);

            rh->addWidget(k);
            rh->addWidget(*outLabel, 1);
            v->addWidget(row);
        }
        v->addStretch();
        return w;
    };

    h->addWidget(makeKVSection(card, "Domain (Time)", {
        {"Ticks Since Origin", &overviewTicksSinceOriginValue},
        {"Current Time",       &overviewCurrentTimeValue},
    }));

    // ── Separator ─────────────────────────────────────────────────────────────
    h->addWidget(makeVSep(card));

    // ── Connection Status section ─────────────────────────────────────────────
    auto* connW = new QWidget(card);
    auto* connV = new QVBoxLayout(connW);
    connV->setContentsMargins(0, 0, 0, 0);
    connV->setSpacing(8);
    connV->addStretch();

    auto* connTitle = new QLabel("Connection Status", connW);
    connTitle->setObjectName("sectionTitle");
    connV->addWidget(connTitle);

    // Rows are populated dynamically in refresh() using connectionName as label
    overviewConnStatusContainer = new QWidget(connW);
    overviewConnStatusContainer->setLayout(new QVBoxLayout());
    overviewConnStatusContainer->layout()->setContentsMargins(0, 0, 0, 0);
    static_cast<QVBoxLayout*>(overviewConnStatusContainer->layout())->setSpacing(8);
    connV->addWidget(overviewConnStatusContainer);

    connV->addStretch();
    h->addWidget(connW);

    return card;
}

QWidget* DeviceWidget::createDeviceInfoCard()
{
    auto* card = makeCard(this);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 14, 20, 16);
    v->setSpacing(0);

    auto* menuBtn = new QToolButton(card);
    menuBtn->setObjectName("kebabBtn");
    menuBtn->setText("⋮");
    menuBtn->setFixedSize(28, 28);
    menuBtn->setAutoRaise(true);
    v->addWidget(makeCardHeader("Device Info", menuBtn, card));
    v->addSpacing(8);
    v->addWidget(makeHSep(card));
    v->addSpacing(12);

    auto* grid = new QWidget(card);
    auto* gl = new QGridLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setHorizontalSpacing(16);
    gl->setVerticalSpacing(10);
    gl->setColumnStretch(1, 1);

    struct RowDef { QString key; QLabel** out; bool hasCopy = false; };
    const QList<RowDef> rows = {
        {"Serial Number",     &deviceInfoSerialNumberValue},
        {"Location",          &deviceInfoLocationValue},
        {"Connection String", &deviceInfoConnectionStringValue, true},
        {"Vendor",            &deviceInfoVendorValue},
        {"Model",             &deviceInfoModelValue},
        {"Firmware Version",  &deviceInfoFirmwareVersionValue},
        {"Hardware Version",  &deviceInfoHardwareVersionValue},
        {"Driver Version",    &deviceInfoDriverVersionValue},
        {"Domain",            &deviceInfoDomainValue},
    };

    for (int r = 0; r < rows.size(); ++r)
    {
        const auto& row = rows[r];

        auto* keyLbl = new QLabel(row.key, grid);
        keyLbl->setObjectName("kvKey");
        gl->addWidget(keyLbl, r, 0, Qt::AlignLeft | Qt::AlignVCenter);

        if (row.hasCopy)
        {
            auto* cellW = new QWidget(grid);
            auto* cellH = new QHBoxLayout(cellW);
            cellH->setContentsMargins(0, 0, 0, 0);
            cellH->setSpacing(4);

            *row.out = new QLabel("-", cellW);
            (*row.out)->setObjectName("kvValue");
            (*row.out)->setTextInteractionFlags(Qt::TextSelectableByMouse);
            cellH->addWidget(*row.out, 1);

            auto* copyBtn = new QToolButton(cellW);
            copyBtn->setObjectName("copyBtn");
            copyBtn->setText("⎘");
            copyBtn->setToolTip("Copy");
            copyBtn->setFixedSize(20, 20);
            copyBtn->setAutoRaise(true);
            copyBtn->setCursor(Qt::PointingHandCursor);
            connect(copyBtn, &QToolButton::clicked, this, [this]() {
                if (deviceInfoConnectionStringValue)
                    QApplication::clipboard()->setText(deviceInfoConnectionStringValue->text());
            });
            cellH->addWidget(copyBtn);
            gl->addWidget(cellW, r, 1);
        }
        else
        {
            *row.out = new QLabel("-", grid);
            (*row.out)->setObjectName("kvValue");
            (*row.out)->setTextInteractionFlags(Qt::TextSelectableByMouse);
            gl->addWidget(*row.out, r, 1, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    v->addWidget(grid);
    v->addStretch();
    return card;
}

QWidget* DeviceWidget::createLogFilesCard()
{
    auto* card = makeCard(this);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 14, 20, 16);
    v->setSpacing(0);

    auto* refreshBtn = new QToolButton(card);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setText("↺");
    refreshBtn->setToolTip("Refresh");
    refreshBtn->setFixedSize(28, 28);
    refreshBtn->setAutoRaise(true);
    connect(refreshBtn, &QToolButton::clicked, this, &DeviceWidget::refresh);
    v->addWidget(makeCardHeader("Log Files", refreshBtn, card));
    v->addSpacing(8);
    v->addWidget(makeHSep(card));
    v->addSpacing(8);

    logFilesTable = new QTableWidget(0, 4, card);
    logFilesTable->setHorizontalHeaderLabels({"File ID", "Size", "Modified", ""});
    logFilesTable->verticalHeader()->setVisible(false);
    logFilesTable->setShowGrid(false);
    logFilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logFilesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    logFilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logFilesTable->horizontalHeader()->setStretchLastSection(false);
    logFilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    logFilesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    logFilesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    logFilesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    logFilesTable->setMinimumHeight(160);
    v->addWidget(logFilesTable, 1);

    v->addSpacing(8);
    v->addWidget(makeHSep(card));
    v->addSpacing(8);

    auto* footer = new QWidget(card);
    auto* footerH = new QHBoxLayout(footer);
    footerH->setContentsMargins(0, 0, 0, 0);
    logFilesCountLabel = new QLabel("0 files", footer);
    logFilesCountLabel->setObjectName("footerLabel");
    logFilesTotalSizeLabel = new QLabel("Total size: 0 B", footer);
    logFilesTotalSizeLabel->setObjectName("footerLabel");
    footerH->addWidget(logFilesCountLabel);
    footerH->addStretch();
    footerH->addWidget(logFilesTotalSizeLabel);
    v->addWidget(footer);

    return card;
}


// ─────────────────────────────────────────────────────────────────────────────
// Styling
// ─────────────────────────────────────────────────────────────────────────────

void DeviceWidget::applyStyle()
{
    setStyleSheet(R"(
        QFrame#card {
            background: palette(base);
            border: 1px solid palette(mid);
            border-radius: 10px;
        }

        QFrame#hSep {
            background: palette(mid);
            border: none;
        }

        QFrame#vSep {
            background: palette(mid);
            border: none;
        }

        QLabel#deviceName {
            font-size: 22px;
            font-weight: 700;
        }

        QLabel#deviceType {
            font-size: 13px;
            color: palette(shadow);
        }

        QLabel#activeLabel {
            font-size: 13px;
            font-weight: 600;
        }

        QLabel#opModeKey {
            font-size: 13px;
            color: palette(shadow);
        }

        QLabel#opModeValue {
            color: #3b82f6;
            font-size: 13px;
            font-weight: 600;
        }

        QLabel#sectionTitle {
            font-size: 12px;
            font-weight: 700;
            color: palette(shadow);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        QLabel#cardTitle {
            font-size: 14px;
            font-weight: 700;
        }

        QLabel#kvKey {
            font-size: 13px;
            color: palette(shadow);
        }

        QLabel#kvValue {
            font-size: 13px;
        }

        QLabel#statusValue {
            font-size: 13px;
        }

        QLabel#footerLabel {
            font-size: 12px;
            color: palette(shadow);
        }

        QLabel#deviceImage {
            border: 1px solid palette(mid);
            border-radius: 8px;
            background: palette(window);
        }

        QToolButton#kebabBtn,
        QToolButton#refreshBtn {
            color: palette(shadow);
            font-size: 16px;
            border: none;
        }
        QToolButton#kebabBtn:hover,
        QToolButton#refreshBtn:hover {
            color: palette(text);
        }

        QToolButton#copyBtn {
            color: palette(shadow);
            font-size: 13px;
            border: none;
        }
        QToolButton#copyBtn:hover {
            color: palette(text);
        }

        QTableWidget {
            border: none;
            background: palette(base);
            outline: none;
        }

        QHeaderView::section {
            border: none;
            border-bottom: 1px solid palette(mid);
            background: palette(base);
            padding: 6px 10px;
            font-weight: 600;
            font-size: 12px;
        }

        QScrollArea {
            border: none;
            background: transparent;
        }
    )");
}

// ─────────────────────────────────────────────────────────────────────────────
// Data refresh
// ─────────────────────────────────────────────────────────────────────────────

void DeviceWidget::refresh()
{
    if (!device.assigned())
        return;

    try
    {
        overviewNameLabel->setText(QString::fromStdString(device.getName().toStdString()));

        const bool active = device.getActive();
        overviewActiveLabel->setText(
            active ? "<span style='color:#27ae60'>&#9679;</span>&nbsp;Active"
                   : "<span style='color:#9aa4b2'>&#9679;</span>&nbsp;Inactive");

        const auto info = device.getInfo();
        if (info.assigned())
        {
            overviewTypeLabel->setText(
                QString::fromStdString(info.getDeviceType().getName().toStdString()));

            deviceInfoSerialNumberValue  ->setText(QString::fromStdString(info.getSerialNumber().toStdString()));
            deviceInfoLocationValue      ->setText(QString::fromStdString(info.getLocation().toStdString()));
            deviceInfoConnectionStringValue->setText(QString::fromStdString(info.getConnectionString().toStdString()));
            deviceInfoVendorValue        ->setText(QString::fromStdString(info.getManufacturer().toStdString()));
            deviceInfoModelValue         ->setText(QString::fromStdString(info.getModel().toStdString()));
            deviceInfoFirmwareVersionValue->setText(QString::fromStdString(info.getSoftwareRevision().toStdString()));
            deviceInfoHardwareVersionValue->setText(QString::fromStdString(info.getHardwareRevision().toStdString()));
            deviceInfoDriverVersionValue ->setText(QString::fromStdString(info.getSdkVersion().toStdString()));
            deviceInfoDomainValue        ->setText("Time");
        }

        // Operation mode
        const auto opMode = device.getOperationMode();
        overviewOpModeValue->setText(
            QString("<span style='color:#3b82f6;font-weight:600;'>%1&nbsp;&#x25BE;</span>")
                .arg(opModeToString(opMode).toHtmlEscaped()));

        // Domain time is updated every second via onScheduledUpdate()
        onScheduledUpdate();

        // Connection status — rebuild rows dynamically using connectionName as label
        if (overviewConnStatusContainer)
        {
            QLayoutItem* item;
            while ((item = overviewConnStatusContainer->layout()->takeAt(0)) != nullptr)
            {
                delete item->widget();
                delete item;
            }

            try
            {
                const auto connectionStatuses = device.getConnectionStatusContainer().getStatuses();
                for (const auto& [connectionName, connectionStatus] : connectionStatuses)
                {
                    const QString name  = QString::fromStdString(connectionName.toStdString());
                    const QString value = QString::fromStdString(connectionStatus.getValue().toStdString());

                    auto* row = new QWidget(overviewConnStatusContainer);
                    auto* rh  = new QHBoxLayout(row);
                    rh->setContentsMargins(0, 0, 0, 0);
                    rh->setSpacing(16);

                    auto* keyLbl = new QLabel(name, row);
                    keyLbl->setObjectName("kvKey");

                    auto* valLbl = new QLabel(row);
                    valLbl->setObjectName("statusValue");
                    valLbl->setTextFormat(Qt::RichText);
                    valLbl->setText(connStatusHtml(value));

                    rh->addWidget(keyLbl);
                    rh->addWidget(valLbl, 1);
                    overviewConnStatusContainer->layout()->addWidget(row);
                }
            }
            catch (...) {}
        }

        // Log files table
        logFilesTable->setRowCount(0);
        qulonglong totalSize = 0;
        int fileCount = 0;

        const auto logInfos = device.getLogFileInfos();
        for (size_t i = 0; logInfos.assigned() && i < logInfos.getCount(); ++i)
        {
            const auto li = logInfos[i];
            const QString id   = QString::fromStdString(li.getId().toStdString());
            const QString name = QString::fromStdString(li.getName().toStdString());
            const qulonglong bytes = static_cast<qulonglong>(li.getSize());
            totalSize += bytes;
            ++fileCount;

            const QString sizeStr = humanBytes(bytes);

            const QString rawDate = QString::fromStdString(li.getLastModified().toStdString());
            QString displayDate = rawDate;
            const auto dt = QDateTime::fromString(rawDate, Qt::ISODate);
            if (dt.isValid())
                displayDate = dt.toString("dd.MM.yyyy HH:mm:ss");

            const int row = logFilesTable->rowCount();
            logFilesTable->insertRow(row);
            logFilesTable->setItem(row, 0, new QTableWidgetItem(id));
            logFilesTable->setItem(row, 1, new QTableWidgetItem(sizeStr));
            logFilesTable->setItem(row, 2, new QTableWidgetItem(displayDate));

            auto* btn = new QToolButton(logFilesTable);
            btn->setText("↓");
            btn->setToolTip("Download log file");
            btn->setAutoRaise(true);
            btn->setCursor(Qt::PointingHandCursor);
            logFilesTable->setCellWidget(row, 3, btn);

            connect(btn, &QToolButton::clicked, this,
                [this, id, name]() { openLogFile(id, name); });
        }

        logFilesCountLabel   ->setText(
            QString("%1 file%2").arg(fileCount).arg(fileCount == 1 ? "" : "s"));
        logFilesTotalSizeLabel->setText("Total size:  " + humanBytes(totalSize));

    }
    catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Events / interactions
// ─────────────────────────────────────────────────────────────────────────────

bool DeviceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == overviewOpModeValue && event->type() == QEvent::MouseButtonRelease)
    {
        const auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton)
        {
            showOperationModeMenu();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DeviceWidget::showOperationModeMenu()
{
    if (!device.assigned() || !overviewOpModeValue)
        return;

    QMenu menu(this);
    try
    {
        const auto currentMode = device.getOperationMode();
        const auto available   = device.getAvailableOperationModes();
        if (!available.assigned() || available.getCount() == 0)
            return;

        for (size_t i = 0; i < available.getCount(); ++i)
        {
            const int modeInt = static_cast<int>(available[i]);
            const auto mode   = static_cast<daq::OperationModeType>(modeInt);
            QAction* a = menu.addAction(opModeToString(mode));
            a->setCheckable(true);
            a->setChecked(mode == currentMode);
            a->setData(modeInt);
        }

        QAction* chosen = menu.exec(
            overviewOpModeValue->mapToGlobal(QPoint(0, overviewOpModeValue->height())));
        if (!chosen)
            return;

        device.setOperationMode(static_cast<daq::OperationModeType>(chosen->data().toInt()));
        refresh();
    }
    catch (...) {}
}

void DeviceWidget::openLogFile(const QString& id, const QString& title)
{
    if (!device.assigned())
        return;

    try
    {
        const auto logStr = device.getLog(id.toStdString());

        const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir dir(tempRoot);
        dir.mkpath("opendaq_qt_gui/logs");
        dir.cd("opendaq_qt_gui/logs");

        QString baseName = title.isEmpty() ? id : title;
        if (baseName.isEmpty())
            baseName = "log";
        baseName.replace("/", "_").replace("\\", "_").replace(":", "_");

        const QString ts       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
        const QString filePath = dir.filePath(QString("%1_%2.log").arg(baseName, ts));

        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QMessageBox::warning(this, "Open log",
                QString("Failed to create temp file:\n%1").arg(filePath));
            return;
        }
        f.write(QString::fromStdString(logStr.toStdString()).toUtf8());
        f.close();

        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath)))
            QMessageBox::warning(this, "Open log",
                QString("System open failed.\nFile saved at:\n%1").arg(filePath));
    }
    catch (...) {}
}
