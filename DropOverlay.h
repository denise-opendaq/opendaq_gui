#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QEvent>

enum class DropZone {
    None,
    Full,      // Whole window highlighted (VSCode-style initial state)
    Left,
    Right,
    Top,
    Bottom,
    Center,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

class DropOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal highlightOpacity READ highlightOpacity WRITE setHighlightOpacity)

public:
    explicit DropOverlay(QWidget *parent = nullptr);
    ~DropOverlay() override;

    void setTargetWidget(QWidget *target);
    DropZone getDropZone(const QPoint &pos) const;
    void updateHighlight(const QPoint &pos);
    void stopAnimations();

    qreal highlightOpacity() const { return m_highlightOpacity; }
    void setHighlightOpacity(qreal opacity) { m_highlightOpacity = opacity; update(); }

Q_SIGNALS:
    void dropZoneChanged(DropZone zone);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QRect getZoneRect(DropZone zone) const;

    QWidget *targetWidget = nullptr;
    DropZone currentZone = DropZone::None;
    DropZone previousZone = DropZone::None;

    const int edgeThreshold = 100;  // Distance from edge for split zones
    const int centerThreshold = 150; // Distance from all edges for center zone

    qreal m_highlightOpacity = 1.0;
    QPropertyAnimation *fadeAnimation = nullptr;
};
