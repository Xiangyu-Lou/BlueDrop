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

    // Called from QML to keep menu in sync
    Q_INVOKABLE void updateBoostState(bool enabled);
    // connectionState: 0=Disconnected 1=WaitingPair 2=Connecting 3=Connected 4=Reconnecting
    Q_INVOKABLE void updateBtState(const QString& statusText, int connectionState);

signals:
    void showWindowRequested();
    void boostToggleRequested();
    void listenToggleRequested();   // start listening or disconnect
    void exitRequested();

private:
    QSystemTrayIcon* m_tray          = nullptr;
    QMenu*           m_menu          = nullptr;
    QAction*         m_statusAction  = nullptr;  // read-only status label
    QAction*         m_listenAction  = nullptr;  // start/stop/disconnect
    QAction*         m_boostAction   = nullptr;
    int              m_btState       = 0;
};

} // namespace BlueDrop
