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
                text: qsTr("%1 trophies").arg(root.model ? root.model.count : 0)
                color: Theme.muted
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Export icons…")
                flat: true
                font.pixelSize: 12
                enabled: !!root.model && root.model.count > 0
                          && root.controller !== null
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
            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: !root.model || root.model.count === 0
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

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: mouse.containsMouse ? Theme.card2 : "transparent"
                }

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
                                height: 16; radius: 3
                                color: {
                                    if (grade === "P") return "#e0b94b"
                                    if (grade === "G") return "#c8cdd6"
                                    if (grade === "S") return "#e08a4e"
                                    return "#b06fc4" // B(ronze)
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

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
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
