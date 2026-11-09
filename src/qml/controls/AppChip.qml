import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Clickable inline chip (recent-file shortcut, rename pattern token).
// Styled as a small pill, but a real Button underneath: Tab reaches it,
// Space or Enter activates it, screen readers announce the text, and the
// accent border doubles as the keyboard focus ring.
Controls.Button {
    id: root

    font.family: Theme.fontFamily
    font.pixelSize: 12

    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    opacity: enabled ? 1.0 : 0.45

    contentItem: Text {
        text: root.text
        font: root.font
        // muted at rest, full text on interaction: muted text on the
        // accentSoft hover fill would fail 4.5:1
        color: root.hovered || root.activeFocus ? Theme.text : Theme.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle
    }

    background: Rectangle {
        radius: Theme.radiusSm + 2
        color: root.hovered ? Theme.accentSoft : Theme.card
        border.width: root.activeFocus ? 2 : 1
        border.color: root.hovered || root.activeFocus ? Theme.accent
                                                       : Theme.border
    }
}
