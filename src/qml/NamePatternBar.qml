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

    // Restore the last choice (preset, custom pattern and the include
    // ticks) from the settings. Shared by both rename dialogs so the
    // "remembered" promise holds everywhere.
    function loadPrefs() {
        var inc = AppSettings.renameInclude || {}
        tickTitle.checked = inc.title !== false
        tickTid.checked = inc.tid !== false
        tickVer.checked = inc.ver !== false
        tickRegion.checked = inc.region !== false
        var pid = AppSettings.renamePreset
        var idx = 0
        for (var i = 0; i < presetBox.count; ++i) {
            if (presetBox.valueAt(i) === pid) { idx = i; break }
        }
        if (pid === "custom")
            idx = presetBox.count - 1
        presetBox.currentIndex = idx
        if (AppSettings.renamePattern)
            customField.text = AppSettings.renamePattern
    }

    function savePrefs() {
        AppSettings.saveRenamePrefs(include, currentPresetId(), pattern)
    }

    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        Label {
            text: qsTr("Name:")
            color: Theme.muted
            font.pixelSize: 12
        }
        AppComboBox {
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
        AppTextField {
            id: customField
            Layout.preferredWidth: 180
            font.pixelSize: 12
            placeholderText: qsTr("custom pattern")
            enabled: presetBox.currentValue === "custom"
        }
    }

    RowLayout {
        spacing: 4
        Layout.fillWidth: true
        Repeater {
            model: ["title", "tid", "cid", "version", "region", "type", "orig"]
            delegate: AppChip {
                id: chip
                required property string modelData
                text: "{" + modelData + "}"
                topPadding: 3
                bottomPadding: 3
                // "{title}" reads badly; spell the action out instead
                Accessible.name: qsTr("Insert the %1 token").arg(modelData)
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

    RowLayout {
        visible: root.withInclude
        spacing: 12
        Label {
            text: qsTr("Include:")
            color: Theme.muted
            font.pixelSize: 12
        }
        AppCheckBox {
            id: tickTitle
            text: qsTr("Title")
            font.pixelSize: 12
            checked: true
        }
        AppCheckBox {
            id: tickTid
            text: qsTr("Title ID")
            font.pixelSize: 12
            checked: true
        }
        AppCheckBox {
            id: tickVer
            text: qsTr("Version")
            font.pixelSize: 12
            checked: true
        }
        AppCheckBox {
            id: tickRegion
            text: qsTr("Region")
            font.pixelSize: 12
            checked: true
        }
    }
}
