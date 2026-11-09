pragma Singleton
import QtQuick
import PSGameViewer

// Color system shared by every page. Dark is the default; a light palette
// is selected through the theme setting (AppSettings.theme).
QtObject {
    readonly property bool light: AppSettings.theme === "light"

    readonly property color bg: light ? "#f4f5f7" : "#171717"
    readonly property color card: light ? "#ffffff" : "#202020"
    readonly property color card2: light ? "#eceef2" : "#2a2a2a"
    readonly property color accent: light ? "#2f6fd0" : "#4f8ef7"
    readonly property color accentPressed: light ? "#2558a6" : "#3b70c9"
    readonly property color text: light ? "#1d2330" : "#f1f3f8"
    readonly property color muted: light ? "#5c6575" : "#8b93a5"
    // component boundaries must clear WCAG 1.4.11 (3:1) against every
    // surface they touch: card, bg, card2 and inputBg. The strictest
    // surface per theme picks the value (card2 in both), so anything
    // drawn with this token is identifiable without the fill.
    readonly property color border: light ? "#82868e" : "#767676"
    readonly property color danger: light ? "#c0392b" : "#e26055"
    readonly property color good: light ? "#27863a" : "#5bbf6b"

    // text color used on top of an accent-filled surface
    readonly property color accentText: light ? "#ffffff" : "#0e1116"
    // accent-tinted fill for selected tabs, menu highlights, hovers.
    // Strong on purpose: a 3:1 fill (WCAG 1.4.11 for state) instead of
    // a whisper tint, while text on top still clears 4.5:1 in both
    // themes, so the same token works as a hover fill and a selection
    // background.
    readonly property color accentSoft: light ? "#598cd9" : "#3b6bb9"
    // neutral overlay for hover states on transparent controls
    readonly property color hoverOverlay: light ? "#0d000000" : "#12ffffff"
    // pressed overlay darkens whatever base the control has
    readonly property color pressOverlay: light ? "#14000000" : "#1af0f0f0"
    // input surfaces sit slightly apart from cards
    readonly property color inputBg: light ? "#ffffff" : "#1b1b1b"

    // corner radius scale: small chips -> cards -> dialogs
    readonly property int radiusSm: 6
    readonly property int radius: 8
    readonly property int radiusLg: 12

    readonly property string fontFamily: "Segoe UI"
}
