import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Pick an installed VST3 / LV2 effect to add to the Plugins stage.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(900, parent ? parent.width - 60 : 900)
    height: Math.min(760, parent ? parent.height - 60 : 760)
    padding: 26
    background: Rectangle { color: Theme.panel; radius: 18; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Theme.scrim }
    onOpened: { search.text = ""; search.forceActiveFocus() }

    readonly property var pm: controller.plugins
    property string format: ""   // "" = all
    readonly property bool full: pm.chain.length >= pm.maxPlugins
    readonly property var filtered: {
        var q = search.text.toLowerCase()
        return pm.available.filter(function(p) {
            if (root.format !== "" && p.format !== root.format) return false
            if (q === "") return true
            return (p.name + " " + p.vendor + " " + p.category).toLowerCase().indexOf(q) >= 0
        })
    }

    contentItem: ColumnLayout {
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Text { text: "Add a plugin"; color: Theme.text; font.pixelSize: 24; font.weight: Font.Bold }
                Text {
                    text: root.pm.scanning ? root.pm.scanStatus : root.pm.scanStatus + "  ·  searched ~/.vst3, /usr/lib/vst3 and the usual LV2 folders"
                    color: Theme.textDim; font.pixelSize: 12
                }
            }
            Item { Layout.fillWidth: true }
            PillButton { visible: root.pm.scanning; text: "Stop"; onClicked: root.pm.cancelScan() }
            PillButton { text: "Rescan"; enabled: !root.pm.scanning; onClicked: root.pm.rescan(true) }
            PillButton { text: "Close"; onClicked: root.close() }
        }
        Rectangle {
            visible: root.pm.scanning && root.pm.scanProgress > 0
            Layout.fillWidth: true
            implicitHeight: 4; radius: 2
            color: Theme.track
            Rectangle { width: parent.width * root.pm.scanProgress; height: parent.height; radius: 2; color: Theme.plugins }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            MfTextField {
                id: search
                Layout.fillWidth: true
                placeholderText: "Search by name, maker or type (e.g. compressor, reverb, LSP)…"
            }
            Repeater {
                model: [["All", ""], ["VST3", "VST3"], ["LV2", "LV2"]]
                Rectangle {
                    width: lbl.implicitWidth + 26; height: 32; radius: 16
                    color: root.format === modelData[1] ? Theme.alpha(Theme.plugins, 0.2) : Theme.card
                    border.color: root.format === modelData[1] ? Theme.plugins : Theme.border
                    Text { id: lbl; anchors.centerIn: parent; text: modelData[0]; color: Theme.text; font.pixelSize: 12; font.bold: true }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.format = modelData[1] }
                }
            }
        }
        Text {
            visible: root.full
            text: "The Plugins stage is full (" + root.pm.maxPlugins + " plugins). Remove one to add another."
            color: Theme.warn; font.pixelSize: 12
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.filtered
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                width: list.width - 12
                height: 58
                radius: 10
                color: m.containsMouse ? Theme.cardHi : Theme.card
                border.color: Theme.border
                RowLayout {
                    anchors { fill: parent; leftMargin: 14; rightMargin: 10 }
                    spacing: 12
                    Rectangle {
                        width: 46; height: 22; radius: 6
                        color: modelData.format === "VST3" ? Theme.alpha(Theme.voice, 0.2) : Theme.alpha(Theme.input, 0.2)
                        Text { anchors.centerIn: parent; text: modelData.format; font.pixelSize: 10; font.bold: true
                               color: modelData.format === "VST3" ? Theme.voice : Theme.input }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { text: modelData.name; color: Theme.text; font.pixelSize: 14; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text {
                            text: (modelData.blocked ? "Blocked · " : "") + [modelData.vendor, modelData.category].filter(function(s) { return s && s.length > 0 }).join(" · ")
                            color: Theme.textFaint; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                    PillButton {
                        text: modelData.blocked ? "Unblock" : "Block"
                        implicitHeight: 30
                        onClicked: root.pm.setBlocked(modelData.format, modelData.id, !modelData.blocked)
                        ToolTipLite { visible: bh.hovered; text: modelData.blocked ? "Allow this plugin again." : "Hide this plugin from MicForge (e.g. if it crashes or misbehaves)." }
                        HoverHandler { id: bh }
                    }
                    PillButton {
                        visible: !modelData.blocked
                        text: "Add"
                        primary: true
                        accent: Theme.plugins
                        implicitHeight: 30
                        enabled: !root.full
                        onClicked: { root.pm.add(modelData.format, modelData.id); root.close() }
                    }
                }
                MouseArea { id: m; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            }
            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                width: list.width * 0.7
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: 13
                text: root.pm.scanning ? "Looking for plugins…"
                    : root.pm.available.length === 0
                      ? "No VST3 or LV2 effects found. Install some, e.g. “lsp-plugins” or “x42-plugins” from your package manager, then click Rescan."
                      : "Nothing matches your search."
            }
        }
    }
}
