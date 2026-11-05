import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

Popup {
    id: root
    anchors.centerIn: parent
    width: 440
    height: 400
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 22

    // fired when the user asks for an update check; Main.qml routes it
    // to the shared UpdateDialog.
    signal updateCheckRequested()

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius + 4
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            spacing: 12
            Rectangle {
                width: 46; height: 46; radius: 10
                color: Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: "PS"
                    color: "#171717"
                    font.bold: true
                    font.pixelSize: 19
                }
            }
            ColumnLayout {
                spacing: 2
                Label {
                    text: qsTr("PSGame-Viewer")
                    color: Theme.text
                    font.pixelSize: 18
                    font.bold: true
                }
                Label {
                    text: qsTr("Version %1").arg(AppVersion)
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: 12
            text: qsTr("See what is inside PlayStation game files without extracting them: PS2 and PS3 disc images, PS3 packages and folders, PS4 and PS5 packages (including split parts), PS5 exFAT images and app folders. Cover art, spec cards, file listings, trophies, extraction and uniform renaming.")
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            font.pixelSize: 11
            text: qsTr("Built with Qt 6, QML and SQLite. Licensed under the GNU General Public License v3.")
        }

        Item { Layout.fillHeight: true }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Check for updates")
            onClicked: updateCheckRequested()
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Close")
            onClicked: root.close()
        }
    }
}
