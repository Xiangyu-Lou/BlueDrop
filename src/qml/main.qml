import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "pages"
import "theme"

ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 600
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("聚音 BlueDrop")
    color: Theme.background
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left navigation sidebar
        NavSidebar {
            id: sidebar
            Layout.fillHeight: true
            onPageSelected: (index) => {
                pageStack.currentIndex = index
            }
        }

        // Separator line
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme.border
        }

        // Right content area
        StackLayout {
            id: pageStack
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: Theme.spacing

            HomePage {}
            BroadcastPage {}
            SettingsPage {}
            HelpPage {}
        }
    }
}
