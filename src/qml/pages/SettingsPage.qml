import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        // System info card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: sysContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: sysContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("系统信息")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                // OS Version
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.osVersionOk ? Theme.success : Theme.error
                    }
                    Text {
                        text: qsTr("操作系统：") + (settingsVM ? settingsVM.osVersion : "")
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }

                // Bluetooth
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.bluetoothAvailable ? Theme.success : Theme.error
                    }
                    Text {
                        text: qsTr("蓝牙适配器：") + (settingsVM && settingsVM.bluetoothAvailable
                            ? settingsVM.bluetoothAdapterName
                            : qsTr("未检测到"))
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }

                // VB-Cable
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: settingsVM && settingsVM.vbCableInstalled ? Theme.success : Theme.warning
                    }
                    Text {
                        text: qsTr("虚拟声卡：") + (settingsVM && settingsVM.vbCableInstalled
                            ? settingsVM.vbCableDeviceName
                            : qsTr("未安装 VB-Cable"))
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                    }
                }
            }
        }

        // Close behavior card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: closeContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: closeContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 16

                Text {
                    text: qsTr("关闭行为")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    text: qsTr("点击窗口关闭按钮时的默认行为")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                }

                // Segmented control
                RowLayout {
                    spacing: 8

                    Repeater {
                        model: [
                            { label: qsTr("每次询问"),    value: ""         },
                            { label: qsTr("最小化到托盘"), value: "minimize" },
                            { label: qsTr("退出程序"),    value: "close"    }
                        ]

                        delegate: Rectangle {
                            property bool selected: (settingsVM ? settingsVM.closeAction : "") === modelData.value
                            implicitWidth: segLabel.implicitWidth + 24
                            height: 34
                            radius: Theme.smallRadius
                            color: selected ? Theme.accent : "#F0F0F2"
                            border.color: selected ? Theme.accent : "transparent"

                            Text {
                                id: segLabel
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pixelSize: Theme.fontBody
                                color: selected ? "white" : Theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (settingsVM)
                                        settingsVM.closeAction = modelData.value
                                }
                            }
                        }
                    }
                }
            }
        }

        // Check for updates card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: updateContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: updateContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("检查更新")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                RowLayout {
                    spacing: 16
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("当前版本：") + (settingsVM ? settingsVM.appVersion : "")
                            font.pixelSize: Theme.fontBody
                            color: Theme.textPrimary
                        }

                        Text {
                            text: settingsVM ? settingsVM.updateStatusText : qsTr("尚未检查更新")
                            font.pixelSize: Theme.fontCaption
                            color: {
                                if (!settingsVM) return Theme.textSecondary
                                if (settingsVM.updateAvailable) return Theme.accent
                                if (settingsVM.updateStatusText.indexOf("失败") >= 0) return Theme.error
                                if (settingsVM.updateStatusText.indexOf("最新") >= 0) return Theme.success
                                return Theme.textSecondary
                            }
                        }
                    }

                    // Check / Download button
                    Rectangle {
                        height: 34
                        implicitWidth: updateBtnLabel.implicitWidth + 28
                        radius: Theme.smallRadius
                        property bool busy: settingsVM && (settingsVM.updateChecking || settingsVM.updateDownloading)
                        property bool showDownload: settingsVM && settingsVM.updateAvailable && !settingsVM.updateDownloading
                        color: busy ? Theme.textSecondary
                             : (updateBtnArea.containsMouse ? Theme.accentHover : Theme.accent)
                        opacity: busy ? 0.5 : 1.0

                        Text {
                            id: updateBtnLabel
                            anchors.centerIn: parent
                            text: {
                                if (!settingsVM) return qsTr("检查更新")
                                if (settingsVM.updateDownloading) return qsTr("下载中…")
                                if (settingsVM.updateChecking) return qsTr("检查中…")
                                if (settingsVM.updateAvailable) return qsTr("下载并更新")
                                return qsTr("检查更新")
                            }
                            font.pixelSize: Theme.fontBody
                            color: "white"
                        }

                        MouseArea {
                            id: updateBtnArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: parent.busy ? Qt.ArrowCursor : Qt.PointingHandCursor
                            enabled: !parent.busy
                            onClicked: {
                                if (!settingsVM) return
                                if (settingsVM.updateAvailable)
                                    settingsVM.downloadAndInstall()
                                else
                                    settingsVM.checkForUpdates()
                            }
                        }
                    }
                }

                // Download progress bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: 3
                    color: Theme.sliderTrack
                    visible: settingsVM && settingsVM.updateDownloading

                    Rectangle {
                        width: parent.width * (settingsVM ? settingsVM.downloadProgress / 100.0 : 0)
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent

                        Behavior on width { NumberAnimation { duration: 150 } }
                    }
                }
            }
        }

        // Debug / Logging card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: debugContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: debugContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("调试日志")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: qsTr("记录详细运行日志（用于问题诊断）")
                        font.pixelSize: Theme.fontBody
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    Switch {
                        checked: settingsVM ? settingsVM.loggingEnabled : false
                        onToggled: { if (settingsVM) settingsVM.loggingEnabled = checked }
                    }
                }

                Text {
                    visible: settingsVM && settingsVM.loggingEnabled
                    text: qsTr("日志路径：") + (settingsVM ? settingsVM.logFilePath : "")
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textSecondary
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                RowLayout {
                    visible: settingsVM && settingsVM.loggingEnabled
                    spacing: 8

                    Rectangle {
                        height: 30
                        implicitWidth: openFolderLabel.implicitWidth + 20
                        radius: Theme.smallRadius
                        color: openFolderArea.containsMouse ? Theme.navSelected : "#F0F0F2"

                        Text {
                            id: openFolderLabel
                            anchors.centerIn: parent
                            text: qsTr("打开文件夹")
                            font.pixelSize: Theme.fontBody
                            color: Theme.textPrimary
                        }

                        MouseArea {
                            id: openFolderArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (settingsVM) {
                                    var path = settingsVM.logFilePath
                                    var dir = path.substring(0, path.lastIndexOf("/"))
                                    Qt.openUrlExternally("file:///" + dir)
                                }
                            }
                        }
                    }

                    Rectangle {
                        height: 30
                        implicitWidth: clearLogLabel.implicitWidth + 20
                        radius: Theme.smallRadius
                        color: clearLogArea.containsMouse ? Theme.navSelected : "#F0F0F2"

                        Text {
                            id: clearLogLabel
                            anchors.centerIn: parent
                            text: qsTr("清空日志")
                            font.pixelSize: Theme.fontBody
                            color: Theme.textPrimary
                        }

                        MouseArea {
                            id: clearLogArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { if (settingsVM) settingsVM.clearLog() }
                        }
                    }
                }
            }
        }

        // About card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
            implicitHeight: aboutContent.implicitHeight + Theme.cardPadding * 2

            ColumnLayout {
                id: aboutContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 8

                Text {
                    text: qsTr("关于")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    text: "聚音 BlueDrop v" + (settingsVM ? settingsVM.appVersion : "0.1.0")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textPrimary
                }

                Text {
                    text: qsTr("音频路由与混流软件")
                    font.pixelSize: Theme.fontCaption
                    color: Theme.textSecondary
                }
            }
        }
    }
}
