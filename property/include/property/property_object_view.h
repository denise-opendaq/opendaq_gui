#pragma once

// Save Qt keywords and undefine them before including openDAQ
#ifdef signals
#pragma push_macro("signals")
#pragma push_macro("slots")
#pragma push_macro("emit")
#undef signals
#undef slots
#undef emit
#endif

// Include openDAQ headers
#include <opendaq/opendaq.h>

// Restore Qt keywords
#ifdef __cplusplus
#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#endif

#include <QtWidgets>

class PropertyObjectView : public QTableWidget
{
    Q_OBJECT

public:
    PropertyObjectView(const daq::PropertyObjectPtr& propObj)
        : QTableWidget()
        , objPtr(propObj)
    {
        setColumnCount(2);
        setHorizontalHeaderLabels({"Property", "Value"});
        horizontalHeader()->setStretchLastSection(true);
        verticalHeader()->setVisible(false);

        // Populate the table
        refreshProperties();

        // Connect signal to handle value changes
        connect(this, &QTableWidget::itemChanged, this, &PropertyObjectView::onItemChanged);
    }

public Q_SLOTS:
    // Refresh the table from the PropertyObject
    void refreshProperties()
    {
        // Temporarily disconnect to avoid triggering itemChanged during refresh
        disconnect(this, &QTableWidget::itemChanged, this, &PropertyObjectView::onItemChanged);

        auto properties = objPtr.getAllProperties();

        setRowCount(properties.getCount());
        for (size_t i = 0; i < properties.getCount(); ++i)
        {
            const auto& prop = properties[i];

            const QString name = QString::fromStdString(prop.getName());
            const QString value = QString::fromStdString(prop.getValue().toString());

            // Property name (read-only)
            auto nameItem = new QTableWidgetItem(name);
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            setItem(static_cast<int>(i), 0, nameItem);

            // Property value (editable)
            auto valueItem = new QTableWidgetItem(value);
            setItem(static_cast<int>(i), 1, valueItem);
        }

        // Reconnect the signal
        connect(this, &QTableWidget::itemChanged, this, &PropertyObjectView::onItemChanged);
    }

protected Q_SLOTS:
    // Handle value changes and update the PropertyObject
    void onItemChanged(QTableWidgetItem* item)
    {
        // Only handle value column changes
        if (item->column() != 1)
            return;

        int row = item->row();
        auto properties = objPtr.getAllProperties();

        if (row >= 0 && row < static_cast<int>(properties.getCount()))
        {
            const auto& prop = properties[row];
            QString newValue = item->text();

            try
            {
                // Update the property value in the PropertyObject
                objPtr.setPropertyValue(prop.getName(), newValue.toStdString());

                // Optionally emit a signal that property was changed
                Q_EMIT propertyChanged(QString::fromStdString(prop.getName()), newValue);
            }
            catch (const std::exception& e)
            {
                // If setting fails, revert to old value
                disconnect(this, &QTableWidget::itemChanged, this, &PropertyObjectView::onItemChanged);
                item->setText(QString::fromStdString(prop.getValue().toString()));
                connect(this, &QTableWidget::itemChanged, this, &PropertyObjectView::onItemChanged);

                // Show error message
                QMessageBox::warning(this, "Property Update Error",
                    QString("Failed to update property: %1").arg(e.what()));
            }
        }
    }

Q_SIGNALS:
    // Emitted when a property value is successfully changed
    void propertyChanged(const QString& propertyName, const QString& newValue);

protected:
    daq::PropertyObjectPtr objPtr;
};

