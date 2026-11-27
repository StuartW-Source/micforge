import QtQuick
import QtQuick.Layouts

// A row (wrapping) of one-click preset buttons. The preset matching the current settings is highlighted.
Flow {
    id: root
    property var presets: []
    property color accent: Theme.input
    property bool big: false       // large tiles with an icon (voice changer)
    property string group: ""      // only presets with this `group` ("" = all)
    property var keys: ({})        // preset id -> shortcut key description, shown on the tile
    spacing: 8

    Repeater {
        model: root.group === "" ? root.presets : root.presets.filter((p) => p.group === root.group)
        Rectangle {
            id: chip
            readonly property bool current: Theme.matches(controller.params, modelData.values)
            width: root.big ? 132 : label.implicitWidth + 28
            height: root.big ? 74 : 30
            radius: root.big ? 12 : 15
            color: current ? Theme.alpha(root.accent, 0.22) : (m.containsMouse ? Theme.cardHi : Theme.card)
            border.color: current ? root.accent : Theme.border
            border.width: current ? 2 : 1
            Column {
                anchors.centerIn: parent
                spacing: 2
                Text {
                    visible: root.big && modelData.icon !== undefined
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.icon || ""
                    font.pixelSize: 24
                    height: 30
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    id: label
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.name
                    color: chip.current ? root.accent : Theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
            Rectangle {
                readonly property string key: (modelData.id !== undefined && root.keys[modelData.id]) || ""
                visible: key !== ""
                anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 4
                width: keyText.implicitWidth + 8; height: keyText.implicitHeight + 2
                radius: Theme.radiusSm - 2
                color: Theme.well
                border.color: Theme.border
                Text {
                    id: keyText
                    anchors.centerIn: parent
                    text: parent.key
                    color: Theme.textDim
                    font.pixelSize: Theme.fsCaption
                }
            }
            MouseArea {
                id: m
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Theme.applyPreset(controller, modelData.values)
            }
            HoverHandler { id: hh }
            ToolTipLite {
                visible: hh.hovered && (modelData.tip || "") !== ""
                text: (modelData.tip || "") + ((modelData.id !== undefined && root.keys[modelData.id]) ? "\nShortcut: " + root.keys[modelData.id] : "")
            }
        }
    }
}
