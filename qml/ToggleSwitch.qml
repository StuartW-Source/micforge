import QtQuick

// Compact on/off switch bound to a controller parameter (or a plain bool via `checked`).
Item {
    id: root
    property string paramKey: ""
    property color accent: Theme.input
    property bool checked: paramKey !== "" ? controller.params[paramKey] > 0.5 : false
    signal toggled(bool on)

    implicitWidth: 40
    implicitHeight: 22

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.accent : Theme.track
        opacity: root.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            width: parent.height - 6
            height: width
            radius: width / 2
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? "#10131a" : Theme.textDim
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            var on = !root.checked
            if (root.paramKey !== "") controller.setParam(root.paramKey, on ? 1 : 0)
            root.toggled(on)
        }
    }
}
