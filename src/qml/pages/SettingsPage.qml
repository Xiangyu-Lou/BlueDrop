import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        // System info card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: sysContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: sysContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("系统信息")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                // OS Version
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.osVersionOk ? Theme.success : Theme.error
                    }
                    Text {
                        text: qsTr("操作系统：") + (settingsVM ? settingsVM.osVersion : "")
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }

                // Bluetooth
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.bluetoothAvailable ? Theme.success : Theme.error
                    }
                    Text {
                        text: qsTr("蓝牙适配器：") + (settingsVM && settingsVM.bluetoothAvailable
                            ? settingsVM.bluetoothAdapterName
                            : qsTr("未检测到"))
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }

                // VB-Cable
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.vbCableInstalled ? Theme.success : Theme.warning
                    }
                    Text {
                        text: qsTr("虚拟声卡：") + (settingsVM && settingsVM.vbCableInstalled
                            ? settingsVM.vbCableDeviceName
                            : qsTr("未安装 VB-Cable"))
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }
            }
        }

        // About card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: aboutContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: aboutContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 8

                Text {
                    text: qsTr("关于")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    text: "聚音 BlueDrop v" + (settingsVM ? settingsVM.appVersion : "0.1.0")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textPrimary
                }

                Text {
                    text: qsTr("音频路由与混流软件")
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textSecondary
                }
            }
        }
    }
}
