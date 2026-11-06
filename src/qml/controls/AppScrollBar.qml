import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Slim rounded scrollbar thumb; the accent color appears on interaction.
Controls.ScrollBar {
    id: root

    implicitWidth: 10
    implicitHeight: 10

    background: null

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: root.pressed ? Theme.accentPressed
               : (root.hovered || root.active) ? Theme.accent
               : Theme.muted
    }
}
