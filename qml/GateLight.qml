import QtQuick

// Live "is my voice getting through?" indicator for the Voice Gate (held lit briefly to avoid flicker).
Rectangle {
    id: root
    property var app
    property bool sticky: false
    readonly property var p: controller.params
    readonly property bool gateActive: p.gateAdded > 0.5 && p.gateOn > 0.5 && !app.bypassed && !controller.muted
    readonly property bool lit: (controller.gateOpen || sticky) && gateActive

    implicitHeight: 40
    radius: Theme.radiusSm + 4
    color: lit ? Theme.alpha(Theme.gate, 0.2) : Theme.well
    border.color: lit ? Theme.gate : Theme.border
    Accessible.role: Accessible.Indicator
    Accessible.name: label.text

    Connections {
        target: controller
        function onMetersChanged() { if (controller.gateOpen) { root.sticky = true; hold.restart() } }
    }
    Timer { id: hold; interval: 150; onTriggered: root.sticky = false }
    Text {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, parent.width - 12)
        elide: Text.ElideRight
        text: controller.muted ? "Muted"
            : !root.gateActive ? "Voice Gate off"
            : root.lit ? "● Your voice is passing"
            : controller.gateReason === "far" ? "Closed: not close to the mic"
            : controller.gateReason === "noise" ? "Closed: background noise"
            : controller.gateReason === "notvoice" ? "Closed: not a voice"
            : "Gate closed"
        color: root.lit ? Theme.gate : Theme.textFaint
        font.pixelSize: Theme.fsBody
        font.weight: Font.Bold
    }
}
