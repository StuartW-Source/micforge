import QtQuick

// Segmented choice (e.g. Left / Right / Both) bound to an integer parameter.
Rectangle {
    id: root
    property string paramKey: ""
    property var options: []
    property color accent: Theme.input
    readonly property int current: Math.round(controller.params[paramKey])

    implicitHeight: 30
    radius: 8
    color: Theme.track

    Row {
        anchors.fill: parent
        anchors.margins: 3
        Repeater {
            model: root.options
            Rectangle {
                width: (root.width - 6) / root.options.length
                height: parent.height
                radius: 6
                color: index === root.current ? root.accent : (m.containsMouse ? Theme.cardHi : "transparent")
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: index === root.current ? "#10131a" : Theme.textDim
                }
                MouseArea {
                    id: m
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: controller.setParam(root.paramKey, index)
                }
            }
        }
    }
}
