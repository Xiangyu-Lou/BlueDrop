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
        anchors.topMargin: 28
        spacing: 2

        // App title
        RowLayout {
            Layout.leftMargin: 24
            Layout.bottomMargin: 24
            spacing: 8

            Text {
                text: "聚音"
                font.pixelSize: 22
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
        }

        // Nav items
        Repeater {
            model: [
                { icon: "🏠", label: qsTr("主页") },
                { icon: "🎙", label: qsTr("广播") },
                { icon: "⚙", label: qsTr("设置") },
                { icon: "?", label: qsTr("帮助") }
            ]

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                radius: Theme.smallRadius
                color: {
                    if (root.currentIndex === index) return Theme.navSelected
                    if (mouseArea.containsMouse) return Theme.navHover
                    return "transparent"
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        root.currentIndex = index
                        root.pageSelected(index)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    spacing: 12

                    Text {
                        text: modelData.icon
                        font.pixelSize: 16
                        Layout.preferredWidth: 20
                        horizontalAlignment: Text.AlignHCenter
                        opacity: root.currentIndex === index ? 1.0 : 0.7
                    }

                    Text {
                        text: modelData.label
                        font.pixelSize: Theme.fontBody
                        font.weight: root.currentIndex === index ? Font.DemiBold : Font.Normal
                        color: root.currentIndex === index ? Theme.textPrimary : Theme.textSecondary
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Version info
        Text {
            Layout.leftMargin: 24
            Layout.bottomMargin: 20
            text: settingsVM ? ("v" + settingsVM.appVersion) : ""
            font.pixelSize: Theme.fontSmall
            color: Theme.textSecondary
        }
    }
}
