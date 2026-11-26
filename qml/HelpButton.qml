import QtQuick
import QtQuick.Controls

// Small "?" that explains a control in plain English on hover or click.
Rectangle {
    id: root
    property string text: ""
    width: 18; height: 18; radius: 9
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: "Help"
    Accessible.description: text
    Keys.onSpacePressed: tip.visible = !tip.visible
    Keys.onReturnPressed: tip.visible = !tip.visible
    Keys.onEscapePressed: tip.visible = false
    color: hover.hovered || tip.visible ? Theme.cardHi : "transparent"
    border.color: Theme.border

    Text {
        anchors.centerIn: parent
        text: "?"
        color: Theme.textDim
        font.pixelSize: 11
        font.bold: true
    }
    HoverHandler { id: hover; cursorShape: Qt.WhatsThisCursor }
    TapHandler { onTapped: tip.visible = !tip.visible }

    ToolTip {
        id: tip
        visible: hover.hovered || root.activeFocus
        delay: 250
        width: Math.min(320, implicitContentWidth + 24)
        contentItem: Text {
            text: root.text
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: 12
            lineHeight: 1.2
        }
        background: Rectangle { color: Theme.tooltip; border.color: Theme.border; radius: 8 }
    }
    FocusRing {}
}
