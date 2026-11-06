import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Files tab: flat expandable tree over the container entries.
ColumnLayout {
    id: root
    property var model: null

    signal extractRequested(int entryIndex)
    signal extractAllRequested()

    RowLayout {
        spacing: 8
        Label {
            text: root.model
                  ? qsTr("%1 entries").arg(root.model.totalEntries) : ""
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
        }
        AppButton {
            text: qsTr("Expand")
            flat: true
            onClicked: root.model ? root.model.expandAll() : 0
        }
        AppButton {
            text: qsTr("Collapse")
            flat: true
            onClicked: root.model ? root.model.collapseAll() : 0
        }
        AppButton {
            text: qsTr("Extract all…")
            onClicked: root.extractAllRequested()
        }
    }

    Frame {
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            color: Theme.card
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}

            // header
            header: Item {
                width: list.width
                height: 26
                Label {
                    x: list.depthIndent(0) + 26
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Name")
                    color: Theme.muted
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 120
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Size")
                    color: Theme.muted
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Info")
                    color: Theme.muted
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            function depthIndent(d) { return 8 + d * 16 }

            delegate: Item {
                width: list.width
                height: 28
                required property int index
                required property string name
                required property string path
                required property string sizeText
                required property string infoText
                required property int depth
                required property bool isFolder
                required property bool expanded
                required property int entryIndex

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Theme.radiusSm
                    color: mouse.containsMouse ? Theme.card2 : "transparent"
                }

                Text {
                    x: list.depthIndent(depth) + (isFolder ? 4 : 26)
                    anchors.verticalCenter: parent.verticalCenter
                    text: isFolder ? (expanded ? "▾ " : "▸ ") : ""
                    color: Theme.muted
                    font.pixelSize: 12
                    visible: isFolder
                }
                Text {
                    x: list.depthIndent(depth) + 26
                    width: parent.width - x - 220
                    anchors.verticalCenter: parent.verticalCenter
                    text: name
                    color: isFolder ? Theme.text : Theme.text
                    font.pixelSize: 12
                    font.bold: isFolder
                    elide: Text.ElideMiddle
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 120
                    anchors.verticalCenter: parent.verticalCenter
                    text: sizeText
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: infoText
                    color: Theme.muted
                    font.pixelSize: 11
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (isFolder)
                            root.model.toggle(index)
                        else
                            root.model.toggle(index) // no-op for files
                    }
                    onDoubleClicked: {
                        if (!isFolder && entryIndex >= 0)
                            root.extractRequested(entryIndex)
                    }
                }
            }
        }
    }
}
