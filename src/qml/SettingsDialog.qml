import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PSGameViewer

// Settings dialog: theme, interface language and online lookups.
Popup {
    id: root
    anchors.centerIn: parent
    width: 420
    height: 400
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 20

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius + 4
        border.color: Theme.border
    }

    onAboutToShow: {
        var lang = AppSettings.language
        var idx = langCombo.find(lang)
        langCombo.currentIndex = idx >= 0 ? idx : 0
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
        ComboBox {
            id: themeCombo
            Layout.fillWidth: true
            font.pixelSize: 13
            model: ListModel {
                ListElement { text: qsTr("Dark"); value: "dark" }
                ListElement { text: qsTr("Light"); value: "light" }
            }
            textRole: "text"
            valueRole: "value"
        }

        Label {
            text: qsTr("Language")
            color: Theme.muted
            font.pixelSize: 12
        }
        ComboBox {
            id: langCombo
            Layout.fillWidth: true
            font.pixelSize: 13
            // value codes match the translator file names
            model: ListModel {
                ListElement { text: qsTr("System default"); value: "system" }
                ListElement { text: "English"; value: "en" }
                ListElement { text: "中文 (简体)"; value: "zh_CN" }
                ListElement { text: "Bahasa Indonesia"; value: "id" }
            }
            textRole: "text"
            valueRole: "value"
        }
        Label {
            text: qsTr("Restart the app to apply a language change.")
            color: Theme.muted
            font.pixelSize: 11
            visible: langCombo.currentValue !== AppSettings.language
        }

        CheckBox {
            id: onlineBox
            text: qsTr("Online lookups (covers, latest patch)")
            font.pixelSize: 13
        }

        CheckBox {
            id: updatesBox
            text: qsTr("Check for updates on startup")
            font.pixelSize: 13
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: root.close()
            }
            Button {
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
