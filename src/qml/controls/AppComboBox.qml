pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls
import PSGameViewer

// Rounded combo box with a floating rounded dropdown. Mirrors the Basic
// style's structure (ListModel and array-of-maps models with a textRole)
// but takes every color from the Theme singleton.
Controls.ComboBox {
    id: root

    font.family: Theme.fontFamily

    leftPadding: 12
    rightPadding: 26

    indicator: Text {
        x: root.mirrored ? 12 : root.width - width - 10
        y: (root.height - height) / 2
        text: "▾"
        color: Theme.muted
        font.pixelSize: 12
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: root.displayText
        font: root.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent
                      : root.hovered ? Qt.lighter(Theme.border, 1.2)
                      : Theme.border
    }

    delegate: Controls.ItemDelegate {
        id: item
        required property var model
        required property int index

        width: ListView.view.width
        // "model" resolves to the element for both array-of-maps and
        // ListModel models, so no unqualified "modelData" access is needed
        text: root.textRole ? item.model[root.textRole] : ""
        font.weight: root.currentIndex === item.index ? Font.DemiBold
                                                     : Font.Normal
        highlighted: root.highlightedIndex === item.index
        hoverEnabled: root.hoverEnabled

        contentItem: Text {
            leftPadding: 6
            rightPadding: 6
            text: item.text
            font: item.font
            color: item.highlighted ? Theme.accentText : Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: item.highlighted ? Theme.accent : "transparent"
        }
    }

    popup: Controls.Popup {
        y: root.height + 4
        width: root.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         root.Window.height - root.y)
        padding: 6

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            spacing: 2

            Controls.ScrollBar.vertical: Controls.ScrollBar {
                implicitWidth: 8
                contentItem: Rectangle {
                    radius: 3
                    color: Theme.muted
                }
                background: null
            }
        }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.card
            border.color: Theme.border
        }
    }
}
