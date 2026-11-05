import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

ColumnLayout {
    id: root
    spacing: 12

    Item { Layout.fillHeight: true }
    Rectangle {
        width: 84; height: 84; radius: 18
        color: Theme.card2
        Layout.alignment: Qt.AlignHCenter
        Text {
            anchors.centerIn: parent
            text: "PS"
            color: Theme.accent
            font.bold: true
            font.pixelSize: 34
        }
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Drop a game file here")
        color: Theme.text
        font.pixelSize: 18
        font.bold: true
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("PKG · ISO · exFAT images · game folders")
        color: Theme.muted
        font.pixelSize: 13
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("or use Open in the header")
        color: Theme.muted
        font.pixelSize: 12
    }
    Item { Layout.fillHeight: true }
}
