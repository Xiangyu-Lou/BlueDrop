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

        // Input devices
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: micDeviceContent.implicitHeight + Theme.cardPadding * 2

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
                id: micDeviceContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("输入设备")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: qsTr("物理麦克风")
                        font.pixelSize: Theme.fontCaption
                        color: Theme.textSecondary
                    }

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: deviceVM ? deviceVM.inputDeviceNames : []
                        currentIndex: deviceVM ? deviceVM.selectedMicIndex : -1
                        onActivated: (index) => {
                            if (deviceVM) deviceVM.selectedMicIndex = index
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RowLayout {
                        spacing: 8
                        Text {
                            text: qsTr("虚拟麦克风输出")
                            font.pixelSize: Theme.fontCaption
                            color: Theme.textSecondary
                        }
                        Rectangle {
                            visible: deviceVM && deviceVM.vbCableInstalled
                            width: installedLabel.implicitWidth + 12
                            height: installedLabel.implicitHeight + 4
                            radius: 4
                            color: Theme.success
                            Text {
                                id: installedLabel
                                anchors.centerIn: parent
                                text: qsTr("已安装")
                                font.pixelSize: Theme.fontSmall
                                color: "white"
                            }
                        }
                    }

                    StyledComboBox {
                        Layout.fillWidth: true
                        model: deviceVM ? deviceVM.outputDeviceNames : []
                        currentIndex: deviceVM ? deviceVM.selectedVirtualIndex : -1
                        onActivated: (index) => {
                            if (deviceVM) deviceVM.selectedVirtualIndex = index
                        }
                    }
                }
            }
        }

        // Broadcast mixing
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: mixContent.implicitHeight + Theme.cardPadding * 2

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
                id: mixContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("广播混音")
                        font.pixelSize: Theme.fontTitle
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }

                    Button {
                        text: mixerVM && mixerVM.engineRunning ? qsTr("停止") : qsTr("启动")
                        onClicked: {
                            if (!mixerVM) return
                            if (mixerVM.engineRunning) {
                                mixerVM.stopEngine()
                            } else {
                                mixerVM.startEngine()
                            }
                        }
                        background: Rectangle {
                            implicitWidth: 64
                            implicitHeight: 32
                            radius: Theme.smallRadius
                            color: {
                                var isRunning = mixerVM && mixerVM.engineRunning
                                if (parent.hovered)
                                    return isRunning ? "#E0342A" : "#2DB84D"
                                return isRunning ? Theme.error : Theme.success
                            }
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

                Text {
                    Layout.fillWidth: true
                    text: qsTr("将手机音频与麦克风混合后输出至虚拟麦克风，供通讯软件使用")
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                VolumeSlider {
                    Layout.fillWidth: true
                    label: qsTr("手机混入")
                    value: mixerVM ? mixerVM.phoneVolume : 1.0
                    muted: mixerVM ? mixerVM.phoneMuted : false
                    onVolumeChanged: (v) => { if (mixerVM) mixerVM.phoneVolume = v }
                    onMuteToggled: (m) => { if (mixerVM) mixerVM.phoneMuted = m }
                }

                VolumeSlider {
                    Layout.fillWidth: true
                    label: qsTr("麦克风")
                    value: mixerVM ? mixerVM.micVolume : 1.0
                    muted: mixerVM ? mixerVM.micMuted : false
                    onVolumeChanged: (v) => { if (mixerVM) mixerVM.micVolume = v }
                    onMuteToggled: (m) => { if (mixerVM) mixerVM.micMuted = m }
                }

                Text {
                    Layout.fillWidth: true
                    visible: mixerVM && mixerVM.lastError && mixerVM.lastError.length > 0
                    text: mixerVM ? mixerVM.lastError : ""
                    color: Theme.error
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
