#pragma once

#include <QList>
#include <QMainWindow>
#include <QShowEvent>

#include "UpdateChecker.h"
#include "context/release_asset.h"

class QAction;
class QLineEdit;
class QMenu;
class QSplitter;
class QWidget;

class BaseTreeElement;
class ComponentTreeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void clearContent();

private Q_SLOTS:
    void onShowHiddenComponentsToggled(bool checked);
    void onComponentSelected(BaseTreeElement* element);
    void onUpdateAvailable(const QString& version, const QString& changelog, const QString& releaseUrl,
                           const QList<ReleaseAsset>& assets);
    void onLoadModuleTriggered();

private:
    // Main splitters
    QSplitter* mainSplitter = nullptr;
    QSplitter* verticalSplitter = nullptr;

    // Single content slot: hosts the widget of the currently selected component
    QWidget* contentHost = nullptr;

    // Left panel
    QLineEdit* searchBox = nullptr;
    ComponentTreeWidget* componentTreeWidget = nullptr;

    // Menu
    QAction* showHiddenAction = nullptr;
    QAction* expandAllPropertiesAction = nullptr;

    UpdateChecker* m_updateChecker = nullptr;
};
