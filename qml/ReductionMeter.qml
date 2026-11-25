import QtQuick

// Gain-reduction meter: bar grows DOWN from the top as a processor turns the signal down.
Item {
    id: root
    property real reduction: 0     // positive dB
    property real maxDb: 24
    property string label: "GR"
    property color accent: Theme.comp
    property real shown: 0

    implicitWidth: 30
    implicitHeight: 200

    onReductionChanged: shown = reduction > shown ? reduction : Math.max(reduction, shown - 0.8)

    Rectangle {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: labelText.top; bottomMargin: 6 }
        radius: 5
        color: "#0e1015"
        border.color: Theme.border
        clip: true
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 2 }
            height: (parent.height - 4) * Math.min(1, root.shown / root.maxDb)
            radius: 3
            color: root.accent
            opacity: 0.9
        }
        Repeater {
            model: [6, 12, 18]
            Rectangle {
                visible: modelData < root.maxDb
                width: bar.width; height: 1
                y: bar.height * modelData / root.maxDb
                color: Theme.border
            }
        }
    }
    Text {
        id: labelText
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter }
        text: root.label
        color: Theme.textDim
        font.pixelSize: 10
        font.weight: Font.DemiBold
    }
}
