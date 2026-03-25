import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 600
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("聚音 BlueDrop")
    color: "#F5F5F7"

    Text {
        anchors.centerIn: parent
        text: "聚音 BlueDrop v0.1.0"
        font.pixelSize: 24
        color: "#1D1D1F"
    }
}
