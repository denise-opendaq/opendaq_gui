#include "widgets/device_logs_widget.h"
#include "context/AppContext.h"
#include "context/gui_styles.h"
#include "context/icon_provider.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QTime>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

DeviceLogsWidget::DeviceLogsWidget(const daq::DevicePtr& dev, QWidget* parent)
    : QWidget(parent)
    , device(dev)
{
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(16, 12, 16, 16);
    pageLayout->setSpacing(8);

    auto* card = new QFrame(this);
    card->setObjectName("logsCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(GUIStyles::cardStyle("logsCard", "QFrame", 12));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // — Card header —
    auto* cardHeader = new QWidget(card);
    cardHeader->setAttribute(Qt::WA_StyledBackground, true);
    cardHeader->setStyleSheet("background: transparent; border: none;");
    auto* cardHeaderLayout = new QHBoxLayout(cardHeader);
    cardHeaderLayout->setContentsMargins(16, 12, 12, 12);
    cardHeaderLayout->setSpacing(12);

    countLbl = new QLabel(card);
    countLbl->setStyleSheet(GUIStyles::SMALL_CAPS_LABEL);
    cardHeaderLayout->addWidget(countLbl);
    cardHeaderLayout->addStretch();

    updatedLbl = new QLabel(card);
    updatedLbl->setStyleSheet(
        "color: palette(placeholder-text); font-size: 12px; background: transparent; border: none;");
    cardHeaderLayout->addWidget(updatedLbl);

    cardLayout->addWidget(cardHeader);

    auto* headerSep = new QFrame(card);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setStyleSheet(GUIStyles::HAIRLINE_HORIZONTAL);
    cardLayout->addWidget(headerSep);

    // — Scrollable list —
    auto* scroll = new QScrollArea(card);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    listWidget = new QWidget();
    listWidget->setStyleSheet("background: transparent;");
    listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);
    listLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(listWidget);
    cardLayout->addWidget(scroll, 1);

    pageLayout->addWidget(card, 1);
}

void DeviceLogsWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void DeviceLogsWidget::refresh()
{
    while (listLayout->count())
    {
        auto* item = listLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    updatedLbl->setText(tr("Last updated: %1").arg(QTime::currentTime().toString("HH:mm:ss")));

    try
    {
        const auto infos = device.getLogFileInfos();
        if (!infos.assigned() || infos.getCount() == 0)
        {
            countLbl->setText(tr("No log files"));
            auto* empty = new QLabel(tr("No log files available"), listWidget);
            empty->setStyleSheet(QString(GUIStyles::CAPTION_LABEL) +
                                 " padding: 20px 16px; background: transparent; border: none;");
            listLayout->addWidget(empty);
            return;
        }

        const int count = static_cast<int>(infos.getCount());
        countLbl->setText(tr("%1 log file%2").arg(count).arg(count == 1 ? "" : "s"));

        bool first = true;
        for (const auto& info : infos)
        {
            if (!first)
            {
                auto* sep = new QFrame(listWidget);
                sep->setFrameShape(QFrame::HLine);
                sep->setStyleSheet(GUIStyles::HAIRLINE_HORIZONTAL);
                listLayout->addWidget(sep);
            }
            first = false;

            const QString id       = QString::fromStdString(info.getId().toStdString());
            const QString fileName = QString::fromStdString(info.getName().toStdString());

            QString sizeStr;
            try
            {
                const int64_t sz = info.getSize();
                if (sz < 1024)             sizeStr = QString("%1 B").arg(sz);
                else if (sz < 1024 * 1024) sizeStr = QString("%1 KB").arg(sz / 1024);
                else                       sizeStr = QString("%1 MB").arg(sz / (1024 * 1024));
            }
            catch (...) {}

            auto* row = new QWidget(listWidget);
            row->setAttribute(Qt::WA_StyledBackground, true);
            row->setStyleSheet(
                "QWidget { background: transparent; border: none; }"
                "QWidget:hover { background: palette(alternate-base); }");

            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(16, 12, 12, 12);
            rowLayout->setSpacing(12);

            auto* nameLbl = new QLabel(fileName, row);
            nameLbl->setStyleSheet(
                "color: palette(text); font-size: 13px; font-weight: 600;"
                " background: transparent; border: none;");

            auto* sizeLbl = new QLabel(sizeStr, row);
            sizeLbl->setStyleSheet(
                "color: palette(placeholder-text); font-size: 12px;"
                " background: transparent; border: none;");

            auto* openBtn = new QToolButton(row);
            openBtn->setCursor(Qt::PointingHandCursor);
            openBtn->setIcon(IconProvider::instance().icon("folder"));
            openBtn->setIconSize(QSize(16, 16));
            openBtn->setToolTip(tr("Open in system editor"));
            openBtn->setStyleSheet(
                "QToolButton { border: 1px solid palette(mid); border-radius: 6px;"
                "  padding: 5px; background: palette(alternate-base); }"
                "QToolButton:hover { background: palette(midlight); border-color: palette(highlight); }");

            connect(openBtn, &QToolButton::clicked, this, [this, id, fileName]()
            {
                try
                {
                    const QString content = QString::fromStdString(
                        device.getLog(id.toStdString()).toStdString());
                    const QString path = QDir::tempPath() + "/" + fileName;
                    QFile file(path);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        file.write(content.toUtf8());
                        file.close();
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    }
                }
                catch (const std::exception& e)
                {
                    const auto loggerComponent = AppContext::LoggerComponent();
                    LOG_W("Failed to open log '{}': {}", id.toStdString(), e.what());
                }
            });

            rowLayout->addWidget(nameLbl);
            rowLayout->addWidget(sizeLbl);
            rowLayout->addStretch();
            rowLayout->addWidget(openBtn);
            listLayout->addWidget(row);
        }
    }
    catch (...) {}
}
