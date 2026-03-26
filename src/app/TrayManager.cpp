#include "TrayManager.h"

#include <QMenu>
#include <QAction>
#include <QIcon>

namespace BlueDrop {

static QString listenLabel(int state)
{
    switch (state) {
    case 3:  return QString::fromUtf8(u8"断开连接");   // Connected
    case 1:
    case 2:
    case 4:  return QString::fromUtf8(u8"停止监听");   // WaitingPair / Connecting / Reconnecting
    default: return QString::fromUtf8(u8"开始监听");   // Disconnected
    }
}

TrayManager::TrayManager(QObject* parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(QIcon(":/icons/icon.png"), this);
    m_tray->setToolTip(QString::fromUtf8(u8"聚音 BlueDrop"));

    m_menu = new QMenu();

    // ── BT status (read-only label) ──
    m_statusAction = m_menu->addAction(QString::fromUtf8(u8"状态：未连接蓝牙设备"));
    m_statusAction->setEnabled(false);

    // ── Listen / disconnect toggle ──
    m_listenAction = m_menu->addAction(listenLabel(0));
    connect(m_listenAction, &QAction::triggered, this, &TrayManager::listenToggleRequested);

    m_menu->addSeparator();

    // ── Boost mode ──
    m_boostAction = m_menu->addAction(QString::fromUtf8(u8"增益模式"));
    m_boostAction->setCheckable(true);
    connect(m_boostAction, &QAction::triggered, this, &TrayManager::boostToggleRequested);

    m_menu->addSeparator();

    // ── Show window / exit ──
    auto showAction = m_menu->addAction(QString::fromUtf8(u8"显示窗口"));
    connect(showAction, &QAction::triggered, this, &TrayManager::showWindowRequested);

    auto exitAction = m_menu->addAction(QString::fromUtf8(u8"退出"));
    connect(exitAction, &QAction::triggered, this, &TrayManager::exitRequested);

    m_tray->setContextMenu(m_menu);

    // Single or double click → show window
    connect(m_tray, &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger ||
            reason == QSystemTrayIcon::DoubleClick) {
            emit showWindowRequested();
        }
    });
}

TrayManager::~TrayManager()
{
    delete m_menu;
}

void TrayManager::show()
{
    m_tray->show();
}

void TrayManager::updateBoostState(bool enabled)
{
    if (m_boostAction)
        m_boostAction->setChecked(enabled);
}

void TrayManager::updateBtState(const QString& statusText, int connectionState)
{
    m_btState = connectionState;
    if (m_statusAction)
        m_statusAction->setText(QString::fromUtf8(u8"状态：") + statusText);
    if (m_listenAction)
        m_listenAction->setText(listenLabel(connectionState));
}

} // namespace BlueDrop
