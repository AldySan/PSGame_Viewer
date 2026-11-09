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
            text: {
                if (!root.model)
                    return ""
                const n = root.model.totalEntries
                return n === 1 ? qsTr("1 entry")
                               : qsTr("%1 entries").arg(n)
            }
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
            objectName: "filesList"
            anchors.fill: parent
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}

            // keyboard equivalents of the mouse gestures: Left/Right fold
            // and unfold the current folder, Space toggles it, Enter opens
            // the extract dialog for a file
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("File entries")
            onActiveFocusChanged: {
                if (activeFocus && currentIndex < 0 && count > 0)
                    currentIndex = 0
            }
            Keys.onLeftPressed: {
                if (currentItem && currentItem.isFolder && currentItem.expanded)
                    root.model.toggle(currentItem.index)
            }
            Keys.onRightPressed: {
                if (currentItem && currentItem.isFolder && !currentItem.expanded)
                    root.model.toggle(currentItem.index)
            }
            Keys.onSpacePressed: {
                if (currentItem && currentItem.isFolder)
                    root.model.toggle(currentItem.index)
            }
            Keys.onReturnPressed: activateCurrent()
            Keys.onEnterPressed: activateCurrent()

            function activateCurrent() {
                if (!currentItem)
                    return
                if (currentItem.isFolder)
                    root.model.toggle(currentItem.index)
                else if (currentItem.entryIndex >= 0)
                    root.extractRequested(currentItem.entryIndex)
            }

            highlight: Rectangle {
                // current-row focus ring; mouse users never see it
                visible: list.activeFocus
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
                radius: Theme.radiusSm
            }
            highlightMoveDuration: 0

            // empty container: honest state instead of a bare frame
            Label {
                anchors.centerIn: parent
                visible: root.model && root.model.totalEntries === 0
                text: qsTr("No entries in this container")
                color: Theme.muted
            }

            // header
            header: Item {
                width: list.width
                height: 26
                Label {
                    // depth-1 rows (the shallowest visible rows, files of
                    // the root sit at depth 1) put their names at
                    // depthIndent(1) + 26 - align the header with those
                    x: list.depthIndent(1) + 26
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
                Accessible.role: Accessible.ListItem
                // the fold state is named in the announcement so the row
                // makes sense without the ▾/▸ glyphs
                Accessible.name: isFolder
                                  ? qsTr("Folder %1").arg(name)
                                  : name
                Accessible.description: isFolder
                        ? expanded ? qsTr("Expanded")
                                   : qsTr("Collapsed")
                        : qsTr("Double-click to extract")

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
                    // the fold state is in Accessible.description; the
                    // glyph itself is decoration for sighted users
                    Accessible.ignored: true
                }
                Text {
                    x: list.depthIndent(depth) + 26
                    width: parent.width - x - 220
                    anchors.verticalCenter: parent.verticalCenter
                    text: name
                    color: isFolder ? Theme.text : Theme.muted
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
                    // only folders fold; toggle() already ignores files
                    onClicked: {
                        if (isFolder)
                            root.model.toggle(index)
                    }
                    onDoubleClicked: {
                        if (!isFolder && entryIndex >= 0)
                            root.extractRequested(entryIndex)
                    }
                }

                // the extract gesture is invisible without this hint
                ToolTip.visible: mouse.containsMouse && !isFolder
                                 && entryIndex >= 0
                ToolTip.delay: 600
                ToolTip.text: qsTr("Double-click to extract")
            }
        }
    }
}
