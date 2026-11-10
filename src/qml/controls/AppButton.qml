import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Rounded button shared by the whole app. Three variants map onto the
// standard Button properties:
//   default     - subtle surface with a border
//   flat        - ghost button (transparent, tinted on hover)
//   highlighted - accent-filled primary action
// plus `destructive` for the rare danger-filled action (confirming a
// removal), so danger fills stay deliberate and findable.
// Colors are taken from the Theme singleton so the runtime dark/light
// switch keeps working without depending on the system palette.
Controls.Button {
    id: root

    property bool destructive: false

    font.family: Theme.fontFamily

    leftPadding: 16
    rightPadding: 16
    topPadding: 7
    bottomPadding: 7

    opacity: enabled ? 1.0 : 0.45

    scale: pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.highlighted || root.destructive
               ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.highlighted
               ? (root.pressed ? Theme.accentPressed
                  : root.hovered ? Qt.lighter(Theme.accent, 1.06)
                  : Theme.accent)
               : root.destructive
               ? (root.pressed ? Qt.darker(Theme.danger, 1.15)
                  : root.hovered ? Qt.lighter(Theme.danger, 1.06)
                  : Theme.danger)
               : (root.flat ? "transparent" : Theme.card2)
        border.width: root.flat || root.highlighted || root.destructive
                      ? 0 : 1
        border.color: root.hovered && !root.highlighted
                      ? Qt.lighter(Theme.border, 1.18) : Theme.border

        // hover / press tint layered on the base color
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: !root.highlighted && !root.destructive
            color: root.pressed ? Theme.pressOverlay
                   : root.hovered ? Theme.hoverOverlay
                   : "transparent"
        }

        // keyboard focus ring
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: root.activeFocus ? 2 : 0
            border.color: root.highlighted
                           ? Qt.rgba(1, 1, 1, 0.55) : Theme.accent
        }
    }
}
