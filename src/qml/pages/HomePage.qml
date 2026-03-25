import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: root
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        // Bluetooth connection status
        StatusCard {
            Layout.fillWidth: true
        }

        // Output device selection
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: deviceContent.implicitHeight + Theme.cardPadding * 2

            // Subtle shadow
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

            ColumnLayout {
                id: deviceContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("输出设备")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: qsTr("本地监听耳机")
                        font.pixelSize: Theme.fontCaption
                        color: Theme.textSecondary
                    }

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: deviceVM ? deviceVM.outputDeviceNames : []
                        currentIndex: deviceVM ? deviceVM.selectedMonitorIndex : -1
                        onActivated: (index) => {
                            if (deviceVM) deviceVM.selectedMonitorIndex = index
                        }
                    }
                }
            }
        }

        // Monitor volume (phone audio session volume)
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: volumeContent.implicitHeight + Theme.cardPadding * 2

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

            ColumnLayout {
                id: volumeContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 16

                Text {
                    text: qsTr("监听音量")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                VolumeSlider {
                    Layout.fillWidth: true
                    label: qsTr("手机音频")
                    value: mixerVM ? mixerVM.btMonitorVolume : 1.0
                    muted: mixerVM ? mixerVM.btMonitorMuted : false
                    from: 0.0
                    to: 1.0
                    onVolumeChanged: (v) => { if (mixerVM) mixerVM.btMonitorVolume = v }
                    onMuteToggled: (m) => { if (mixerVM) mixerVM.btMonitorMuted = m }
                }

                Text {
                    Layout.fillWidth: true
                    visible: mixerVM && !mixerVM.btSessionFound
                    text: qsTr("未找到蓝牙音频会话，请先连接手机并播放音频")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
