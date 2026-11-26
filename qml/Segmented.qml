import QtQuick

// Segmented choice (e.g. Left / Right / Both) bound to an integer parameter.
Rectangle {
    id: root
    property string paramKey: ""
    property var options: []
    property color accent: Theme.input
    readonly property int current: Math.round(controller.params[paramKey])

    function choose(i) { if (enabled && i >= 0 && i < options.length) controller.setParam(paramKey, i) }

    activeFocusOnTab: true
    Accessible.role: Accessible.PageTabList
    Keys.onLeftPressed: choose(current - 1)
    Keys.onRightPressed: choose(current + 1)
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
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData
                Accessible.selected: index === root.current
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: index === root.current ? Theme.onAccent : Theme.textDim
                }
                MouseArea {
                    id: m
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.choose(index)
                }
            }
        }
    }
    FocusRing {}
}
