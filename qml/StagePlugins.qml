import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Plugins"
    accent: "#4dd0e1"
    enableParam: "pluginsOn"
    effectName: "plugins"
    visualWidth: 0
    summary: "Run your own VST3 and LV2 effects inside MicForge, such as LSP, Calf, x42 or any plugin installed on your computer. They run in the order shown, between Tone and Warmth."
    help: "MicForge finds plugins in ~/.vst3, /usr/lib/vst3 and the standard LV2 folders. Each plugin's settings appear as sliders here; plugin windows (their own graphics) aren't supported yet.\n\nPlugins are processed in mono. Stereo plugins get your voice in both channels and their output is folded back to mono. If a plugin crashes MicForge while loading, it's switched off automatically next time."

    readonly property var pm: controller.plugins
    property var expanded: ({})

    PluginBrowser { id: browser }
    Component.onCompleted: if (initialExpandPlugin) expanded = ({ 0: true }) // development aid for screenshots

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: root.columns
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            PillButton {
                text: "Add plugin"
                icon: "+"
                primary: true
                accent: "#4dd0e1"
                enabled: root.pm.chain.length < root.pm.maxPlugins
                onClicked: browser.open()
            }
            Text { text: root.pm.scanStatus; color: Theme.textFaint; font.pixelSize: 12 }
            Item { Layout.fillWidth: true }
            Text { text: root.pm.chain.length + " / " + root.pm.maxPlugins; color: Theme.textFaint; font.pixelSize: 12 }
        }

        Rectangle {
            visible: root.pm.chain.length === 0
            Layout.fillWidth: true
            implicitHeight: 90
            radius: 12
            color: "#0e1015"
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: 13
                text: "No plugins yet. Click “Add plugin” to load a VST3 or LV2 effect installed on your computer."
            }
        }

        Repeater {
            model: root.pm.chain
            Rectangle {
                id: card
                readonly property int slot: index
                readonly property bool open: root.expanded[index] === true
                Layout.fillWidth: true
                implicitHeight: cardCol.implicitHeight + 24
                radius: 12
                color: Theme.cardHi
                border.color: modelData.error ? Theme.danger : (modelData.bypass ? Theme.border : Qt.rgba(0.30, 0.82, 0.88, 0.45))

                ColumnLayout {
                    id: cardCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            width: 24; height: 24; radius: 12
                            color: Qt.rgba(0.30, 0.82, 0.88, 0.18)
                            Text { anchors.centerIn: parent; text: index + 1; color: "#4dd0e1"; font.bold: true; font.pixelSize: 11 }
                        }
                        Rectangle {
                            width: 44; height: 20; radius: 6
                            color: modelData.format === "VST3" ? Qt.rgba(1, 0.62, 0.26, 0.2) : Qt.rgba(0.36, 0.66, 1, 0.2)
                            Text { anchors.centerIn: parent; text: modelData.format; font.pixelSize: 10; font.bold: true
                                   color: modelData.format === "VST3" ? "#ff9f43" : Theme.input }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text { text: modelData.name; color: modelData.bypass ? Theme.textFaint : Theme.text; font.pixelSize: 14; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text {
                                text: (modelData.vendor || "") + (modelData.latency > 0 ? "  ·  adds " + (modelData.latency / 48).toFixed(1) + " ms delay" : "")
                                color: Theme.textFaint; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }
                        PillButton { text: "▲"; implicitHeight: 26; enabled: index > 0; onClicked: root.pm.move(index, -1) }
                        PillButton { text: "▼"; implicitHeight: 26; enabled: index < root.pm.chain.length - 1; onClicked: root.pm.move(index, 1) }
                        PillButton {
                            visible: modelData.loaded
                            text: card.open ? "Hide settings" : "Settings (" + modelData.paramCount + ")"
                            implicitHeight: 26
                            onClicked: { var e = Object.assign({}, root.expanded); e[index] = !card.open; root.expanded = e }
                        }
                        ToggleSwitch {
                            visible: modelData.loaded
                            checked: !modelData.bypass
                            accent: "#4dd0e1"
                            onToggled: (on) => root.pm.setBypass(index, !on)
                        }
                        PillButton { text: "Remove"; implicitHeight: 26; onClicked: { root.expanded = ({}); root.pm.remove(index) } }
                    }
                    RowLayout {
                        visible: modelData.error !== ""
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: modelData.error; color: Theme.danger; font.pixelSize: 12; wrapMode: Text.WordWrap }
                        PillButton { visible: !modelData.loaded; text: "Retry"; implicitHeight: 26; onClicked: root.pm.retry(index) }
                    }

                    // Parameters (generated): only built when expanded.
                    Loader {
                        Layout.fillWidth: true
                        active: card.open && modelData.loaded
                        visible: active
                        sourceComponent: ColumnLayout {
                            spacing: 10
                            readonly property var all: root.pm.params(card.slot)
                            TextField {
                                id: filter
                                visible: parent.all.length > 12
                                Layout.fillWidth: true
                                placeholderText: "Filter " + parent.all.length + " settings…"
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: width > 700 ? 3 : 2
                                columnSpacing: 24
                                rowSpacing: 12
                                Repeater {
                                    model: parent.parent.all.filter(function(p) {
                                        return filter.text === "" || p.name.toLowerCase().indexOf(filter.text.toLowerCase()) >= 0
                                    })
                                    PluginParamControl {
                                        Layout.preferredWidth: 1
                                        slot: card.slot
                                        p: modelData
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
