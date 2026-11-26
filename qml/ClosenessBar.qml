import QtQuick

// Live "how close is this sound?" bar. Left = far away / thin, right = close / bass-heavy.
// Shows the measured "others" and "you" points from Setup and the threshold needed to open.
Item {
    id: root
    property real live: -120          // current closeness in dB (-120 = silence)
    property real threshold: 6
    property bool hasMarks: false
    property bool hasOthersMark: false
    property real youMark: 20
    property real othersMark: 0
    property real minDb: -15
    property real maxDb: 40
    property bool enabledCheck: true
    implicitHeight: 52

    property real shown: -120
    property int quietTicks: 0
    onLiveChanged: {
        if (live < -100) { if (++quietTicks > 8) shown = -120; return }
        quietTicks = 0
        shown = shown < -100 ? live : shown * 0.6 + live * 0.4
    }

    function xOf(db) { return Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb))) * bar.width }

    Rectangle {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 22
        radius: 11
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Theme.alpha(Theme.tone, 0.25) }
            GradientStop { position: 1; color: Theme.alpha(Theme.gate, 0.25) }
        }
        border.color: Theme.border
        opacity: root.enabledCheck ? 1 : 0.5

        // Measured marks.
        Rectangle { visible: root.hasOthersMark; x: root.xOf(root.othersMark) - 1; width: 3; height: parent.height; color: Theme.tone }
        Rectangle { visible: root.hasMarks; x: root.xOf(root.youMark) - 1; width: 3; height: parent.height; color: Theme.gate }
        // Threshold.
        Rectangle { visible: root.enabledCheck; x: root.xOf(root.threshold) - 1; width: 3; height: parent.height + 6; y: -3; color: Theme.warn }
        // Live marker.
        Rectangle {
            visible: root.shown > -100
            x: root.xOf(root.shown) - width / 2
            y: (parent.height - height) / 2
            width: 14; height: 14; radius: 7
            color: Theme.text
            border.color: Theme.onAccent; border.width: 2
            Behavior on x { NumberAnimation { duration: 80 } }
        }
    }
    Text { anchors { left: parent.left; top: bar.bottom; topMargin: 5 } text: "← far away"; color: Theme.textFaint; font.pixelSize: 10 }
    Text {
        visible: root.hasOthersMark
        x: Math.max(60, Math.min(bar.width - width - 60, root.xOf(root.othersMark) - width / 2)); anchors { top: bar.bottom; topMargin: 5 }
        text: "others"; color: Theme.tone; font.pixelSize: 10; font.bold: true
    }
    Text {
        visible: root.hasMarks
        x: Math.max(60, Math.min(bar.width - width - 60, root.xOf(root.youMark) - width / 2)); anchors { top: bar.bottom; topMargin: 5 }
        text: "you"; color: Theme.gate; font.pixelSize: 10; font.bold: true
    }
    Text { anchors { right: parent.right; top: bar.bottom; topMargin: 5 } text: "close up →"; color: Theme.textFaint; font.pixelSize: 10 }
}
