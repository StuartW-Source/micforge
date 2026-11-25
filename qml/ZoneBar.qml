import QtQuick

// Horizontal summary of the voice-separation result: room / others / you zones,
// the gate's open (yellow) and close (dashed) points, and a live marker.
Item {
    id: root
    property real room: -70
    property real others: -200
    property real you: -20
    property real threshold: -30
    property real closeAt: -34
    property real live: -120
    property real minDb: -80
    property real maxDb: 0
    implicitHeight: 64

    function xOf(db) { return Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb))) * bar.width }

    property real shown: -120
    onLiveChanged: shown = live > shown ? live : Math.max(live, shown - 1.5)

    Rectangle {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 30
        radius: 6
        color: "#0e1015"
        border.color: Theme.border
        clip: true

        Rectangle { x: 0; width: root.xOf(root.room); height: parent.height; color: Theme.textFaint; opacity: 0.3 }
        Rectangle {
            visible: root.others > root.room
            x: root.xOf(root.room); width: root.xOf(root.others) - x; height: parent.height
            color: Theme.tone; opacity: 0.35
        }
        Rectangle { x: root.xOf(root.you); width: parent.width - x; height: parent.height; color: Theme.gate; opacity: 0.25 }

        // Live level.
        Rectangle {
            x: 0; y: parent.height / 2 - 3
            width: root.xOf(root.shown); height: 6; radius: 3
            color: Theme.text; opacity: 0.85
        }
        // Close (dashed) and open (solid) points.
        Column {
            x: root.xOf(root.closeAt)
            spacing: 3
            Repeater { model: 6; Rectangle { width: 1; height: 3; color: Theme.warn } }
        }
        Rectangle { x: root.xOf(root.threshold) - 1; width: 3; height: parent.height; color: Theme.warn }
    }

    Text { x: Math.max(0, root.xOf(root.room) / 2 - width / 2); y: bar.height + 6; text: "Room"; color: Theme.textDim; font.pixelSize: 11 }
    Text {
        visible: root.others > root.room
        x: (root.xOf(root.room) + root.xOf(root.others)) / 2 - width / 2; y: bar.height + 6
        text: "Others"; color: Theme.tone; font.pixelSize: 11; font.bold: true
    }
    Text {
        x: Math.min(bar.width - width, (root.xOf(root.you) + bar.width) / 2 - width / 2); y: bar.height + 6
        text: "You"; color: Theme.gate; font.pixelSize: 11; font.bold: true
    }
    Text {
        x: Math.min(bar.width - width, Math.max(0, root.xOf(root.threshold) - width / 2)); y: bar.height + 20
        text: "gate opens"; color: Theme.warn; font.pixelSize: 10
    }
}
