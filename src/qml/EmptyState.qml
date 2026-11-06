import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Placeholder shown when no file is open: quick hint plus the recent
// files (from the library database) one click away.
ColumnLayout {
    id: root

    // list of file paths, newest first (bound to Library.recentFiles)
    property var recents: []
    // how many chips to show; the full list lives in the Library page
    readonly property int maxChips: 6

    signal openRecent(url path)

    Item { Layout.fillHeight: true }
    Rectangle {
        width: 84; height: 84; radius: 18
        color: Theme.card2
        Layout.alignment: Qt.AlignHCenter
        Text {
            anchors.centerIn: parent
            text: "PS"
            color: Theme.accent
            font.bold: true
            font.pixelSize: 34
        }
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Drop a game file here")
        color: Theme.text
        font.pixelSize: 18
        font.bold: true
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("PKG · ISO · exFAT images · game folders")
        color: Theme.muted
        font.pixelSize: 13
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("or use Open in the header")
        color: Theme.muted
        font.pixelSize: 12
    }

    Label {
        visible: root.recents.length > 0
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 14
        text: qsTr("Recent files")
        color: Theme.text
        font.pixelSize: 13
        font.bold: true
    }
    Flow {
        id: chips
        visible: root.recents.length > 0
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 560
        spacing: 6

        Repeater {
            model: root.recents.slice(0, root.maxChips)
            delegate: Rectangle {
                id: chip
                required property var modelData
                readonly property string fileName:
                    String(chip.modelData).split('/').pop()
                width: chipLabel.implicitWidth + 22
                height: 28
                radius: Theme.radiusSm + 2
                color: chipMouse.containsMouse ? Theme.accentSoft : Theme.card
                border.width: 1
                border.color: chipMouse.containsMouse ? Theme.accent : Theme.border

                Text {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: chip.fileName
                    color: chipMouse.containsMouse ? Theme.text : Theme.muted
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }

                MouseArea {
                    id: chipMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openRecent(chip.modelData)
                }
            }
        }
    }
    Item { Layout.fillHeight: true }
}
