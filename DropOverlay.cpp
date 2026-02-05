#include "DropOverlay.h"

#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QEvent>

DropOverlay::DropOverlay(QWidget* parent)
    : QWidget(parent)
{
    // Make this overlay NOT steal drag/drop or mouse input.
    // We'll position it as a transparent tool window over the content area.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::WindowTransparentForInput, true);

    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAcceptDrops(false);

    // Setup fade animation for smooth transitions
    fadeAnimation = new QPropertyAnimation(this, "highlightOpacity");
    fadeAnimation->setDuration(150);
    fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    
    // Install event filter to track target widget resize events
    if (parent)
    {
        parent->installEventFilter(this);
    }
}

DropOverlay::~DropOverlay()
{
    // Stop and clean up animation to prevent crashes during widget destruction
    stopAnimations();
    if (fadeAnimation)
    {
        fadeAnimation->deleteLater();
        fadeAnimation = nullptr;
    }
}

void DropOverlay::setTargetWidget(QWidget* target)
{
    targetWidget = target;
    if (!targetWidget)
        return;

    const QPoint topLeft = targetWidget->mapToGlobal(QPoint(0, 0));
    const QRect globalRect(topLeft, targetWidget->size());

    setGeometry(globalRect);
    raise();
}

DropZone DropOverlay::getDropZone(const QPoint& pos) const
{
    if (!rect().contains(pos))
        return DropZone::None;

    const int w = width();
    const int h = height();
    const int x = pos.x();
    const int y = pos.y();

    const int minSide = qMin(w, h);
    const int edge = qMin(edgeThreshold, qMax(16, minSide / 4));
    const int center = qMin(centerThreshold, qMax(24, minSide / 3));

    // VSCode-style: only edges + center + full.
    // (Quarter/corner zones exist in enum but are intentionally unused.)

    if (x < edge)
        return DropZone::Left;

    if (x > w - edge)
        return DropZone::Right;

    if (y < edge)
        return DropZone::Top;

    if (y > h - edge)
        return DropZone::Bottom;

    const QRect centerRect(center, center, w - 2 * center, h - 2 * center);
    if (centerRect.isValid() && centerRect.contains(pos))
        return DropZone::Center;

    return DropZone::Full;
}

void DropOverlay::updateHighlight(const QPoint& pos)
{
    const DropZone newZone = getDropZone(pos);
    if (newZone != currentZone) 
    {
        previousZone = currentZone;
        currentZone = newZone;

        stopAnimations();

        if (fadeAnimation)
        {
            fadeAnimation->setStartValue(0.5);
            fadeAnimation->setEndValue(1.0);
            fadeAnimation->start();
        }

        update();
        Q_EMIT dropZoneChanged(newZone);
    }
}

void DropOverlay::stopAnimations()
{
    if (fadeAnimation && fadeAnimation->state() == QAbstractAnimation::Running)
    {
        fadeAnimation->stop();
    }
}

void DropOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (currentZone == DropZone::None)
        return;

    const QRect zoneRect = getZoneRect(currentZone);

    const int alphaFill = static_cast<int>(120 * m_highlightOpacity);
    const int alphaBorder = static_cast<int>(255 * m_highlightOpacity);

    painter.fillRect(zoneRect, QColor(37, 99, 235, alphaFill));

    painter.setPen(QPen(QColor(37, 99, 235, alphaBorder), 3));
    painter.drawRoundedRect(zoneRect.adjusted(2, 2, -2, -2), 4, 4);

    painter.setPen(QColor(255, 255, 255, alphaBorder));
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);

    QString text;
    switch (currentZone) 
    {
        case DropZone::Full:   text = "Drop Here"; break;
        case DropZone::Left:   text = "← Split Left"; break;
        case DropZone::Right:  text = "Split Right →"; break;
        case DropZone::Top:    text = "↑ Split Top"; break;
        case DropZone::Bottom: text = "↓ Split Bottom"; break;
        case DropZone::Center: text = "⊕ Add Tab"; break;
        default: break;
    }

    painter.drawText(zoneRect, Qt::AlignCenter, text);
}

QRect DropOverlay::getZoneRect(DropZone zone) const
{
    const int w = width();
    const int h = height();
    const int minSide = qMin(w, h);
    const int center = qMin(centerThreshold, qMax(24, minSide / 3));

    switch (zone) 
    {
        case DropZone::Full:
            return rect();
        case DropZone::Left:
            return QRect(0, 0, w / 2, h);
        case DropZone::Right:
            return QRect(w / 2, 0, w / 2, h);
        case DropZone::Top:
            return QRect(0, 0, w, h / 2);
        case DropZone::Bottom:
            return QRect(0, h / 2, w, h / 2);
        case DropZone::Center:
            return rect().adjusted(center, center, -center, -center);
        default:
            return QRect();
    }
}

bool DropOverlay::eventFilter(QObject* obj, QEvent* event)
{
    // Update overlay geometry when target widget is resized
    if (obj == targetWidget && event->type() == QEvent::Resize)
    {
        stopAnimations();
        setTargetWidget(targetWidget); // Update geometry
    }
    return QWidget::eventFilter(obj, event);
}
