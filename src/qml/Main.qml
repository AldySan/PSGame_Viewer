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
        background: Item {
            Rectangle { anchors.fill: parent; color: Theme.card }
            // hairline separator under the header
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Rectangle {
                width: 30; height: 30; radius: Theme.radiusSm + 2
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

            AppButton {
                text: qsTr("Open")
                highlighted: true
                onClicked: openMenu.popup()
            }
            AppButton {
                text: stack.currentIndex === 0 ? qsTr("Library") : qsTr("Viewer")
                onClicked: stack.currentIndex = stack.currentIndex === 0 ? 1 : 0
            }
            AppButton {
                text: qsTr("Rename")
                enabled: Viewer.hasFile
                onClicked: renameDialog.open()
            }
            AppButton {
                text: qsTr("Batch")
                onClicked: batchDialog.open()
            }
            AppButton {
                text: qsTr("Settings")
                flat: true
                onClicked: settingsDialog.open()
            }
            AppButton {
                text: qsTr("About")
                flat: true
                onClicked: aboutDialog.open()
            }
        }
    }

    StackLayout {
        id: stack
        objectName: "pages"
        anchors.fill: parent
        currentIndex: 0
        // re-sync from the database every time the page is shown: the
        // rows may have changed while the page was hidden
        onCurrentIndexChanged: {
            if (currentIndex === 1)
                Library.refresh()
        }

        ViewerPage {
            controller: Viewer
            onStatusMessage: function(msg) { root.statusText = msg }
            onOpenRecentRequested: function(path) {
                Viewer.openPath(path)
            }
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
        background: Item {
            Rectangle { anchors.fill: parent; color: Theme.card }
            // hairline separator above the footer
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.border
            }
        }
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

    // PS3 game folders and PS5 app folders are supported formats but a
    // file dialog cannot select them — offer both entry points
    Menu {
        id: openMenu
        objectName: "openMenu"
        padding: 6
        // a Menu never derives its width from its items, and the custom
        // background carries no implicit size of its own — without an
        // explicit content width the popup collapses to the padding and
        // shows up as a small empty rectangle
        contentWidth: Math.max(openFileItem.implicitWidth,
                               openFolderItem.implicitWidth)
        background: Rectangle {
            radius: Theme.radius
            color: Theme.card
            border.color: Theme.border
        }
        MenuItem {
            id: openFileItem
            text: qsTr("Open file…")
            padding: 6
            leftPadding: 10
            rightPadding: 10
            contentItem: Text {
                text: openFileItem.text
                color: Theme.text
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSm
                color: openFileItem.hovered || openFileItem.highlighted
                       ? Theme.accentSoft : "transparent"
            }
            onTriggered: fileDialog.open()
        }
        MenuItem {
            id: openFolderItem
            text: qsTr("Open game folder…")
            padding: 6
            leftPadding: 10
            rightPadding: 10
            contentItem: Text {
                text: openFolderItem.text
                color: Theme.text
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSm
                color: openFolderItem.hovered || openFolderItem.highlighted
                       ? Theme.accentSoft : "transparent"
            }
            onTriggered: folderDialog.open()
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Open a game folder (PS3 game or PS5 app folder)")
        onAccepted: Viewer.openPath(selectedFolder)
    }

    // transient feedback pill for library actions (scan/clean results);
    // the footer stays the home of the longer-lived viewer status
    AppToast {
        id: toast
        objectName: "toast"
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
