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
        spacing: 16

        // === Section: 监听音量 (Route A: what user hears) ===
        Text {
            text: qsTr("监听音量")
            font.pixelSize: Theme.fontTitle
            font.bold: true
            color: Theme.textPrimary
        }

        // BT monitor volume - controls system-level BT audio session volume
        // Slider 0~200%, maps to WASAPI 0.0~1.0 (50% slider = current full volume)
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

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // === Section: 广播混音 (Route B: BT+mic -> VB-Cable) ===
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("广播混音")
                font.pixelSize: Theme.fontTitle
                font.bold: true
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
                    implicitWidth: 60
                    implicitHeight: 28
                    radius: Theme.smallRadius
                    color: mixerVM && mixerVM.engineRunning ? Theme.error : Theme.success
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

        // Phone audio in mix
        VolumeSlider {
            Layout.fillWidth: true
            label: qsTr("手机混入")
            value: mixerVM ? mixerVM.phoneVolume : 1.0
            muted: mixerVM ? mixerVM.phoneMuted : false
            onVolumeChanged: (v) => { if (mixerVM) mixerVM.phoneVolume = v }
            onMuteToggled: (m) => { if (mixerVM) mixerVM.phoneMuted = m }
        }

        // Mic in mix
        VolumeSlider {
            Layout.fillWidth: true
            label: qsTr("麦克风")
            value: mixerVM ? mixerVM.micVolume : 1.0
            muted: mixerVM ? mixerVM.micMuted : false
            onVolumeChanged: (v) => { if (mixerVM) mixerVM.micVolume = v }
            onMuteToggled: (m) => { if (mixerVM) mixerVM.micMuted = m }
        }

        // Error message
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
