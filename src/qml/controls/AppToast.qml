import QtQuick
import PSGameViewer

// Transient status pill for one-shot action feedback (library scans,
// clean-missing results). Main.qml shows it above the footer; it fades
// out by itself, so no dismissal is needed.
Rectangle {
    id: root

    property string message: ""

    function show(msg) {
        root.message = msg
        root.opacity = 1
        hideTimer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 14
    width: Math.min(label.implicitWidth + 30, parent.width - 32)
    height: label.implicitHeight + 18
    radius: Theme.radiusLg
    color: Theme.card
    border.color: Theme.border

    opacity: 0
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: 180 } }

    Text {
        id: label
        anchors.centerIn: parent
        width: root.width - 30
        text: root.message
        color: Theme.text
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideMiddle
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: root.opacity = 0
    }
}
