#pragma once

#include <QWidget>

class QLabel;
class BasePropertyItem;
class QToolButton;

class PropertyInspector : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyInspector(QWidget* parent = nullptr);

    struct Row
    {
        QLabel* label;
        QLabel* value;
    };

public Q_SLOTS:
    void showProperty(BasePropertyItem* item);
    void clearProperty();

private:
    void setupUI();
    void setRowText(Row& row, const QString& text);
    void setRowVisible(Row& row, bool visible);

    Row rowName;
    Row rowPath;
    Row rowDescription;
    Row rowType;
    Row rowUnit;
    Row rowDefault;
    Row rowCurrent;
    Row rowAllowed;
    Row rowWritable;
    Row rowMin;
    Row rowMax;

    QLabel* emptyState;
    QWidget* content;
};
