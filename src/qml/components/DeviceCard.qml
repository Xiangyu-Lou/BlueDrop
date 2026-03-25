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

        Text {
            text: qsTr("音频设备")
            font.pixelSize: Theme.fontTitle
            font.bold: true
            color: Theme.textPrimary
        }

        // Microphone selection
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("物理麦克风")
                font.pixelSize: Theme.fontCaption
                color: Theme.textSecondary
            }
            ComboBox {
                Layout.fillWidth: true
                model: deviceVM ? deviceVM.inputDeviceNames : []
                currentIndex: deviceVM ? deviceVM.selectedMicIndex : -1
                onActivated: (index) => { if (deviceVM) deviceVM.selectedMicIndex = index }
            }
        }

        // Monitor output selection
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("本地监听耳机")
                font.pixelSize: Theme.fontCaption
                color: Theme.textSecondary
            }
            ComboBox {
                Layout.fillWidth: true
                model: deviceVM ? deviceVM.outputDeviceNames : []
                currentIndex: deviceVM ? deviceVM.selectedMonitorIndex : -1
                onActivated: (index) => { if (deviceVM) deviceVM.selectedMonitorIndex = index }
            }
        }

        // Virtual cable selection
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            RowLayout {
                Text {
                    text: qsTr("虚拟麦克风输出")
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textSecondary
                }
                Rectangle {
                    visible: deviceVM ? deviceVM.vbCableInstalled : false
                    width: installedLabel.implicitWidth + 8
                    height: 16
                    radius: 4
                    color: "#E8F5E9"
                    Text {
                        id: installedLabel
                        anchors.centerIn: parent
                        text: qsTr("已安装")
                        font.pixelSize: 10
                        color: Theme.success
                    }
                }
            }
            ComboBox {
                Layout.fillWidth: true
                model: deviceVM ? deviceVM.outputDeviceNames : []
                currentIndex: deviceVM ? deviceVM.selectedVirtualIndex : -1
                onActivated: (index) => { if (deviceVM) deviceVM.selectedVirtualIndex = index }
            }
        }
    }
}
