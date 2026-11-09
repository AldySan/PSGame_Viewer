import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

Item {
    id: root
    required property var controller

    signal statusMessage(string msg)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.card
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1

            Image {
                id: cover
                anchors.fill: parent
                anchors.margins: 12
                source: controller.coverKey
                        ? "image://psg/" + controller.coverKey : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
            }

            // gallery strip: real buttons so the strip is reachable and
            // operable with the keyboard (Tab + Space), not mouse-only
            Row {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 8
                spacing: 6
                visible: controller.galleryKeys.length > 1

                Repeater {
                    model: controller.galleryKeys
                    delegate: Button {
                        id: shot
                        required property var modelData
                        required property int index
                        readonly property bool selected:
                            shot.modelData === controller.coverKey
                        width: 40; height: 40
                        padding: 0
                        topPadding: 0
                        bottomPadding: 0
                        leftPadding: 0
                        rightPadding: 0
                        Accessible.name: qsTr("Screenshot %1").arg(index + 1)
                        Accessible.description: qsTr("Use this screenshot as the cover")

                        contentItem: Item {
                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                source: "image://psg/" + shot.modelData
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: false
                            }
                        }

                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: shot.selected ? Theme.accentSoft : Theme.card2
                            // 2px marks both the selected state and the
                            // keyboard focus ring
                            border.width: shot.selected || shot.activeFocus ? 2 : 1
                            border.color: shot.selected || shot.activeFocus
                                          ? Theme.accent : Theme.border
                        }

                        onClicked: controller.selectCover(shot.modelData)
                    }
                }
            }
        }

        RowLayout {
            spacing: 8
            AppButton {
                text: qsTr("Save")
                enabled: controller.coverKey.length > 0
                Layout.fillWidth: true
                onClicked: coverSaveDialog.open()
            }
            AppButton {
                text: qsTr("Copy")
                enabled: controller.coverKey.length > 0
                Layout.fillWidth: true
                onClicked: root.statusMessage(controller.copyCoverToClipboard())
            }
        }
    }

    FileDialog {
        id: coverSaveDialog
        title: qsTr("Save cover image")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            if (controller.saveCover(selectedFile))
                root.statusMessage(qsTr("Cover saved"))
            else
                root.statusMessage(qsTr("Could not save the cover"))
        }
    }
}
