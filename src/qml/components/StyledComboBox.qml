import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ComboBox {
    id: control

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        radius: Theme.smallRadius
        color: control.pressed ? "#ECECF0" : (control.hovered ? "#F5F5F7" : "#F0F0F2")
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: control.activeFocus ? 1 : 0
    }

    contentItem: Text {
        leftPadding: 14
        rightPadding: control.indicator.width + 12
        text: control.displayText
        font.pixelSize: Theme.fontBody
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 14
        y: (control.height - height) / 2
        text: "▾"
        font.pixelSize: 12
        color: Theme.textSecondary
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4

        background: Rectangle {
            radius: Theme.smallRadius
            color: Theme.cardBackground
            border.color: "#E0E0E4"
            border.width: 1
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    delegate: ItemDelegate {
        width: control.width - 8
        height: 38
        x: 4

        contentItem: Text {
            text: modelData
            font.pixelSize: Theme.fontBody
            color: highlighted ? "white" : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: highlighted ? Theme.accent : (hovered ? Theme.navHover : "transparent")
            radius: 6
        }

        highlighted: control.highlightedIndex === index
    }
}
