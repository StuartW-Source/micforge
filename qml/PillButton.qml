import QtQuick

// Rounded button used in the top bar. `checkable` makes it a toggle.
Rectangle {
    id: root
    property string text: ""
    property string icon: ""
    property color accent: Theme.input
    property bool checkable: false
    property bool checked: false
    property bool primary: false
    signal clicked()

    implicitHeight: 34
    implicitWidth: row.implicitWidth + 28
    radius: height / 2
    color: checked ? Qt.rgba(accent.r, accent.g, accent.b, 0.22)
                   : primary ? accent
                   : mouse.containsMouse ? Theme.cardHi : Theme.card
    border.color: checked ? accent : primary ? accent : Theme.border
    opacity: enabled ? 1 : 0.45
    Behavior on color { ColorAnimation { duration: 100 } }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7
        Text {
            visible: root.icon !== ""
            text: root.icon
            color: root.primary && !root.checked ? "#10131a" : (root.checked ? root.accent : Theme.text)
            font.pixelSize: 14
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.text
            color: root.primary && !root.checked ? "#10131a" : (root.checked ? root.accent : Theme.text)
            font.pixelSize: 13
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
