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

        StatusCard {
            Layout.fillWidth: true
        }

        DeviceCard {
            Layout.fillWidth: true
        }

        MixerCard {
            Layout.fillWidth: true
        }
    }
}
