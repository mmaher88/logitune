pragma Singleton
import QtQuick

QtObject {
    // Detect system dark mode — Qt.styleHints.colorScheme available in Qt 6.5+
    // Detect dark mode: check if the system window background is dark
    // Works across all Qt 6 versions and desktop environments
    readonly property color _windowBg: Qt.application && Qt.application.palette
        ? Qt.application.palette.window : "#FFFFFF"
    readonly property bool dark: (_windowBg.r * 0.299 + _windowBg.g * 0.587 + _windowBg.b * 0.114) < 0.5

    // Colors
    readonly property color accent: dark ? "#00EAD0" : "#814EFA"
    readonly property color accentHover: dark ? "#00C4AD" : "#673EC8"
    readonly property color background: dark ? "#000000" : "#FFFFFF"
    readonly property color surface: dark ? "#1A1A1C" : "#F5F5F5"
    readonly property color text: dark ? "#FBFBFB" : "#222425"
    readonly property color textSecondary: "#999999"
    readonly property color border: dark ? "#333333" : "#F0F0F0"
    readonly property color inputBg: dark ? "#333333" : "#E1E2E3"
    readonly property color cardBg: dark ? "#222425" : "#FFFFFF"
    readonly property color cardBorder: dark ? "#333333" : "#E8E8E8"
    readonly property color hoverBg: dark ? "#2A2A2C" : "#F0EDFF"
    readonly property color selectedBg: accent
    readonly property color batteryGreen: "#79E053"
    readonly property color batteryWarning: "#FFA414"

    // Callout colors for Point & Scroll page
    readonly property color calloutGradientTop: dark ? "#00EAD0" : "#A04EFA"
    readonly property color calloutGradientBottom: dark ? "#00C4AD" : "#814EFA"

    // Sidebar active tab
    readonly property color activeTabBg: accent
    readonly property color activeTabText: dark ? "#000000" : "#FFFFFF"
}
