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

        AppTextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search title, Title ID or Content ID…")
            font.pixelSize: 13
            // placeholder text is the only label; keep it as the accessible
            // name so screen readers announce what the field is for
            Accessible.name: placeholderText
            // debounce: without the timer every keystroke wrote the
            // property, which re-queries SQLite and resets the whole
            // ListView (scroll jump + flicker on large libraries)
            Timer {
                id: searchDebounce
                interval: 200
                onTriggered: Library.search = searchField.text
            }
            // property write (not a method call): setSearch() is the
            // Q_PROPERTY write accessor and is not callable from QML
            onTextChanged: searchDebounce.restart()
        }
        AppButton {
            text: qsTr("Scan folder…")
            onClicked: scanDialog.open()
        }
        AppButton {
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
            objectName: "libraryList"
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2
            model: Library
            ScrollBar.vertical: AppScrollBar {}

            // keyboard path for the double-click shortcut: Tab lands on
            // the list, arrows move, Enter opens the current game
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Library")
            onActiveFocusChanged: {
                if (activeFocus && currentIndex < 0 && count > 0)
                    currentIndex = 0
            }
            Keys.onReturnPressed: if (currentItem) root.openRequested(currentItem.path)
            Keys.onEnterPressed: if (currentItem) root.openRequested(currentItem.path)

            highlight: Rectangle {
                // current-row focus ring; mouse users never see it
                visible: list.activeFocus
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
                radius: Theme.radiusSm
            }
            highlightMoveDuration: 0

            Label {
                anchors.centerIn: parent
                visible: Library.count === 0
                width: Math.min(parent.width - 24, implicitWidth)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: Library.error !== ""
                      ? qsTr("The library database could not be opened:<br>%1<br>Check that the folder with the executable is writable.")
                        .arg(Library.error)
                      : qsTr("Library is empty. Open a file or scan a folder")
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
                Accessible.role: Accessible.ListItem
                Accessible.name: title

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Theme.radiusSm
                    color: mouse.containsMouse ? Theme.card2 : "transparent"
                }

                // declared BEFORE the row: as a later sibling it would sit
                // on top of the row and swallow every click on the Open
                // and Remove buttons (only the double-click worked)
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: root.openRequested(path)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 30
                        radius: Theme.radiusSm
                        color: {
                            if (platform === "PS2") return Theme.platformPs2
                            if (platform === "PS3") return Theme.platformPs3
                            if (platform === "PS4") return Theme.platformPs4
                            return Theme.platformPs5
                        }
                        Text {
                            anchors.centerIn: parent
                            text: platform
                            color: Theme.platformChipText
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

                    AppButton {
                        text: qsTr("Open")
                        flat: true
                        // route through the page signal: Main.qml opens the
                        // file in the viewer and switches the page
                        onClicked: root.openRequested(path)
                    }
                    AppButton {
                        text: qsTr("Remove")
                        flat: true
                        onClicked: Library.removeAt(index)
                    }
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
