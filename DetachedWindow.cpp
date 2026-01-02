#include "DetachedWindow.h"
#include <QCloseEvent>
#include <QVBoxLayout>

DetachedWindow::DetachedWindow(QWidget *contentWidget, const QString &windowTitle, QWidget *parent)
    : QMainWindow(parent)
    , content(contentWidget)
    , title(windowTitle)
{
    setWindowTitle(title);
    resize(800, 600);

    // Ensure content widget is properly parented and visible
    if (contentWidget) 
    {
        contentWidget->setParent(this);
        setCentralWidget(contentWidget);
        contentWidget->show();
    }
}

void DetachedWindow::closeEvent(QCloseEvent *event)
{
    Q_EMIT windowClosed(content, title);
    event->accept();
}
