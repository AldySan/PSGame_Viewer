import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

// Library page: searchable list of known games (SQLite), folder scan and
// per-entry open/remove.
ColumnLayout {
    id: root
    signal openRequested(url path)
    signal statusMessage(string msg)

    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search title, Title ID or Content ID…")
            font.pixelSize: 13
            color: Theme.text
            onTextChanged: Library.setSearch(text)
            background: Rectangle {
                radius: 4
                color: Theme.card2
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
            }
        }
        Button {
            text: qsTr("Scan folder…")
            onClicked: scanDialog.open()
        }
        Button {
            text: qsTr("Clean missing")
            flat: true
            onClicked: Library.removeMissing()
        }
    }

    Frame {
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            color: Theme.card
            radius: Theme.radius
            border.color: Theme.border
        }

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2
            model: Library
            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: Library.count === 0
                text: qsTr("Library is empty — open files or scan a folder")
                color: Theme.muted
            }

            delegate: Item {
                width: list.width
                height: 52
                required property int index
                required property string title
                required property string path
                required property string platform
                required property string titleId
                required property string region
                required property string version
                required property string sizeText
                required property string addedText

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: mouse.containsMouse ? Theme.card2 : "transparent"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 30
                        radius: 4
                        color: {
                            if (platform === "PS2") return "#1d4ed8"
                            if (platform === "PS3") return "#0e7490"
                            if (platform === "PS4") return "#5b21b6"
                            return "#9a3412"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: platform
                            color: "#f1f3f8"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Text {
                            text: title
                            color: Theme.text
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: {
                                var bits = []
                                if (titleId) bits.push(titleId)
                                if (region && region !== "-") bits.push(region)
                                if (version && version !== "-") bits.push("v" + version)
                                bits.push(sizeText)
                                bits.push(addedText)
                                return bits.join("  ·  ")
                            }
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        text: qsTr("Open")
                        flat: true
                        onClicked: Library.openPath(path)
                    }
                    Button {
                        text: qsTr("Remove")
                        flat: true
                        onClicked: Library.removeAt(index)
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: Library.openPath(path)
                }
            }
        }
    }

    FolderDialog {
        id: scanDialog
        title: qsTr("Folder to scan")
        onAccepted: Library.scanFolder(selectedFolder, true)
    }

    Connections {
        target: Library
        function onStatusMessage(message) { root.statusMessage(message) }
    }
}
