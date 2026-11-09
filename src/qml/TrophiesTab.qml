import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import PSGameViewer

// Trophies tab: grade, name, detail and the carved 240x240 icons,
// plus an exporter that saves every icon as a PNG file.
Frame {
    id: root
    property var model: null
    property var controller: null

    signal statusMessage(string msg)
    background: Rectangle {
        color: Theme.card
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: {
                    const n = root.model ? root.model.count : 0
                    return n === 1 ? qsTr("1 trophy")
                                   : qsTr("%1 trophies").arg(n)
                }
                color: Theme.muted
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Export icons…")
                flat: true
                font.pixelSize: 12
                enabled: !!root.model && root.model.count > 0
                          && root.controller !== null
                          && !root.controller.trophiesLoading
                onClicked: exportDialog.open()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 4
            model: root.model
            ScrollBar.vertical: AppScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: root.controller && root.controller.trophiesLoading
                text: qsTr("Loading trophies…")
                color: Theme.muted
            }
            Label {
                anchors.centerIn: parent
                visible: !root.model || (root.model.count === 0
                            && !(root.controller && root.controller.trophiesLoading))
                text: qsTr("No trophy pack in this file")
                color: Theme.muted
            }

            delegate: Item {
                width: list.width
                height: 56
                required property int index
                required property string name
                required property string detail
                required property string tid
                required property string grade
                required property bool isHidden
                required property int iconIndex
                required property bool hasIcon
                Accessible.role: Accessible.ListItem
                Accessible.name: name

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        source: hasIcon
                                ? "image://psg/trophy/" + iconIndex : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        visible: hasIcon
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        RowLayout {
                            spacing: 8
                            Text {
                                text: name
                                color: Theme.text
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                visible: grade.length > 0
                                width: gradeText.implicitWidth + 10
                                height: 16; radius: Theme.radiusSm - 2
                                color: {
                                    // conventional grade colors:
                                    // platinum, gold, silver, bronze
                                    if (grade === "P") return "#d8dde7"
                                    if (grade === "G") return "#e0b94b"
                                    if (grade === "S") return "#aab3c0"
                                    return "#c07a45"
                                }
                                Text {
                                    id: gradeText
                                    anchors.centerIn: parent
                                    text: grade
                                    color: "#171717"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Text {
                                visible: isHidden
                                text: qsTr("hidden")
                                color: Theme.muted
                                font.pixelSize: 11
                                font.italic: true
                            }
                        }
                        Text {
                            text: detail
                            color: Theme.muted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
                // rows are read-only: no hover tint, no mouse area — a
                // highlighted row that does nothing reads as broken
            }
        }
    }

    FolderDialog {
        id: exportDialog
        title: qsTr("Save trophy icons to folder")
        onAccepted: {
            var msg = root.controller.exportTrophyIcons(selectedFolder)
            root.statusMessage(msg)
        }
    }
}
