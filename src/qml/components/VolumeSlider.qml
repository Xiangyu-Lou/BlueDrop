import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 10

    property string label: ""
    property real value: 1.0
    property bool muted: false
    property real from: 0.0
    property real to: 2.0

    signal valueChanged(real newValue)
    signal mutedToggled(bool newMuted)

    Text {
        text: root.label
        font.pixelSize: Theme.fontCaption
        color: Theme.textSecondary
        Layout.preferredWidth: 60
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        value: root.value
        enabled: !root.muted

        onMoved: {
            root.valueChanged(value)
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
            implicitWidth: 16
            implicitHeight: 16
            radius: 8
            color: root.muted ? Theme.textSecondary : Theme.accent
            border.color: Theme.border
            border.width: 1
        }
    }

    // Value display
    Text {
        text: Math.round(root.value * 100) + "%"
        font.pixelSize: Theme.fontCaption
        color: root.muted ? Theme.textSecondary : Theme.textPrimary
        Layout.preferredWidth: 40
        horizontalAlignment: Text.AlignRight
    }

    // Mute button
    Rectangle {
        width: 28
        height: 28
        radius: Theme.smallRadius
        color: root.muted ? Theme.error : "transparent"
        border.color: root.muted ? Theme.error : Theme.border
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.muted ? "🔇" : "🔊"
            font.pixelSize: 14
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.mutedToggled(!root.muted)
        }
    }
}
