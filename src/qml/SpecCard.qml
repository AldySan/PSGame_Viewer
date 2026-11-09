import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Spec card: two-column grid of label/value rows. Values copy on click
// (with a tooltip saying so); each value is a real button so the copy
// gesture also works from the keyboard.
Frame {
    id: root
    property var rows: []

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    implicitHeight: grid.implicitHeight + 20

    GridLayout {
        id: grid
        columns: 2
        columnSpacing: 16
        rowSpacing: 4
        width: parent.width

        Repeater {
            model: root.rows
            Label {
                Layout.column: 0
                Layout.row: index
                text: modelData.label
                color: Theme.muted
                font.pixelSize: 12
            }
        }
        Repeater {
            model: root.rows
            Button {
                id: valueButton
                Layout.column: 1
                Layout.row: index
                Layout.fillWidth: true
                // required properties opt out of the injected context, so
                // index has to be declared here too
                required property var modelData
                required property int index

                flat: true
                topPadding: 2
                bottomPadding: 2
                leftPadding: 2
                rightPadding: 2
                text: modelData.value

                contentItem: Text {
                    text: valueButton.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    color: Theme.text
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: valueButton.pressed ? Theme.pressOverlay
                           : valueButton.hovered ? Theme.hoverOverlay
                           : "transparent"
                    border.width: valueButton.activeFocus ? 2 : 0
                    border.color: Theme.accent
                }

                onClicked: Viewer.copyText(modelData.value)

                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: qsTr("Click to copy")
            }
        }
    }
}
