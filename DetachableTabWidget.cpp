#include "DetachableTabWidget.h"

#include <QApplication>
#include <QCursor>
#include <QPointer>

namespace {

QByteArray encodeTabDragPayload(quintptr sourcePtr, int tabIndex)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_DefaultCompiledVersion);
    out << sourcePtr << tabIndex;
    return data;
}

bool decodeTabDragPayload(const QMimeData* mime, DetachableTabWidget*& outSource, int& outIndex)
{
    outSource = nullptr;
    outIndex = -1;

    if (!mime || !mime->hasFormat(DetachableTabWidget::TabMimeType))
        return false;

    const QByteArray raw = mime->data(DetachableTabWidget::TabMimeType);
    QDataStream in(raw);
    in.setVersion(QDataStream::Qt_DefaultCompiledVersion);

    quintptr sourcePtr = 0;
    int tabIndex = -1;
    in >> sourcePtr >> tabIndex;

    if (sourcePtr == 0 || tabIndex < 0)
        return false;

    outSource = reinterpret_cast<DetachableTabWidget*>(sourcePtr);
    outIndex = tabIndex;
    return true;
}

} // namespace

// ============================================================================
// DetachableTabBar
// ============================================================================

DetachableTabBar::DetachableTabBar(QWidget* parent)
    : QTabBar(parent)
{
    // We implement our own drag, so disable the built-in "movable" behavior.
    setMovable(false);
    setTabsClosable(false);
    setAcceptDrops(false);
}

void DetachableTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPos = event->pos();
        dragIndex = tabAt(event->pos());
        dragArmed = (dragIndex >= 0);
    }

    QTabBar::mousePressEvent(event);
}

void DetachableTabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton) || !dragArmed || dragIndex < 0) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    if ((event->pos() - dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    // Start drag once.
    dragArmed = false;

    auto* sourceTabWidget = qobject_cast<DetachableTabWidget*>(parentWidget());
    if (!sourceTabWidget || dragIndex >= sourceTabWidget->count())
        return;

    QMimeData* mime = new QMimeData();
    mime->setData(
        DetachableTabWidget::TabMimeType,
        encodeTabDragPayload(reinterpret_cast<quintptr>(sourceTabWidget), dragIndex)
    );

    QDrag* drag = new QDrag(qApp);
    drag->setMimeData(mime);

    // Nice "ghost" like VSCode: the grabbed tab pixmap
    const QRect tr = tabRect(dragIndex);
    QPixmap pm = grab(tr);
    drag->setPixmap(pm);
    drag->setHotSpot(event->pos() - tr.topLeft());

    // Guard against the source tab bar being deleted while the drag runs
    // (e.g. when the last tab is moved out and the container is destroyed).
    QPointer<DetachableTabBar> self(this);
    QPointer<QWidget> topWin = window(); // capture before exec
    const int idx = dragIndex;

    const Qt::DropAction result = drag->exec(Qt::MoveAction);
    drag->deleteLater();

    if (!self)
        return;

    // If nothing accepted the drop and we released outside the top-level window -> detach.
    if (result == Qt::IgnoreAction) {
        const QPoint globalPos = QCursor::pos();
        QWidget* top = topWin.data();
        if (top && !top->frameGeometry().contains(globalPos)) {
            Q_EMIT detachRequested(idx, globalPos);
        }
    }

}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    dragArmed = false;
    dragIndex = -1;
    QTabBar::mouseReleaseEvent(event);
}

void DetachableTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Optional convenience: double-click detaches the tab (similar to many IDEs)
    const int idx = tabAt(event->pos());
    if (idx >= 0) {
        Q_EMIT detachRequested(idx, QCursor::pos());
    }
    QTabBar::mouseDoubleClickEvent(event);
}

// ============================================================================
// DetachableTabWidget
// ============================================================================

DetachableTabWidget::DetachableTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    auto* bar = new DetachableTabBar(this);
    setTabBar(bar);

    setAcceptDrops(true);

    connect(bar, &DetachableTabBar::detachRequested,
            this, &DetachableTabWidget::onDetachRequested);
}

