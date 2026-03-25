pragma Singleton
import QtQuick

QtObject {
    // Colors
    readonly property color background: "#F5F5F7"
    readonly property color cardBackground: "#FFFFFF"
    readonly property color sidebarBackground: "#F0F0F2"
    readonly property color accent: "#4A90D9"
    readonly property color accentHover: "#3A7BC8"
    readonly property color success: "#34C759"
    readonly property color warning: "#FF9500"
    readonly property color error: "#FF3B30"
    readonly property color textPrimary: "#1D1D1F"
    readonly property color textSecondary: "#86868B"
    readonly property color border: "#E5E5EA"
    readonly property color sliderTrack: "#D1D1D6"
    readonly property color sliderFill: "#4A90D9"

    // Dimensions
    readonly property int cardRadius: 12
    readonly property int smallRadius: 8
    readonly property int spacing: 16
    readonly property int sidebarWidth: 200
    readonly property int cardPadding: 20

    // Font sizes
    readonly property int fontTitle: 18
    readonly property int fontBody: 14
    readonly property int fontCaption: 12
    readonly property int fontSmall: 11
}
