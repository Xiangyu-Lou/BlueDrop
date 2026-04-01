import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ScrollView {
    id: root
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        // ── 使用说明 ──────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: helpContent.implicitHeight + Theme.cardPadding * 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                z: -1
                radius: Theme.cardRadius + 1
                color: "transparent"
                border.color: "#E0E0E4"
                border.width: 1
                opacity: 0.5
            }

            ColumnLayout {
                id: helpContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 16

                Text {
                    text: qsTr("使用说明")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("聚音 BlueDrop 可以将手机通过蓝牙连接电脑，实现本地监听和音量控制，也可以将手机音频混入直播/录音。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                // Steps
                Repeater {
                    model: [
                        {
                            step: "1",
                            title: qsTr("连接蓝牙"),
                            desc: qsTr("在手机蓝牙设置中，搜索并连接到你的电脑。确保已将电脑的蓝牙设置为可被发现。")
                        },
                        {
                            step: "2",
                            title: qsTr("开始监听"),
                            desc: qsTr("在主页点击「开始监听」按钮，等待手机连接成功（状态变为「已连接」）。")
                        },
                        {
                            step: "3",
                            title: qsTr("播放音频"),
                            desc: qsTr("在手机上开始播放音频（音乐、通话等）。聚音会自动检测到蓝牙音频会话。")
                        },
                        {
                            step: "4",
                            title: qsTr("调节音量"),
                            desc: qsTr("在主页的「监听音量」卡片中拖动滑块调节音量，或点击 🔇 静音。")
                        }
                    ]

                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            width: 28
                            height: 28
                            radius: 14
                            color: Theme.accent
                            Layout.alignment: Qt.AlignTop

                            Text {
                                anchors.centerIn: parent
                                text: modelData.step
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.DemiBold
                                color: "white"
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: modelData.title
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.desc
                                font.pixelSize: Theme.fontBody
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // ── 后台保持说明 ──────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: persistContent.implicitHeight + Theme.cardPadding * 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                z: -1
                radius: Theme.cardRadius + 1
                color: "transparent"
                border.color: "#E0E0E4"
                border.width: 1
                opacity: 0.5
            }

            ColumnLayout {
                id: persistContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("蓝牙连接的持久化")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("首次通过聚音成功连接手机后，Windows 会记住该设备的 A2DP 配置。此后即使不启动聚音，手机重新连接时也能自动将音频传输到电脑。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("建议每次使用时启动聚音：除了音量调节、静音、增益等功能需要聚音运行外，聚音还会在连接时主动优化音频管道初始化，避免偶发的连接后无声问题。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.warning
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── 增益模式说明 ──────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: boostContent.implicitHeight + Theme.cardPadding * 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                z: -1
                radius: Theme.cardRadius + 1
                color: "transparent"
                border.color: "#E0E0E4"
                border.width: 1
                opacity: 0.5
            }

            ColumnLayout {
                id: boostContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("增益模式")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("当手机音量过低时，可开启增益模式进行软件放大（最高 1000%）。增益模式通过 VB-Audio Virtual Cable 中转音频，再由软件增益后输出到耳机。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("注意：增益模式会增加约 15ms 延迟，且可能造成轻微卡顿。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.warning
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("增益模式需要安装 VB-Audio Virtual Cable。如尚未安装，可在主页点击下载链接获取。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── 广播混音说明 ──────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.cardRadius
            color: Theme.cardBackground
            implicitHeight: broadcastContent.implicitHeight + Theme.cardPadding * 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                z: -1
                radius: Theme.cardRadius + 1
                color: "transparent"
                border.color: "#E0E0E4"
                border.width: 1
                opacity: 0.5
            }

            ColumnLayout {
                id: broadcastContent
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: 12

                Text {
                    text: qsTr("广播混音（Route B）")
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("广播页面可以将手机蓝牙音频与麦克风混合，输出到虚拟声卡（VB-Audio Virtual Cable）。直播软件（OBS、Streamlabs 等）可将虚拟声卡设置为音频输入源，从而采集混合后的音频。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("广播混音也需要安装 VB-Audio Virtual Cable。")
                    font.pixelSize: Theme.fontBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Bottom padding
        Item { implicitHeight: Theme.spacing }
    }
}
