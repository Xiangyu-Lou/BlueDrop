#include "TrayManager.h"

#include <QMenu>
#include <QAction>
#include <QPixmap>
#include <QPainter>

namespace BlueDrop {

TrayManager::TrayManager(QObject* parent)
    : QObject(parent)
{
    // Create a simple 32x32 blue circle icon
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#4A90D9"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(2, 2, 28, 28);
    p.end();

    m_tray = new QSystemTrayIcon(QIcon(pm), this);
    m_tray->setToolTip("聚音 BlueDrop");

    m_menu = new QMenu();

    auto showAction = m_menu->addAction("显示窗口");
    connect(showAction, &QAction::triggered, this, &TrayManager::showWindowRequested);

    m_menu->addSeparator();

    m_boostAction = m_menu->addAction("增益模式");
    m_boostAction->setCheckable(true);
    connect(m_boostAction, &QAction::triggered, this, &TrayManager::boostToggleRequested);

    m_menu->addSeparator();

    auto exitAction = m_menu->addAction("退出");
    connect(exitAction, &QAction::triggered, this, &TrayManager::exitRequested);

    m_tray->setContextMenu(m_menu);

    // Single or double click on tray icon → show window
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

} // namespace BlueDrop
