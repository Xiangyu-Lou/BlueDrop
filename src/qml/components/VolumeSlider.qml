import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 12

    property string label: ""
    property real value: 1.0
    property bool muted: false
    property real from: 0.0
    property real to: 2.0

    signal volumeChanged(real newValue)
    signal muteToggled(bool newMuted)

    Text {
        text: root.label
        font.pixelSize: Theme.fontBody
        color: Theme.textSecondary
        Layout.preferredWidth: 64
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        value: root.value
        enabled: !root.muted

        onMoved: {
            root.volumeChanged(value)
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: slider.availableWidth
            height: implicitHeight
            radius: 2
            color: Theme.sliderTrack

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                color: root.muted ? Theme.textSecondary : Theme.sliderFill
                radius: 2
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: root.muted ? Theme.textSecondary : Theme.accent
            border.color: root.muted ? "#C0C0C0" : "#3A7BC8"
            border.width: 2

            Behavior on scale { NumberAnimation { duration: 100 } }
            scale: slider.pressed ? 1.15 : 1.0
        }
    }

    // Value display
    Text {
        text: Math.round(slider.value / root.to * 100) + "%"
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        color: root.muted ? Theme.textSecondary : Theme.textPrimary
        Layout.preferredWidth: 44
        horizontalAlignment: Text.AlignRight
    }

    // Mute button
    Rectangle {
        width: 32
        height: 32
        radius: 8
        color: root.muted ? "#FFF0F0" : (muteArea.containsMouse ? Theme.navHover : "transparent")
        border.color: root.muted ? Theme.error : "transparent"
        border.width: root.muted ? 1 : 0

        Text {
            anchors.centerIn: parent
            text: root.muted ? "🔇" : "🔊"
            font.pixelSize: 15
        }

        MouseArea {
            id: muteArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: root.muteToggled(!root.muted)
        }
    }
}
