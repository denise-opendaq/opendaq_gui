#pragma once

#include <QPainter>
#include <QStyledItemDelegate>

// Paints a rounded selection / hover background for sidebar tree widgets.
// Clears State_Selected and State_HasFocus before delegating to the base class
// so Qt's flat highlight does not paint over the rounded rectangle.
//
// Usage:
//   tree->setItemDelegate(new SidebarItemDelegate(tree));
//   tree->viewport()->setAttribute(Qt::WA_Hover);
class SidebarItemDelegate : public QStyledItemDelegate
{
public:
    explicit SidebarItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered  = opt.state & QStyle::State_MouseOver;

        // Draw the row background only on the first column.
        // Clipping is disabled so the rounded rect spans the full viewport
        // width and covers all subsequent columns painted in the same pass.
        if ((selected || hovered) && index.column() == 0)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setClipping(false);
            painter->setPen(Qt::NoPen);

            const int fullWidth = painter->device()->width();
            const QRectF r(4, opt.rect.top() + 1, fullWidth - 8, opt.rect.height() - 2);

            if (selected)
                painter->setBrush(QColor(59, 130, 246, 45));
            else
                painter->setBrush(opt.palette.color(QPalette::Midlight));

            painter->drawRoundedRect(r, 8, 8);
            painter->restore();
        }

        opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
        QStyledItemDelegate::paint(painter, opt, index);
    }
};
