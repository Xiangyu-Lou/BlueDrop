import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    radius: Theme.cardRadius
    color: Theme.cardBackground
    border.color: Theme.border
    border.width: 1
    implicitHeight: content.implicitHeight + Theme.cardPadding * 2

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.cardPadding
        spacing: 12

        // Header
        Text {
            text: qsTr("蓝牙连接")
            font.pixelSize: Theme.fontTitle
            font.bold: true
            color: Theme.textPrimary
        }

        // Status row
        RowLayout {
            spacing: 10

            // Status dot
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: bluetoothVM ? bluetoothVM.statusColor : "#86868B"

                SequentialAnimation on opacity {
                    running: bluetoothVM ? (bluetoothVM.connectionState === 1 || bluetoothVM.connectionState === 2) : false
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 800 }
                    NumberAnimation { to: 1.0; duration: 800 }
                }
            }

            Text {
                text: bluetoothVM ? bluetoothVM.statusText : qsTr("未连接")
                font.pixelSize: Theme.fontBody
                color: Theme.textPrimary
                Layout.fillWidth: true
            }
        }

        // Control button
        RowLayout {
            spacing: 8

            Button {
                text: {
                    if (!bluetoothVM) return qsTr("开始监听")
                    switch (bluetoothVM.connectionState) {
                        case 0: return qsTr("开始监听")    // Disconnected
                        case 1: return qsTr("停止监听")    // WaitingPair
                        case 2: return qsTr("取消连接")    // Connecting
                        case 3: return qsTr("断开连接")    // Connected
                        case 4: return qsTr("停止重连")    // Reconnecting
                    }
                    return qsTr("开始监听")
                }

                onClicked: {
                    if (!bluetoothVM) return
                    if (bluetoothVM.connectionState === 0) {
                        bluetoothVM.startListening()
                    } else {
                        bluetoothVM.stopListening()
                    }
                }

                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 32
                    radius: Theme.smallRadius
                    color: parent.hovered ? Theme.accentHover : Theme.accent
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontCaption
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
