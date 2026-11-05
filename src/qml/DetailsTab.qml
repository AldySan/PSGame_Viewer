import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Details tab: curated lines with a "show all" toggle.
Frame {
    id: root
    property var curated: []
    property var full: []

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Metadata")
                color: Theme.muted
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            CheckBox {
                id: showAll
                text: qsTr("Show all")
                font.pixelSize: 12
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: showAll.checked ? root.full : root.curated

            delegate: Item {
                width: list.width
                height: 22
                required property string modelData
                required property int index

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    width: parent.width - 8
                    text: parent.modelData
                    color: Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
                Rectangle {
                    anchors.fill: parent
                    color: Theme.card2
                    visible: mouse.containsMouse
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Viewer.copyText(parent.modelData)
                }
            }
        }
    }
}
