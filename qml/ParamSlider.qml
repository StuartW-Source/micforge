import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Labelled slider bound to a controller parameter.
// Double-click the value to reset it to the default.
ColumnLayout {
    id: root
    property string paramKey: ""
    property string label: ""
    property string help: ""
    property real from: 0
    property real to: 1
    property real stepSize: 0
    property bool logScale: false
    property bool bipolar: false          // fill from the middle (for ± controls)
    property color accent: Theme.input
    property var format: function(v) { return v.toFixed(1) }

    readonly property real value: controller.params[paramKey]

    spacing: 4
    Layout.fillWidth: true

    function toPos(v) {
        if (logScale) return Math.log(v / from) / Math.log(to / from)
        return (v - from) / (to - from)
    }
    function fromPos(p) {
        var v = logScale ? from * Math.pow(to / from, p) : from + p * (to - from)
        if (stepSize > 0) v = Math.round(v / stepSize) * stepSize
        return v
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Text {
            text: root.label
            color: Theme.textDim
            font.pixelSize: 12
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        HelpButton { visible: root.help !== ""; text: root.help }
        Text {
            id: valueText
            text: root.format(root.value)
            color: Theme.text
            font.pixelSize: 12
            font.family: Theme.mono
            font.weight: Font.DemiBold
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onDoubleClicked: controller.resetParam(root.paramKey)
                ToolTip.visible: containsMouse
                ToolTip.delay: 800
                ToolTip.text: "Double-click to reset"
                hoverEnabled: true
            }
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: 0; to: 1
        value: root.toPos(root.value)
        onMoved: controller.setParam(root.paramKey, root.fromPos(position))
        implicitHeight: 22
        padding: 0
        activeFocusOnTab: true
        Accessible.name: root.label
        // Keyboard: arrow keys move 1%, Page Up/Down 10%, Home resets to the default.
        stepSize: 0.01
        Keys.onPressed: (e) => {
            var step = e.key === Qt.Key_PageUp ? 0.1 : e.key === Qt.Key_PageDown ? -0.1 : 0
            if (step !== 0) { controller.setParam(root.paramKey, root.fromPos(Math.max(0, Math.min(1, position + step)))); e.accepted = true }
            else if (e.key === Qt.Key_Home) { controller.resetParam(root.paramKey); e.accepted = true }
        }

        background: Item {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            Rectangle { anchors.fill: parent; radius: 3; color: Theme.track }
            Rectangle {
                readonly property real centre: root.bipolar ? 0.5 : 0
                x: Math.min(centre, slider.visualPosition) * parent.width
                width: Math.abs(slider.visualPosition - centre) * parent.width
                height: parent.height
                radius: 3
                color: root.accent
                opacity: root.enabled ? 0.85 : 0.3
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8
            color: slider.pressed ? Theme.knobPressed : Theme.knob
            border.color: root.accent
            border.width: 3
            opacity: root.enabled ? 1 : 0.4
            FocusRing { target: slider; radius: 11 }
        }
    }
}
