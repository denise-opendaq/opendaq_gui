#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QTextEdit>
#include <QSplitter>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QEvent>

#include "LayoutManager.h"

class DetachedWindow;
class BaseTreeElement;
class ComponentTreeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();

private Q_SLOTS:
    void onViewSelectionChanged(int index);
    void onShowHiddenComponentsToggled(bool checked);
    void onComponentSelected(BaseTreeElement* element);

private:
    // Main splitters
    QSplitter* mainSplitter = nullptr;      // Horizontal: left + right
    QSplitter* verticalSplitter = nullptr;  // Vertical: tabs + log
    QSplitter* contentSplitter = nullptr;   // Root for split views in content area

    // Left panel
    QComboBox* viewSelector = nullptr;
    ComponentTreeWidget* componentTreeWidget = nullptr;

    // Layout manager for tab and window management
    LayoutManager* layoutManager = nullptr;

    // Menu
    QAction* showHiddenAction = nullptr;
    QAction* resetLayoutAction = nullptr;
    QMenu* availableTabsMenu = nullptr;
};
