import QtQuick
import QtQuick.Controls

// One entry in the sidebar. `compact` shows only the glyph (narrow windows).
Rectangle {
    id: root
    property string text: ""
    property string glyph: ""
    property bool selected: false
    property bool compact: false
    property string shortcutHint: ""
    signal clicked()

    implicitHeight: 40
    radius: Theme.radiusSm + 2
    color: selected ? Theme.tint(Theme.focus) : mouse.containsMouse ? Theme.cardHi : "transparent"

    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.selected: selected
    Accessible.onPressAction: clicked()
    Keys.onSpacePressed: clicked()
    Keys.onReturnPressed: clicked()

    Rectangle {
        visible: root.selected
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        width: 3; height: parent.height - 16; radius: 2
        color: Theme.focus
    }
    Row {
        anchors { left: parent.left; leftMargin: root.compact ? (root.width - glyphText.width) / 2 : Theme.sp3 + 2; verticalCenter: parent.verticalCenter }
        spacing: Theme.sp3
        Text {
            id: glyphText
            width: 20
            horizontalAlignment: Text.AlignHCenter
            text: root.glyph
            color: root.selected ? Theme.focus : Theme.textDim
            font.pixelSize: 16
        }
        Text {
            visible: !root.compact
            text: root.text
            color: root.selected ? Theme.text : Theme.textDim
            font.pixelSize: Theme.fsBodyLg
            font.weight: root.selected ? Font.DemiBold : Font.Normal
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
    ToolTipLite {
        visible: root.compact && mouse.containsMouse
        text: root.text + (root.shortcutHint !== "" ? "  (" + root.shortcutHint + ")" : "")
    }
    FocusRing { gap: 1 }
}