void DetachableTabWidget::setEnableDetach(bool enable)
{
    detachEnabled = enable;
}

void DetachableTabWidget::setEnableSplit(bool enable)
{
    splitEnabled = enable;
}

DetachableTabWidget::TabInfo DetachableTabWidget::takeTabInfo(int index)
{
    TabInfo info;

    if (index < 0 || index >= count())
        return info;

    info.page = widget(index);
    info.title = tabText(index);
    info.icon = tabIcon(index);
    info.toolTip = tabToolTip(index);
    info.whatsThis = tabWhatsThis(index);
    info.data = tabBar()->tabData(index);

    removeTab(index);
    if (info.page) {
        info.page->setParent(nullptr);
    }

    return info;
}

int DetachableTabWidget::insertTabInfo(int index, TabInfo&& info)
{
    if (!info.page)
        return -1;

    const int clamped = qBound(0, index, count());
    const int newIndex = insertTab(clamped, info.page, info.icon, info.title);

    setTabToolTip(newIndex, info.toolTip);
    setTabWhatsThis(newIndex, info.whatsThis);
    tabBar()->setTabData(newIndex, info.data);

    return newIndex;
}

bool DetachableTabWidget::decodeTabMime(const QMimeData* mime, DetachableTabWidget*& outSource, int& outIndex)
{
    return decodeTabDragPayload(mime, outSource, outIndex);
}

void DetachableTabWidget::onDetachRequested(int index, const QPoint& globalPos)
{
    if (!detachEnabled)
        return;

    TabInfo info = takeTabInfo(index);
    if (!info.page)
        return;

    Q_EMIT tabDetached(info.page, info.title, globalPos);
}

bool DetachableTabWidget::isPointOnTabBar(const QPoint& widgetPos) const
{
    if (!tabBar())
        return false;

    // tabBar()->geometry() is in this widget's coordinate system.
    return tabBar()->geometry().contains(widgetPos);
}

int DetachableTabWidget::dropInsertIndexFromPos(const QPoint& widgetPos) const
{
    if (!tabBar())
        return count();

    const QPoint barPos = tabBar()->mapFrom(this, widgetPos);
    int idx = tabBar()->tabAt(barPos);
    if (idx < 0)
        idx = count();
    return idx;
}

void DetachableTabWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event || !event->mimeData()) {
        QTabWidget::dragEnterEvent(event);
        return;
    }

    // Only accept drops onto the tab bar itself.
    if (event->mimeData()->hasFormat(TabMimeType) &&
        isPointOnTabBar(event->position().toPoint()))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    event->ignore();
}

void DetachableTabWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event || !event->mimeData()) {
        QTabWidget::dragMoveEvent(event);
        return;
    }

    if (event->mimeData()->hasFormat(TabMimeType) &&
        isPointOnTabBar(event->position().toPoint()))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    event->ignore();
}

void DetachableTabWidget::dropEvent(QDropEvent* event)
{
    if (!event || !event->mimeData()) {
        QTabWidget::dropEvent(event);
        return;
    }

    if (!event->mimeData()->hasFormat(TabMimeType) ||
        !isPointOnTabBar(event->position().toPoint()))
    {
        event->ignore();
        return;
    }

    DetachableTabWidget* source = nullptr;
    int sourceIndex = -1;
    if (!decodeTabMime(event->mimeData(), source, sourceIndex) || !source) {
        event->ignore();
        return;
    }

    // Compute insert position before we take (remove) the source tab.
    int insertAt = dropInsertIndexFromPos(event->position().toPoint());

    // If we reorder within the same tab widget, taking the tab shifts indices.
    if (source == this && sourceIndex < insertAt)
        insertAt--;

    TabInfo info = source->takeTabInfo(sourceIndex);
    if (!info.page) {
        event->ignore();
        return;
    }

    const int newIndex = insertTabInfo(insertAt, std::move(info));
    if (newIndex >= 0) {
        setCurrentIndex(newIndex);
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();

    Q_EMIT tabMoveCompleted(source);
}
