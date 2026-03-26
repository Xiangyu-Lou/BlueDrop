pragma Singleton
import QtQuick

QtObject {
    // Colors - lighter, cleaner palette
    readonly property color background: "#F5F5F7"
    readonly property color cardBackground: "#FFFFFF"
    readonly property color sidebarBackground: "#FAFAFA"
    readonly property color accent: "#00A878"
    readonly property color accentHover: "#008F66"
    readonly property color accentLight: "#E6F7F3"
    readonly property color success: "#34C759"
    readonly property color warning: "#FF9500"
    readonly property color error: "#FF3B30"
    readonly property color textPrimary: "#1D1D1F"
    readonly property color textSecondary: "#86868B"
    readonly property color border: "#F0F0F2"
    readonly property color sliderTrack: "#E5E5EA"
    readonly property color sliderFill: "#00A878"
    readonly property color navSelected: "#F0F0F2"
    readonly property color navHover: "#F5F5F7"

    // Dimensions
    readonly property int cardRadius: 16
    readonly property int smallRadius: 10
    readonly property int spacing: 20
    readonly property int sidebarWidth: 220
    readonly property int cardPadding: 24

    // Font - Noto Sans CJK SC (embedded)
    readonly property string fontFamily: "Noto Sans CJK SC"

    // Font sizes
    readonly property int fontTitle: 16
    readonly property int fontBody: 13
    readonly property int fontCaption: 12
    readonly property int fontSmall: 11
}
