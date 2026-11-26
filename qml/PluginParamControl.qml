import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One parameter of a third-party plugin: switch, dropdown or slider depending on its type.
ColumnLayout {
    id: root
    property int slot: 0
    property var p: ({})               // parameter description from PluginManager.params()
    property color accent: Theme.plugins
    readonly property var pm: controller.plugins
    readonly property real value: (pm.revision, pm.value(slot, p.index))

    spacing: 4
    Layout.fillWidth: true

    function toPos(v) {
        if (p.log && p.min > 0) return Math.log(v / p.min) / Math.log(p.max / p.min)
        return (v - p.min) / (p.max - p.min)
    }
    function fromPos(t) {
        var v = (p.log && p.min > 0) ? p.min * Math.pow(p.max / p.min, t) : p.min + t * (p.max - p.min)
        return p.integer ? Math.round(v) : v
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: root.p.name; color: Theme.textDim; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        Text {
            visible: !root.p.toggle && (root.p.choices || []).length === 0
            text: root.pm.text(root.slot, root.p.index, root.value)
            color: Theme.text; font.pixelSize: 12; font.family: Theme.mono; font.weight: Font.DemiBold
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onDoubleClicked: root.pm.resetValue(root.slot, root.p.index)
            }
        }
        ToggleSwitch {
            visible: root.p.toggle === true
            checked: root.value > 0.5
            accent: root.accent
            onToggled: (on) => root.pm.setValue(root.slot, root.p.index, on ? 1 : 0)
        }
    }

    MfComboBox {
        visible: !root.p.toggle && (root.p.choices || []).length > 0
        Layout.fillWidth: true
        model: root.p.choices || []
        currentIndex: {
            var cv = root.p.choiceValues || []
            var best = -1, bestD = 1e9
            for (var i = 0; i < cv.length; ++i) { var d = Math.abs(cv[i] - root.value); if (d < bestD) { bestD = d; best = i } }
            return best
        }
        onActivated: (i) => root.pm.setValue(root.slot, root.p.index, root.p.choiceValues[i])
    }

    Slider {
        id: slider
        visible: !root.p.toggle && (root.p.choices || []).length === 0
        Layout.fillWidth: true
        from: 0; to: 1
        value: root.toPos(root.value)
        onMoved: root.pm.setValue(root.slot, root.p.index, root.fromPos(position))
        implicitHeight: 22
        padding: 0
        background: Item {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            Rectangle { anchors.fill: parent; radius: 3; color: Theme.track }
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 3; color: root.accent; opacity: 0.85 }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8
            color: slider.pressed ? Theme.knobPressed : Theme.knob
            border.color: root.accent
            border.width: 3
        }
    }
}
