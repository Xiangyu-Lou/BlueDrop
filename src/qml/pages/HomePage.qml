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

        // Monitor volume (phone audio)
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

                // Title row with boost toggle
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: qsTr("监听音量")
                        font.pixelSize: Theme.fontTitle
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }

                    // Boost toggle
                    RowLayout {
                        spacing: 6
                        visible: mixerVM ? mixerVM.boostAvailable : false

                        Text {
                            text: qsTr("增益模式")
                            font.pixelSize: Theme.fontCaption
                            color: boostSwitch.checked ? Theme.accent : Theme.textSecondary
                        }

                        // Debounce timer: ignore rapid toggles within 600ms to prevent
                        // the audio endpoint from switching back and forth too quickly.
                        Timer {
                            id: boostDebounce
                            interval: 600
                            repeat: false
                            property bool pendingState: false
                            onTriggered: {
                                if (mixerVM) mixerVM.boostEnabled = pendingState
                            }
                        }

                        Switch {
                            id: boostSwitch
                            checked: mixerVM ? mixerVM.boostEnabled : false
                            onToggled: {
                                boostDebounce.pendingState = checked
                                boostDebounce.restart()
                            }

                            indicator: Rectangle {
                                implicitWidth: 40
                                implicitHeight: 22
                                x: boostSwitch.leftPadding
                                y: parent.height / 2 - height / 2
                                radius: 11
                                color: boostSwitch.checked ? Theme.accent : Theme.sliderTrack

                                Rectangle {
                                    x: boostSwitch.checked ? parent.width - width - 3 : 3
                                    y: 3
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: "white"

                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                        }
                    }
                }

                // Normal mode: session volume 0~100%
                VolumeSlider {
                    Layout.fillWidth: true
                    visible: !(mixerVM && mixerVM.boostEnabled)
                    label: qsTr("手机音频")
                    value: mixerVM ? mixerVM.btMonitorVolume : 1.0
                    muted: mixerVM ? mixerVM.btMonitorMuted : false
                    from: 0.0
                    to: 1.0
                    onVolumeChanged: (v) => { if (mixerVM) mixerVM.btMonitorVolume = v }
                    onMuteToggled: (m) => { if (mixerVM) mixerVM.btMonitorMuted = m }
                }

                // Boost mode: software gain 0~1000%
                VolumeSlider {
                    Layout.fillWidth: true
                    visible: mixerVM && mixerVM.boostEnabled
                    label: qsTr("手机音频 (增益)")
                    value: mixerVM ? mixerVM.boostGain : 1.0
                    muted: false
                    from: 0.0
                    to: 10.0
                    onVolumeChanged: (v) => { if (mixerVM) mixerVM.boostGain = v }
                }

                // Status hints
                Text {
                    Layout.fillWidth: true
                    visible: mixerVM && mixerVM.boostEnabled
                    text: qsTr("增益模式已启用：音频通过 VB-Cable 中转放大输出")
                    color: Theme.accent
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: mixerVM && !mixerVM.btSessionFound && !(mixerVM && mixerVM.boostEnabled)
                    text: qsTr("未找到蓝牙音频会话，请先连接手机并播放音频")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: mixerVM && !mixerVM.boostAvailable
                    text: qsTr("增益模式需要安装 VB-Audio Virtual Cable")
                    color: Theme.warning
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
