import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

// Central viewer: cover panel on the left, spec card and tabs on the right.
Item {
    id: root
    required property var controller

    signal statusMessage(string msg)

    readonly property bool empty: !controller.hasFile

    EmptyState {
        anchors.fill: parent
        visible: root.empty
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14
        visible: !root.empty

        CoverPanel {
            id: coverPanel
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            controller: root.controller
            onStatusMessage: function(msg) { root.statusMessage(msg) }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            RowLayout {
                spacing: 10
                Label {
                    text: controller.title
                    color: Theme.text
                    font.pixelSize: 20
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Rectangle {
                    visible: controller.packageBadge.length > 0
                    width: badgeText.implicitWidth + 16
                    height: 24; radius: Theme.radiusSm
                    color: controller.packageBadge === "OFC" ? Theme.good : Theme.accent
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: controller.packageBadge
                        color: "#171717"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
            Label {
                visible: controller.patchInfo.length > 0
                text: qsTr("Latest patch: %1").arg(controller.patchInfo)
                color: Theme.muted
                font.pixelSize: 12
            }

            SpecCard {
                Layout.fillWidth: true
                rows: controller.rows
            }

            TabBar {
                id: tabBar
                objectName: "tabBar"
                Layout.fillWidth: true
                background: null
                spacing: 6
                AppTabButton { text: qsTr("Files") }
                AppTabButton { text: qsTr("Details") }
                AppTabButton { text: qsTr("Trophies") }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabBar.currentIndex

                FilesTab {
                    model: Viewer.entryModel
                    onExtractRequested: function(entryIndex) {
                        extractFolder.selectedEntry = entryIndex
                        extractFolder.open()
                    }
                    onExtractAllRequested: {
                        extractFolder.selectedEntry = -1
                        extractFolder.open()
                    }
                }
                DetailsTab {
                    curated: controller.detailsCurated
                    full: controller.detailsAll
                }
                TrophiesTab {
                    model: Viewer.trophyModel
                    controller: root.controller
                    onStatusMessage: function(msg) { root.statusMessage(msg) }
                }
            }
        }
    }

    FolderDialog {
        id: extractFolder
        title: qsTr("Extract to folder")
        property int selectedEntry: -1
        onAccepted: {
            if (selectedEntry >= 0)
                controller.extractEntry(selectedEntry, selectedFolder)
            else
                controller.extractAll(selectedFolder)
        }
    }
}
