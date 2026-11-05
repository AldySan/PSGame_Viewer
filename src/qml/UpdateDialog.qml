import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Update check result dialog: fed from Main.qml with the map emitted by
// Updater.checkFinished(). Opened manually from the About dialog and
// automatically when the startup check finds a newer release.
Popup {
    id: root
    anchors.centerIn: parent
    width: 480
    height: 420
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 22

    property var result: null
    property bool busy: false

    function showResult(map) {
        root.result = map
        root.busy = false
        open()
    }

    function startCheck() {
        root.result = null
        root.busy = true
        open()
        Updater.checkNow()
    }

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius + 4
        border.color: Theme.border
    }

    onOpened: {
        if (!root.result && !root.busy)
            startCheck()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: {
                if (root.busy) return qsTr("Checking for updates…")
                if (!root.result) return qsTr("Update checker")
                if (root.result.state === "update-available")
                    return qsTr("Update available")
                if (root.result.state === "up-to-date")
                    return qsTr("Up to date")
                if (root.result.state === "no-release")
                    return qsTr("No release published yet")
                return qsTr("Update check failed")
            }
            color: {
                if (!root.busy && root.result
                        && root.result.state === "update-available")
                    return Theme.good
                if (!root.busy && root.result
                        && (root.result.state === "error"))
                    return Theme.danger
                return Theme.text
            }
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            visible: root.busy
            text: qsTr("Contacting the release server…")
            color: Theme.muted
            font.pixelSize: 12
        }

        Label {
            visible: !root.busy && root.result
                     && root.result.state === "update-available"
            text: qsTr("PSGame-Viewer %1 is available (you are running %2).")
                  .arg(root.result ? root.result.latest : "")
                  .arg(root.result ? root.result.current : "")
            color: Theme.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: !root.busy && root.result
                     && root.result.state === "up-to-date"
            text: qsTr("You are running the latest version (%1).")
                  .arg(root.result ? root.result.current : "")
            color: Theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: !root.busy && root.result
                     && root.result.state === "error"
            text: qsTr("Could not reach the release server: %1")
                  .arg(root.result ? root.result.error : "")
            color: Theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: !root.busy && root.result
                     && root.result.state === "no-release"
            text: qsTr("Nothing to check against yet. Publish a GitHub release to enable update checks.")
            color: Theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Flickable {
            visible: !root.busy && root.result
                     && root.result.state === "update-available"
                     && (root.result.notes || "").length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: notesText.implicitHeight
            ScrollBar.vertical: ScrollBar {}

            Text {
                id: notesText
                width: parent.width
                text: root.result ? (root.result.notes || "") : ""
                color: Theme.muted
                font.pixelSize: 12
                wrapMode: Text.Wrap
                linkColor: Theme.accent
            }
        }

        Item { Layout.fillHeight: !root.busy }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                visible: !root.busy && root.result
                         && root.result.state === "update-available"
                         && (root.result.pageUrl || "").length > 0
                text: qsTr("Open release page")
                highlighted: true
                onClicked: Qt.openUrlExternally(root.result.pageUrl)
            }
            Button {
                visible: root.busy
                text: qsTr("Cancel")
                flat: true
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: !root.busy
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
