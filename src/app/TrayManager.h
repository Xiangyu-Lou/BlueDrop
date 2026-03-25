#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class QAction;

namespace BlueDrop {

class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(QObject* parent = nullptr);
    ~TrayManager();

    void show();

    // Called from QML when boost state changes to keep menu in sync
    Q_INVOKABLE void updateBoostState(bool enabled);

signals:
    void showWindowRequested();
    void boostToggleRequested();
    void exitRequested();

private:
    QSystemTrayIcon* m_tray = nullptr;
    QMenu*   m_menu         = nullptr;
    QAction* m_boostAction  = nullptr;
};

} // namespace BlueDrop
