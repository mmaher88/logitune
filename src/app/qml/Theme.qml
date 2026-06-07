pragma Singleton
import QtQuick

QtObject {
    id: theme

    // Dark mode — initialized from system theme via C++, toggled by user in Settings
    property bool dark: false

    // Colors
    readonly property color accent: dark ? "#00EAD0" : "#814EFA"
    readonly property color accentHover: dark ? "#00C4AD" : "#673EC8"
    readonly property color background: dark ? "#000000" : "#FFFFFF"
    readonly property color surface: dark ? "#1A1A1C" : "#F5F5F5"
    readonly property color text: dark ? "#FBFBFB" : "#222425"
    readonly property color textSecondary: "#999999"
    readonly property color border: dark ? "#252527" : "#F0F0F0"
    readonly property color inputBg: dark ? "#2C2C2F" : "#E1E2E3"
    readonly property color cardBg: dark ? "#222425" : "#FFFFFF"
    readonly property color cardBorder: dark ? "#2C2C2F" : "#E8E8E8"
    readonly property color hoverBg: dark ? "#1E1E21" : "#F0EDFF"
    readonly property color selectedBg: accent
    readonly property color batteryGreen: "#79E053"
    readonly property color batteryWarning: "#FFA414"

    // Sidebar active tab
    readonly property color activeTabBg: accent
    readonly property color activeTabText: dark ? "#000000" : "#FFFFFF"
}
