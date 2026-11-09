import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

// Batch rename: scan folders, preview every rename, apply with a revert
// log, and revert from an earlier log.
Popup {
    id: root
    objectName: "batchDialog"
    required property var controller
    signal statusMessage(string msg)

    anchors.centerIn: parent
    width: 760
    height: 560
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    padding: 18

    background: Rectangle {
        color: Theme.card
        radius: Theme.radiusLg
        border.color: Theme.border
    }

    property var plan: []
    property int okCount: 0

    onAboutToShow: {
        patternBar.loadPrefs()
    }

    // previewing re-parses every scanned file; debounce so typing a
    // pattern does not parse on every keystroke
    Timer {
        id: refreshTimer
        interval: 250
        onTriggered: root.refresh()
    }
    function scheduleRefresh() { refreshTimer.restart() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: qsTr("Batch rename")
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }

        NamePatternBar {
            id: patternBar
            controller: root.controller
            Layout.fillWidth: true
            onPatternChanged: root.scheduleRefresh()
            onIncludeChanged: root.scheduleRefresh()
        }

        RowLayout {
            spacing: 8
            AppButton {
                text: qsTr("Add folder…")
                onClicked: scanDialog.open()
            }
            AppButton {
                text: qsTr("Add files…")
                onClicked: addDialog.open()
            }
            AppCheckBox {
                id: recursive
                text: qsTr("Recursive")
                font.pixelSize: 12
                onCheckedChanged: root.scheduleRefresh()
            }
            Label {
                id: scanInfo
                color: Theme.muted
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                color: Theme.bg
                radius: Theme.radiusSm
                border.color: Theme.border
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                model: root.plan
                ScrollBar.vertical: AppScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: root.plan.length === 0
                    text: qsTr("Add folders or files to see the preview")
                    color: Theme.muted
                }

                delegate: Item {
                    width: list.width
                    height: 24
                    required property var modelData
                    required property int index
                    Accessible.role: Accessible.ListItem
                    Accessible.name: modelData.ok
                          ? (modelData.newFiles[0] || "")
                          : modelData.reason

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.42
                        text: modelData.files.length > 1
                              ? ("%1 (+%2)").arg(modelData.files[0])
                                            .arg(modelData.files.length - 1)
                              : modelData.files[0]
                        color: modelData.ok ? Theme.text : Theme.danger
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        // direction marker for the eyes; the row above
                        // reads as "old name, then new name"
                        Accessible.ignored: true
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.42
                        horizontalAlignment: Text.AlignRight
                        text: modelData.ok
                              ? (modelData.newFiles[0] || "")
                              : modelData.reason
                        color: modelData.ok ? Theme.text : Theme.danger
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        RowLayout {
            spacing: 8
            Label {
                text: root.plan.length > 0
                      ? qsTr("%1 of %2 ready").arg(root.okCount)
                          .arg(root.plan.length)
                      : ""
                color: Theme.muted
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            AppButton {
                text: qsTr("Revert from log…")
                flat: true
                onClicked: revertDialog.open()
            }
            AppButton {
                text: qsTr("Close")
                flat: true
                onClicked: root.close()
            }
            AppButton {
                text: qsTr("Apply")
                highlighted: true
                enabled: root.okCount > 0
                onClicked: {
                    patternBar.savePrefs()
                    var res = root.controller.batchApply()
                    if (res.logError)
                        root.statusMessage(qsTr(
                            "Renamed %1, skipped %2. The revert log could not be written (%3), so these renames cannot be reverted from a log.")
                            .arg(res.renamed).arg(res.skipped).arg(res.logError))
                    else if (res.logPath)
                        root.statusMessage(qsTr(
                            "Renamed %1, skipped %2. Log: %3")
                            .arg(res.renamed).arg(res.skipped).arg(res.logPath))
                    else
                        root.statusMessage(qsTr(
                            "Renamed %1, skipped %2").arg(res.renamed)
                            .arg(res.skipped))
                    refresh()
                }
            }
        }
    }

    function refresh() {
        if (scanPaths.length === 0) {
            root.plan = []
            root.okCount = 0
            return
        }
        // re-scan first: the preview (and the plan Apply executes) must
        // reflect the current scan paths and the Recursive tick
        scanInfo.text = qsTr("%1 file(s) scanned").arg(root.controller.batchScan(
            scanPaths, recursive.checked))
        root.plan = root.controller.batchPreview(patternBar.pattern,
                                                 patternBar.include)
        var ok = 0
        for (var i = 0; i < root.plan.length; ++i)
            if (root.plan[i].ok)
                ++ok
        root.okCount = ok
    }

    property var scanPaths: []

    FolderDialog {
        id: scanDialog
        title: qsTr("Folder to scan")
        onAccepted: {
            var paths = root.scanPaths
            paths.push(selectedFolder)
            root.scanPaths = paths
            root.controller.batchScan(root.scanPaths, recursive.checked)
            root.refresh()
        }
    }

    FileDialog {
        id: addDialog
        title: qsTr("Add game files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Game files (*.pkg *.iso *.exfat *.ffpkg *.ffpfsc)")]
        onAccepted: {
            var paths = root.scanPaths
            for (var i = 0; i < selectedFiles.length; ++i)
                paths.push(selectedFiles[i])
            root.scanPaths = paths
            root.controller.batchScan(root.scanPaths, recursive.checked)
            root.refresh()
        }
    }

    FileDialog {
        id: revertDialog
        title: qsTr("Revert log file")
        nameFilters: [qsTr("Revert logs (batch_rename_*.log)"), qsTr("All files (*)")]
        onAccepted: {
            root.statusMessage(root.controller.batchRevert(selectedFile, true))
            root.refresh()
        }
    }
}
