import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Single-file rename dialog: live preview, apply, and undo.
Popup {
    id: root
    objectName: "renameDialog"
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
        radius: Theme.radiusLg
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
            radius: Theme.radiusSm
            color: Theme.card2
            border.width: 1
            // accent marks the result box; the name itself stays in text
            // color (accent text on card2 failed 4.5:1 in both themes)
            border.color: Theme.accent
            Label {
                id: previewLabel
                anchors.fill: parent
                anchors.margins: 8
                // `hasFile` is read here on purpose: the dialog (and this
                // binding) is created before any file is opened, so the
                // preview must re-run when the opened file changes
                text: {
                    const fileLoaded = root.controller.hasFile
                    return patternBar.pattern.length === 0
                        ? root.controller.renamePreview("", patternBar.include)
                        : root.controller.renamePreview(patternBar.pattern,
                                                        patternBar.include)
                }
                color: Theme.text
                font.pixelSize: 13
                elide: Text.ElideMiddle
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            AppButton {
                text: qsTr("Undo")
                enabled: root.controller.undoAvailable
                onClicked: {
                    if (root.controller.undoRename())
                        root.statusMessage(qsTr("Rename undone"))
                }
            }
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Cancel")
                flat: true
                onClicked: root.close()
            }
            AppButton {
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
