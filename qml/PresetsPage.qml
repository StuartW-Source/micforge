import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Presets: switch between saved sound profiles, save the current sound, delete your own presets.
Flickable {
    id: root
    property var app
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: body
        width: Math.min(root.width, 900)
        spacing: Theme.sp4

        Card {
            Layout.fillWidth: true
            title: "Save the current sound"
            subtitle: "Saves every effect setting, including which effects are in the chain. Your microphone choice and Setup results are kept separately and aren't changed by loading a preset."
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.sp2
                MfTextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: "Preset name, e.g. Late night gaming"
                    enabled: !controller.safeMode
                    onAccepted: if (saveBtn.enabled) saveBtn.clicked()
                    Accessible.name: "New preset name"
                }
                PillButton {
                    id: saveBtn
                    readonly property string name: nameField.text.trim()
                    text: controller.presetNames.indexOf(name) >= 0 && !controller.isBuiltinPreset(name) ? "Replace" : "Save"
                    primary: true
                    enabled: name !== "" && !controller.isBuiltinPreset(name) && !controller.safeMode
                    onClicked: { controller.savePreset(name); nameField.text = "" }
                }
            }
            Text {
                visible: nameField.text.trim() !== "" && controller.isBuiltinPreset(nameField.text.trim())
                text: "That name belongs to a built-in preset. Choose another name."
                color: Theme.warn; font.pixelSize: Theme.fsSmall
            }
            Text {
                visible: controller.safeMode
                text: "Presets can't be saved in safe mode."
                color: Theme.warn; font.pixelSize: Theme.fsSmall
            }
        }

        SectionLabel { text: "Presets"; Layout.topMargin: Theme.sp1 }
        Text {
            visible: controller.currentPreset === ""
            text: "You've changed settings since loading a preset (“Custom”). Save them above to keep them."
            color: Theme.textDim; font.pixelSize: Theme.fsBody
            Layout.fillWidth: true; wrapMode: Text.WordWrap
        }

        Repeater {
            model: controller.presetNames
            Rectangle {
                id: row
                readonly property bool active: modelData === controller.currentPreset
                readonly property bool builtin: controller.isBuiltinPreset(modelData)
                Layout.fillWidth: true
                implicitHeight: 52
                radius: Theme.radius
                color: active ? Theme.tint(Theme.focus) : Theme.card
                border.color: active ? Theme.tintBorder(Theme.focus) : Theme.border
                RowLayout {
                    anchors { fill: parent; leftMargin: Theme.sp4; rightMargin: Theme.sp3 }
                    spacing: Theme.sp3
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.text
                        font.pixelSize: Theme.fsBodyLg
                        font.weight: row.active ? Font.Bold : Font.Normal
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: row.builtin
                        implicitWidth: bl.implicitWidth + 14; implicitHeight: 20; radius: 10
                        color: Theme.cardHi
                        Text { id: bl; anchors.centerIn: parent; text: "built-in"; color: Theme.textFaint; font.pixelSize: Theme.fsCaption }
                    }
                    Text { visible: row.active; text: "✓ In use"; color: Theme.focus; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                    PillButton {
                        visible: !row.active
                        text: "Load"
                        implicitHeight: Theme.controlHeightSm
                        onClicked: controller.loadPreset(modelData)
                        Accessible.name: "Load " + modelData
                    }
                    PillButton {
                        visible: !row.builtin
                        text: "Delete"
                        implicitHeight: Theme.controlHeightSm
                        enabled: !controller.safeMode
                        onClicked: { confirmDelete.name = modelData; confirmDelete.open() }
                        Accessible.name: "Delete " + modelData
                    }
                }
            }
        }
        Item { implicitHeight: Theme.sp2 }
    }

    Popup {
        id: confirmDelete
        property string name: ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 400
        padding: Theme.sp5
        background: Rectangle { color: Theme.panel; radius: Theme.radiusLg; border.color: Theme.border }
        Overlay.modal: Rectangle { color: Theme.scrim }
        contentItem: ColumnLayout {
            spacing: Theme.sp3
            Text { text: "Delete “" + confirmDelete.name + "”?"; color: Theme.text; font.pixelSize: Theme.fsHeading; font.weight: Font.Bold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { text: "This preset will be removed. It can't be undone."; color: Theme.textDim; font.pixelSize: Theme.fsBody; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                PillButton { text: "Cancel"; onClicked: confirmDelete.close() }
                PillButton { text: "Delete"; primary: true; accent: Theme.danger; onClicked: { controller.deletePreset(confirmDelete.name); confirmDelete.close() } }
            }
        }
    }
}
