import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Shared preset + token pattern + include ticks bar for the rename
// dialogs. The last choice is remembered in the settings.
ColumnLayout {
    id: root
    required property var controller
    property bool withInclude: true
    readonly property string pattern: {
        var p = presetBox.currentValue
        if (p === "custom")
            return customField.text.trim()
        // preset patterns; the clean preset uses the include ticks
        var presets = controller.renamePresetList()
        for (var i = 0; i < presets.length; ++i) {
            if (presets[i].id === p)
                return presets[i].pattern
        }
        return ""
    }
    readonly property var include: ({
        title: tickTitle.checked,
        tid: tickTid.checked,
        ver: tickVer.checked,
        region: tickRegion.checked
    })

    function currentPresetId() { return presetBox.currentValue }

    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        Label {
            text: qsTr("Name:")
            color: Theme.muted
            font.pixelSize: 12
        }
        ComboBox {
            id: presetBox
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "id"
            font.pixelSize: 12
            model: {
                var list = controller.renamePresetList()
                list.push({id: "custom", label: qsTr("Custom pattern"),
                           pattern: ""})
                return list
            }
        }
        TextField {
            id: customField
            Layout.preferredWidth: 180
            font.pixelSize: 12
            placeholderText: qsTr("custom pattern")
            enabled: presetBox.currentValue === "custom"
            color: Theme.text
            background: Rectangle {
                radius: 4
                color: Theme.card2
                border.color: customField.activeFocus ? Theme.accent : Theme.border
            }
        }
    }

    RowLayout {
        spacing: 4
        Layout.fillWidth: true
        Repeater {
            model: ["title", "tid", "cid", "version", "region", "type", "orig"]
            delegate: Label {
                required property string modelData
                text: "{" + modelData + "}"
                color: Theme.muted
                font.pixelSize: 11
                padding: 3
                background: Rectangle { color: Theme.card2; radius: 3 }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        presetBox.currentIndex = presetBox.count - 1
                        customField.text += (customField.text.length ? " " : "")
                                            + "{" + modelData + "}"
                        customField.cursorPosition = customField.text.length
                        customField.forceActiveFocus()
                    }
                }
            }
        }
    }

    RowLayout {
        visible: root.withInclude
        spacing: 12
        Label {
            text: qsTr("Include:")
            color: Theme.muted
            font.pixelSize: 12
        }
        CheckBox {
            id: tickTitle
            text: qsTr("Title")
            font.pixelSize: 12
            checked: true
        }
        CheckBox {
            id: tickTid
            text: qsTr("Title ID")
            font.pixelSize: 12
            checked: true
        }
        CheckBox {
            id: tickVer
            text: qsTr("Version")
            font.pixelSize: 12
            checked: true
        }
        CheckBox {
            id: tickRegion
            text: qsTr("Region")
            font.pixelSize: 12
            checked: true
        }
    }
}
