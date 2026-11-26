import QtQuick

// Keyboard focus indicator. Place inside the focusable item; it only shows while that item has
// keyboard focus (mouse clicks on these custom controls never move focus).
Rectangle {
    property Item target: parent
    property real gap: 3
    anchors.fill: parent
    anchors.margins: -gap
    radius: (target && target.radius !== undefined ? target.radius : 6) + gap
    color: "transparent"
    border.color: Theme.focus
    border.width: 2
    visible: target ? target.activeFocus : false
    z: 100
}
