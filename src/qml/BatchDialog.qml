import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

// Batch rename: scan folders, preview every rename, apply with a revert
// log, and revert from an earlier log.
Popup {
    id: root
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
        radius: Theme.radius + 4
        border.color: Theme.border
    }

    property var plan: []
    property int okCount: 0

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
            onPatternChanged: refresh()
            onIncludeChanged: refresh()
        }

        RowLayout {
            spacing: 8
            Button {
                text: qsTr("Add folder…")
                onClicked: scanDialog.open()
            }
            Button {
                text: qsTr("Add files…")
                onClicked: addDialog.open()
            }
            CheckBox {
                id: recursive
                text: qsTr("Recursive")
                font.pixelSize: 12
                onCheckedChanged: refresh()
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
                radius: 4
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
                ScrollBar.vertical: ScrollBar {}

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
                        color: modelData.ok ? Theme.accent : Theme.danger
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
            Button {
                text: qsTr("Revert from log…")
                flat: true
                onClicked: revertDialog.open()
            }
            Button {
                text: qsTr("Close")
                flat: true
                onClicked: root.close()
            }
            Button {
                text: qsTr("Apply")
                highlighted: true
                enabled: root.okCount > 0
                onClicked: {
                    patternBar.savePrefs()
                    var res = root.controller.batchApply()
                    root.statusMessage(qsTr(
                        "Renamed %1, skipped %2").arg(res.renamed)
                        .arg(res.skipped))
                    if (res.logPath)
                        root.statusMessage(qsTr(
                            "Renamed %1, skipped %2. Log: %3")
                            .arg(res.renamed).arg(res.skipped).arg(res.logPath))
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
        root.plan = root.controller.batchPreview(patternBar.pattern,
                                                 patternBar.include)
        var ok = 0
        for (var i = 0; i < root.plan.length; ++i)
            if (root.plan[i].ok)
                ++ok
        root.okCount = ok
        scanInfo.text = qsTr("%1 file(s) scanned").arg(root.controller.batchScan(
            scanPaths, recursive.checked))
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
            refresh()
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
            refresh()
        }
    }

    FileDialog {
        id: revertDialog
        title: qsTr("Revert log file")
        nameFilters: [qsTr("Revert logs (batch_rename_*.log)"), qsTr("All files (*)")]
        onAccepted: {
            root.statusMessage(root.controller.batchRevert(selectedFile, true))
            refresh()
        }
    }
}
