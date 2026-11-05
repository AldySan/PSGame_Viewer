import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Single-file rename dialog: live preview, apply, and undo.
Popup {
    id: root
    required property var controller
    signal statusMessage(string msg)

    anchors.centerIn: parent
    width: 560
    height: 320
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 18

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius + 4
        border.color: Theme.border
    }

    onAboutToShow: {
        patternBar.loadPrefs()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: qsTr("Rename")
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            text: root.controller.fileName
            color: Theme.muted
            font.pixelSize: 12
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        NamePatternBar {
            id: patternBar
            controller: root.controller
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: previewLabel.implicitHeight + 16
            radius: 4
            color: Theme.card2
            Label {
                id: previewLabel
                anchors.fill: parent
                anchors.margins: 8
                text: patternBar.pattern.length === 0
                      ? root.controller.renamePreview("", patternBar.include)
                      : root.controller.renamePreview(patternBar.pattern,
                                                      patternBar.include)
                color: Theme.accent
                font.pixelSize: 13
                elide: Text.ElideMiddle
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            Button {
                text: qsTr("Undo")
                enabled: root.controller.undoAvailable
                onClicked: {
                    if (root.controller.undoRename())
                        root.statusMessage(qsTr("Rename undone"))
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: root.close()
            }
            Button {
                text: qsTr("Rename")
                highlighted: true
                enabled: previewLabel.text.length > 0
                onClicked: {
                    patternBar.savePrefs()
                    var target = root.controller.applyRename(
                        patternBar.pattern, patternBar.include)
                    if (target.length > 0) {
                        root.close()
                    } else {
                        root.statusMessage(qsTr(
                            "Rename failed (target exists or not writable)"))
                    }
                }
            }
        }
    }
}
