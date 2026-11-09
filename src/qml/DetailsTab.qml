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
            AppCheckBox {
                id: showAll
                text: qsTr("Show all")
                font.pixelSize: 12
            }
        }

        ListView {
            id: list
            objectName: "detailsList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}
            model: showAll.checked ? root.full : root.curated

            // keyboard path for the click-to-copy gesture: Tab lands on
            // the list, arrows move, Enter or Space copies the line
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Metadata")
            Keys.onReturnPressed: copyCurrent()
            Keys.onEnterPressed: copyCurrent()
            Keys.onSpacePressed: copyCurrent()
            onActiveFocusChanged: {
                if (activeFocus && currentIndex < 0 && count > 0)
                    currentIndex = 0
            }
            function copyCurrent() {
                if (currentItem)
                    Viewer.copyText(currentItem.lineText)
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

            delegate: Item {
                width: list.width
                height: 22
                required property string modelData
                required property int index
                // exposed for the view's copyCurrent() above
                readonly property string lineText: modelData
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData

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
                    anchors.margins: 1
                    radius: Theme.radiusSm
                    color: Theme.card2
                    visible: mouse.containsMouse
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Viewer.copyText(parent.lineText)
                }

                // the copy gesture is invisible without this hint
                ToolTip.visible: mouse.containsMouse
                ToolTip.delay: 600
                ToolTip.text: qsTr("Click to copy")
            }
        }
    }
}
