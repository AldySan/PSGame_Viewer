import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Rounded chip-style tab button. Unselected tabs are ghost buttons; the
// selected tab gets an accent-tinted pill with a hairline accent border.
Controls.TabButton {
    id: root

    font.family: Theme.fontFamily
    font.weight: checked ? Font.DemiBold : Font.Normal

    leftPadding: 14
    rightPadding: 14
    topPadding: 6
    bottomPadding: 6

    opacity: enabled ? 1.0 : 0.45

    contentItem: Text {
        text: root.text
        font: root.font
        // selected tabs keep plain text color: the state is carried by the
        // accentSoft fill plus the accent border, and accent text on that
        // fill failed 4.5:1 in both themes
        color: Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.checked ? Theme.accentSoft
               : root.hovered ? Theme.hoverOverlay
               : "transparent"
        border.width: root.activeFocus ? 2 : root.checked ? 1 : 0
        border.color: Theme.accent
    }
}
