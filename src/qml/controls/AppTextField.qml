import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Rounded single-line input with an accent focus ring. Colors come from
// the Theme singleton (no system-palette dependency).
Controls.TextField {
    id: root

    font.family: Theme.fontFamily

    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText

    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent
                      : root.hovered ? Qt.lighter(Theme.border, 1.2)
                      : Theme.border
    }
}
