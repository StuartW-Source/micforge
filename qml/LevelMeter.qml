import QtQuick

// Vertical level meter (dB). Optional: a draggable threshold line, a dashed close line,
// and shaded "zones" from the voice-separation setup (room / others / you).
Item {
    id: root
    property real level: -120          // incoming dB value (max since last poll)
    property real minDb: -60
    property real maxDb: 0
    property bool showScale: true
    property string label: ""
    property color accent: Theme.input

    property bool hasThreshold: false
    property real threshold: -20
    property real secondaryThreshold: NaN   // e.g. gate close point (dashed)
    property string thresholdParam: ""      // dragging writes this parameter
    property color thresholdColor: Theme.warn

    property var zones: null                // {valid, room, others, you, youTilt, othersTilt}
    property bool zoneLabels: false
    readonly property bool hasZones: zones !== null && zones !== undefined && zones.valid === true

    // Internal smoothed display values.
    property real shown: -120
    property real peak: -120
    property int peakAge: 0

    implicitWidth: (showScale ? 58 : 30) + (zoneLabels ? 58 : 0)
    implicitHeight: 200

    function frac(db) { return Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb))) }
    function yOf(db) { return (1 - frac(db)) * bar.height }
    function dbAt(y) { return minDb + (1 - y / bar.height) * (maxDb - minDb) }

    onLevelChanged: {
        shown = level > shown ? level : Math.max(level, shown - 1.6)
        if (level >= peak) { peak = level; peakAge = 0 }
        else if (++peakAge > 40) peak = Math.max(level, peak - 1.2)
    }

    // dB scale.
    Item {
        id: scale
        visible: root.showScale
        width: 24
        anchors { left: parent.left; top: parent.top; bottom: labelText.top; bottomMargin: 6 }
        Repeater {
            model: [0, -6, -12, -18, -24, -36, -48, -60, -72]
            Text {
                visible: modelData >= root.minDb && modelData <= root.maxDb
                text: modelData
                color: Theme.textFaint
                font.pixelSize: 9
                font.family: Theme.mono
                anchors.right: parent.right
                anchors.rightMargin: 4
                y: root.yOf(modelData) - height / 2
            }
        }
    }

    Rectangle {
        id: bar
        anchors {
            left: root.showScale ? scale.right : parent.left
            right: parent.right; rightMargin: root.zoneLabels ? 58 : 0
            top: parent.top; bottom: labelText.top; bottomMargin: 6
        }
        radius: 5
        color: "#0e1015"
        border.color: Theme.border
        clip: true

        // Measured zones (behind the level).
        Rectangle {
            visible: root.hasZones
            width: parent.width; y: root.yOf(root.hasZones ? root.zones.room : -200)
            height: parent.height - y
            color: Theme.textFaint; opacity: 0.18
        }
        Rectangle {
            visible: root.hasZones && root.zones.others > root.zones.room
            width: parent.width
            y: root.yOf(root.hasZones ? root.zones.others : -200)
            height: root.hasZones ? root.yOf(root.zones.room) - y : 0
            color: Theme.tone; opacity: 0.22
        }
        Rectangle {
            visible: root.hasZones
            width: parent.width; y: 0
            height: root.yOf(root.hasZones ? root.zones.you : 0)
            color: Theme.gate; opacity: 0.12
        }

        // Coloured fill (green -> yellow -> red), revealed from the bottom.
        Item {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 }
            height: (parent.height - 4) * root.frac(root.shown)
            clip: true
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: bar.height - 4
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.danger }
                    GradientStop { position: Math.max(0.001, 1 - root.frac(-3)); color: Theme.danger }
                    GradientStop { position: 1 - root.frac(-9); color: Theme.warn }
                    GradientStop { position: 1 - root.frac(-18); color: root.accent }
                    GradientStop { position: 1.0; color: Qt.darker(root.accent, 1.6) }
                }
            }
        }

        // Peak hold tick.
        Rectangle {
            visible: root.peak > root.minDb
            width: parent.width - 4; x: 2; height: 2
            y: 2 + (bar.height - 4) * (1 - root.frac(root.peak)) - 1
            color: root.peak > -1 ? Theme.danger : Theme.text
            opacity: 0.8
        }

        // Secondary (dashed) threshold.
        Row {
            visible: root.hasThreshold && !isNaN(root.secondaryThreshold)
            y: root.yOf(root.secondaryThreshold)
            spacing: 3
            Repeater {
                model: Math.max(0, Math.ceil(bar.width / 6))
                Rectangle { width: 3; height: 1; color: root.thresholdColor; opacity: 0.8 }
            }
        }
    }

    // Zone labels to the right of the bar.
    Item {
        visible: root.zoneLabels && root.hasZones
        anchors { left: bar.right; leftMargin: 6; top: bar.top; bottom: bar.bottom; right: parent.right }
        Text {
            y: Math.min(parent.height - height, Math.max(0, root.yOf(root.hasZones ? root.zones.you : 0) / 2 - height / 2))
            text: "You"; color: Theme.gate; font.pixelSize: 11; font.weight: Font.Bold
        }
        Text {
            visible: root.hasZones && root.zones.others > root.zones.room
            y: root.hasZones ? (root.yOf(root.zones.others) + root.yOf(root.zones.room)) / 2 - height / 2 : 0
            text: "Others"; color: Theme.tone; font.pixelSize: 11; font.weight: Font.Bold
        }
        Text {
            y: root.hasZones ? Math.min(parent.height - height, (root.yOf(root.zones.room) + parent.height) / 2 - height / 2) : 0
            text: "Room"; color: Theme.textDim; font.pixelSize: 11; font.weight: Font.Bold
        }
    }

    // Threshold line + drag handle (sits on top, can extend past the bar).
    Item {
        visible: root.hasThreshold
        x: bar.x - 4
        width: bar.width + 8
        height: 14
        y: bar.y + root.yOf(root.threshold) - height / 2
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 2; color: root.thresholdColor }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            width: 8; height: 12; radius: 2
            color: root.thresholdColor
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            enabled: root.thresholdParam !== ""
            cursorShape: Qt.SizeVerCursor
            preventStealing: true
            onPositionChanged: (mouse) => {
                if (!pressed) return
                var p = mapToItem(bar, mouse.x, mouse.y)
                var db = Math.round(root.dbAt(p.y) * 2) / 2
                controller.setParam(root.thresholdParam, Math.max(root.minDb, Math.min(root.maxDb, db)))
            }
        }
    }

    Text {
        id: labelText
        anchors { bottom: parent.bottom; horizontalCenter: bar.horizontalCenter }
        text: root.label
        color: Theme.textDim
        font.pixelSize: 10
        font.weight: Font.DemiBold
    }
}
