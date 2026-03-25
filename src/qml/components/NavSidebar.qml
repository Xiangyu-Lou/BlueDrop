import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: Theme.sidebarWidth
    color: Theme.sidebarBackground

    property int currentIndex: 0

    signal pageSelected(int index)

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        spacing: 4

        // App title
        Text {
            Layout.leftMargin: 20
            Layout.bottomMargin: 20
            text: "聚音"
            font.pixelSize: 20
            font.bold: true
            color: Theme.textPrimary
        }

        // Nav items
        Repeater {
            model: [
                { icon: "🏠", label: qsTr("主页") },
                { icon: "⚙", label: qsTr("设置") }
            ]

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                radius: Theme.smallRadius
                color: root.currentIndex === index ? Theme.accent : "transparent"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentIndex = index
                        root.pageSelected(index)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    spacing: 10

                    Text {
                        text: modelData.icon
                        font.pixelSize: 16
                    }

                    Text {
                        text: modelData.label
                        font.pixelSize: Theme.fontBody
                        color: root.currentIndex === index ? "white" : Theme.textPrimary
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Version info
        Text {
            Layout.leftMargin: 20
            Layout.bottomMargin: 16
            text: "v0.1.0"
            font.pixelSize: Theme.fontSmall
            color: Theme.textSecondary
        }
    }
}
