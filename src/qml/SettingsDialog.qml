import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Settings dialog: theme, interface language and online lookups.
Popup {
    id: root
    objectName: "settingsDialog"
    anchors.centerIn: parent
    width: 420
    height: 400
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 20

    background: Rectangle {
        color: Theme.card
        radius: Theme.radiusLg
        border.color: Theme.border
    }

    onAboutToShow: {
        // resolve the saved language by VALUE: find() would match the
        // translated display text ("English", "中文 (简体)", ...) and never
        // the saved code ("en", "zh_CN", "id", "system")
        var lang = AppSettings.language
        var idx = 0
        for (var i = 0; i < langCombo.count; ++i) {
            if (langCombo.valueAt(i) === lang) { idx = i; break }
        }
        langCombo.currentIndex = idx
        onlineBox.checked = AppSettings.onlineEnabled
        themeCombo.currentIndex = AppSettings.theme === "light" ? 1 : 0
        updatesBox.checked = AppSettings.checkUpdatesOnStartup
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: qsTr("Settings")
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            text: qsTr("Theme")
            color: Theme.muted
            font.pixelSize: 12
        }
        AppComboBox {
            id: themeCombo
            Layout.fillWidth: true
            font.pixelSize: 13
            // a JS array (not ListModel): qsTr() in a plain binding
            // re-evaluates when the language changes, ListElement
            // values are captured once and would stay untranslated
            model: [
                { text: qsTr("Dark"), value: "dark" },
                { text: qsTr("Light"), value: "light" }
            ]
            textRole: "text"
            valueRole: "value"
        }

        Label {
            text: qsTr("Language")
            color: Theme.muted
            font.pixelSize: 12
        }
        AppComboBox {
            id: langCombo
            Layout.fillWidth: true
            font.pixelSize: 13
            // value codes match the translator file names
            model: [
                { text: qsTr("System default"), value: "system" },
                { text: "English", value: "en" },
                { text: "中文 (简体)", value: "zh_CN" },
                { text: "Bahasa Indonesia", value: "id" }
            ]
            textRole: "text"
            valueRole: "value"
        }
        AppCheckBox {
            id: onlineBox
            text: qsTr("Online lookups (covers, latest patch)")
            font.pixelSize: 13
        }

        AppCheckBox {
            id: updatesBox
            text: qsTr("Check for updates on startup")
            font.pixelSize: 13
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Cancel")
                flat: true
                onClicked: root.close()
            }
            AppButton {
                text: qsTr("Save")
                highlighted: true
                onClicked: {
                    AppSettings.language = langCombo.currentValue
                    AppSettings.onlineEnabled = onlineBox.checked
                    AppSettings.theme = themeCombo.currentValue
                    AppSettings.checkUpdatesOnStartup = updatesBox.checked
                    root.close()
                }
            }
        }
    }
}
