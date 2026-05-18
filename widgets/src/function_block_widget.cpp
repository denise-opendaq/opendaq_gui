#include "widgets/function_block_widget.h"
#include "widgets/input_port_folder_selector.h"
#include "context/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QSignalBlocker>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <opendaq/recorder_ptr.h>

FunctionBlockWidget::FunctionBlockWidget(const daq::FunctionBlockPtr& fb,
                                         ComponentTreeWidget* tree,
                                         QWidget* parent,
                                         const QString& treeIcon)
    : ComponentWidget(fb, parent, Qt::Uninitialized, treeIcon)
    , functionBlock(fb)
    , componentTree(tree)
{
    setupUI();
}

FunctionBlockWidget::~FunctionBlockWidget() = default;

void FunctionBlockWidget::populateTabs()
{
    ComponentWidget::populateTabs();

    auto* signalSelectorPage = new QWidget();
    signalSelectorPage->setAttribute(Qt::WA_StyledBackground, true);
    signalSelectorPage->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* layout = new QVBoxLayout(signalSelectorPage);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    setupRecorderControls(signalSelectorPage, layout);

    auto* sectionTitle = new QLabel(tr("Input ports"), signalSelectorPage);
    sectionTitle->setStyleSheet(
        QStringLiteral("color: #111827; font-size: 16px; font-weight: 600;"
                       " background: transparent; border: none;"));
    layout->addWidget(sectionTitle);

    auto* scroll = new QScrollArea(signalSelectorPage);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    auto* scrollContent = new QWidget();
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    scrollContent->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(12);

    inputPortFolderSelector = new InputPortFolderSelector(functionBlock.getItem("IP"), componentTree, scrollContent);
    scrollLayout->addWidget(inputPortFolderSelector);
    scrollLayout->addStretch();

    scroll->setWidget(scrollContent);
    layout->addWidget(scroll, 1);

    tabs->addTab(signalSelectorPage, tr("Signal Selector"));
}

void FunctionBlockWidget::setupRecorderControls(QWidget* page, QVBoxLayout* layout)
{
    if (!functionBlock.supportsInterface<daq::IRecorder>())
        return;

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("recordingCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(
        QStringLiteral("QFrame#recordingCard {"
                       "  background: #ffffff;"
                       "  border: 1px solid #e5e7eb;"
                       "  border-radius: 12px;"
                       "}"));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto* cardHeader = new QWidget(card);
    cardHeader->setAttribute(Qt::WA_StyledBackground, true);
    cardHeader->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* cardHeaderLayout = new QHBoxLayout(cardHeader);
    cardHeaderLayout->setContentsMargins(16, 12, 16, 12);

    auto* titleLbl = new QLabel(tr("Recording"), cardHeader);
    titleLbl->setStyleSheet(
        QStringLiteral("color: #111827; font-size: 13px; font-weight: 600;"
                       " background: transparent; border: none;"));
    cardHeaderLayout->addWidget(titleLbl);
    cardHeaderLayout->addStretch();

    cardLayout->addWidget(cardHeader);

    auto* headerSep = new QFrame(card);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setStyleSheet(QStringLiteral("background: #e5e7eb; border: none; max-height: 1px;"));
    cardLayout->addWidget(headerSep);

    auto* body = new QWidget(card);
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    recorderLayout = new QHBoxLayout(body);
    recorderLayout->setContentsMargins(16, 12, 16, 16);
    recorderLayout->setSpacing(12);

    auto* label = new QLabel(tr("Status"), body);
    label->setStyleSheet(
        QStringLiteral("color: #6b7280; font-size: 12px; font-weight: 600;"
                       " background: transparent; border: none;"));

    recordingToggle = new QCheckBox(body);
    recordingToggle->setStyleSheet(
        QStringLiteral("QCheckBox { spacing: 8px; font-size: 13px; color: #111827; }"
                       "QCheckBox::indicator { width: 18px; height: 18px; }"));

    recorderLayout->addWidget(label);
    recorderLayout->addStretch();
    recorderLayout->addWidget(recordingToggle);

    cardLayout->addWidget(body);
    layout->addWidget(card);

    connect(recordingToggle, &QCheckBox::toggled, this, &FunctionBlockWidget::onRecordingToggled);

    updateRecordingStatus();
}

void FunctionBlockWidget::onRecordingToggled(bool checked)
{
    auto recorder = functionBlock.asPtrOrNull<daq::IRecorder>(true);
    if (!recorder.assigned())
        return;

    try
    {
        if (checked)
        {
            recorder.startRecording();
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_I("Started recording");
        }
        else
        {
            recorder.stopRecording();
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_I("Stopped recording");
        }
    }
    catch (const std::exception& e)
    {
        QSignalBlocker blocker(recordingToggle);
        recordingToggle->setChecked(!checked);

        QMessageBox::critical(this, tr("Recording Error"),
                              tr("Failed to toggle recording: %1").arg(e.what()));
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_E("Failed to toggle recording: {}", e.what());
    }
}

void FunctionBlockWidget::updateRecordingStatus()
{
    auto recorder = functionBlock.asPtrOrNull<daq::IRecorder>(true);
    if (!recorder.assigned() || !recordingToggle)
        return;

    try
    {
        const bool isRecording = recorder.getIsRecording();

        QSignalBlocker blocker(recordingToggle);
        recordingToggle->setChecked(isRecording);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Failed to get recording status: {}", e.what());
    }
}
