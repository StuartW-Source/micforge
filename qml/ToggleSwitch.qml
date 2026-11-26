import QtQuick

// Compact on/off switch bound to a controller parameter (or a plain bool via `checked`).
Item {
    id: root
    property string paramKey: ""
    property color accent: Theme.input
    property bool checked: paramKey !== "" ? controller.params[paramKey] > 0.5 : false
    signal toggled(bool on)
    property string label: ""             // spoken name for screen readers

    function toggle() {
        if (!enabled) return
        var on = !checked
        if (paramKey !== "") controller.setParam(paramKey, on ? 1 : 0)
        toggled(on)
    }

    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onToggleAction: toggle()
    Accessible.onPressAction: toggle()
    Keys.onSpacePressed: toggle()
    Keys.onReturnPressed: toggle()

    implicitWidth: 40
    implicitHeight: 22

    Rectangle {
        id: track
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
            color: root.checked ? Theme.onAccent : Theme.textDim
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggle()
    }
    FocusRing { target: root; radius: track.radius + gap }
}
