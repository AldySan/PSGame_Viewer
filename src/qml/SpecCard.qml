import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Spec card: two-column grid of label/value rows.
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
            Label {
                Layout.column: 1
                Layout.row: index
                Layout.fillWidth: true
                text: modelData.value
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideMiddle
                // allow copying the value out
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        valueMenu.textToCopy = modelData.value
                        valueMenu.popup()
                    }
                }
            }
        }
    }

    Menu {
        id: valueMenu
        property string textToCopy: ""
        padding: 6
        background: Rectangle {
            radius: Theme.radius
            color: Theme.card
            border.color: Theme.border
        }
        MenuItem {
            id: copyItem
            text: qsTr("Copy")
            padding: 6
            leftPadding: 10
            rightPadding: 10
            contentItem: Text {
                text: copyItem.text
                color: Theme.text
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSm
                color: copyItem.hovered || copyItem.highlighted
                       ? Theme.accentSoft : "transparent"
            }
            onTriggered: {
                // small helper via controller-free clipboard access
                Viewer.copyText(valueMenu.textToCopy)
            }
        }
    }
}
