import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    radius: Theme.cardRadius
    color: Theme.cardBackground
    border.color: Theme.border
    border.width: 0

    // Subtle shadow via layered border
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        z: -1
        radius: Theme.cardRadius + 1
        color: "transparent"
        border.color: "#E0E0E4"
        border.width: 1
        opacity: 0.5
    }

    implicitHeight: content.implicitHeight + Theme.cardPadding * 2

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.cardPadding
        spacing: 16

        // Header
        Text {
            text: qsTr("蓝牙连接")
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        // Status row
        RowLayout {
            spacing: 10

            Rectangle {
                width: 10
                height: 10
                radius: 5
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
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
        }

        // Control button
        Button {
            text: {
                if (!bluetoothVM) return qsTr("开始监听")
                switch (bluetoothVM.connectionState) {
                    case 0: return qsTr("开始监听")
                    case 1: return qsTr("停止监听")
                    case 2: return qsTr("取消连接")
                    case 3: return qsTr("断开连接")
                    case 4: return qsTr("停止重连")
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
                implicitWidth: 110
                implicitHeight: 36
                radius: Theme.smallRadius
                color: parent.hovered ? Theme.accentHover : Theme.accent
            }

            contentItem: Text {
                text: parent.text
                font.pixelSize: Theme.fontBody
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
