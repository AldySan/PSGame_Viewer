import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 940
    minimumHeight: 600
    visible: true
    title: qsTr("PSGame-Viewer")
    color: Theme.bg

    property string statusText: ""
    property bool startupCheckDone: false

    // startup update check: silent, but a found update opens the dialog
    function onUpdaterFinished(map) {
        // a check initiated from the Update dialog owns the result
        if (updateDialog.busy) {
            updateDialog.showResult(map)
            return
        }
        if (!startupCheckDone)
            return
        startupCheckDone = false
        if (map && map.state === "update-available")
            updateDialog.showResult(map)
    }

    Component.onCompleted: {
        // background release check
        if (AppSettings.checkUpdatesOnStartup) {
            startupCheckDone = true
            Updater.checkNow()
        }
    }

    Connections {
        target: Updater
        function onCheckFinished(map) { root.onUpdaterFinished(map) }
    }
    header: ToolBar {
        height: 52
        background: Rectangle { color: Theme.card }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Rectangle {
                width: 30; height: 30; radius: 6
                color: Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: "PS"
                    color: "#171717"
                    font.bold: true
                    font.pixelSize: 13
                }
            }
            Label {
                text: qsTr("PSGame-Viewer")
                color: Theme.text
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Open")
                highlighted: true
                onClicked: fileDialog.open()
            }
            Button {
                text: stack.currentIndex === 0 ? qsTr("Library") : qsTr("Viewer")
                onClicked: {
                    if (stack.currentIndex === 0) {
                        Library.refresh()
                        stack.currentIndex = 1
                    } else {
                        stack.currentIndex = 0
                    }
                }
            }
            Button {
                text: qsTr("Rename")
                enabled: Viewer.hasFile
                onClicked: renameDialog.open()
            }
            Button {
                text: qsTr("Batch")
                onClicked: batchDialog.open()
            }
            Button {
                text: qsTr("Settings")
                flat: true
                onClicked: settingsDialog.open()
            }
            Button {
                text: qsTr("About")
                flat: true
                onClicked: aboutDialog.open()
            }
        }
    }

    StackLayout {
        id: stack
        anchors.fill: parent
        currentIndex: 0

        ViewerPage {
            controller: Viewer
            onStatusMessage: function(msg) { root.statusText = msg }
        }

        LibraryPage {
            onOpenRequested: function(path) {
                Viewer.openPath(path)
                stack.currentIndex = 0
            }
            onStatusMessage: function(msg) { root.statusText = msg }
        }
    }

    footer: ToolBar {
        height: 34
        background: Rectangle { color: Theme.card }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                text: root.statusText
                color: Theme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: Viewer.hasFile
                text: Viewer.statusSummary
                color: Theme.text
                font.pixelSize: 12
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open a game file")
        nameFilters: [
            qsTr("Game files (*.pkg *.iso *.exfat *.ffpkg *.ffpfsc)"),
            qsTr("All files (*)")
        ]
        onAccepted: Viewer.openPath(selectedFile)
    }

    FolderDialog {
        id: extractFolderDialog
        title: qsTr("Extract to folder")
        property int pendingEntry: -1
        onAccepted: {
            if (pendingEntry >= 0)
                Viewer.extractEntry(pendingEntry, selectedFolder)
            else
                Viewer.extractAll(selectedFolder)
            pendingEntry = -1
        }
    }

    RenameDialog {
        id: renameDialog
        controller: Viewer
        onStatusMessage: function(msg) { root.statusText = msg }
    }

    BatchDialog {
        id: batchDialog
        controller: Viewer
        onStatusMessage: function(msg) { root.statusText = msg }
    }

    AboutDialog {
        id: aboutDialog
        onUpdateCheckRequested: updateDialog.startCheck()
    }

    SettingsDialog {
        id: settingsDialog
    }

    UpdateDialog {
        id: updateDialog
    }

    // drag & drop: accept files and folders onto the window
    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.urls && drop.urls.length > 0) {
                Viewer.openPath(drop.urls[0])
                root.statusText = ""
            }
        }
    }

    Connections {
        target: Viewer
        function onStatusMessage(message) { root.statusText = message }
    }
}
