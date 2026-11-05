import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Rounded check box with an accent-filled checked state and a keyboard
// focus ring. Colors come from the Theme singleton.
Controls.CheckBox {
    id: root

    font.family: Theme.fontFamily

    spacing: 8
    opacity: enabled ? 1.0 : 0.45

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: root.text ? 0 : (root.width - width) / 2
        y: root.height / 2 - height / 2
        radius: Theme.radiusSm - 2

        color: root.checked ? Theme.accent : Theme.inputBg
        border.width: root.checked ? 0 : (root.hovered ? 2 : 1)
        border.color: root.hovered ? Theme.muted : Theme.border

        // check mark, drawn as text so the software renderer is enough
        Text {
            anchors.centerIn: parent
            text: "✓"
            visible: root.checked
            color: Theme.accentText
            font.pixelSize: 12
            font.bold: true
        }

        // keyboard focus ring
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: root.activeFocus ? 2 : 0
            border.color: Theme.accent
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        leftPadding: root.indicator.width + root.spacing
    }
}
