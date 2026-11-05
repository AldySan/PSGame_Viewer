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

            // gallery strip
            Row {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 8
                spacing: 6
                visible: controller.galleryKeys.length > 1

                Repeater {
                    model: controller.galleryKeys
                    delegate: Rectangle {
                        width: 40; height: 40
                        radius: Theme.radiusSm
                        color: modelData === controller.coverKey
                               ? Theme.accentSoft : Theme.card2
                        border.width: modelData === controller.coverKey ? 1 : 1
                        border.color: modelData === controller.coverKey
                                      ? Theme.accent : Theme.border
                        Image {
                            anchors.fill: parent
                            anchors.margins: 3
                            source: "image://psg/" + modelData
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: controller.selectCover(modelData)
                        }
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
