#include "widgets/synchronization_widget.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>
#include <opendaq/synchronization_ptr.h>
#include <opendaq/sync_interface_ptr.h>

SynchronizationWidget::SynchronizationWidget(const daq::DevicePtr& dev, QWidget* parent)
    : QWidget(parent)
    , device(dev)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    daq::SynchronizationPtr sync;
    try { sync = device.getSynchronization(); } catch (...) {}

    if (!sync.assigned())
    {
        layout->setAlignment(Qt::AlignCenter);
        auto* lbl = new QLabel(tr("No synchronization available"), this);
        lbl->setStyleSheet(GUIStyles::EMPTY_STATE_LABEL);
        layout->addWidget(lbl);
        return;
    }

    buildSourceRow(layout);

    try
    {
        const daq::PropertyObjectPtr interfaces =
            sync.asPtr<daq::IPropertyObject>(true).getPropertyValue("Interfaces");
        layout->addWidget(new PropertyObjectView(interfaces, this, device), 1);
    }
    catch (...)
    {
        layout->addStretch();
    }
}

void SynchronizationWidget::buildSourceRow(QVBoxLayout* layout)
{
    auto* sourceRow = new QWidget(this);
    auto* sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(8);

    auto* sourceCaption = new QLabel(tr("Source"), sourceRow);
    sourceCaption->setStyleSheet(GUIStyles::CAPTION_LABEL);
    sourceLayout->addWidget(sourceCaption);

    try
    {
        auto sync = device.getSynchronization();
        auto source = sync.getSelectedSource();
        if (source.assigned())
        {
            QString name;
            try { name = QString::fromStdString(source.getName().toStdString()); } catch (...) {}

            auto* nameLbl = new QLabel(name.isEmpty() ? tr("—") : name, sourceRow);
            nameLbl->setStyleSheet(GUIStyles::VALUE_LABEL);
            sourceLayout->addWidget(nameLbl);

            bool synced = false;
            try { synced = source.getSynced(); } catch (...) {}

            auto* dot = new QLabel("●", sourceRow);
            dot->setStyleSheet(GUIStyles::statusDotStyle(
                synced ? GUIConstants::Colors::SUCCESS : GUIConstants::Colors::NEUTRAL));
            sourceLayout->addWidget(dot);

            auto* syncedLbl = new QLabel(synced ? tr("Synced") : tr("Not synced"), sourceRow);
            syncedLbl->setStyleSheet(GUIStyles::VALUE_LABEL);
            sourceLayout->addWidget(syncedLbl);

            if (synced)
            {
                QString refId;
                try { refId = QString::fromStdString(source.getReferenceDomainId().toStdString()); } catch (...) {}

                auto* refCaption = new QLabel(tr("Reference domain ID"), sourceRow);
                refCaption->setStyleSheet(QString(GUIStyles::CAPTION_LABEL) + " margin-left: 12px;");
                sourceLayout->addWidget(refCaption);

                auto* refLbl = new QLabel(refId.isEmpty() ? tr("—") : refId, sourceRow);
                refLbl->setStyleSheet(GUIStyles::VALUE_LABEL);
                refLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
                sourceLayout->addWidget(refLbl);
            }
        }
        else
        {
            auto* noneLbl = new QLabel(tr("—"), sourceRow);
            noneLbl->setStyleSheet(GUIStyles::VALUE_LABEL);
            sourceLayout->addWidget(noneLbl);
        }
    }
    catch (...) {}

    sourceLayout->addStretch();
    layout->addWidget(sourceRow);
}
