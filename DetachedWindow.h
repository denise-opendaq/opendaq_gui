#pragma once

#include <QMainWindow>
#include <QWidget>

class DetachedWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DetachedWindow(QWidget *contentWidget, const QString &title, QWidget *parent = nullptr);
    ~DetachedWindow() override = default;

    QWidget* contentWidget() const { return content; }
    QString windowTitle() const { return title; }

Q_SIGNALS:
    void windowClosed(QWidget *contentWidget, const QString &title);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *content = nullptr;
    QString title;
};
