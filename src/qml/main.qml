import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "pages"
import "theme"

ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 600
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("聚音 BlueDrop")
    color: Theme.background
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody

    // Restore saved state and handle first-launch help page
    Timer {
        interval: 200
        running: true
        repeat: false
        onTriggered: {
            if (mixerVM) mixerVM.autoRestoreState()
            if (settingsVM && settingsVM.consumeFirstLaunch()) {
                pageStack.currentIndex = 3
                sidebar.currentIndex = 3
            }
        }
    }

    // Intercept the window close button
    onClosing: (close) => {
        var action = settingsVM ? settingsVM.closeAction : ""
        if (action === "close") {
            // User previously chose "always exit" — let it close
            return
        }
        if (action === "minimize") {
            close.accepted = false
            root.hide()
            return
        }
        // First time or preference not set: show dialog
        close.accepted = false
        closeDialog.open()
    }

    // Tray icon signal handlers
    Connections {
        target: trayManager
        function onShowWindowRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
        function onBoostToggleRequested() {
            if (mixerVM && mixerVM.boostAvailable)
                mixerVM.boostEnabled = !mixerVM.boostEnabled
        }
        function onExitRequested() {
            Qt.quit()
        }
    }

    // Keep tray boost checkmark in sync
    Connections {
        target: mixerVM
        function onBoostEnabledChanged() {
            if (trayManager) trayManager.updateBoostState(mixerVM.boostEnabled)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left navigation sidebar
        NavSidebar {
            id: sidebar
            Layout.fillHeight: true
            onPageSelected: (index) => {
                pageStack.currentIndex = index
            }
        }

        // Separator line
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.border
        }

        // Right content area
        StackLayout {
            id: pageStack
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: Theme.spacing

            HomePage {}
            BroadcastPage {}
            SettingsPage {}
            HelpPage {}
        }
    }

    // ── Close behaviour dialog ──────────────────────────────────────
    Dialog {
        id: closeDialog
        title: qsTr("关闭聚音 BlueDrop")
        modal: true
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose

        background: Rectangle {
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
        }

        header: Item {
            implicitHeight: 48
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("关闭聚音 BlueDrop")
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
        }

        contentItem: ColumnLayout {
            spacing: 16
            width: 340

            Text {
                Layout.fillWidth: true
                text: qsTr("请选择关闭行为：")
                font.pixelSize: Theme.fontBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            CheckBox {
                id: rememberCheck
                text: qsTr("记住该选择，不再询问")
                font.pixelSize: Theme.fontBody

                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: 4
                    border.color: rememberCheck.checked ? Theme.accent : "#CCCCCC"
                    border.width: 1
                    color: rememberCheck.checked ? Theme.accent : "white"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 12
                        color: "white"
                        visible: rememberCheck.checked
                    }
                }
            }
        }

        footer: RowLayout {
            spacing: 8
            Layout.margins: 16

            Item { Layout.fillWidth: true }

            // Minimize to tray button
            Rectangle {
                height: 36
                implicitWidth: minLabel.implicitWidth + 24
                radius: Theme.smallRadius
                color: minArea.containsMouse ? Theme.accent : "#E8E8EC"

                Text {
                    id: minLabel
                    anchors.centerIn: parent
                    text: qsTr("最小化到托盘")
                    font.pixelSize: Theme.fontBody
                    color: minArea.containsMouse ? "white" : Theme.textPrimary
                }

                MouseArea {
                    id: minArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (rememberCheck.checked && settingsVM)
                            settingsVM.closeAction = "minimize"
                        closeDialog.close()
                        root.hide()
                    }
                }
            }

            // Exit button
            Rectangle {
                height: 36
                implicitWidth: exitLabel.implicitWidth + 24
                radius: Theme.smallRadius
                color: exitArea.containsMouse ? Theme.error : "#E8E8EC"

                Text {
                    id: exitLabel
                    anchors.centerIn: parent
                    text: qsTr("退出程序")
                    font.pixelSize: Theme.fontBody
                    color: exitArea.containsMouse ? "white" : Theme.textPrimary
                }

                MouseArea {
                    id: exitArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (rememberCheck.checked && settingsVM)
                            settingsVM.closeAction = "close"
                        closeDialog.close()
                        Qt.quit()
                    }
                }
            }

            Item { implicitWidth: 4 }
        }
    }
}
