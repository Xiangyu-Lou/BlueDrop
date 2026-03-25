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

        // Header with engine control
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("混音台")
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

        // Phone audio slider
        VolumeSlider {
            Layout.fillWidth: true
            label: qsTr("手机音频")
            value: mixerVM ? mixerVM.phoneVolume : 1.0
            muted: mixerVM ? mixerVM.phoneMuted : false
            onValueChanged: (v) => { if (mixerVM) mixerVM.phoneVolume = v }
            onMutedToggled: (m) => { if (mixerVM) mixerVM.phoneMuted = m }
        }

        // Mic slider
        VolumeSlider {
            Layout.fillWidth: true
            label: qsTr("麦克风")
            value: mixerVM ? mixerVM.micVolume : 1.0
            muted: mixerVM ? mixerVM.micMuted : false
            onValueChanged: (v) => { if (mixerVM) mixerVM.micVolume = v }
            onMutedToggled: (m) => { if (mixerVM) mixerVM.micMuted = m }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // Mix ratio slider
        VolumeSlider {
            Layout.fillWidth: true
            label: qsTr("混入比例")
            value: mixerVM ? mixerVM.phoneMixRatio : 1.0
            muted: false
            from: 0.0
            to: 1.0
            onValueChanged: (v) => { if (mixerVM) mixerVM.phoneMixRatio = v }
        }
    }
}
